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
uniform sampler2D texture1;


uniform color4 outlineColor = color4(1, 1, 1, 1);
uniform color4 backgroundColor = color4(0, 0, 0, 0);
uniform slider(0, 10) blurRadius = 2.0;
uniform slider(0, 1) alphaThreshold = 0.98;
uniform int additive = 0;
uniform int overlay = 1;
uniform int useAlpha = 1;

#define pi 3.141
#define hpi (pi * 0.5)

// Input uv coordinates, (width,height) vector, and blur radius in pixels
vec4 boxBlur(sampler2D tex, vec2 uv, vec2 wh, vec2 r) {
    vec4 acc = vec4(0,0,0,0);
    if (r.x < 1.0 || r.y < 1.0) return acc;
    r /= wh;
    float count = 0;
    for (float y=uv.y-r.y; y<=uv.y+r.y; y+=r.y) {
        for (float x=uv.x-r.x; x<=uv.x+r.x; x+=r.x) {
            acc += texture(tex, vec2(x, y));
            count++;
        }
    }
    return acc / count;
}
vec4 boxBlur(sampler2D tex, vec2 uv, vec2 wh, float r) {
    return boxBlur(tex, uv, wh, vec2(r,r));
}
float isEdge(sampler2D tex, vec2 uv, vec2 wh, vec2 r, float t) {
    if (r.x < 1.0 || r.y < 1.0) return 0.0;
    r /= wh;
    float mid = texture(tex, uv).a;
    float mindist = 1.0;
    if (mid >= t) {
        for (float y=uv.y-r.y; y<=uv.y+r.y; y+=r.y) {
            for (float x=uv.x-r.x; x<=uv.x+r.x; x+=r.x) {
                float l = length(uv - vec2(x, y));
                float alp = texture(tex, vec2(x, y)).a;
                if (alp < t) {
                    mindist = min(mindist, l);
                }
            }
        }
    } else {
        for (float y=uv.y-r.y; y<=uv.y+r.y; y+=r.y) {
            for (float x=uv.x-r.x; x<=uv.x+r.x; x+=r.x) {
                float l = length(uv - vec2(x, y));
                float alp = texture(tex, vec2(x, y)).a;
                if (alp >= t) {
                    mindist = min(mindist, l);
                }
            }
        }
    }
    return mindist;
}
float isEdge(sampler2D tex, vec2 uv, vec2 wh, float r, float t) {
    return isEdge(tex, uv, wh, vec2(r,r), t);
}


void main() {
    vec2 coord = fragTexCoord;
    coord.y = 1.0 - coord.y;
    vec2 wh = textureSize(texture0, 0);
    vec4 col;
    float v;
    if (useAlpha > 0) {
        v = 1.0 - isEdge(texture0, coord, wh, blurRadius, alphaThreshold);
        // v = smoothstep(0.5, 1.0, v);
    } else {
        vec4 blur0 = boxBlur(texture0, coord + vec2(-1, 0)/wh, wh, blurRadius);
        vec4 blur1 = boxBlur(texture0, coord + vec2( 1, 0)/wh, wh, blurRadius);
        vec4 blur2 = boxBlur(texture0, coord + vec2( 0,-1)/wh, wh, blurRadius);
        vec4 blur3 = boxBlur(texture0, coord + vec2( 0, 1)/wh, wh, blurRadius);
        col = max(abs(blur0 - blur1), abs(blur2 - blur3));
        v = max(max(col.r, col.g), max(col.b, col.a));
    }
    vec4 oc = vec4(outlineColor.rgb, v);
    vec4 mid = texture(texture1, coord);
    if (overlay > 0) {
        col = mix(oc, mid, mid.a);
    } else {
        col = backgroundColor + oc;
    }
    if (additive > 0) {
        col += mid;
    }
    gl_FragColor = col;
    // gl_FragColor = vec4(coord, 1.0, 1.0);
}










