#version 330 core

// these are here so the gui shows a color picker, sliders etc
#define color3 vec3
#define color4 vec4
#define slider(a,b) float
#define slider2(a,b) vec2
#define slider3(a,b) vec3
#define slider4(a,b) vec4

in vec2 fragTexCoord;

#define RED vec3(0.75, 0.0, 0.0)
#define BLUE vec3(0.0, 0.0, 0.75)
#define BLACK vec3(0.0, 0.0, 0.0)
#define DARKGRAY vec3(0.15, 0.15, 0.15)
#define WHITE vec3(1.0, 1.0, 1.0)

uniform color4 diffuse = vec4(1.0,1.0,1.0,1.0);
uniform color3 color_left = RED;
uniform color3 color_right = BLUE;
uniform color3 background1 = WHITE;
uniform color3 background2 = DARKGRAY;


void main() {
    ivec2 icoord = ivec2(fragTexCoord * 8.0);
    vec2 coord = fract(fragTexCoord * 8.0);
    bool c = bool((icoord.x ^ icoord.y) & 1);
    vec3 col = c ? background1 : background2;
    float dist = length(coord - 0.5) - 0.4;
    if (icoord.x < 2 || icoord.x >= 6) {
        vec3 col2 = icoord.x < 4 ? color_left : color_right;
        col2 *= mix(1.0, 0.7, smoothstep(-0.125, -0.01, dist));
        col = mix(col2, col, smoothstep(-0.01, 0.01, dist));
    }
    vec4 texelColor = vec4(col, 1.0);
    gl_FragColor = texelColor*diffuse;
    // gl_FragColor = vec4(coord, 1.0, 1.0);
}