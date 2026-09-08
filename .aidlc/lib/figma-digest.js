#!/usr/bin/env node
/*
 * Reduce a Figma REST response (or a single frame node) to a deterministic,
 * model-friendly JSON tree. The implementation is deliberately whitelist-only:
 * vectors, binary payloads, plugin data, and other unneeded REST fields can
 * never leak into the digest by accident.
 */
"use strict";

const fs = require("fs");
const path = require("path");

const TEXT_LIMIT = 1200;
const STRING_LIMIT = 300;
const STYLE_OVERRIDE_LIMIT = 24;
const VECTOR_TYPES = new Set(["VECTOR", "BOOLEAN_OPERATION", "STAR", "LINE", "ELLIPSE", "POLYGON"]);
const OMITTED_PAYLOAD_KEYS = [
  "fillGeometry", "strokeGeometry", "vectorNetwork", "vectorPaths", "path", "paths",
  "pluginData", "sharedPluginData", "imageData", "blob", "bytes", "binaryData",
];

function usage() {
  return "usage: node figma-digest.js <input.json> [output.json]";
}

function fail(message) {
  process.stderr.write("\u2717 figma-digest: " + message + "\n");
  process.exitCode = 1;
}

function isObject(value) {
  return value !== null && typeof value === "object" && !Array.isArray(value);
}

function hasOwn(object, key) {
  return Object.prototype.hasOwnProperty.call(object, key);
}

function finite(value) {
  return typeof value === "number" && Number.isFinite(value);
}

function rounded(value) {
  if (!finite(value)) return undefined;
  const result = Math.round(value * 1000) / 1000;
  return Object.is(result, -0) ? 0 : result;
}

function codePointLength(value) {
  let count = 0;
  for (let i = 0; i < value.length; i += 1) {
    const code = value.charCodeAt(i);
    if (code >= 0xd800 && code <= 0xdbff && i + 1 < value.length) {
      const next = value.charCodeAt(i + 1);
      if (next >= 0xdc00 && next <= 0xdfff) i += 1;
    }
    count += 1;
  }
  return count;
}

function takeCodePoints(value, count, fromEnd) {
  if (!fromEnd) {
    let points = 0;
    let index = 0;
    while (index < value.length && points < count) {
      const code = value.charCodeAt(index++);
      if (code >= 0xd800 && code <= 0xdbff && index < value.length) {
        const next = value.charCodeAt(index);
        if (next >= 0xdc00 && next <= 0xdfff) index += 1;
      }
      points += 1;
    }
    return value.slice(0, index);
  }
  let points = 0;
  let index = value.length;
  while (index > 0 && points < count) {
    const code = value.charCodeAt(--index);
    if (code >= 0xdc00 && code <= 0xdfff && index > 0) {
      const previous = value.charCodeAt(index - 1);
      if (previous >= 0xd800 && previous <= 0xdbff) index -= 1;
    }
    points += 1;
  }
  return value.slice(index);
}

function compactString(value, limit) {
  if (value === undefined || value === null) return undefined;
  const string = String(value);
  const max = limit || STRING_LIMIT;
  const length = codePointLength(string);
  if (length <= max) return string;
  return takeCodePoints(string, Math.max(1, max - 1), false) + "\u2026";
}

function truncateText(value, stats) {
  const text = String(value);
  const length = codePointLength(text);
  if (length <= TEXT_LIMIT) return text;
  const headCount = 900;
  const tailCount = 240;
  const omitted = length - headCount - tailCount;
  stats.truncated_text_nodes += 1;
  stats.text_chars_omitted += omitted;
  return takeCodePoints(text, headCount, false) + "\u2026[" + omitted + " chars omitted]\u2026" + takeCodePoints(text, tailCount, true);
}

function stableKeys(object) {
  return Object.keys(object || {}).sort((a, b) => {
    const aNumber = /^\d+$/.test(a) ? Number(a) : NaN;
    const bNumber = /^\d+$/.test(b) ? Number(b) : NaN;
    if (Number.isFinite(aNumber) && Number.isFinite(bNumber)) return aNumber - bNumber;
    return a < b ? -1 : a > b ? 1 : 0;
  });
}

