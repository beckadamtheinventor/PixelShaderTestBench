#include "raylib.h"
#include "imgui.h"
#include "rlImGui.h"
#include "rlgl.h"
#include "external/glad.h"
#include <cctype>
#include <cstring>
#include <ctime>
#include <fstream>
#include <ios>
#include <map>
#include <utility>

const char* vertex_shader_code =
"#version 330 core                  \n"
"in vec3 vertexPosition;     \n"
"in vec2 vertexTexCoord;     \n"
"in vec4 vertexColor;        \n"
"out vec2 fragTexCoord;         \n"
"uniform mat4 mvp;                  \n"
"void main()                        \n"
"{                                  \n"
"    fragTexCoord = vec2(vertexTexCoord.x, vertexTexCoord.y); \n"
"    gl_Position = mvp*vec4(vertexPosition, 1.0); \n"
"}                                  \n";

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
const std::map<std::string, ShaderUniformData> shader_uniform_type_strings = {
    {"float", FLOAT},
    {"int", INT},
    {"vec2", VEC2},
    {"vec3", VEC3},
    {"vec4", VEC4},
    {"color3", COLOR3},
    {"color4", COLOR4},
    {"slider", {SLIDER, true}},
    {"slider2", {SLIDER2, true}},
    {"slider3", {SLIDER3, true}},
    {"slider4", {SLIDER4, true}},
    {"sampler2D", SAMPLER2D},
};
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


std::map<std::string, std::pair<unsigned int, ShaderUniformType>> shader_locs;
#define IMAGE_NAME_BUFFER_LENGTH 512
std::map<std::string, std::pair<char*, Texture2D>> image_uniform_buffers;
std::map<std::string, Uniform> other_uniform_buffers;
Image blankImage = GenImageColor(1, 1, WHITE);
Texture2D blankTexture;
RenderTexture2D renderTexture, selfTexture;
Shader pixelShader;

