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
uniform sampler2D normal_map;

uniform float time;
uniform slider(0.0,20.0) timeScale = 1.0;
uniform color3 diffuse = vec3(1.0, 1.0, 1.0);
uniform color3 specular_color = vec3(1.0, 1.0, 1.0);
uniform float specular_intensity = 1.0;
uniform color3 ambient_color = vec3(1.0, 1.0, 1.0);
uniform float ambient_intensity = 0.05;
uniform color3 directional_color = vec3(1.0, 1.0, 1.0);
uniform float directional_intensity = 2.0;
uniform slider3(-1.0,1.0) light_position1 = vec3(-1.0, 1.0, 0.0);
uniform slider3(-1.0,1.0) light_position2 = vec3(1.0, 1.0, 0.0);
uniform slider(0.0, 2.0) shininess = 0.1;

#define pi 3.141
#define hpi (pi * 0.5)
#define screenGamma 2.2

void main() {
    float sq = sin(time * timeScale) * 0.5 + 0.5;
    vec3 light_pos = mix(light_position1, light_position2, sq);
    vec3 vert_pos = vec3(fragTexCoord.x - 0.5, 0.0, fragTexCoord.y - 0.5) * 0.25;

    vec3 normal = texture(normal_map, fragTexCoord).rbg;
    if (normal.x >= 0.5) {
        normal.x = 1.0 - normal.x;
    }
    if (normal.z >= 0.5) {
        normal.z = 1.0 - normal.z;
    }
    normal.xz *= 0.5;
    normal = normalize(normal);
    vec3 lightDir = light_pos;
    // float distance = dot(lightDir, lightDir);
    lightDir = normalize(lightDir);

    float lambertian = max(dot(lightDir, normal), 0.0);
    float specular = 0.0;

    if (lambertian > 0.0) {
        vec3 viewDir = normalize(-vert_pos);
        vec3 halfDir = normalize(lightDir + viewDir);
        float specAngle = max(dot(halfDir, normal), 0.0);
        specular = pow(specAngle, shininess);
    }
    vec3 diff = texture(texture0, fragTexCoord).rgb * diffuse;
    vec3 colorLinear = ambient_color * ambient_intensity +
                        diff * lambertian * directional_color * directional_intensity +
                        specular_color * specular * specular_color * specular_intensity;
    // apply gamma correction (assume ambientColor, diffuseColor and specColor
    // have been linearized, i.e. have no gamma correction in them)
    colorLinear = pow(colorLinear, vec3(screenGamma));
    gl_FragColor = vec4(colorLinear, 1.0);
}