function nonEmpty(object) {
  return object && Object.keys(object).length > 0;
}

function isFigmaNode(value) {
  return isObject(value) && typeof value.type === "string" && (value.id !== undefined || Array.isArray(value.children));
}

function componentMetadata(id, value, kind) {
  const result = { id: compactString(id) };
  if (isObject(value)) {
    if (value.key !== undefined) result.key = compactString(value.key);
    if (value.name !== undefined) result.name = compactString(value.name);
    if (value.componentSetId !== undefined) result.setId = compactString(value.componentSetId);
    if (value.remote === true) result.remote = true;
  }
  if (kind === "set") result.kind = "set";
  return result;
}

function normalizeInput(input) {
  const roots = [];
  const rootObjects = new WeakSet();
  const wrappers = new WeakSet();
  const components = new Map();
  const file = Object.create(null);

  function addRegistry(registry, kind) {
    if (!isObject(registry)) return;
    stableKeys(registry).forEach((id) => {
      components.set(String(id), componentMetadata(id, registry[id], kind));
    });
  }

  function addRoot(node) {
    if (!isFigmaNode(node) || rootObjects.has(node)) return;
    rootObjects.add(node);
    roots.push(node);
  }

  function visitWrapper(value) {
    if (Array.isArray(value)) {
      value.forEach(visitWrapper);
      return;
    }
    if (!isObject(value) || wrappers.has(value)) return;
    wrappers.add(value);
    addRegistry(value.components, "component");
    addRegistry(value.componentSets, "set");

    if (isFigmaNode(value)) {
      addRoot(value);
      return;
    }
    if (file.name === undefined && typeof value.name === "string") file.name = compactString(value.name);
    if (file.version === undefined && value.version !== undefined) file.version = compactString(value.version);
    if (file.lastModified === undefined && value.lastModified !== undefined) file.lastModified = compactString(value.lastModified);
    if (file.schemaVersion === undefined && value.schemaVersion !== undefined) file.schemaVersion = Number(value.schemaVersion) || compactString(value.schemaVersion);

    if (value.document !== undefined) visitWrapper(value.document);
    if (value.frame !== undefined) visitWrapper(value.frame);
    if (value.frames !== undefined) visitWrapper(value.frames);
    if (isObject(value.nodes)) stableKeys(value.nodes).forEach((id) => visitWrapper(value.nodes[id]));
    if (value.file !== undefined) visitWrapper(value.file);
    if (value.data !== undefined) visitWrapper(value.data);
  }

  visitWrapper(input);
  if (!roots.length) throw new Error("input contains no Figma document or frame nodes");

  function indexComponents(node) {
    if (!isFigmaNode(node)) return;
    if ((node.type === "COMPONENT" || node.type === "COMPONENT_SET") && node.id !== undefined) {
      const id = String(node.id);
      const existing = components.get(id) || { id: compactString(id) };
      if (existing.name === undefined && node.name !== undefined) existing.name = compactString(node.name);
      if (existing.key === undefined && node.key !== undefined) existing.key = compactString(node.key);
      if (existing.setId === undefined && node.componentSetId !== undefined) existing.setId = compactString(node.componentSetId);
      if (node.type === "COMPONENT_SET") existing.kind = "set";
      components.set(id, existing);
    }
    if (Array.isArray(node.children)) node.children.forEach(indexComponents);
  }
  roots.forEach(indexComponents);
  return { roots, components, file };
}

function simplifyBounds(node) {
  const source = [node.absoluteBoundingBox, node.boundingBox, node.absoluteRenderBounds, node.bounds]
    .find((candidate) => isObject(candidate));
  const fallback = isObject(node.size)
    ? { x: node.x, y: node.y, width: node.size.x, height: node.size.y }
    : { x: node.x, y: node.y, width: node.width, height: node.height };
  const box = source || fallback;
  const result = Object.create(null);
  for (const key of ["x", "y", "width", "height"]) {
    const value = rounded(box[key]);
    if (value !== undefined) result[key] = value;
  }
  return Object.keys(result).length >= 2 ? result : undefined;
}

