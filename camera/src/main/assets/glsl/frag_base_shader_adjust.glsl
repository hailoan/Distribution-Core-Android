vec4 highlightAdjust(vec4 color, float l) {
    float a = 1.357697966704323E-01;
    float b = 1.006045552016985E+00;
    float c = 4.674339906510876E-01;
    float d = 8.029414702292208E-01;
    float e = 1.127806558508491E-01;
    float maxx = max(color.r, max(color.g, color.b));
    float minx = min(color.r, min(color.g, color.b));
    float lum = (maxx+minx)/2.0;
    float x1 = abs(l * 2.0);
    float x2 = lum;
    float lum_new =  lum < 0.5 ? lum : lum + a * sign(l) * exp(-0.5 * (((x1-b)/c)*((x1-b)/c) + ((x2-d)/e)*((x2-d)/e)));
    return color * lum_new / lum;
}

vec4 darkAdjust(vec4 color, float gamma){
    return vec4(pow(color.rgb, vec3(gamma)), color.w);
}

vec4 lightAdjust(vec4 color, float value) {
    color.rgb = min(max(color.rgb, vec3(0.0)) / (vec3(value)), vec3(1.0));
    return color;
}

vec4 shadowAdjust(vec4 color, float value) {
    vec3 luminanceWeighting = vec3(0.3, 0.3, 0.3);
    float luminance = dot(color.rgb, luminanceWeighting);
    float shadow = clamp((pow(luminance, 1.0/(value+1.0)) + (-0.76)*pow(luminance, 2.0/(value+1.0))) - luminance, 0.0, 1.0);
    vec3 result = vec3(0.0, 0.0, 0.0) + ((luminance + shadow) - 0.0) * ((color.rgb - vec3(0.0, 0.0, 0.0))/(luminance - 0.0));
    return vec4(result, color.a);
}

