
#include <cstring>
#include <functional>
#include <list>
#include <raylib.h>
#include <fstream>
#include <utility>
#include <vector>

#include <imgui.h>
#include <rlImGui.h>

#include "PixelShader.hpp"
#include "FileDialogs.hpp"
#include "nlohmann/json.hpp"

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

int numLoadedShadersEver = 0;

std::vector<unsigned int> texturesNeedingCleanup;
extern std::list<std::pair<FileDialogs::FileDialog*, std::function<bool(std::string)>>> fileDialogs;

void PushBackTextureNeedingCleanup(const Texture2D& tex) {
    for (int i=0; i<texturesNeedingCleanup.size(); i++) {
        if (texturesNeedingCleanup[i] == -1) {
            texturesNeedingCleanup[i] = tex.id;
            return;
        }
    }
    texturesNeedingCleanup.push_back(tex.id);
}

int TextureNeedsCleanup(const Texture2D& tex) {
    for (int i=0; i<texturesNeedingCleanup.size(); i++) {
        if (texturesNeedingCleanup[i] == tex.id) {
            return i;
        }
    }
    return -1;
}

void CleanupTexture(Texture2D& tex) {
    int i = TextureNeedsCleanup(tex);
    if (i != -1) {
        UnloadTexture(tex);
        texturesNeedingCleanup[i] = -1;
        tex = {0};
    }
}

Texture2D BlankTexture() {
    Image blankImage = GenImageColor(1, 1, WHITE);
    Texture2D blankTexture = LoadTextureFromImage(blankImage);
    UnloadImage(blankImage);
    PushBackTextureNeedingCleanup(blankTexture);
    return blankTexture;
}