function assignString(target, outputKey, source, inputKey) {
  if (source[inputKey] !== undefined && source[inputKey] !== null && source[inputKey] !== "") {
    target[outputKey] = compactString(source[inputKey], 120);
  }
}

function assignNumber(target, outputKey, source, inputKey, omitZero) {
  const value = rounded(source[inputKey]);
  if (value !== undefined && (!omitZero || value !== 0)) target[outputKey] = value;
}

function simplifyLayout(node) {
  const result = Object.create(null);
  assignString(result, "mode", node, "layoutMode");
  assignString(result, "wrap", node, "layoutWrap");
  assignString(result, "position", node, "layoutPositioning");
  assignString(result, "primarySizing", node, "primaryAxisSizingMode");
  assignString(result, "counterSizing", node, "counterAxisSizingMode");
  assignString(result, "horizontalSizing", node, "layoutSizingHorizontal");
  assignString(result, "verticalSizing", node, "layoutSizingVertical");
  assignString(result, "primaryAlign", node, "primaryAxisAlignItems");
  assignString(result, "counterAlign", node, "counterAxisAlignItems");
  assignString(result, "align", node, "layoutAlign");
  assignNumber(result, "grow", node, "layoutGrow", true);
  assignNumber(result, "minWidth", node, "minWidth", false);
  assignNumber(result, "maxWidth", node, "maxWidth", false);
  assignNumber(result, "minHeight", node, "minHeight", false);
  assignNumber(result, "maxHeight", node, "maxHeight", false);
  if (node.preserveRatio === true) result.preserveRatio = true;
  if (node.clipsContent === true) result.clipsContent = true;
  if (isObject(node.constraints)) {
    const constraints = Object.create(null);
    assignString(constraints, "horizontal", node.constraints, "horizontal");
    assignString(constraints, "vertical", node.constraints, "vertical");
    if (nonEmpty(constraints)) result.constraints = constraints;
  }
  return nonEmpty(result) ? result : undefined;
}

function simplifyPadding(node) {
  let top = rounded(node.paddingTop);
  let right = rounded(node.paddingRight);
  let bottom = rounded(node.paddingBottom);
  let left = rounded(node.paddingLeft);
  const vertical = rounded(node.verticalPadding);
  const horizontal = rounded(node.horizontalPadding);
  if (top === undefined) top = vertical;
  if (bottom === undefined) bottom = vertical;
  if (left === undefined) left = horizontal;
  if (right === undefined) right = horizontal;
  const values = [top, right, bottom, left];
  if (values.every((value) => value === undefined || value === 0)) return undefined;
  if (values.every((value) => value !== undefined && value === values[0])) return values[0];
  const result = Object.create(null);
  ["top", "right", "bottom", "left"].forEach((key, index) => {
    if (values[index] !== undefined) result[key] = values[index];
  });
  return result;
}

function simplifySpacing(node) {
  const result = Object.create(null);
  assignNumber(result, "item", node, "itemSpacing", false);
  assignNumber(result, "cross", node, "counterAxisSpacing", false);
  assignNumber(result, "gap", node, "gap", false);
  const padding = simplifyPadding(node);
  if (padding !== undefined) result.padding = padding;
  return nonEmpty(result) ? result : undefined;
}

function simplifyCorners(node) {
  const radius = rounded(node.cornerRadius);
  if (radius !== undefined && radius !== 0) return radius;
  if (!Array.isArray(node.rectangleCornerRadii)) return undefined;
  const radii = node.rectangleCornerRadii.map(rounded);
  if (!radii.some((value) => value !== undefined && value !== 0)) return undefined;
  if (radii.every((value) => value !== undefined && value === radii[0])) return radii[0];
  return radii;
}