vec3 gammaCorrect(vec3 color, float gamma){
    return pow(color, vec3(1.0/gamma));
}
vec3 levelRange(vec3 color, float minInput, float maxInput){
    return min(max(color - vec3(minInput), vec3(0.0)) / (vec3(maxInput) - vec3(minInput)), vec3(1.0));
}
vec3 finalLevels(vec3 color, float minInput, float gamma, float maxInput){
    return gammaCorrect(levelRange(color, minInput, maxInput), gamma);
}
vec4 brightnessAdjust(vec4 color, float b){
    color.rgb+=b;
    return color;
}
vec4 exposureAdjust(vec4 color, float e){
    color.rgb*=pow(2.0, e);
    return color;
}
vec4 contrastAdjust(vec4 color, float c){
    float t= 0.5-c*0.5;
    color.rgb=color.rgb *c+ t;
    return color;
}
vec4 saturationAdjust(vec4 color, float s){
    vec3 result = vec3(dot(color.rgb, W));
    return vec4 (mix(result, color.rgb, s), color.a);
}
vec3 vibranceAdjust(vec3 color, float vibrance) {
    float luminance = color.r*0.299 + color.g*0.587 + color.b*0.114;
    float mn = min(min(color.r, color.g), color.b);
    float mx = max(max(color.r, color.g), color.b);
    float sat = (1.0-(mx - mn)) * (1.0-mx) * luminance * 5.0;
    vec3 lightness = vec3((mn + mx)/2.0);
    color = mix(color, mix(color, lightness, -vibrance), sat);
    return color;
}
vec4 vignetteAdjust(vec4 color, float end, vec2 uv){
    vec2 center_point=vec2(0.5);
    vec3 vignette_color=vec3(0.0);
    float start=0.0;
    float d = distance(uv, center_point);
    float percent = smoothstep(start, end, d);
    color=vec4(mix(color.r, vignette_color.r, percent), mix(color.g, vignette_color.g, percent), mix(color.b, vignette_color.b, percent), 1.0);
    return color;
}
vec4 vibranceAdjust(vec4 color, float vib){
    vec4 coeff = vec4(0.299, 0.587, 0.114, 0.0);
    float lum = dot(color, coeff);
    vec4 mask = (color - vec4(lum));
    mask = clamp(mask, 0.0, 1.0);
    float lumMask = dot(coeff, mask);
    lumMask = 1.0 - lumMask;
    return mix(vec4(lum), color, 1.0 + vib * lumMask);
}
vec4 hueAdjust(vec4 color, float h){
    const vec4  kRGBToYPrime = vec4 (0.299, 0.587, 0.114, 0.0);
    const vec4  kRGBToI     = vec4 (0.596, -0.275, -0.321, 0.0);
    const vec4  kRGBToQ     = vec4 (0.212, -0.523, 0.311, 0.0);
    const vec4  kYIQToR   = vec4 (1.0, 0.956, 0.621, 0.0);
    const vec4  kYIQToG   = vec4 (1.0, -0.272, -0.647, 0.0);
    const vec4  kYIQToB   = vec4 (1.0, -1.107, 1.704, 0.0);

    float   YPrime  = dot (color, kRGBToYPrime);
    float   I      = dot (color, kRGBToI);
    float   Q      = dot (color, kRGBToQ);
    float   hue     = atan (Q, I);
    float   chroma  = sqrt (I * I + Q * Q);
    hue += h;
    Q = chroma * sin (hue);
    I = chroma * cos (hue);
    vec4    yIQ   = vec4 (YPrime, I, Q, 0.0);
    color.r = dot (yIQ, kYIQToR);
    color.g = dot (yIQ, kYIQToG);
    color.b = dot (yIQ, kYIQToB);
    return color;
}
vec4 temperatureAdjust(vec4 color, float temperature){
    vec3 refWhite, refWhiteRGB;
    vec3 d, s;
    vec3 to, from;
    float tint=0.0;
    if (temperature < 0.0) {
        to = CCT20K;
        from = D65;
    } else {
        to = CCT4K;
        from = D65;
    }
    vec3 base = color.rgb;
    float lum = Lum(base);
    float temp = abs(temperature) * (1.0 - pow(lum, 2.72));
    refWhiteRGB = from;
    refWhite = vec3(mix(from.x, to.x, temp), mix(1.0, 0.9, tint), mix(from.z, to.z, temp));
    refWhite = mix(refWhiteRGB, refWhite, color.a);
    d = matAdapt * refWhite;
    s = matAdapt * refWhiteRGB;
    vec3 xyz = RGBtoXYZ(base, d, s);
    vec3 rgb = XYZtoRGB(xyz, d, s);
    vec3 res = rgb * (1.0 + (temp + tint) / 10.0);
    return vec4(mix(base, res, color.a), color.a);
}
vec4 balanceFilter(vec4 color, vec3 shadowsShift, vec3 midtonesShift, vec3 highlightsShift) {
    int preserveLuminosity=1;
    vec3 lightness = color.rgb;
    vec3 shadows = shadowsShift * (clamp((lightness - 0.333) / -0.25 + .55, 0.1, 1.0) * 0.75);
    vec3 midtones = midtonesShift * (clamp((lightness - 0.333) / 0.9 + 0.5, 0.7, .5)
    *  clamp((lightness + 0.333 - 1.0) / -0.9 + 0.5, 0.0, 1.0) * 0.65);
    vec3 highlights = highlightsShift * (clamp((lightness + 0.933 - 0.5) / 0.025 + 0.5, 0.5, .3) * 0.65);
    vec3 newColor = color.rgb + shadows + midtones + highlights;
    newColor = clamp(newColor, 0.0, 1.0);
    if (preserveLuminosity != 0) {
        vec3 newHSL = RGBtoHSL(newColor);
        float oldLum = RGBToL(color.rgb);
        color.rgb = HSLtoRGB(vec3(newHSL.x, newHSL.y, oldLum));
        return color;
    } else {
        return vec4(newColor.rgb, color.w);
    }
}
vec4 applyCurve(vec4 color, sampler2D curve) {
    float red = texture(curve, vec2(color.r, 0.0)).r;
    float green = texture(curve, vec2(color.g, 0.0)).g;
    float blue = texture(curve, vec2(color.b, 0.0)).b;
    return vec4(red, green, blue, color.a);
}

