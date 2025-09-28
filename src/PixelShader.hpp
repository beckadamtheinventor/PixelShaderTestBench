#pragma once

#include "ImGuiColorTextEdit/TextEditor.h"
#include "external/msf_gif.h"
#include "nlohmann/json.hpp"
#include <cstring>
#include <map>
#include <string>

#include <raylib.h>
#include <imgui.h>
#include <rlgl.h>
#include <external/glad.h>

extern const char* vertex_shader_code_default;

typedef enum {
    NONE = 0,
    TEXTURE,
    MODEL,
} ShaderDrawType;

typedef enum {
    UNKNOWN = 0,
    FLOAT,
    INT,
    VEC2,
    VEC3,
    VEC4,
    COLOR3,
    COLOR4,
    SLIDER,
    SLIDER2,
    SLIDER3,
    SLIDER4,
    MATRIX,
    SAMPLER2D,
} ShaderUniformType;
class ShaderUniformData {
    public:
    ShaderUniformType type;
    bool isRange = false;
    ShaderUniformData(ShaderUniformType t) : type(t) {}
    ShaderUniformData(ShaderUniformType t, bool r) : type(t), isRange(r) {}
    operator ShaderUniformType() {
        return type;
    }
};
extern const std::vector<std::pair<std::string, ShaderUniformData>> shader_uniform_type_strings;

typedef struct {
    union {
        int i;
        float f;
        float v[4];
        int iv[4];
    };
    bool isSet;
    float min, max;
} Uniform;


#define IMAGE_NAME_BUFFER_LENGTH 512

void DrawEmptyTriangleStrip();
Texture2D LoadTextureFromString(const char* str);
void InputTextureOptions(Texture2D& tex);

class PixelShader {
    public:
    std::map<std::string, std::pair<int, ShaderUniformType>> shader_locs;
    std::map<std::string, std::pair<char*, Texture2D>> image_uniform_buffers;
    std::map<std::string, Uniform> other_uniform_buffers;
    RenderTexture2D renderTexture={0}, selfTexture={0};
    Color clearColor = {0, 0, 0, 0};
    // Texture2D albedo_tex;
    Shader pixelShader = {0};
    std::string saving_filename;
    MsfGifState gifState;
    int rt_width, rt_height;
    int num;
    ShaderDrawType drawType;
    unsigned int sampler_count = 0;
    unsigned int frame_counter = 0;
    float runtime = 0, fovx = 90;
    bool is_active, saving_sequence, saving_single, saving_gif;
    bool requested_clone : 1;
    bool requested_reference : 1;
    bool requested_reload : 1;
    bool focused : 1;
    bool controlling_camera : 1;
    std::string name;
    const char* filename;
    char image_input[IMAGE_NAME_BUFFER_LENGTH] = {0};
    char image_output[IMAGE_NAME_BUFFER_LENGTH] = {0};
    TextEditor editor;
    Rectangle outputArea;
    Model model = {0};
    char* modelFilebuf = nullptr;
    Camera3D camera = {
        .position = {0, 0, -4},
        .target = {0, 0, -3},
        .up = {0, 1, 0},
        .fovy = 90.0,
        .projection = CAMERA_PERSPECTIVE
    };

    PixelShader() {
        is_active = true;
        requested_clone = false;
        requested_reference = false;
        saving_sequence = false;
        saving_single = false;
        saving_gif = false;
        controlling_camera = false;
        shader_locs.clear();
        image_uniform_buffers.clear();
        other_uniform_buffers.clear();
    }
    PixelShader(PixelShader* other) : PixelShader(other->filename) {
        Setup(other->rt_width, other->rt_height);
        memcpy(image_output, other->image_output, sizeof(image_output));
        memcpy(image_input, other->image_input, sizeof(image_output));
    }
    PixelShader(PixelShader& other) : PixelShader(&other) {}
    PixelShader(const char* fname);
    PixelShader(const char* fname, int id);
    bool operator==(PixelShader ps) {
        return ps.num == num;
    }
    bool IsReady();
    void Update(float dt);
    protected:
    bool Load(const char* filename);
    public:
    bool New(const char* filename);
    void Unload();
    void Reload();
    void Setup(int width, int height);
    void SetRTSize(int width, int height);
    void SetClearColor(int r, int g, int b, int a);
    bool InputTextureFields(std::string str);
    void UpdateCamera(float dt);
    void DrawGUI(float dt);
    void DrawTextEditor();
    void SetUniform(std::string name, ShaderUniformType type, void* value);
    void LoadUniforms(nlohmann::json json);
    nlohmann::json DumpUniforms();
};

extern PixelShader* pixelShaderReference;