function channel(value) {
  if (!finite(value)) return 0;
  const scaled = value <= 1 && value >= 0 ? value * 255 : value;
  return Math.max(0, Math.min(255, Math.round(scaled)));
}

function colorHex(color) {
  if (typeof color === "string") return compactString(color, 40);
  if (!isObject(color) || !finite(color.r) || !finite(color.g) || !finite(color.b)) return undefined;
  const values = [channel(color.r), channel(color.g), channel(color.b)];
  if (finite(color.a) && color.a < 0.9995) values.push(channel(color.a));
  return "#" + values.map((value) => value.toString(16).padStart(2, "0").toUpperCase()).join("");
}

function variableId(value) {
  if (typeof value === "string") return compactString(value);
  if (isObject(value) && value.id !== undefined) return compactString(value.id);
  return undefined;
}

function simplifyPaint(paint, stats) {
  if (!isObject(paint) || paint.visible === false) return undefined;
  const result = Object.create(null);
  if (paint.type !== undefined) result.type = compactString(paint.type, 40);
  const color = colorHex(paint.color);
  if (color !== undefined) result.color = color;
  if (Array.isArray(paint.gradientStops)) {
    result.stops = paint.gradientStops.map((stop) => {
      const item = Object.create(null);
      const position = rounded(stop && stop.position);
      const stopColor = colorHex(stop && stop.color);
      if (position !== undefined) item.at = position;
      if (stopColor !== undefined) item.color = stopColor;
      return item;
    }).filter(nonEmpty);
  }
  if (paint.imageRef !== undefined) result.ref = compactString(paint.imageRef);
  if (paint.videoRef !== undefined) result.videoRef = compactString(paint.videoRef);
  assignString(result, "scale", paint, "scaleMode");
  const opacity = rounded(paint.opacity);
  if (opacity !== undefined && opacity !== 1) result.opacity = opacity;
  if (paint.blendMode !== undefined && paint.blendMode !== "NORMAL") result.blend = compactString(paint.blendMode, 40);
  if (isObject(paint.boundVariables)) {
    const id = variableId(paint.boundVariables.color);
    if (id !== undefined) result.variable = id;
  }
  if (!nonEmpty(result)) return undefined;
  stats.paint_count += 1;
  return result;
}

function simplifyPaints(paints, stats) {
  if (!Array.isArray(paints)) return undefined;
  const result = paints.map((paint) => simplifyPaint(paint, stats)).filter(Boolean);
  return result.length ? result : undefined;
}

function simplifyStrokeStyle(node) {
  const result = Object.create(null);
  assignNumber(result, "weight", node, "strokeWeight", false);
  assignNumber(result, "top", node, "strokeTopWeight", false);
  assignNumber(result, "right", node, "strokeRightWeight", false);
  assignNumber(result, "bottom", node, "strokeBottomWeight", false);
  assignNumber(result, "left", node, "strokeLeftWeight", false);
  assignString(result, "align", node, "strokeAlign");
  assignString(result, "cap", node, "strokeCap");
  assignString(result, "join", node, "strokeJoin");
  if (Array.isArray(node.dashPattern)) {
    const dash = node.dashPattern.map(rounded).filter((value) => value !== undefined);
    if (dash.length) result.dash = dash;
  }
  return nonEmpty(result) ? result : undefined;
}

function simplifyEffects(effects) {
  if (!Array.isArray(effects)) return undefined;
  const result = effects.filter((effect) => isObject(effect) && effect.visible !== false).map((effect) => {
    const item = Object.create(null);
    assignString(item, "type", effect, "type");
    const color = colorHex(effect.color);
    if (color !== undefined) item.color = color;
    if (isObject(effect.offset)) {
      const offset = Object.create(null);
      assignNumber(offset, "x", effect.offset, "x", false);
      assignNumber(offset, "y", effect.offset, "y", false);
      if (nonEmpty(offset)) item.offset = offset;
    }
    assignNumber(item, "radius", effect, "radius", false);
    assignNumber(item, "spread", effect, "spread", false);
    if (effect.blendMode !== undefined && effect.blendMode !== "NORMAL") item.blend = compactString(effect.blendMode, 40);
    return item;
  }).filter(nonEmpty);
  return result.length ? result : undefined;
}

