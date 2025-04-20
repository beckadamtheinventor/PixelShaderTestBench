#pragma once

#include <cstring>
#include <map>
#include <string>

#include <raylib.h>
#include <imgui.h>
#include <rlgl.h>
#include <external/glad.h>

extern const char* vertex_shader_code;

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
extern const std::map<std::string, ShaderUniformData> shader_uniform_type_strings;

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

Texture2D LoadTextureFromString(const char* str);
void InputTextureOptions(Texture2D& tex);

class PixelShader {
    public:
    std::map<std::string, std::pair<unsigned int, ShaderUniformType>> shader_locs;
    std::map<std::string, std::pair<char*, Texture2D>> image_uniform_buffers;
    std::map<std::string, Uniform> other_uniform_buffers;
    RenderTexture2D renderTexture, selfTexture;
    Texture2D albedo_tex;
    Shader pixelShader = {0};
    int rt_width;
    int num;
    bool is_active;
    bool requested_clone : 1;
    bool requested_reference : 1;
    std::string name;
    const char* filename;
    char image_input[IMAGE_NAME_BUFFER_LENGTH] = {0};
    char image_output[IMAGE_NAME_BUFFER_LENGTH] = {0};

    PixelShader() {
        is_active = true;
        requested_clone = false;
        requested_reference = false;
    }
    PixelShader(PixelShader* other) : PixelShader(other->filename) {
        Setup(other->rt_width);
        memcpy(image_output, other->image_output, sizeof(image_output));
        memcpy(image_input, other->image_input, sizeof(image_output));
    }
    PixelShader(PixelShader& other) : PixelShader(&other) {}
    PixelShader(const char* fname) : PixelShader() {
        filename = fname;
        if (filename != nullptr) {
            Load(filename);
        }
    }
    bool operator==(PixelShader ps) {
        return ps.num == num;
    }
    bool IsReady();
    void Update(float dt, int frame_counter);
    protected:
    bool Load(const char* filename);
    public:
    void Unload();
    void Setup(int width);
    void InputTextureFields(const char* str, char* buf, Uniform* uniform, Texture2D& tex, unsigned int loc);
    void DrawGUI();
};


