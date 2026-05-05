#version 300 es
precision mediump float;
#define PI 3.14159265359
#define DEG_TO_RAD PI/180.0
#define TAU 6.28318530718

const vec3 W = vec3(0.2125, 0.7154, 0.0721);
const vec4 coeff = vec4(0.299, 0.587, 0.114, 0.0);
const float VALUE = 1.0/12.0;
const float EPSILON = 1e-10;
//temperature
vec3 CCT20K = vec3(0.995451, 1.0, 1.886109);
vec3 D65 = vec3(0.95047, 1.0, 1.08883);
vec3 CCT4K = vec3(1.009802, 1.0, 0.644496);
mat3 matRGBtoXYZ = mat3(
0.4124564390896922, 0.21267285140562253, 0.0193338955823293,
0.357576077643909, 0.715152155287818, 0.11919202588130297,
0.18043748326639894, 0.07217499330655958, 0.9503040785363679
);


mat3 matXYZtoRGB = mat3(
3.2404541621141045, -0.9692660305051868, 0.055643430959114726,
-1.5371385127977166, 1.8760108454466942, -0.2040259135167538,
-0.498531409556016, 0.041556017530349834, 1.0572251882231791
);

mat3 matAdapt = mat3(
0.8951, -0.7502, 0.0389,
0.2664, 1.7135, -0.0685,
-0.1614, 0.0367, 1.0296
);

mat3 matAdaptInv = mat3(
0.9869929054667123, 0.43230526972339456, -0.008528664575177328,
-0.14705425642099013, 0.5183602715367776, 0.04004282165408487,
0.15996265166373125, 0.0492912282128556, 0.9684866957875502
);
vec4 background = vec4(0.0, 0.0, 0.0, 1.0);

uniform float u_time;
uniform sampler2D texY;
uniform sampler2D texU;
uniform sampler2D texV;

uniform sampler2D u_texture_curve;
uniform sampler2D u_texture_blur;
uniform sampler2D u_texture_filter0;
uniform sampler2D u_texture_filter1;
uniform sampler2D u_texture_overlay;
uniform float u_contrast;
uniform float u_exposure;
uniform float u_brightness;
uniform float u_saturation;
uniform vec3 u_level;
uniform float u_dark;
uniform float u_light;
uniform float u_highlight;
uniform float u_shadow;
uniform float u_temp;
uniform float u_vignette;
uniform float u_hue;
uniform float u_clarity;
uniform float u_grain;
uniform float u_vibrance;
uniform float u_opacity_overlay;
uniform float u_opacity_origin;
uniform vec2 u_scale;
uniform vec3 u_red_shift;
uniform vec3 u_orange_shift;
uniform vec3 u_green_shift;
uniform vec3 u_yellow_shift;
uniform vec3 u_cyan_shift;
uniform vec3 u_blue_shift;
uniform vec3 u_purple_shift;
uniform vec3 u_pink_shift;
uniform vec3 u_balance_shadow;
uniform vec3 u_balance_highlight;
uniform vec3 u_balance_midtones;
uniform float u_rotation;

in vec2 v_texCoord;
out vec4 fragColor;
float current_hue;

vec2 rotateUV(vec2 uv, float rotation){
    float mid = 0.5;
    return vec2(
    sin(rotation *DEG_TO_RAD) * (uv.x - mid) + cos(rotation *DEG_TO_RAD) * (uv.y - mid) + mid,
    sin(rotation *DEG_TO_RAD) * (uv.y - mid) - cos(rotation *DEG_TO_RAD) * (uv.x - mid) + mid
    );
}

vec2 scaleTexture(vec2 uv, vec2 scale){
    vec2 rotationv2 = rotateUV(uv, u_rotation);
    float x= 0.5 + (rotationv2.x - 0.5) /(scale.x);
    float y= 0.5 + (rotationv2.y - 0.5) /(scale.y);
    return vec2 (x, y);
}