function simplifyDimension(value, fallbackUnit) {
  if (finite(value)) return { value: rounded(value), unit: fallbackUnit };
  if (!isObject(value) || !finite(value.value)) return undefined;
  const result = { value: rounded(value.value) };
  if (value.unit !== undefined) result.unit = compactString(value.unit, 20);
  return result;
}

function simplifyTextStyle(style) {
  if (!isObject(style)) return undefined;
  const result = Object.create(null);
  assignString(result, "fontFamily", style, "fontFamily");
  assignString(result, "fontPostScript", style, "fontPostScriptName");
  assignNumber(result, "fontWeight", style, "fontWeight", false);
  assignNumber(result, "fontSize", style, "fontSize", false);
  if (style.italic === true) result.italic = true;
  assignString(result, "alignHorizontal", style, "textAlignHorizontal");
  assignString(result, "alignVertical", style, "textAlignVertical");
  const letterSpacing = simplifyDimension(style.letterSpacing, "PIXELS");
  if (letterSpacing) result.letterSpacing = letterSpacing;
  let lineHeight;
  if (finite(style.lineHeightPx)) lineHeight = { value: rounded(style.lineHeightPx), unit: "PIXELS" };
  else if (finite(style.lineHeightPercent)) lineHeight = { value: rounded(style.lineHeightPercent), unit: "PERCENT" };
  else if (style.lineHeight !== undefined) lineHeight = simplifyDimension(style.lineHeight, style.lineHeightUnit);
  if (lineHeight) result.lineHeight = lineHeight;
  assignString(result, "case", style, "textCase");
  assignString(result, "decoration", style, "textDecoration");
  assignNumber(result, "paragraphSpacing", style, "paragraphSpacing", true);
  assignNumber(result, "paragraphIndent", style, "paragraphIndent", true);
  return nonEmpty(result) ? result : undefined;
}

function simplifyTextOverrides(node, stats) {
  if (!isObject(node.styleOverrideTable)) return undefined;
  const keys = stableKeys(node.styleOverrideTable);
  const selected = keys.slice(0, STYLE_OVERRIDE_LIMIT);
  if (keys.length > selected.length) stats.style_overrides_omitted += keys.length - selected.length;
  const result = selected.map((id) => {
    const style = simplifyTextStyle(node.styleOverrideTable[id]);
    return style ? { id: compactString(id, 80), style } : null;
  }).filter(Boolean);
  return result.length ? result : undefined;
}

function simplifyPropertyValue(value) {
  if (value === null || typeof value === "boolean" || finite(value)) return value;
  if (typeof value === "string") return compactString(value, 240);
  if (!isObject(value)) return compactString(value, 240);
  if (value.type === "VARIABLE_ALIAS" && value.id !== undefined) return { variable: compactString(value.id) };
  if (value.id !== undefined && Object.keys(value).length <= 3) return compactString(value.id);
  return undefined;
}

function simplifyProperties(properties, definitionMode) {
  if (!isObject(properties)) return undefined;
  const result = Object.create(null);
  stableKeys(properties).forEach((name) => {
    const raw = properties[name];
    if (!isObject(raw)) {
      const value = simplifyPropertyValue(raw);
      if (value !== undefined) result[compactString(name, 180)] = value;
      return;
    }
    const item = Object.create(null);
    assignString(item, "type", raw, "type");
    const value = simplifyPropertyValue(definitionMode ? raw.defaultValue : raw.value);
    if (value !== undefined) item[definitionMode ? "default" : "value"] = value;
    if (Array.isArray(raw.variantOptions)) item.options = raw.variantOptions.map((option) => compactString(option, 120));
    if (Array.isArray(raw.preferredValues)) {
      item.preferred = raw.preferredValues.map((preferred) => simplifyPropertyValue(preferred)).filter((preferred) => preferred !== undefined);
    }
    if (nonEmpty(item)) result[compactString(name, 180)] = item;
  });
  return nonEmpty(result) ? result : undefined;
}

