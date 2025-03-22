#version 330 core

in vec2 fragTexCoord;
uniform sampler2D texture0;
uniform vec4 colDiffuse;


#define RED vec3(0.75, 0.0, 0.0)
#define BLUE vec3(0.0, 0.0, 0.75)
#define BLACK vec3(0.0, 0.0, 0.0)
#define DARKGRAY vec3(0.15, 0.15, 0.15)
#define WHITE vec3(1.0, 1.0, 1.0)


void main() {
    ivec2 icoord = ivec2(fragTexCoord * 8.0);
    vec2 coord = fract(fragTexCoord * 8.0);
    bool c = bool((icoord.x ^ icoord.y) & 1);
    vec3 col = c ? DARKGRAY : WHITE;
    float dist = length(coord - 0.5) - 0.4;
    if (icoord.x < 2 || icoord.x >= 6) {
        vec3 col2 = icoord.x < 4 ? RED : BLUE;
        col2 *= mix(1.0, 0.7, smoothstep(-0.125, -0.01, dist));
        col = mix(col2, col, smoothstep(-0.01, 0.01, dist));
    }
    vec4 texelColor = vec4(col, 1.0);
    gl_FragColor = texelColor*colDiffuse;
    // gl_FragColor = vec4(coord, 1.0, 1.0);
}