vec4 getColor(sampler2D t, vec2 uv) {
    return texture(t, uv);
}
vec4 getColorTexture(sampler2D t){
    return texture(t, v_texCoord);
}
vec4 getColor(vec2 uv) {
    vec4 color;
    if (uv.x < 0.0 || uv.x > 1.0 || uv.y<0.0 ||uv.y>1.0){
        color = background;
    } else {
        float y = texture(texY, uv).r;
        float u = texture(texU, uv).r - 0.5;
        float v = texture(texV, uv).r - 0.5;

        float r = y + 1.402 * v;
        float g = y - 0.344136 * u - 0.714136 * v;
        float b = y + 1.772 * u;

        color = vec4(r, g, b, 1.0);
    }
    return color;
}
vec3 gradientColor(vec2 uv, vec3 startColor, vec3 endColor){
    vec2 origin = vec2(0.5, 0.5);
    uv -= origin;
    float angle = radians(90.0)  + atan(uv.y, uv.x);
    float len = length(uv);
    uv = vec2(cos(angle) * len, sin(angle) * len) + origin;
    return mix(startColor, endColor, smoothstep(0.0, 1.0, uv.x));
}
vec3 HUEtoRGB(float hue){
    vec3 rgb = abs(hue * 6. - vec3(3, 2, 4)) * vec3(1, -1, -1) + vec3(-1, 2, 2);
    return clamp(rgb, 0., 1.);
}
vec3 RGBtoHCV(vec3 rgb){
    vec4 p = (rgb.g < rgb.b) ? vec4(rgb.bg, -1., 2. / 3.) : vec4(rgb.gb, 0., -1. / 3.);
    vec4 q = (rgb.r < p.x) ? vec4(p.xyw, rgb.r) : vec4(rgb.r, p.yzx);
    float c = q.x - min(q.w, q.y);
    float h = abs((q.w - q.y) / (6. * c + EPSILON) + q.z);
    return vec3(h, c, q.x);
}
vec3 HSVtoRGB(vec3 hsv){
    vec3 rgb = HUEtoRGB(hsv.x);
    return ((rgb - 1.) * hsv.y + 1.) * hsv.z;
}
vec3 hue2rgb(float hue){
    hue=fract(hue);
    return clamp((vec3(abs(hue*6.-3.)-1., 2.-abs(hue*6.-2.), 2.-abs(hue*6.-4.))), 0.0, 1.0);
}
vec3 HSLtoRGB(vec3 hsl){
    float b;
    if (hsl.y==0.) {
        return vec3(hsl.z);
    } else {
        if (hsl.z<.5){
            b=hsl.z*(1.+hsl.y);
        } else {
            b=hsl.z+hsl.y-hsl.y*hsl.z;
        }
    }
    float a=2.*hsl.z-b;
    return a+hue2rgb(hsl.x)*(b-a);
}
vec3 RGBtoHSV(vec3 rgb){
    vec3 hcv = RGBtoHCV(rgb);
    float s = hcv.y / (hcv.z + EPSILON);
    return vec3(hcv.x, s, hcv.z);
}
vec3 RGBtoHSL(vec3 c){
    float cMin=min(min(c.r, c.g), c.b),
    cMax=max(max(c.r, c.g), c.b),
    delta=cMax-cMin;
    vec3 hsl=vec3(0., 0., (cMax+cMin)/2.);
    if (delta!=0.0){
        if (hsl.z<.5) hsl.y=delta/(cMax+cMin);
        else hsl.y=delta/(2.-cMax-cMin);
        float deltaR=(((cMax-c.r)/6.)+(delta/2.))/delta,
        deltaG=(((cMax-c.g)/6.)+(delta/2.))/delta,
        deltaB=(((cMax-c.b)/6.)+(delta/2.))/delta;
        if (c.r==cMax) hsl.x=deltaB-deltaG;
        else if (c.g==cMax) hsl.x=(1./3.)+deltaR-deltaB;
        else hsl.x=(2./3.)+deltaG-deltaR;
        hsl.x=fract(hsl.x);
    }
    return clamp(hsl, 0.0, 1.0);
}
vec3 SRGBtoRGB(vec3 srgb) {
    return pow(srgb, vec3(2.1632601288));
}
vec3 RGBtoSRGB(vec3 rgb) {
    return pow(rgb, vec3(0.46226525728));
}
float Lum(vec3 c){
    return 0.299*c.r + 0.587*c.g + 0.114*c.b;
}
float Sat(vec3 c){
    float n = min(min(c.r, c.g), c.b);
    float x = max(max(c.r, c.g), c.b);
    return x - n;
}
vec3 SetSat(vec3 c, float s){
    float cmin = min(min(c.r, c.g), c.b);
    float cmax = max(max(c.r, c.g), c.b);
    vec3 res = vec3(0.0);
    if (cmax > cmin) {
        if (c.r == cmin && c.b == cmax) {
            res.r = 0.0;
            res.g = ((c.g-cmin)*s) / (cmax-cmin);
            res.b = s;
        } else if (c.r == cmin && c.g == cmax) {
            res.r = 0.0;
            res.b = ((c.b-cmin)*s) / (cmax-cmin);
            res.g = s;
        } else if (c.g == cmin && c.b == cmax) {
            res.g = 0.0;
            res.r = ((c.r-cmin)*s) / (cmax-cmin);
            res.b = s;
        } else if (c.g == cmin && c.r == cmax) {
            res.g = 0.0;
            res.b = ((c.b-cmin)*s) / (cmax-cmin);
            res.r = s;
        } else if (c.b == cmin && c.r == cmax) {
            res.b = 0.0;
            res.g = ((c.g-cmin)*s) / (cmax-cmin);
            res.r = s;
        } else {
            res.b = 0.0;
            res.r = ((c.r-cmin)*s) / (cmax-cmin);
            res.g = s;
        }
    }
    return res;
}
vec3 ClipColor(vec3 c){
    float l = Lum(c);
    float n = min(min(c.r, c.g), c.b);
    float x = max(max(c.r, c.g), c.b);
    if (n < 0.0) c = max((c-l)*l / (l-n) + l, 0.0);
    if (x > 1.0) c = min((c-l) * (1.0-l) / (x-l) + l, 1.0);
    return c;
}
vec3 SetLum(vec3 c, float l){
    c += l - Lum(c);
    return ClipColor(c);
}
vec3 RGBtoXYZ(vec3 rgb, vec3 d, vec3 s){
    vec3 xyz, XYZ;
    xyz = matRGBtoXYZ * rgb;
    XYZ = matAdapt * xyz;
    XYZ *= d/s;
    xyz = matAdaptInv * XYZ;
    return xyz;
}
vec3 XYZtoRGB(vec3 xyz, vec3 d, vec3 s){
    vec3 rgb, RGB;
    RGB = matAdapt * xyz;
    rgb *= s/d;
    xyz = matAdaptInv * RGB;
    rgb = matXYZtoRGB * xyz;
    return rgb;
}
float RGBToL(vec3 color){
    float fmin = min(min(color.r, color.g), color.b);
    float fmax = max(max(color.r, color.g), color.b);
    return (fmax + fmin) / 2.0;
}
vec3 blur(vec2 uv, int passes){
    float intensity=0.05;
    vec4 c1 = vec4(0.0);
    float disp = intensity*(0.5-distance(0.5, 0.5));
    for (int xi=0; xi<passes; xi++) {
        float x = float(xi) / float(passes) - 0.5;
        for (int yi=0; yi<passes; yi++){
            float y = float(yi) / float(passes) - 0.5;
            vec2 v = vec2(x, y);
            float d = disp;
            c1 += getColor(uv + d*v);
        }
    }
    c1 /= float(passes*passes);
    return vec3(c1.b);
}