function simplifyComponent(node, registry, stats) {
  const result = Object.create(null);
  let componentId = node.componentId;
  if (componentId === undefined && isObject(node.mainComponent)) componentId = node.mainComponent.id;
  if (componentId !== undefined) {
    const id = String(componentId);
    result.id = compactString(id);
    const metadata = registry.get(id);
    if (metadata) {
      if (metadata.key !== undefined) result.key = metadata.key;
      if (metadata.name !== undefined) result.name = metadata.name;
      if (metadata.setId !== undefined) result.setId = metadata.setId;
      if (metadata.remote === true) result.remote = true;
    } else if (isObject(node.mainComponent)) {
      if (node.mainComponent.key !== undefined) result.key = compactString(node.mainComponent.key);
      if (node.mainComponent.name !== undefined) result.name = compactString(node.mainComponent.name);
    }
    stats.component_refs += 1;
  }
  if (node.componentSetId !== undefined && result.setId === undefined) result.setId = compactString(node.componentSetId);
  const properties = simplifyProperties(node.componentProperties, false);
  if (properties) result.properties = properties;
  if (isObject(node.variantProperties)) {
    const variants = Object.create(null);
    stableKeys(node.variantProperties).forEach((key) => {
      variants[compactString(key, 180)] = compactString(node.variantProperties[key], 180);
    });
    if (nonEmpty(variants)) result.variants = variants;
  }
  return nonEmpty(result) ? result : undefined;
}

function hiddenNode(node, hiddenByParent) {
  return hiddenByParent || node.visible === false || node.isVisible === false || (finite(node.opacity) && node.opacity <= 0);
}

function digestNode(node, registry, stats, hiddenByParent) {
  if (!isFigmaNode(node)) return null;
  stats.nodes_seen += 1;
  const hidden = hiddenNode(node, hiddenByParent);
  if (hidden) stats.hidden_nodes += 1;
  if (Array.isArray(node.children) && hidden) node.children.forEach((child) => digestNode(child, registry, stats, true));
  if (hidden) return null;

  stats.nodes_emitted += 1;
  const type = compactString(node.type, 80);
  if (VECTOR_TYPES.has(node.type)) stats.vector_nodes += 1;
  OMITTED_PAYLOAD_KEYS.forEach((key) => {
    if (hasOwn(node, key)) stats.payload_fields_omitted += 1;
  });

  const result = Object.create(null);
  if (node.id !== undefined) result.id = compactString(node.id);
  if (node.name !== undefined) result.name = compactString(node.name);
  result.type = type;

  const bounds = simplifyBounds(node);
  if (bounds) result.bounds = bounds;
  const rotation = rounded(node.rotation);
  if (rotation !== undefined && rotation !== 0) result.rotation = rotation;
  const opacity = rounded(node.opacity);
  if (opacity !== undefined && opacity !== 1) result.opacity = opacity;
  if (node.blendMode !== undefined && node.blendMode !== "PASS_THROUGH" && node.blendMode !== "NORMAL") {
    result.blend = compactString(node.blendMode, 40);
  }

  const layout = simplifyLayout(node);
  if (layout) result.layout = layout;
  const spacing = simplifySpacing(node);
  if (spacing) result.spacing = spacing;
  const corners = simplifyCorners(node);
  if (corners !== undefined) result.corners = corners;

  const fills = simplifyPaints(node.fills, stats);
  if (fills) result.fills = fills;
  else {
    const background = colorHex(node.backgroundColor);
    if (background !== undefined) result.background = background;
  }
  const strokes = simplifyPaints(node.strokes, stats);
  if (strokes) result.strokes = strokes;
  const strokeStyle = simplifyStrokeStyle(node);
  if (strokeStyle) result.strokeStyle = strokeStyle;
  const effects = simplifyEffects(node.effects);
  if (effects) result.effects = effects;

  const characters = typeof node.characters === "string" ? node.characters : (node.type === "TEXT" && typeof node.text === "string" ? node.text : null);
  if (characters !== null) {
    stats.text_nodes += 1;
    result.text = truncateText(characters, stats);
    const textStyle = simplifyTextStyle(node.style);
    if (textStyle) result.textStyle = textStyle;
    const textOverrides = simplifyTextOverrides(node, stats);
    if (textOverrides) result.textStyleOverrides = textOverrides;
  }

  const component = simplifyComponent(node, registry, stats);
  if (component) result.component = component;
  const definitions = simplifyProperties(node.componentPropertyDefinitions, true);
  if (definitions) result.componentPropertyDefinitions = definitions;

  if (Array.isArray(node.children)) {
    const children = node.children.map((child) => digestNode(child, registry, stats, false)).filter(Boolean);
    if (children.length) result.children = children;
  }
  return result;
}