bool LoadPixelShader(const char* filename) {
    std::ifstream fd(filename, std::ios::in | std::ios::binary);
    char* fragment_code;
    size_t len = 0;
    if (fd.is_open()) {
        fd.seekg(0, std::ios::end);
        len = fd.tellg();
        fd.seekg(0, std::ios::beg);
        fragment_code = new char[len+1];
        fd.read(fragment_code, len);
        fragment_code[len] = 0;
        fd.close();
    }
    if (len == 0) {
        return false;
    }
    Shader newPixelShader = LoadShaderFromMemory(vertex_shader_code, fragment_code);
    if (!IsShaderReady(newPixelShader)) {
        delete[] fragment_code;
        return false;
    }
    if (IsShaderReady(pixelShader)) {
        UnloadShader(pixelShader);
    }
    pixelShader = newPixelShader;

    typeof(other_uniform_buffers) new_other_uniform_buffers;
    shader_locs.clear();
    for (size_t i = 0; i < len; i++) {
        if (!memcmp(&fragment_code[i], "uniform ", strlen("uniform "))) {
            // TraceLog(LOG_INFO, "found uniform at offset %u", i);
            i += strlen("uniform");
            while (isspace(fragment_code[i]) && i < len) i++;
            for (auto p : shader_uniform_type_strings) {
                std::string s = p.first;
                size_t l = s.length();
                if (!memcmp(&fragment_code[i], s.c_str(), s.length()) &&
                    !(fragment_code[i+l]=='_' || isalnum(fragment_code[i+l]))) {
                        size_t j = i + l + 1;
                        char* eol = (char*)memchr(&fragment_code[j], '\n', len-j);
                        if (eol == nullptr) eol = &fragment_code[len];
                        char* eos = &fragment_code[j];
                        if (fragment_code[i+l] == '(') {
                            while (eos < eol && *eos != ')') eos++;
                            while (eos < eol && !(isalnum(*eos) || *eos == '_')) eos++;
                            j = eos - fragment_code;
                        }
                        while (eos < eol && (isalnum(*eos) || *eos == '_')) eos++;
                        std::string name = std::string(&fragment_code[j], eos - &fragment_code[j]);
                        unsigned int loc = rlGetLocationUniform(pixelShader.id, name.c_str());
                        if (name == "texture0") {
                            continue;
                        }
                        if (loc == -1) {
                            TraceLog(LOG_WARNING,
                                "Failed to locate uniform \"%s\" in shader despite it appearing in the shader source!", name.c_str());
                        } else {
                            Uniform uni = {0};
                            shader_locs.insert(std::make_pair(name, std::make_pair(loc, p.second)));
                            if (p.second.isRange) {
                                uni.min = 0.0;
                                uni.max = 1.0;
                                if (fragment_code[i+l] == '(') {
                                    size_t sob = i + l;
                                    size_t eob = sob;
                                    while (fragment_code[eob] != ')' && eob < len) eob++;
                                    if (eob >= len) {
                                        TraceLog(LOG_WARNING, "Missing closing bracket in range specifier for uniform \"%s\"", name.c_str());
                                    } else {
                                        uni.max = atof(&fragment_code[sob+1]);
                                        while (strchr(",\n", fragment_code[sob]) == nullptr && sob < eob) sob++;
                                        if (fragment_code[sob] == ',') {
                                            // two numbers -> range X..Y
                                            uni.min = uni.max;
                                            uni.max = atof(&fragment_code[sob+1]);
                                        } else {
                                            // single number -> range 0..X
                                        }
                                    }
                                }
                            }
                            if (other_uniform_buffers.count(name) > 0) {
                                if (new_other_uniform_buffers.count(name) < 1) {
                                    Uniform uni2 = other_uniform_buffers[name];
                                    uni2.min = uni.min;
                                    uni2.max = uni.max;
                                    uni = uni2;
                                }
                                new_other_uniform_buffers.insert(std::make_pair(name, uni));
                                switch (p.second) {
                                    case FLOAT:
                                    case SLIDER:
                                        SetShaderValue(pixelShader, loc, &uni.f, SHADER_UNIFORM_FLOAT);
                                        break;
                                    case INT:
                                        SetShaderValue(pixelShader, loc, &uni.i, SHADER_UNIFORM_INT);
                                        break;
                                    case VEC2:
                                    case SLIDER2:
                                        SetShaderValue(pixelShader, loc, &uni.v, SHADER_UNIFORM_VEC2);
                                        break;
                                    case VEC3:
                                    case SLIDER3:
                                    case COLOR3:
                                        SetShaderValue(pixelShader, loc, &uni.v, SHADER_UNIFORM_VEC3);
                                        break;
                                    case VEC4:
                                    case COLOR4:
                                    case SLIDER4:
                                        SetShaderValue(pixelShader, loc, &uni.v, SHADER_UNIFORM_VEC4);
                                        break;
                                    case UNKNOWN:
                                    case SAMPLER2D:
                                        break;
                                }
                            } else {
                                switch (p.second) {
                                    case FLOAT:
                                        glGetUniformfv(pixelShader.id, loc, &uni.f);
                                        break;
                                    case INT:
                                        glGetUniformiv(pixelShader.id, loc, &uni.i);
                                        break;
                                    case VEC2:
                                    case VEC3:
                                    case VEC4:
                                    case COLOR3:
                                    case COLOR4:
                                    case SLIDER:
                                    case SLIDER2:
                                    case SLIDER3:
                                    case SLIDER4:
                                        glGetUniformfv(pixelShader.id, loc, (float*)&uni.v);
                                        break;
                                    case SAMPLER2D:
                                    case UNKNOWN:
                                        break;
                                }
                                if (p.second < SAMPLER2D) {
                                    new_other_uniform_buffers.insert(std::make_pair(name, uni));
                                }
                            }
                            if (p.second.isRange) {
                                TraceLog(LOG_INFO, "found uniform %s %s (range %.3f to %.3f)", s.c_str(), name.c_str(), uni.min, uni.max);
                            } else {
                                TraceLog(LOG_INFO, "found uniform %s %s", s.c_str(), name.c_str());
                            }
                        }
                        i = eol - fragment_code;
                        break;
                }
            }
        }
    }
    // other_uniform_buffers.clear();
    other_uniform_buffers = new_other_uniform_buffers;
    return true;
}

Texture2D LoadTextureFromString(const char* str) {
    Image image;
    Texture2D tex;
    bool generate_image_texture = false;
    int r, g, b, a=255;
    if (!str[0]) {
        return blankTexture;
    }
    if (!memcmp(str, "rgb(", 4)) {
        sscanf(str, "rgb(%d,%d,%d)", &r, &g, &b);
        generate_image_texture = true;
    } else if (!memcmp(str, "rgba(", 5)) {
        sscanf(str, "rgba(%d,%d,%d,%d)", &r, &g, &b, &a);
        generate_image_texture = true;
    }
    if (generate_image_texture) {
        image = GenImageColor(1, 1,
            {(unsigned char)r, (unsigned char)g, (unsigned char)b, (unsigned char)a});
        tex = LoadTextureFromImage(image);
        GenTextureMipmaps(&tex);
        UnloadImage(image);
    } else {
        tex = LoadTexture(str);
        if (IsTextureReady(tex)) {
            GenTextureMipmaps(&tex);
        } else {
            TraceLog(LOG_WARNING, "Failed to load image file %s!", str);
            tex = blankTexture;
        }
    }
    return tex;
}

