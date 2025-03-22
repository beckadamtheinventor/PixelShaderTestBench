# Pixel Shader Test Bench

A scaffolding program for testing and configuring pixel shaders.

Recognizes shader uniforms and provides configuration options accordingly, with a handful of additional types for easier editing.

- color3 ; rgb color
- color4 ; rgba color
- slider(min,max) ; single float slider from minimum to maximum
- slider2(min,max) ; vec2 slider from minimum to maximum
- slider3(min,max) ; vec3 slider from minimum to maximum
- slider4(min,max) ; vec4 slider from minimum to maximum

Note that in order to use these additional uniform types you will need to provide corresponding #define macros.

```
#define color3 vec3
#define color4 vec4
#define slider(a,b) float
#define slider2(a,b) vec2
#define slider3(a,b) vec3
#define slider4(a,b) vec4
```

Additionally, the test bench will automatically set the following uniform values:

- `float time` ; set to the number of clock seconds since the program started
- `float dt` ; set to the time in seconds the last frame took to render