float cubicPulse(float c, float w, float x)
{
    return smoothstep(c-w, c, x)-smoothstep(c, c+w, x);
}

vec3 mixColorRed(vec3 hsl, float hue, float saturation, float light) {
    if (hue == 1.0 && saturation == 1.0 && light == 1.0) return hsl;
    float smoothValue1 = cubicPulse(0.0, 1.0, current_hue/ (0.8 * VALUE));
    float smoothValue2 = cubicPulse(0.0, 1.0, (current_hue - 11.5 * VALUE)/ (0.5 * VALUE));
    if ((smoothValue1 >= 0.0 && smoothValue1 <= 1.0)) {
        float x = hsl.x + (hue - 1.0)*VALUE;
        float y = min(1.5, hsl.y * abs(saturation));
        float z = hsl.z + (light - 1.0)*VALUE;
        hsl.x = mix(hsl.x, x, smoothValue1);
        hsl.y = mix(hsl.y, y, smoothValue1);
        hsl.z = mix(hsl.z, z, smoothValue1);
    } else if ((smoothValue2 >= 0.0 && smoothValue2 <= 1.0)) {
        float x = hsl.x + (hue - 1.0)*VALUE;
        float y = min(1.5, hsl.y * abs(saturation));
        float z = hsl.z + (light - 1.0)*VALUE;
        hsl.x = mix(hsl.x, x, smoothValue2);
        hsl.y = mix(hsl.y, y, smoothValue2);
        hsl.z = mix(hsl.z, z, smoothValue2);
    }
    return hsl;
}