Texture2D LoadTextureFromString(const char* str) {
    Image image;
    Texture2D tex;
    bool generate_image_texture = false;
    int r, g, b, a=255;
    if (!str[0]) {
        return BlankTexture();
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
        PushBackTextureNeedingCleanup(tex);
        UnloadImage(image);
    } else {
        tex = LoadTexture(str);
        if (IsTextureReady(tex)) {
            GenTextureMipmaps(&tex);
            PushBackTextureNeedingCleanup(tex);
        } else {
            TraceLog(LOG_WARNING, "Failed to load image file %s!", str);
            tex = BlankTexture();
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

bool PixelShader::IsReady() {
    return IsShaderReady(pixelShader);
}

void PixelShader::Update(float dt, int frame_counter) {
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


bool PixelShader::Load(const char* filename) {
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

    std::map<std::string, Uniform> new_other_uniform_buffers;
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
    name = "Shader " + std::to_string(numLoadedShadersEver);
    num = numLoadedShadersEver++;
    return true;
}

void PixelShader::Unload() {
    CleanupTexture(albedo_tex);
    UnloadRenderTexture(renderTexture);
    UnloadRenderTexture(selfTexture);
    UnloadShader(pixelShader);
}

void PixelShader::Setup(int width) {
    albedo_tex = BlankTexture();
    rt_width = width;
    renderTexture = LoadRenderTexture(rt_width, rt_width);
    selfTexture = LoadRenderTexture(rt_width, rt_width);
}

void PixelShader::SetRTWidth(int width) {
    UnloadRenderTexture(renderTexture);
    UnloadRenderTexture(selfTexture);
    rt_width = width;
    renderTexture = LoadRenderTexture(rt_width, rt_width);
    selfTexture = LoadRenderTexture(rt_width, rt_width);
}

void PixelShader::InputTextureFields(const char* str, char* buf, Uniform* uniform, Texture2D& tex, unsigned int loc) {
    ImGui::InputTextWithHint(str, "path to image", buf, IMAGE_NAME_BUFFER_LENGTH);
    if (ImGui::Button("Browse")) {
        fileDialogs.push_back(std::make_pair(new FileDialogs::FileDialog("Select an Image"), [uniform, &tex, this, loc] (std::string p) -> bool {
            if (!p.size()) return false;
            Texture2D newtex = LoadTextureFromString(p.c_str());
            if (IsTextureReady(newtex)) {
                GenTextureMipmaps(&newtex);
                if (uniform != nullptr) {
                    uniform->isSet = true;
                }
                CleanupTexture(tex);
                tex = newtex;
                if (loc != -1) {
                    SetShaderValueTexture(pixelShader, loc, tex);
                }
            }
            return true;
        }));
    }
    ImGui::SameLine();
    if (ImGui::Button("Load Image")) {
        Texture2D newtex = LoadTextureFromString(buf);
        if (IsTextureReady(newtex)) {
            if (uniform != nullptr) {
                uniform->isSet = true;
            }
            CleanupTexture(tex);
            tex = newtex;
            if (loc != -1) {
                SetShaderValueTexture(pixelShader, loc, tex);
            }
        }
    }
    ImGui::SameLine();
    InputTextureOptions(tex);
    if (pixelShaderReference != nullptr) {
        if (ImGui::Button("Paste Reference")) {
            CleanupTexture(tex);
            tex = pixelShaderReference->renderTexture.texture;
            snprintf(buf, IMAGE_NAME_BUFFER_LENGTH, "(Shader Output %u)", pixelShaderReference->num);
            pixelShaderReference = nullptr;
        }
    }
}

void PixelShader::DrawGUI() {

    ImGui::Begin((name + " Output").c_str(), &is_active);
    rlImGuiImageSize(&renderTexture.texture, 512, 512);
    ImGui::End();

    ImGui::Begin((name + " Options").c_str(), &is_active);
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
    if (ImGui::Button("Clone Shader")) {
        requested_clone = true;
    }
    ImGui::SameLine();
    if (ImGui::Button("Copy Reference To Output")) {
        requested_reference = true;
    }
    ImGui::End();

    ImGui::Begin((name + " Uniforms").c_str(), &is_active);
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
            case INT:
                if (ImGui::InputInt(p.first.c_str(), &uniform->i)) {
                    SetShaderValue(pixelShader, v.first, &uniform->i, SHADER_UNIFORM_INT);
                    uniform->isSet = true;
                }
                break;
            case FLOAT:
                if (ImGui::InputFloat(p.first.c_str(), &uniform->f)) {
                    SetShaderValue(pixelShader, v.first, &uniform->f, SHADER_UNIFORM_FLOAT);
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
            default:
                break;
        }
    }
    ImGui::End();
}

void PixelShader::SetUniform(std::string name, ShaderUniformType type, void* value) {
    if (shader_locs.count(name) < 1) return;
    if (other_uniform_buffers.count(name) < 1) return;
    auto v = shader_locs[name];
    Uniform* uniform = &other_uniform_buffers[name];
    switch (type) {
        case INT:
            SetShaderValue(pixelShader, v.first, value, SHADER_UNIFORM_INT);
            uniform->i = *(int*)value;
            uniform->isSet = true;
            break;
        case FLOAT:
        case SLIDER:
            SetShaderValue(pixelShader, v.first, value, SHADER_UNIFORM_FLOAT);
            uniform->f = *(float*)value;
            uniform->isSet = true;
            break;
        case VEC2:
        case SLIDER2:
            SetShaderValue(pixelShader, v.first, value, SHADER_UNIFORM_VEC2);
            memcpy(&uniform->v, value, sizeof(float)*2);
            uniform->isSet = true;
            break;
        case VEC3:
        case COLOR3:
        case SLIDER3:
            SetShaderValue(pixelShader, v.first, value, SHADER_UNIFORM_VEC3);
            memcpy(&uniform->v, value, sizeof(float)*3);
            uniform->isSet = true;
            break;
        case VEC4:
        case COLOR4:
        case SLIDER4:
            SetShaderValue(pixelShader, v.first, value, SHADER_UNIFORM_VEC4);
            memcpy(&uniform->v, value, sizeof(float)*4);
            uniform->isSet = true;
            break;
        case SAMPLER2D:
            if (name == "selfTexture") {
                break;
            }
            if (image_uniform_buffers.count(name) < 1) {
                image_uniform_buffers.insert(
                    std::make_pair(name, std::make_pair(new char[IMAGE_NAME_BUFFER_LENGTH] {0}, Texture2D {0})));
            }
            {
                auto& buf = image_uniform_buffers[name];
                strcpy(buf.first, (char*)value);
                buf.second = LoadTextureFromString((char*)value);
            }
            break;
        default:
            break;
    }
}

void PixelShader::LoadUniforms(nlohmann::json json) {
    if (!json.is_object()) return;
    for (auto p : json.items()) {
        auto j = p.value();
        if (j.contains("t") && j["t"].is_number()) {
            switch (j["t"].get<int>()) {
                case INT:
                    if (j["v"].is_number()) {
                        int ival = j["v"].get<int>();
                        SetUniform(p.key(), INT, &ival);
                    }
                    break;
                case FLOAT:
                case SLIDER:
                    if (j["v"].is_number()) {
                        float fval = j["v"].get<float>();
                        SetUniform(p.key(), FLOAT, &fval);
                    }
                    break;
                case VEC2:
                case SLIDER2:
                    if (j["v"].is_array()) {
                        float fval[2];
                        for (int i=0; i<2; i++) {
                            if (j["v"].size() >= i && j["v"][i].is_number()) {
                                fval[i] = j["v"][i].get<float>();
                            }
                        }
                        SetUniform(p.key(), VEC2, &fval);
                    }
                    break;
                case VEC3:
                case COLOR3:
                case SLIDER3:
                    if (j["v"].is_array()) {
                        float fval[3];
                        for (int i=0; i<3; i++) {
                            if (j["v"].size() >= i && j["v"][i].is_number()) {
                                fval[i] = j["v"][i].get<float>();
                            }
                        }
                        SetUniform(p.key(), VEC3, &fval);
                    }
                    break;
                case VEC4:
                case COLOR4:
                case SLIDER4:
                    if (j["v"].is_array()) {
                        float fval[4];
                        for (int i=0; i<4; i++) {
                            if (j["v"].size() >= i && j["v"][i].is_number()) {
                                fval[i] = j["v"][i].get<float>();
                            }
                        }
                        SetUniform(p.key(), VEC4, &fval);
                    }
                    break;
                case SAMPLER2D:

                default:
                    break;
            }
        }
    }
}

nlohmann::json PixelShader::DumpUniforms() {
    nlohmann::json json;
    for (auto l : shader_locs) {
        std::string key = l.first;
        if (other_uniform_buffers.count(key) >= 1) {
            Uniform uniform = other_uniform_buffers[key];
            ShaderUniformType type = l.second.second;
            if (type == SAMPLER2D || uniform.isSet) {
                bool should_include = true;
                nlohmann::json j = {{"t", type}};
                switch (type) {
                case UNKNOWN:
                    break;
                case INT:
                    j["v"] = uniform.i;
                    break;
                case FLOAT:
                case SLIDER:
                    j["v"] = uniform.f;
                    break;
                case VEC2:
                case SLIDER2:
                    j["v"] = nlohmann::json::array({uniform.v[0], uniform.v[1]});
                    break;
                case VEC3:
                case COLOR3:
                case SLIDER3:
                    j["v"] = nlohmann::json::array({uniform.v[0], uniform.v[1], uniform.v[2]});
                    break;
                case VEC4:
                case COLOR4:
                case SLIDER4:
                    j["v"] = nlohmann::json::array({uniform.v[0], uniform.v[1], uniform.v[2], uniform.v[3]});
                    break;
                case SAMPLER2D:
                    if (image_uniform_buffers.count(key) < 1) {
                        should_include = false;
                    } else if (strlen(image_uniform_buffers[key].first) > 0) {
                        j["v"] = std::string(image_uniform_buffers[key].first);
                    } else {
                        should_include = false;
                    }
                    break;
                }
                if (should_include) {
                    json[key] = j;
                }
            }
        }
    }
    return json;
}