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

uniform slider2(0.5, 10.0) copies = vec2(3.5, 4.0);
uniform slider(-4.0, 4.0) rotation = 0.0;
uniform slider2(-10.0, 10.0) position = vec2(0.0, 0.0);
uniform slider2(0.001, 2.0) timeScale = vec2(0.15, 0.23);

uniform float time;

const float PI = 3.14159;

vec2 rotate(float angle, vec2 v) {
    mat2 r = mat2(cos(angle), -sin(angle), sin(angle), cos(angle));
    return r*v;
}

void main() {
	vec2 uv = fragTexCoord;
	uv = fract(copies.x * rotate(time*timeScale.x, (uv - 0.5)));
	uv = fract(copies.y * rotate(time*timeScale.y, abs(uv - 0.5)));
	uv = rotate(time, abs(uv - 0.5));
	uv *= pow(distance(uv, rotate(time*2., vec2(0.4))), 2.);
	uv *= distance(uv, vec2(0.));
	//vec3 col = vec3(sin(uv.x * 20.*PI), sin(uv.x * 20.*PI + time), sin(cos(uv.x * 15.*PI + time)));
	//col = smoothstep(0.1, 0.9, col);
	//gl_FragColor = vec4(col, 1.0);
	vec4 col = texture(texture0, fract(uv));
	gl_FragColor = col;
}