void InputTextureOptions(Texture2D& tex) {
    if (ImGui::Button("Trilinear")) {
        SetTextureFilter(tex, TEXTURE_FILTER_TRILINEAR);
    }
    ImGui::SameLine();
    if (ImGui::Button("Bilinear")) {
        SetTextureFilter(tex, TEXTURE_FILTER_BILINEAR);
    }
    ImGui::SameLine();
    if (ImGui::Button("Point")) {
        SetTextureFilter(tex, TEXTURE_FILTER_POINT);
    }
    if (ImGui::Button("Repeat")) {
        SetTextureWrap(tex, TEXTURE_WRAP_REPEAT);
    }
    ImGui::SameLine();
    if (ImGui::Button("Mirror")) {
        SetTextureWrap(tex, TEXTURE_WRAP_MIRROR_REPEAT);
    }
    ImGui::SameLine();
    if (ImGui::Button("Clamp")) {
        SetTextureWrap(tex, TEXTURE_WRAP_CLAMP);
    }
    ImGui::SameLine();
    if (ImGui::Button("Mirror Once")) {
        SetTextureWrap(tex, TEXTURE_WRAP_MIRROR_CLAMP);
    }
}

void InputTextureFields(const char* str, char* buf, Uniform* uniform, Texture2D& tex, unsigned int loc) {
    ImGui::InputTextWithHint(str, "path to image", buf, IMAGE_NAME_BUFFER_LENGTH);
    if (ImGui::Button("Load Image")) {
        Texture2D newtex = LoadTextureFromString(buf);
        if (IsTextureReady(newtex)) {
            if (uniform != nullptr) {
                uniform->isSet = true;
            }
            if (IsTextureReady(tex)) {
                UnloadTexture(tex);
            }
            tex = newtex;
            if (loc != -1) {
                SetShaderValueTexture(pixelShader, loc, tex);
            }
        }
    }
    ImGui::SameLine();
    InputTextureOptions(tex);
}


