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

uniform float time;
uniform slider2(0.01,5.0) scale = vec2(1.0, 1.0);
uniform slider(0.0,20.0) timeScale = 1.0;
uniform color4 backgroundColor = vec4(0.0,0.0,0.0,0.0);
uniform color4 waveColor = vec4(1.0,1.0,1.0,1.0);
uniform slider(0.0,1.0) randomAffect = 0.1;
uniform slider(0.0,5.0) randomScale = 1.0;

#define pi 3.141
#define hpi (pi * 0.5)

// credit Lygia graphics library
float random(float x) {
    return fract(sin(x) * 43758.5453);
}

float gradientRandom(float x) {
    float i = floor(x);
    float f = fract(x);
    float r1 = random(i+0.0);
    float r2 = random(i+1.0);
    return mix(r1, r2, f) - 0.5;
}

void main() {
    float x = fragTexCoord.x * scale.x + time * timeScale;
    float dist = fragTexCoord.y - (0.5 + 0.5 * scale.y * sin(x));
    dist -= gradientRandom(x * randomScale) * randomAffect;
    vec4 col = mix(waveColor, backgroundColor, smoothstep(0.0, 0.1, abs(dist)));
    gl_FragColor = col;
}
