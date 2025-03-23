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
uniform slider(0.0,3.0) width_scale = 2.266;
uniform slider(0.0,20.0) timeScale = 8.0;
uniform color3 backgroundAbove = vec3(0.0,0.0,0.0);
uniform color3 backgroundBelow = vec3(0.0,0.0,0.0);
uniform color3 backgroundMiddle = vec3(0.25,0.25,0.25);
uniform color3 wallColor = vec3(1.0,1.0,1.0);
uniform color3 playerColor = vec3(1.0,0.0,0.0);
uniform slider(0.001,1.0) wallWidth = 0.02;
uniform slider(0.001,1.0) tunnelHeight = 0.179;
uniform slider(0.0,1.0) randomAffect = 0.257;
uniform slider(0.0,5.0) randomScale = 0.684;
uniform slider(0.0,1.0) randomHeight = 0.237;
uniform slider(-1.0,1.0) tunnelOffset = 0.444;
uniform slider(0.001,0.2) playerSize = 0.058;

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
    return mix(r1, r2, f);
}

void main() {
    float x = fragTexCoord.x * width_scale + time * timeScale;
    float y1 = tunnelOffset + gradientRandom(x * randomScale) * randomAffect;
    float y2 = tunnelOffset + gradientRandom(x * randomScale + 42069.0) * randomAffect;
    float h = tunnelHeight + gradientRandom(x * randomScale + 1337.0) * randomHeight;
    float height1 = y1 - h;
    float height2 = y2 + h;
    vec3 background = fragTexCoord.y < height1 ? backgroundBelow : backgroundAbove;
    if (fragTexCoord.y >= height1 && fragTexCoord.y < height2) {
        background = backgroundMiddle;
    }
    vec3 col = mix(wallColor, background, smoothstep(0.0, wallWidth, min(abs(fragTexCoord.y - height1), abs(fragTexCoord.y - height2))));
    float playerX = 0.5;
    y1 = tunnelOffset + gradientRandom((x + playerX) * randomScale) * randomAffect;
    y2 = tunnelOffset + gradientRandom((x + playerX) * randomScale + 42069.0) * randomAffect;
    float playerY = (y1 + y2) * 0.5;
    vec2 playerPos = vec2(playerX, playerY);
    col = mix(playerColor, col, smoothstep(0.0, playerSize, length(playerPos - fragTexCoord)));
    gl_FragColor = vec4(col, 1.0);
}
