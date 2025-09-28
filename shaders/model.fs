#version 330 core
//#type: model; (this ensures the test bench draws this onto a 3d model instead of a quad covering the texture)

// these are here so the gui shows a color picker, sliders etc
#define color3 vec3
#define color4 vec4
#define slider(a,b) float
#define slider2(a,b) vec2
#define slider3(a,b) vec3
#define slider4(a,b) vec4

in vec2 fragTexCoord;
in vec3 fragPosition;
in vec3 fragNormal;

uniform sampler2D texture0;


void main() {
    gl_FragColor = vec4(texture(texture0, fragTexcoord), 1.0);
}