int main(int argc, char** argv) {
    char pixel_shader_file[IMAGE_NAME_BUFFER_LENGTH] = "shaders/noise.fs";
    if (argc > 1) {
        strcpy(pixel_shader_file, argv[1]);
    }
    int rt_width = 1024;
    if (argc > 2) {
        rt_width = atoi(argv[2]);
    }
    SetConfigFlags(FLAG_VSYNC_HINT | FLAG_WINDOW_RESIZABLE);
    InitWindow(1000, 600, "PixelShaderTestBench");
    SetTargetFPS(60);
    SetExitKey(-1);
    int render_texture_update_rate = 30;
    float render_texture_update_timer = 0;

    rlImGuiSetup(true);
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;

    char image_input[IMAGE_NAME_BUFFER_LENGTH] = {0};
    char image_output[IMAGE_NAME_BUFFER_LENGTH] = {0};
    Texture2D albedo_tex = blankTexture = LoadTextureFromImage(blankImage);
    bool update_shader_uniforms = true;
    renderTexture = LoadRenderTexture(rt_width, rt_width);
    selfTexture = LoadRenderTexture(rt_width, rt_width);
    LoadPixelShader(pixel_shader_file);

    float dt = 0.0f;
    int frame_counter = 0;
    int target_fps = 60;
    while (!WindowShouldClose()) {
        BeginDrawing();
        ClearBackground(BLACK);
        DrawFPS(1, 1);
        if (render_texture_update_timer >= 1.0 / render_texture_update_rate) {
            render_texture_update_timer -= 1.0 / render_texture_update_rate;
            if (IsShaderReady(pixelShader)) {
                BeginTextureMode(renderTexture);
                // if (frame_counter < 3) {
                //     ClearBackground(BLACK);
                // }
                BeginShaderMode(pixelShader);

                if (other_uniform_buffers.count("time") >= 1) {
                    float f = clock() / (float)CLOCKS_PER_SEC;
                    other_uniform_buffers["time"].f = f;
                    SetShaderValue(pixelShader, shader_locs["time"].first, &f, SHADER_UNIFORM_FLOAT);
                }
                if (other_uniform_buffers.count("dt") >= 1) {
                    other_uniform_buffers["time"].f = dt;
                    SetShaderValue(pixelShader, shader_locs["time"].first, &dt, SHADER_UNIFORM_FLOAT);
                }
                if (other_uniform_buffers.count("frame") >= 1) {
                    other_uniform_buffers["frame"].i = frame_counter;
                    SetShaderValue(pixelShader, shader_locs["frame"].first, &frame_counter, SHADER_UNIFORM_INT);
                }
                if (shader_locs.count("selfTexture") >= 1) {
                    if (IsTextureReady(selfTexture.texture)) {
                        SetShaderValueTexture(pixelShader, shader_locs["selfTexture"].first, selfTexture.texture);
                    }
                }

                if (IsTextureReady(albedo_tex)) {
                    SetShaderValueTexture(pixelShader, SHADER_LOC_MAP_ALBEDO, albedo_tex);
                    Rectangle srcrec {0.0, 0.0, (float)albedo_tex.width, (float)albedo_tex.height};
                    Rectangle dstrec {0.0, 0.0, (float)renderTexture.texture.width, (float)renderTexture.texture.height};
                    DrawTexturePro(albedo_tex, srcrec, dstrec, {0.0, 0.0}, 0.0, WHITE);
                } else {
                    DrawRectangle(0, 0, renderTexture.texture.width, renderTexture.texture.height, WHITE);
                }
                EndShaderMode();
                EndTextureMode();
                // BeginTextureMode(selfTexture);
                // ClearBackground(BLACK);
                // Rectangle srcrec {0.0, 0.0, (float)renderTexture.texture.width, (float)renderTexture.texture.height};
                // Rectangle dstrec {0.0, -(float)renderTexture.texture.height, (float)renderTexture.texture.width, -(float)renderTexture.texture.height};
                // DrawTexturePro(renderTexture.texture, srcrec, dstrec, {0.0, 0.0}, 0.0, WHITE);
                // EndTextureMode();
                std::swap(selfTexture, renderTexture);
            }
        }
        rlImGuiBegin();

        ImGui::Begin("Options");
        ImGui::InputTextWithHint("Pixel Shader File", "path to fragment shader", pixel_shader_file, sizeof(pixel_shader_file));
        if (ImGui::Button("Reload Shader")) {
            //if (std::filesystem::exists(image_input)) {
                LoadPixelShader(pixel_shader_file);
                frame_counter = -1;
            //} else {
                if (!IsShaderReady(pixelShader)) {
                    TraceLog(LOG_WARNING, "Failed to load Pixel shader %s!", pixel_shader_file);
                }
            //}
        }
        InputTextureFields("Diffuse/Albedo", image_input, nullptr, albedo_tex, -1);
        ImGui::InputTextWithHint("Image Output", "path to image to save", image_output, sizeof(image_output));
        if (ImGui::Button("Save Image")) {
            Image img = LoadImageFromTexture(renderTexture.texture);
            if (IsFileExtension(image_output, ".jpg") || IsFileExtension(image_output, ".jpeg")) {
                ImageFormat(&img, PIXELFORMAT_UNCOMPRESSED_R8G8B8);
            }
            if (!ExportImage(img, image_output)) {
                TraceLog(LOG_WARNING, "Failed to export image %s!", image_output);
            }
        }
        if (ImGui::InputInt("Render Texture Size", &rt_width)) {
            if (IsRenderTextureReady(renderTexture)) {
                UnloadRenderTexture(renderTexture);
            }
            if (IsRenderTextureReady(selfTexture)) {
                UnloadRenderTexture(selfTexture);
            }
            renderTexture = LoadRenderTexture(rt_width, rt_width);
            selfTexture = LoadRenderTexture(rt_width, rt_width);
        }
        InputTextureOptions(renderTexture.texture);
        ImGui::SliderInt("Target updates per second", &render_texture_update_rate, 1, 60);
        if (ImGui::SliderInt("Target FPS", &target_fps, 10, 500)) {
            SetTargetFPS(target_fps);
        }
        if (ImGui::Button("Unlimited FPS")) {
            SetTargetFPS((target_fps = -1));
        }
        ImGui::End();

        ImGui::Begin("Output");
        rlImGuiImageSize(&renderTexture.texture, 512, 512);
        ImGui::End();

        ImGui::Begin("Uniforms");
        for (auto p : shader_locs) {
            auto v = p.second;
            Uniform* uniform;
            if (v.second<SAMPLER2D) {
                if (other_uniform_buffers.count(p.first) >= 1) {
                    uniform = &other_uniform_buffers[p.first];
                } else {
                    other_uniform_buffers.insert(std::make_pair(p.first, Uniform{0}));
                    uniform = &other_uniform_buffers[p.first];
                }
            }
            switch (v.second) {
            case FLOAT:
                if (ImGui::InputFloat(p.first.c_str(), &uniform->f)) {
                    SetShaderValue(pixelShader, v.first, &uniform->f, SHADER_UNIFORM_FLOAT);
                    uniform->isSet = true;
                }
                break;
            case INT:
                if (ImGui::InputInt(p.first.c_str(), &uniform->i)) {
                    SetShaderValue(pixelShader, v.first, &uniform->i, SHADER_UNIFORM_INT);
                    uniform->isSet = true;
                }
                break;
            case VEC2:
                if (ImGui::InputFloat2(p.first.c_str(), (float*)&uniform->v)) {
                    SetShaderValue(pixelShader, v.first, &uniform->v, SHADER_UNIFORM_VEC2);
                    uniform->isSet = true;
                }
                break;
            case VEC3:
                if (ImGui::InputFloat3(p.first.c_str(), (float*)&uniform->v)) {
                    SetShaderValue(pixelShader, v.first, &uniform->v, SHADER_UNIFORM_VEC3);
                    uniform->isSet = true;
                }
                break;
            case VEC4:
                if (ImGui::InputFloat4(p.first.c_str(), (float*)&uniform->v)) {
                    SetShaderValue(pixelShader, v.first, &uniform->v, SHADER_UNIFORM_VEC4);
                    uniform->isSet = true;
                }
                break;
            case COLOR3:
                if (ImGui::ColorEdit3(p.first.c_str(), (float*)&uniform->v)) {
                    SetShaderValue(pixelShader, v.first, &uniform->v, SHADER_UNIFORM_VEC3);
                    uniform->isSet = true;
                }
                break;
            case COLOR4:
                if (ImGui::ColorEdit4(p.first.c_str(), (float*)&uniform->v)) {
                    SetShaderValue(pixelShader, v.first, &uniform->v, SHADER_UNIFORM_VEC4);
                    uniform->isSet = true;
                }
                break;
            case SLIDER:
                if (ImGui::SliderFloat(p.first.c_str(), (float*)&uniform->v, uniform->min, uniform->max)) {
                    SetShaderValue(pixelShader, v.first, &uniform->v, SHADER_UNIFORM_FLOAT);
                    uniform->isSet = true;
                }
                break;
            case SLIDER2:
                if (ImGui::SliderFloat2(p.first.c_str(), (float*)&uniform->v, uniform->min, uniform->max)) {
                    SetShaderValue(pixelShader, v.first, &uniform->v, SHADER_UNIFORM_VEC2);
                    uniform->isSet = true;
                }
                break;
            case SLIDER3:
                if (ImGui::SliderFloat3(p.first.c_str(), (float*)&uniform->v, uniform->min, uniform->max)) {
                    SetShaderValue(pixelShader, v.first, &uniform->v, SHADER_UNIFORM_VEC3);
                    uniform->isSet = true;
                }
                break;
            case SLIDER4:
                if (ImGui::SliderFloat4(p.first.c_str(), (float*)&uniform->v, uniform->min, uniform->max)) {
                    SetShaderValue(pixelShader, v.first, &uniform->v, SHADER_UNIFORM_VEC4);
                    uniform->isSet = true;
                }
                break;
            case SAMPLER2D:
                if (p.first == "selfTexture") {
                    break;
                }
                if (image_uniform_buffers.count(p.first) < 1) {
                    image_uniform_buffers.insert(
                        std::make_pair(p.first, std::make_pair(new char[IMAGE_NAME_BUFFER_LENGTH] {0}, Texture2D {0})));
                }
                {
                    auto buf = image_uniform_buffers[p.first];
                    InputTextureFields(p.first.c_str(), buf.first, uniform, buf.second, p.second.first);
                }
                break;
            case UNKNOWN:
                break;
            }
        }
        ImGui::End();

        rlImGuiEnd();
        EndDrawing();
        dt = GetFrameTime();
        render_texture_update_timer += dt;
        frame_counter++;
    }

    rlImGuiShutdown();
    CloseWindow();
    return 0;
}
