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
uniform sampler2D heightMap;

uniform color3 sky_color = vec3(0.5, 0.5, 0.8);
uniform color3 _color0 = vec3(2.0, 0.0, 69.0)/255.0;
uniform color3 _color1 = vec3(228.0, 228.8, 137.0)/255.0;
uniform color3 _color2 = vec3(9.0, 118.0, 9.0)/255.0;
uniform color3 _color3 = vec3(203.0, 203.0, 203.0)/255.0;
uniform color3 _color4 = vec3(255.0, 255.0, 255.0)/255.0;
uniform slider(0.0, 1.0) height0 = 0.1;
uniform slider(0.0, 1.0) height1 = 0.15;
uniform slider(0.0, 1.0) height2 = 0.6;
uniform slider(0.0, 1.0) height3 = 0.7;
uniform slider(0.01, 10.0) height_scale = 1.0;

uniform slider(1.0, 1024.0) max_steps = 512.0;
uniform slider(0.01, 1.0) min_distance = 0.01;
uniform slider(1.0, 1024.0) max_distance = 100.0;
uniform slider(0.0, 1.0) time_scale = 0.25;
uniform slider(0.0, 3.0) msaa_amount = 1.0;
uniform slider(0.0, 1.0) smooth_steps = 1.0;
uniform color4 ambient_light = vec4(1.0, 1.0, 1.0, 0.02);
uniform color4 directional_light = vec4(1.0, 1.0, 1.0, 0.25);
uniform slider3(-1.0, 1.0) sun_direction = vec3(0.0, 1.0, 0.6);
uniform slider3(-100.0, 100.0) camera_position = vec3(50.0, 5.0, 50.0);
uniform slider3(-100.0, 100.0) camera_target = vec3(0.0, 0.0, 1.6);

#define msaa_scale 0.0005
uniform float time;


float doubleSmoothStep(float t0, float t1, float t2, float h) {
    return smoothstep(t0, t1, h) - smoothstep(t1, t2, h);
}

vec3 colorStep(vec3 color, float t0, float t1, float t2, float h) {
    return color * doubleSmoothStep(t0, t1, t2, h);
}

vec3 surfaceColor(float height) {
    if (smooth_steps >= 0.5) {
        vec3 col = vec3(0.0, 0.0, 0.0);
        col += _color0 * smoothstep(height0, 0.0, height);
        col += colorStep(_color1, 0.0, height0, height1, height);
        col += colorStep(_color2, height0, height1, height2, height);
        col += colorStep(_color3, height1, height2, height3, height);
        col += colorStep(_color4, height2, height3, 1.0, height);
        return col;
    }
    if (height < height0) return _color0;
    if (height < height1) return _color1;
    if (height < height2) return _color2;
    if (height < height3) return _color3;
    return _color4;
}

float map(vec2 p) {
    return texture(heightMap, p * 0.01).r;
}

float map(float x, float z) {
    return map(vec2(x, z));
}

// credit Inigo Quilez
// https://iquilezles.org/articles/terrainmarching/
const float EPSILON = 1.0/256.0;

vec3 getNormal( const vec3 p )
{
    return normalize( vec3( map(p.x-EPSILON,p.z) - map(p.x+EPSILON,p.z),
                            2.0f*EPSILON,
                            map(p.x,p.z-EPSILON) - map(p.x,p.z+EPSILON) ) );
}

mat3 setCamera( in vec3 ro, in vec3 ta, float cr )
{
	vec3 cw = normalize(ta-ro);
	vec3 cp = vec3(sin(cr), cos(cr),0.0);
	vec3 cu = normalize( cross(cw,cp) );
	vec3 cv = normalize( cross(cu,cw) );
    return mat3( cu, cv, cw );
}

vec2 march(vec3 ro, vec3 rd)
{
    float dt = (max_distance - min_distance) / max_steps;
    float mint = min_distance;
    float maxt = max_distance;
    for( float t=mint; t < maxt; t+=dt )
    {
        vec3 p = ro + rd*t;
        float height = map(p.xz);
        if(p.y < height * height_scale) {
            return vec2(t, height);
        }
    }
    return vec2(max_distance, 0.0);
}

void main() {
    vec3 ro = camera_position;
    // ro.x -= 80.0*cos(0.01*time);
    // ro.z -= 80.0*sin(0.01*time);
    // ro.x += sin(time * time_scale) * 10.0;
    vec3 ta = vec3(ro.x, ro.y-0.5, ro.z);
    if (time_scale > 0.0001) {
        ta.x += cos(time * time_scale);
        ta.z += sin(time * time_scale);
    }
    mat3 ca = setCamera( ro, ta, 0.0 );
    vec3 col = vec3(0.0, 0.0, 0.0);
    float aa = floor(msaa_amount);
    float tot = 0.0;
    for (float dy=-aa; dy<=aa; dy+=1.0) {
        for (float dx=-aa; dx<=aa; dx+=1.0) {
            tot += 1.0;
            vec3 rd = ca * normalize( vec3(fragTexCoord + msaa_scale*vec2(dx, dy), 1.5));
            vec2 m = march(ro, rd);
            if (m.x < max_distance) {
                vec3 sc = surfaceColor(m.y);
                vec3 nor = getNormal(ro + rd * m.x);
                float sun = clamp(dot(nor, normalize(sun_direction)), 0.0, 1.0);
                float sky = clamp( 0.5 + 0.5*nor.y, 0.0, 1.0 );
                // float ind = clamp( dot( nor, normalize(sun_direction*vec3(-1.0,0.0,-1.0)) ), 0.0, 1.0 );
                vec3 lin = ambient_light.rgb * ambient_light.a;
                lin += directional_light.rgb * directional_light.a * sun;
                lin += sky * sky_color;
                col += sc * lin;
            } else {
                col += sky_color;
            }
        }
    }
    col /= tot;
    gl_FragColor = vec4(col, 1.0);
}
