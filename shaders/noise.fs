#version 330 core

// these are here so the gui shows a color picker, sliders etc
#define color3 vec3
#define color4 vec4
#define slider(a,b) float
#define slider2(a,b) vec2
#define slider3(a,b) vec3
#define slider4(a,b) vec4

in vec2 fragTexCoord;
uniform sampler2D texture0;

uniform vec4 diffColor = vec4(1.0, 1.0, 1.0, 1.0);
uniform color3 _color0 = vec3(2.0, 0.0, 69.0)/255.0;
uniform color3 _color1 = vec3(228.0, 228.8, 137.0)/255.0;
uniform color3 _color2 = vec3(9.0, 118.0, 9.0)/255.0;
uniform color3 _color3 = vec3(203.0, 203.0, 203.0)/255.0;
uniform color3 _color4 = vec3(255.0, 255.0, 255.0)/255.0;
uniform slider(0.0, 1.0) height0 = 0.1;
uniform slider(0.0, 1.0) height1 = 0.15;
uniform slider(0.0, 1.0) height2 = 0.6;
uniform slider(0.0, 1.0) height3 = 0.7;
uniform slider2(-10.0, 10.0) offset = vec2(0.0, 0.0);
uniform slider(1.0, 25.0) scale = 4.0;

uniform int octaves = 6;
uniform slider(-5.0, 5.0) init_total = 0.0;
uniform slider(-5.0, 5.0) init_value = 0.1;
uniform slider(-5.0, 5.0) init_amplitude = 1.0;
uniform slider(-5.0, 5.0) mult_amplitude = 0.65;
uniform slider(-5.0, 5.0) init_frequency = 1.0;
uniform slider(-5.0, 5.0) mult_frequency = 1.8;

#define pi 3.141
#define hpi (pi * 0.5)

// Credit Lygia graphics library
vec4 grad4(float j, vec4 ip) {
    const vec4 ones = vec4(1.0, 1.0, 1.0, -1.0);
    vec4 p,s;
    p.xyz = floor( fract (vec3(j) * ip.xyz) * 7.0) * ip.z - 1.0;
    p.w = 1.5 - dot(abs(p.xyz), ones.xyz);
    s = vec4(lessThan(p, vec4(0.0)));
    p.xyz = p.xyz + (s.xyz*2.0 - 1.0) * s.www;
    return p;
}

float mod289(const in float x) { return x - floor(x * (1. / 289.)) * 289.; }
vec2 mod289(const in vec2 x) { return x - floor(x * (1. / 289.)) * 289.; }
vec3 mod289(const in vec3 x) { return x - floor(x * (1. / 289.)) * 289.; }
vec4 mod289(const in vec4 x) { return x - floor(x * (1. / 289.)) * 289.; }
float taylorInvSqrt(in float r) { return 1.79284291400159 - 0.85373472095314 * r; }
vec2 taylorInvSqrt(in vec2 r) { return 1.79284291400159 - 0.85373472095314 * r; }
vec3 taylorInvSqrt(in vec3 r) { return 1.79284291400159 - 0.85373472095314 * r; }
vec4 taylorInvSqrt(in vec4 r) { return 1.79284291400159 - 0.85373472095314 * r; }
float permute(const in float v) { return mod289(((v * 34.0) + 1.0) * v); }
vec2 permute(const in vec2 v) { return mod289(((v * 34.0) + 1.0) * v); }
vec3 permute(const in vec3 v) { return mod289(((v * 34.0) + 1.0) * v); }
vec4 permute(const in vec4 v) { return mod289(((v * 34.0) + 1.0) * v); }
float quintic(const in float v) { return v*v*v*(v*(v*6.0-15.0)+10.0); }
vec2  quintic(const in vec2 v)  { return v*v*v*(v*(v*6.0-15.0)+10.0); }
vec3  quintic(const in vec3 v)  { return v*v*v*(v*(v*6.0-15.0)+10.0); }
vec4  quintic(const in vec4 v)  { return v*v*v*(v*(v*6.0-15.0)+10.0); }

float cnoise(in vec2 P) {
    vec4 Pi = floor(P.xyxy) + vec4(0.0, 0.0, 1.0, 1.0);
    vec4 Pf = fract(P.xyxy) - vec4(0.0, 0.0, 1.0, 1.0);
    Pi = mod289(Pi); // To avoid truncation effects in permutation
    vec4 ix = Pi.xzxz;
    vec4 iy = Pi.yyww;
    vec4 fx = Pf.xzxz;
    vec4 fy = Pf.yyww;

    vec4 i = permute(permute(ix) + iy);

    vec4 gx = fract(i * (1.0 / 41.0)) * 2.0 - 1.0 ;
    vec4 gy = abs(gx) - 0.5 ;
    vec4 tx = floor(gx + 0.5);
    gx = gx - tx;

    vec2 g00 = vec2(gx.x,gy.x);
    vec2 g10 = vec2(gx.y,gy.y);
    vec2 g01 = vec2(gx.z,gy.z);
    vec2 g11 = vec2(gx.w,gy.w);

    vec4 norm = taylorInvSqrt(vec4(dot(g00, g00), dot(g01, g01), dot(g10, g10), dot(g11, g11)));
    g00 *= norm.x;
    g01 *= norm.y;
    g10 *= norm.z;
    g11 *= norm.w;

    float n00 = dot(g00, vec2(fx.x, fy.x));
    float n10 = dot(g10, vec2(fx.y, fy.y));
    float n01 = dot(g01, vec2(fx.z, fy.z));
    float n11 = dot(g11, vec2(fx.w, fy.w));

    vec2 fade_xy = quintic(Pf.xy);
    vec2 n_x = mix(vec2(n00, n01), vec2(n10, n11), fade_xy.x);
    float n_xy = mix(n_x.x, n_x.y, fade_xy.y);
    return 2.3 * n_xy;
}

float fbm(in vec2 p) {
    float amp = init_amplitude;
    float fall = mult_amplitude;
    float freq = init_frequency;
    float lac = mult_frequency;
    float v = init_value;
    float t = init_total;
    for (int i = 0; i < octaves; i++) {
        v += cnoise(p * freq) * amp;
        t += amp;
        freq *= lac;
        amp *= fall;
    }
    return v / t;
}

float doubleSmoothStep(float t0, float t1, float t2, float h) {
    return smoothstep(t0, t1, h) - smoothstep(t1, t2, h);
}

vec3 colorStep(vec3 color, float t0, float t1, float t2, float h) {
    return color * doubleSmoothStep(t0, t1, t2, h);
}

vec3 surfaceColor(float height) {
    vec3 col = vec3(0.0, 0.0, 0.0);
    col += _color0 * smoothstep(height0, 0.0, height);
    col += colorStep(_color1, 0.0, height0, height1, height);
    col += colorStep(_color2, height0, height1, height2, height);
    col += colorStep(_color3, height1, height2, height3, height);
    col += colorStep(_color4, height2, height3, 1.0, height);
    return col;
}

void main() {
    float lvl = fbm(fragTexCoord * scale + offset);
    vec3 col = surfaceColor(lvl);
    vec4 texelColor = texture2D(texture0, fragTexCoord);
    gl_FragColor = vec4(col, 1.0) * diffColor * texelColor;
}