vec3 mixColorOrange(vec3 hsl, float hue, float saturation, float light) {
    if (hue == 1.0 && saturation == 1.0 && light == 1.0) return hsl;
    float smoothValue = cubicPulse(0.0, 1.0, (current_hue - (VALUE * 0.65))/ (VALUE));
    if (smoothValue >= 0.0 && smoothValue <= 1.0) {
        float x = hsl.x + (hue - 1.0)*VALUE;
        float y = min(1.5, hsl.y * abs(saturation));
        float z = hsl.z + (light - 1.0)*VALUE;
        hsl.x = mix(hsl.x, x, smoothValue);
        hsl.y = mix(hsl.y, y, smoothValue);
        hsl.z = mix(hsl.z, z, smoothValue);
    }
    return hsl;
}
vec3 mixColorYellow(vec3 hsl, float hue, float saturation, float light) {
    if (hue == 1.0 && saturation == 1.0 && light == 1.0) return hsl;
    float smoothValue = cubicPulse(0.0, 1.0, (current_hue - VALUE * 1.5)/ (VALUE));
    if (smoothValue >= 0.0 && smoothValue <= 1.0) {
        float x = hsl.x + (hue - 1.0)*VALUE;
        float y = min(1.5, hsl.y * abs(saturation));
        float z = hsl.z + (light - 1.0)*VALUE;
        hsl.x = mix(hsl.x, x, smoothValue);
        hsl.y = mix(hsl.y, y, smoothValue);
        hsl.z = mix(hsl.z, z, smoothValue);
    }
    return hsl;
}
vec3 mixColorGreen(vec3 hsl, float hue, float saturation, float light) {
    if (hue == 1.0 && saturation == 1.0 && light == 1.0) return hsl;
    float smoothValue = cubicPulse(0.0, 1.0, (current_hue - VALUE * 2.5)/ (VALUE * 2.5));
    if (smoothValue >= 0.0 && smoothValue <= 1.0) {
        float x = hsl.x + (hue - 1.0)*VALUE;
        float y = min(1.5, hsl.y * abs(saturation));
        float z = hsl.z + (light - 1.0)*VALUE;
        hsl.x = mix(hsl.x, x, smoothValue);
        hsl.y = mix(hsl.y, y, smoothValue);
        hsl.z = mix(hsl.z, z, smoothValue);
    }
    return hsl;
}
vec3 mixColorCyan(vec3 hsl, float hue, float saturation, float light) {
    if (hue == 1.0 && saturation == 1.0 && light == 1.0) return hsl;
    float smoothValue = cubicPulse(0.0, 1.0, (current_hue - VALUE * 5.0)/ (VALUE * 2.0));
    if (smoothValue >= 0.0 && smoothValue <= 1.0) {
        float x = hsl.x + (hue - 1.0)*VALUE;
        float y = min(1.5, hsl.y * abs(saturation));
        float z = hsl.z + (light - 1.0)*VALUE;
        hsl.x = mix(hsl.x, x, smoothValue);
        hsl.y = mix(hsl.y, y, smoothValue);
        hsl.z = mix(hsl.z, z, smoothValue);
    }
    return hsl;
}
vec3 mixColorBlue(vec3 hsl, float hue, float saturation, float light) {
    if (hue == 1.0 && saturation == 1.0 && light == 1.0) return hsl;
    float smoothValue = cubicPulse(0.0, 1.0, (current_hue - VALUE * 7.0)/ (VALUE * 2.0));
    if (smoothValue >= 0.0 && smoothValue <= 1.0) {
        float x = hsl.x + (hue - 1.0)*VALUE;
        float y = min(1.5, hsl.y * abs(saturation));
        float z = hsl.z + (light - 1.0)*VALUE;
        hsl.x = mix(hsl.x, x, smoothValue);
        hsl.y = mix(hsl.y, y, smoothValue);
        hsl.z = mix(hsl.z, z, smoothValue);
    }
    return hsl;
}
vec3 mixColorPurple(vec3 hsl, float hue, float saturation, float light) {
    if (hue == 1.0 && saturation == 1.0 && light == 1.0) return hsl;
    float smoothValue = cubicPulse(0.0, 1.0, (current_hue - VALUE * 9.0)/ (VALUE * 2.0));
    if (smoothValue >= 0.0 && smoothValue <= 1.0) {
        float x = hsl.x + (hue - 1.0)*VALUE;
        float y = min(1.5, hsl.y * abs(saturation));
        float z = hsl.z + (light - 1.0)*VALUE;
        hsl.x = mix(hsl.x, x, smoothValue);
        hsl.y = mix(hsl.y, y, smoothValue);
        hsl.z = mix(hsl.z, z, smoothValue);
    }
    return hsl;
}
vec3 mixColorPink(vec3 hsl, float hue, float saturation, float light) {
    if (hue == 1.0 && saturation == 1.0 && light == 1.0) return hsl;
    float smoothValue = cubicPulse(0.0, 1.0, (current_hue - VALUE * 10.0)/ (VALUE * 0.5));
    if (smoothValue >= 0.0 && smoothValue <= 1.0) {
        float x = hsl.x + (hue - 1.0)*VALUE;
        float y = min(1.5, hsl.y * abs(saturation));
        float z = hsl.z + (light - 1.0)*VALUE;
        hsl.x = mix(hsl.x, x, smoothValue);
        hsl.y = mix(hsl.y, y, smoothValue);
        hsl.z = mix(hsl.z, z, smoothValue);
    }
    return hsl;
}
vec4 clarityAdjust(vec4 color, float clarity) {
    vec2 uv = scaleTexture(v_texCoord, u_scale);
    vec3 blurColor = blur(uv, 2);
    float intensity = (clarity < 0.0) ? clarity / 2.0 : clarity * 2.0;
    intensity *= color.a;
    float lum = Lum(color.rgb);
    vec3 base = vec3(lum);
    vec3 mask = vec3(1.0 - pow(lum, 1.8));
    vec3 layer = vec3(1.0 - Lum(blurColor));
    vec3 detail = clamp(blendVividLight(base, layer), 0.0, 1.0);
    vec3 inverse = mix(1.0 - detail, detail, (intensity+1.0)/2.0);
    vec3 blend = blendOverlay(color.rgb, mix(vec3(0.5), inverse, mask));
    return vec4(SetLum(SetSat(color.rgb, Sat(blend)), Lum(blend)), color.a);
}