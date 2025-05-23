#version 330 core

in vec2 fragTexCoord;
uniform sampler2D texture0;

uniform int enable_rotate = 1;
uniform int enable_cart2polar = 1;
uniform int enable_polar2cart = 0;
uniform int enable_vertical_flip = 0;
uniform int enable_horizontal_flip = 0;

#define pi 3.141
#define hpi (pi * 0.5)

// input coordinates centered on rotation point
vec2 rotate2d(vec2 p, float r) {
    return p * mat2(cos(r), -sin(r), sin(r), cos(r));
}

// input range 0..1, output range 0..1
vec2 cart2polar(vec2 p) {
    p -= 0.5;
    float r = atan(p.y, p.x) * 0.5 / pi;
    p *= 2.0;
    float l = pow(p.x*p.x + p.y*p.y, 0.5) / pow(2.0, 0.5);
    p = clamp(vec2(r, l), -1.0, 1.0);
    return p;
}

// input range 0..1, output range 0..1
vec2 polar2cart(vec2 p) {
    float r = p.x * 2.0 * pi;
    return clamp(p.y * vec2(cos(r), sin(r)), 0.0, 1.0);
}

void main() {
    vec2 coord = fragTexCoord;
    if (enable_rotate > 0) {
        coord = rotate2d(coord - 0.5, -hpi) + 0.5;
    }
    if (enable_horizontal_flip > 0) {
        coord.x = 1.0f - coord.x;
    }
    if (enable_vertical_flip > 0) {
        coord.y = 1.0f - coord.y;
    }
    if (enable_cart2polar > 0) {
        coord = cart2polar(coord);
    }
    if (enable_polar2cart > 0) {
        coord = polar2cart(coord);
    }
    vec4 texelColor = texture2D(texture0, coord);
    gl_FragColor = texelColor;
    // gl_FragColor = vec4(coord, 1.0, 1.0);
}