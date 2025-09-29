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
uniform vec3 lightPosition = vec3(0, 4, 1);
uniform color3 lightColor = vec3(1.0,1.0,1.0);
uniform float lightIntensity = 1.0;
uniform color3 ambientColor = vec3(0.1,0.1,0.1);

uniform vec3 cameraPosition = vec3(1.0, 0.0, 0.0);

void main() {
	vec4 diff = texture(texture0, fragTexCoord);
	vec3 dir = lightPosition - fragPosition;
	float dist = length(dir);
	dir = normalize(dir);
	float LdotN = dot(dir, fragNormal);
	float intensity = lightIntensity * max(LdotN, 0.0) / (dist * dist);
	vec3 light = ambientColor + lightColor * intensity;
	vec3 col = diff.rgb * light;
	gl_FragColor = vec4(col, 1.0);
}
