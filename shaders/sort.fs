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
uniform sampler2D selfTexture;
uniform vec2 textureSize = vec2(512, 512);
uniform slider(0.0,1.0) load = 1.0;
uniform int frame = 0;
out vec4 fragColor;

float lum(vec3 x) {
    return dot(x, vec3(0.2126, 0.7152, 0.0722));
}

void main() {
    vec2 coord = fragTexCoord;
    coord.y = 1.0 - coord.y;
    ivec2 icoord = ivec2(coord * textureSize);
    if (load >= 0.5) {
        fragColor = texture(texture0, coord);
    } else if (icoord.y < textureSize.y - 1 && icoord.y > 0) {
        vec4 col = texture(selfTexture, coord);
        if ((frame & 1) == (icoord.y & 1)) {
            vec4 col2 = texture(selfTexture, (icoord + ivec2(0, -1)) / textureSize);
            if (lum(col.rgb) >= lum(col2.rgb)) {
                col = col2;
            }
        } else {
            vec4 col2 = texture(selfTexture, (icoord + ivec2(0,  1)) / textureSize);
            if (lum(col.rgb) < lum(col2.rgb)) {
                col = col2;
            }
        }
        fragColor = col;
    }
}