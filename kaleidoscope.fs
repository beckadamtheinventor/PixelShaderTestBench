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
void main() {
    vec4 texelColor = vec4(fragTexCoord, 0.0, 1.0);
    gl_FragColor = texelColor;
}
