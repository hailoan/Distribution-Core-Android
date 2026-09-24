#!/usr/bin/env node
/**
 * figma-gen / split.js
 * Split any figma-data.json into one file per Level-1 Frame.
 *
 * Usage:
 *   node scripts/split.js <input.json> [output-dir]
 */

const fs   = require('fs');
const path = require('path');

function toSlug(name) {
  return name.toLowerCase()
    .replace(/\s+/g, '_')
    .replace(/[^a-z0-9_\-]/g, '')
    .substring(0, 50);
}

function toFilename(frame) {
  return `${frame.id.replace(':', '-')}_${toSlug(frame.name)}.json`;
}

function split(inputPath, outputDir = './figma-screens') {
  if (!fs.existsSync(inputPath)) {
    console.error(`ERROR: not found: ${inputPath}`); process.exit(1);
  }

  const data = JSON.parse(fs.readFileSync(inputPath, 'utf8'));

  if (!data.nodes || !Object.keys(data.nodes).length) {
    console.error('ERROR: no "nodes" key — invalid proxy response'); process.exit(1);
  }

  fs.mkdirSync(outputDir, { recursive: true });

  const results = [];

  for (const nodeId of Object.keys(data.nodes)) {
    const node = data.nodes[nodeId];
    const doc  = node.document;

    if (!doc.children?.length) {
      console.warn(`WARN: node ${nodeId} ("${doc.name}") has no children — skipped`);
      continue;
    }

    for (const [index, frame] of doc.children.entries()) {
      const filename = toFilename(frame);
      const content  = {
        _meta: {
          sourceFile:  data.name        ?? path.basename(inputPath),
          nodeId,
          extractedAt: data.lastModified ?? new Date().toISOString(),
          version:     data.version      ?? 'unknown',
          frameIndex:  index,
        },
        frame,
        components:    node.components    ?? {},
        componentSets: node.componentSets ?? {},
        styles:        node.styles        ?? {},
      };

      const json = JSON.stringify(content, null, 2);
      fs.writeFileSync(path.join(outputDir, filename), json);

      results.push({
        index, nodeId,
        id:       frame.id,
        name:     frame.name,
        children: frame.children?.length ?? 0,
        filename,
        sizeKB:   Math.round(Buffer.byteLength(json) / 1024),
      });
    }
  }

  // summary table
  const col = [4, 12, 30, 10, 44, 8];
  const h   = ['Idx','Frame ID','Name','Children','Output file','Size'];
  console.log(`\n✅  ${data.name ?? inputPath} → ${results.length} frame(s)\n`);
  console.log(h.map((v,i) => v.padEnd(col[i])).join(''));
  console.log('─'.repeat(col.reduce((a,b)=>a+b)));
  for (const r of results) {
    console.log(
      String(r.index).padEnd(col[0]) +
      r.id.padEnd(col[1]) +
      r.name.slice(0,28).padEnd(col[2]) +
      String(r.children).padEnd(col[3]) +
      r.filename.slice(0,42).padEnd(col[4]) +
      `${r.sizeKB} KB`
    );
  }
  console.log(`\nSaved to: ${path.resolve(outputDir)}\n`);
}

const [,,input, outDir] = process.argv;
if (!input) { console.error('Usage: node scripts/split.js <input.json> [output-dir]'); process.exit(1); }
split(input, outDir);