function initialStats(inputBytes) {
  return {
    format: "aidlc-figma-digest/v1",
    input_bytes: inputBytes,
    output_bytes: 0,
    reduction_percent: 0,
    roots: 0,
    nodes_seen: 0,
    nodes_emitted: 0,
    hidden_nodes: 0,
    vector_nodes: 0,
    text_nodes: 0,
    truncated_text_nodes: 0,
    text_chars_omitted: 0,
    component_refs: 0,
    paint_count: 0,
    payload_fields_omitted: 0,
    style_overrides_omitted: 0,
  };
}

function serializeWithSize(output) {
  let serialized = "";
  for (let attempt = 0; attempt < 8; attempt += 1) {
    serialized = JSON.stringify(output) + "\n";
    const bytes = Buffer.byteLength(serialized);
    const reduction = output._meta.input_bytes
      ? Math.round((1 - bytes / output._meta.input_bytes) * 1000) / 10
      : 0;
    if (output._meta.output_bytes === bytes && output._meta.reduction_percent === reduction) return serialized;
    output._meta.output_bytes = bytes;
    output._meta.reduction_percent = reduction;
  }
  return JSON.stringify(output) + "\n";
}

function buildDigest(input, inputBytes) {
  const normalized = normalizeInput(input);
  const stats = initialStats(inputBytes);
  const roots = normalized.roots.map((root) => digestNode(root, normalized.components, stats, false)).filter(Boolean);
  stats.roots = roots.length;
  const output = { _meta: stats };
  if (nonEmpty(normalized.file)) output.file = normalized.file;
  output.roots = roots;
  return { output, serialized: serializeWithSize(output) };
}

function main() {
  const args = process.argv.slice(2);
  if (args.includes("-h") || args.includes("--help")) {
    process.stdout.write(usage() + "\n");
    return;
  }
  if (args.length < 1 || args.length > 2 || args.some((arg) => arg.startsWith("--"))) {
    fail(usage());
    return;
  }
  const inputFile = path.resolve(args[0]);
  const outputFile = args[1] ? path.resolve(args[1]) : null;
  if (outputFile && outputFile === inputFile) {
    fail("output file must differ from input file");
    return;
  }

  let raw;
  let input;
  try {
    raw = fs.readFileSync(inputFile, "utf8");
  } catch (error) {
    fail("cannot read " + inputFile + ": " + error.message);
    return;
  }
  try {
    input = JSON.parse(raw.replace(/^\uFEFF/, ""));
  } catch (error) {
    fail("invalid JSON in " + inputFile + ": " + error.message);
    return;
  }

  let digest;
  try {
    digest = buildDigest(input, Buffer.byteLength(raw));
  } catch (error) {
    fail(error.message || String(error));
    return;
  }
  if (!outputFile) {
    process.stdout.write(digest.serialized);
    return;
  }
  try {
    fs.writeFileSync(outputFile, digest.serialized, "utf8");
  } catch (error) {
    fail("cannot write " + outputFile + ": " + error.message);
  }
}

if (require.main === module) main();

module.exports = { buildDigest, normalizeInput };
