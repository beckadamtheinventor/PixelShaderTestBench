
#include <cmath>
#include <cstring>
#include <ctime>
#include <map>
#include <fstream>
#include <string>
#include <utility>
#include <vector>

#include <raylib.h>
#include <rcamera.h>
#include <imgui.h>
#include <rlImGui.h>

#include "PixelShader.hpp"
#include "FileDialogs.hpp"
#include "external/msf_gif.h"
#include "nlohmann/json.hpp"
#include "ImGuiColorTextEdit/TextEditor.h"
#include "raymath.h"
#include "rlgl.h"


#pragma region Constants

const char* vertex_shader_code_default =
"#version 330 core\n"
"//Special thanks to https://roxlu.com/2014/041/attributeless-vertex-shader-with-opengl\n"
"out vec2 fragTexCoord;\n"
"const vec2 pos[] = vec2[4]("
"  vec2(-1.0, 1.0), "
"  vec2(-1.0, -1.0), "
"  vec2(1.0, 1.0), "
"  vec2(1.0, -1.0)  "
");"
""
" const vec2[] tex = vec2[4]("
"   vec2(0.0, 0.0), "
"   vec2(0.0, 1.0), "
"   vec2(1.0, 0.0), "
"   vec2(1.0, 1.0) "
");"
"void main() {"
"  gl_Position = vec4(pos[gl_VertexID], 0.0, 1.0);"
"  fragTexCoord = tex[gl_VertexID];"
"}\n";

const char* vertex_shader_code_model = 
"#version 330\n"
"in vec3 vertexPosition;\n"
"in vec2 vertexTexCoord;\n"
"in vec4 vertexColor;\n"
"in vec3 vertexNormal;\n"
"out vec2 fragTexCoord;\n"
"out vec4 fragColor;\n"
"out vec3 fragPosition;\n"
"out vec3 fragNormal;\n"
"uniform mat4 mvp;\n"
"uniform mat4 matNormal;\n"
"void main()\n"
"{\n"
"\tfragTexCoord = vertexTexCoord;\n"
"\tfragPosition = vertexPosition;\n"
"\tfragColor = vertexColor;\n"
"\tfragNormal = normalize(vec3(matNormal * vec4(vertexNormal, 0.0)));\n"
"\tgl_Position = mvp*vec4(vertexPosition, 1.0);\n"
"}\n";

const char* fragment_shader_code_default =
"#version 330 core\n"
"// these are here so the gui shows a color picker, sliders etc\n"
"#define color3 vec3\n"
"#define color4 vec4\n"
"#define slider(a,b) float\n"
"#define slider2(a,b) vec2\n"
"#define slider3(a,b) vec3\n"
"#define slider4(a,b) vec4\n"
"\n"
"in vec2 fragTexCoord;\n"
"in vec4 fragColor;\n"
"in vec3 fragPosition;\n"
"uniform sampler2D texture0;\n"
"\n"
"// Compatibility for shadertoy shaders\n"
"// Note: you will need to add \"uniform\" to option values for them to show up in the GUI.\n"
"uniform vec2 iResolution = vec2(1,1);\n"
"#define iTime time\n"
"#define iChannel0 texture0\n"
"\n"
"void mainImage(out vec4 fragColor, in vec2 fragCoord) {\n"
"\tfragColor = vec4(fragCoord, 0.0, 1.0);\n"
"}\n"
"void main() {\n"
"\tmainImage(gl_FragColor, fragTexCoord);\n"
"}\n";


const std::vector<std::pair<std::string, ShaderUniformData>> shader_uniform_type_strings = {
    std::make_pair("float", FLOAT),
    std::make_pair("int", INT),
    std::make_pair("vec2", VEC2),
    std::make_pair("vec3", VEC3),
    std::make_pair("vec4", VEC4),
    std::make_pair("color3", COLOR3),
    std::make_pair("color4", COLOR4),
    std::make_pair("slider2", ShaderUniformData{SLIDER2, true}),
    std::make_pair("slider3", ShaderUniformData{SLIDER3, true}),
    std::make_pair("slider4", ShaderUniformData{SLIDER4, true}),
    std::make_pair("slider", ShaderUniformData{SLIDER, true}),
    std::make_pair("sampler2D", SAMPLER2D),
    std::make_pair("mat2", MATRIX),
    std::make_pair("mat3", MATRIX),
    std::make_pair("mat4", MATRIX),
};

#pragma endregion

#pragma region Global Variables

unsigned int numLoadedShadersEver = 0;

std::vector<unsigned int> texturesNeedingCleanup;
extern FileDialogs::FileDialogManager fileDialogManager;
extern std::map<int, PixelShader*> pixelShaders;

#pragma endregion

#pragma region Global Functions

void CheckOpenGLError(const char* str) {
    auto err = glGetError();
    if (err != 0) {
        TraceLog(LOG_WARNING, "OpenGL Error: %u (where: %s)", err, str);
    }
}

void DrawEmptyTriangleStrip() {
    static unsigned int quadVAO = 0;
    if (quadVAO == 0) {
        glGenVertexArrays(1, &quadVAO);
    }

    glBindVertexArray(quadVAO);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    glBindVertexArray(0);
}

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
        TraceLog(LOG_DEBUG, "Cleaning up texture ID %u", tex.id);
        UnloadTexture(tex);
        texturesNeedingCleanup[i] = -1;
        tex = {0};
    } else {
        TraceLog(LOG_DEBUG, "Not cleaning up texture ID %u", tex.id);
    }
}

Texture2D BlankTexture() {
    TraceLog(LOG_DEBUG, "Creating blank texture");
    Image blankImage = GenImageColor(1, 1, WHITE);
    Texture2D blankTexture = LoadTextureFromImage(blankImage);
    PushBackTextureNeedingCleanup(blankTexture);
    UnloadImage(blankImage);
    return blankTexture;
}

Texture2D LoadTextureFromString(const char* str) {
    Image image;
    Texture2D tex;
    bool generate_image_texture = false;
    int r, g, b, a=255;
    // TraceLog(LOG_INFO, "Loading texture from string: \"%s\"", str);
    if (!str[0]) {
        return BlankTexture();
    }
    if (str[0] == '(' && str[strlen(str)-1] == ')') {
        int psid = -1;
        sscanf(str, "(Shader Output %u)", &psid);
        if (psid >= 0) {
            if (pixelShaders.count(psid) >= 1) {
                auto ps = pixelShaders[psid];
                return ps->renderTexture.texture;
            }
        }
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

#pragma endregion

#pragma region Callback Functors

class TextSaveAsCB : public FileDialogs::Callback {
    std::string textToSave;
    public:
    TextSaveAsCB(std::string t) : textToSave(t) {}
    bool operator()(std::string filename) {
        std::ofstream fd(filename);
        if (fd.is_open()) {
            fd.write(textToSave.c_str(), textToSave.length()-1);
            fd.close();
            return true;
        }
        return false;
    }
};

class BrowseFilenameCB : public FileDialogs::Callback {
    char* buf;
    bool* returned;
    // Uniform* uniform;
    // Texture2D& tex;
    // int loc;
    // Shader pixelShader;
    public:
    // InputTextureFieldsLoadCB(char* buf, Uniform* uniform, Texture2D& tex, int loc, Shader pixelShader) :
    //     buf(buf), uniform(uniform), tex(tex), loc(loc), pixelShader(pixelShader) {}
    BrowseFilenameCB(char* buf, bool* returned) : buf(buf), returned(returned) {}
    bool operator()(std::string p) {
        if (!p.length()) return false;
        strncpy(buf, p.c_str(), IMAGE_NAME_BUFFER_LENGTH-1);
        buf[IMAGE_NAME_BUFFER_LENGTH-1] = 0;
        // Texture2D newtex = LoadTextureFromString(p.c_str());
        // if (IsTextureReady(newtex)) {
            // GenTextureMipmaps(&newtex);
            // if (uniform != nullptr) {
            //     uniform->isSet = true;
            // }
            // CleanupTexture(tex);
            // tex = newtex;
            // if (loc != -1) {
            //     SetShaderValueTexture(pixelShader, loc, tex);
            // }
        // }
        if (returned != nullptr) *returned = true;
        return true;
    }
};

#pragma endregion

#pragma region PixelShader functions

PixelShader::PixelShader(const char* fname) : PixelShader() {
    filename = strdup(fname);
    if (filename != nullptr) {
        Load(filename);
        num = numLoadedShadersEver++;
        name = "Shader " + std::to_string(num);
    }
}

PixelShader::PixelShader(const char* fname, int id) : PixelShader() {
    filename = strdup(fname);
    if (filename != nullptr) {
        Load(filename);
        num = id;
        name = "Shader " + std::to_string(num);
    }
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

void PixelShader::Update(float dt) {

    if (saving_sequence || saving_single || saving_gif) {
        std::string filename = saving_filename;
        if (!saving_gif) {
            filename = std::string(GetDirectoryPath(saving_filename.c_str())) + "/" +
                GetFileNameWithoutExt(saving_filename.c_str()) +
                std::to_string(frame_counter) + GetFileExtension(saving_filename.c_str());
        }
        Image img = LoadImageFromTexture(renderTexture.texture);
        if (saving_gif) {
            msf_gif_frame(&gifState, (uint8_t*)img.data, dt*100.0f, 16, rt_width*4);
        } else {
            if (IsFileExtension(filename.c_str(), ".jpg") || IsFileExtension(filename.c_str(), ".jpeg")) {
                ImageFormat(&img, PIXELFORMAT_UNCOMPRESSED_R8G8B8);
            }
            if (!ExportImage(img, filename.c_str())) {
                TraceLog(LOG_WARNING, "Failed to export image %s!", filename.c_str());
            }
            saving_single = false;
        }
    }

    BeginTextureMode(renderTexture);
    // rlEnableFramebuffer(renderTexture.id);
    ClearBackground(clearColor);

    if (other_uniform_buffers.count("time") >= 1) {
        other_uniform_buffers["time"].f = runtime;
        SetShaderValue(pixelShader, shader_locs["time"].first, &runtime, SHADER_UNIFORM_FLOAT);
    }
    if (other_uniform_buffers.count("dt") >= 1) {
        other_uniform_buffers["dt"].f = dt;
        SetShaderValue(pixelShader, shader_locs["dt"].first, &dt, SHADER_UNIFORM_FLOAT);
    }
    if (other_uniform_buffers.count("frame") >= 1) {
        other_uniform_buffers["frame"].i = frame_counter;
        SetShaderValue(pixelShader, shader_locs["frame"].first, &frame_counter, SHADER_UNIFORM_INT);
    }

    // if (shader_locs.count("selfTexture") >= 1) {
    //     if (IsTextureReady(selfTexture.texture)) {
    //         SetShaderValueTexture(pixelShader, shader_locs["selfTexture"].first, selfTexture.texture);
    //     }
    // }

    // this is seperate because SetShaderValueTexture unbinds the shader program after it runs
    for (auto p : shader_locs) {
        if (p.second.second == SAMPLER2D && image_uniform_buffers.count(p.first) > 0) {
            auto& im = image_uniform_buffers[p.first];
            if (!IsTextureReady(im.second)) {
                im.second = BlankTexture();
            }
            SetShaderValueTexture(pixelShader, p.second.first, im.second);
        }
    }

    // bind the textures to the GL state manually
    glUseProgram(pixelShader.id);
    for (auto p : shader_locs) {
        if (p.second.second == SAMPLER2D && image_uniform_buffers.count(p.first) > 0) {
            auto& im = image_uniform_buffers[p.first];
            // if (!IsTextureReady(im.second)) {
            //     im.second = BlankTexture();
            // }
            glUniform1i(p.second.first, p.second.first);
            glActiveTexture(GL_TEXTURE0 + p.second.first);
            glBindTexture(GL_TEXTURE_2D, im.second.id);
        }
    }

    // DrawRectangle(0, 0, renderTexture.texture.width, renderTexture.texture.height, WHITE);

    // BeginShaderMode(pixelShader); 
    
    rlViewport(0, 0, rt_width, rt_height);
    switch (drawType) {
        case MODEL:
            if (IsModelReady(model) && model.meshCount > 0) {
                BeginMode3D(camera);
                glUseProgram(pixelShader.id);
                Matrix matView = rlGetMatrixModelview();
                Matrix matProjection = rlGetMatrixProjection();
                Matrix matModelView = matView;
                Matrix matNormal = MatrixIdentity();
                Matrix matModelViewProjection = MatrixMultiply(matModelView, matProjection);
                for (auto p : shader_locs) {
                    if (p.second.second == MATRIX) {
                        if (p.first == "mvp") {
                            // Send combined model-view-projection matrix to shader
                            rlSetUniformMatrix(p.second.first, matModelViewProjection);
                        } else if (p.first == "matView") {
                            rlSetUniformMatrix(p.second.first, matView);
                        } else if (p.first == "matProjection") {
                            rlSetUniformMatrix(p.second.first, matProjection);
                        } else if (p.first == "matNormal") {
                            rlSetUniformMatrix(p.second.first, matNormal);
                        }
                    }
                }
                for (int i = 0; i < model.meshCount; i++) {
                    Mesh& mesh = model.meshes[i];
                    rlEnableVertexArray(mesh.vaoId);
                    // Draw mesh
                    if (mesh.indices != NULL) rlDrawVertexArrayElements(0, mesh.triangleCount*3, 0);
                    else rlDrawVertexArray(0, mesh.vertexCount);
                }
                EndMode3D();
            }
            break;
        case TEXTURE:
            DrawEmptyTriangleStrip();
            break;
        default:
            break;
    }
    // EndShaderMode();
    EndTextureMode();
    // BeginTextureMode(selfTexture);
    // ClearBackground(BLACK);
    // Rectangle srcrec {0.0, 0.0, (float)renderTexture.texture.width, (float)renderTexture.texture.height};
    // Rectangle dstrec {0.0, -(float)renderTexture.texture.height, (float)renderTexture.texture.width, -(float)renderTexture.texture.height};
    // DrawTexturePro(renderTexture.texture, srcrec, dstrec, {0.0, 0.0}, 0.0, WHITE);
    // EndTextureMode();
    std::swap(selfTexture, renderTexture);
    frame_counter++;
    runtime += dt;
}

void LoadUniformsFromCode(const char* code, int len,
                            std::map<std::string, Uniform>& other_uniform_buffers,
                            std::map<std::string, Uniform>& new_other_uniform_buffers,
                            PixelShader* ps, bool set_buffers=true) {
    auto& shader_locs = ps->shader_locs;
    auto& sampler_count = ps->sampler_count;
    int i = 0;
    sampler_count = 0;
    while (i < len) {
        if (!memcmp(&code[i], "uniform ", strlen("uniform "))) {
            // TraceLog(LOG_INFO, "found uniform at offset %u", i);
            i += strlen("uniform ");
            while (isspace(code[i]) && i < len) i++;
            for (auto p : shader_uniform_type_strings) {
                std::string s = p.first;
                size_t l = s.length();
                if (!memcmp(&code[i], s.c_str(), s.length())) {
                    size_t j = i + l;
                    while (isspace(code[j]) && j < len) j++;
                    const char* eol = (char*)memchr(&code[j], '\n', len-j);
                    if (eol == nullptr) eol = (char*)memchr(&code[j], 0x0D, len-j);
                    if (eol == nullptr) eol = &code[len];
                    else {
                        while (eol-1>code && strchr("\n\x0D", eol[-1])) {
                            eol--;
                        }
                        // if (eol-1 <= code) continue;
                    }
                    // if (eol[-1] != ';') continue;
                    const char* eos = &code[j];
                    while (isspace(*eos) && eos < eol) eos++;
                    if (code[i+l] == '(') {
                        while (eos < eol && *eos != ')') eos++;
                        while (eos < eol && !(isalnum(*eos) || *eos == '_')) eos++;
                        j = eos - code;
                    }
                    while (isspace(*eos) && eos < eol) eos++;
                    while (eos < eol && (isalnum(*eos) || *eos == '_')) eos++;
                    std::string name = std::string(&code[j], eos - &code[j]);
                    int loc = -1;
                    if (p.second == SAMPLER2D) {
                        // location for samplers is different
                        loc = sampler_count++;
                    } else {
                        loc = rlGetLocationUniform(ps->pixelShader.id, name.c_str());
                    }
                    // if (name == "texture0") {
                    //     continue;
                    // }
                    shader_locs.insert(std::make_pair(name, std::make_pair(loc, p.second)));
                    if (loc >= 0) {
                        Uniform uni = {0};
                        if (p.second.isRange) {
                            uni.min = 0.0;
                            uni.max = 1.0;
                            if (code[i+l] == '(') {
                                size_t sob = i + l;
                                size_t eob = sob;
                                while (code[eob] != ')' && eob < len) eob++;
                                if (eob >= len) {
                                    TraceLog(LOG_WARNING, "Missing closing bracket in range specifier for uniform \"%s\"", name.c_str());
                                } else {
                                    uni.max = atof(&code[sob+1]);
                                    while (strchr(",\n", code[sob]) == nullptr && sob < eob) sob++;
                                    if (code[sob] == ',') {
                                        // two numbers -> range X..Y
                                        uni.min = uni.max;
                                        uni.max = atof(&code[sob+1]);
                                    } else {
                                        // single number -> range 0..X
                                    }
                                }
                            }
                        }
                        if (set_buffers) {
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
                                        SetShaderValue(ps->pixelShader, loc, &uni.f, SHADER_UNIFORM_FLOAT);
                                        break;
                                    case INT:
                                        SetShaderValue(ps->pixelShader, loc, &uni.i, SHADER_UNIFORM_INT);
                                        break;
                                    case VEC2:
                                    case SLIDER2:
                                        SetShaderValue(ps->pixelShader, loc, &uni.v, SHADER_UNIFORM_VEC2);
                                        break;
                                    case VEC3:
                                    case SLIDER3:
                                    case COLOR3:
                                        SetShaderValue(ps->pixelShader, loc, &uni.v, SHADER_UNIFORM_VEC3);
                                        break;
                                    case VEC4:
                                    case COLOR4:
                                    case SLIDER4:
                                        SetShaderValue(ps->pixelShader, loc, &uni.v, SHADER_UNIFORM_VEC4);
                                        break;
                                    default:
                                        break;
                                }
                            } else {
                                switch (p.second) {
                                    case FLOAT:
                                        glGetUniformfv(ps->pixelShader.id, loc, &uni.f);
                                        break;
                                    case INT:
                                        glGetUniformiv(ps->pixelShader.id, loc, &uni.i);
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
                                        glGetUniformfv(ps->pixelShader.id, loc, (float*)&uni.v);
                                        break;
                                    default:
                                        break;
                                }
                                if (p.second < SAMPLER2D) {
                                    new_other_uniform_buffers.insert(std::make_pair(name, uni));
                                }
                            }
                            // if (p.second == SAMPLER2D) {
                            //     SetShaderValueTexture(pixelShader, loc, BlankTexture());
                            // }
                        }
                        if (p.second.isRange) {
                            TraceLog(LOG_INFO, "found uniform %s %s (range %.3f to %.3f)", s.c_str(), name.c_str(), uni.min, uni.max);
                        } else {
                            TraceLog(LOG_INFO, "found uniform %s %s", s.c_str(), name.c_str());
                        }
                    }
                    i = eol - code;
                    break;
                }
            }
        } else {
            i++;
        }
    }
}


bool PixelShader::Load(const char* filename) {
    std::ifstream fd(filename, std::ios::in | std::ios::binary);
    char* fragment_code;
    size_t len = 0;
    drawType = ShaderDrawType::NONE;
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
    // check for shader type
    drawType = ShaderDrawType::TEXTURE;
    size_t i = 0;
    while (i < len) {
        if (!memcmp(&fragment_code[i], "#type: ", strlen("#type: "))) {
            i += strlen("#type: ");
            while (isspace(fragment_code[i]) && i < len) i++;
            if (!memcmp(&fragment_code[i], "model", strlen("model"))) {
                drawType = ShaderDrawType::MODEL;
                break;
            }
        } else {
            i++;
        }
    }
    Shader newPixelShader;
    const char* vertex_code;
    switch (drawType) {
        case MODEL:
            TraceLog(LOG_INFO, "Loading model shader.");
            newPixelShader = LoadShaderFromMemory(vertex_code = vertex_shader_code_model, fragment_code);
            LoadModel("(sphere)");
            break;
        case TEXTURE:
        default:
            TraceLog(LOG_INFO, "Loading pixel shader.");
            newPixelShader = LoadShaderFromMemory(vertex_code = vertex_shader_code_default, fragment_code);
            break;
    }
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

    LoadUniformsFromCode(fragment_code, len, other_uniform_buffers, new_other_uniform_buffers, this);
    LoadUniformsFromCode(vertex_code, strlen(vertex_code), other_uniform_buffers, new_other_uniform_buffers, this, false);

    std::vector<std::string> to_remove;
    for (auto p : shader_locs) {
        if (p.second.first == -1) {
            to_remove.push_back(p.first);
            TraceLog(LOG_WARNING,
                "Uniform \"%s\" is defined but not active, assigning it will have no effect.", p.first.c_str());
        }
    }
    for (auto r : to_remove) {
        shader_locs.erase(r);
    }

    editor.SetText(fragment_code);
    if (IsFileExtension(filename, ".glsl") || IsFileExtension(filename, ".fs") || IsFileExtension(filename, ".vs")) {
        editor.SetLanguageDefinition(TextEditor::LanguageDefinition::GLSL());
    } else if (IsFileExtension(filename, ".hlsl")) {
        editor.SetLanguageDefinition(TextEditor::LanguageDefinition::HLSL());
    } else if (IsFileExtension(filename, ".cpp") || IsFileExtension(filename, ".hpp")) {
        editor.SetLanguageDefinition(TextEditor::LanguageDefinition::CPlusPlus());
    } else if (IsFileExtension(filename, ".c") || IsFileExtension(filename, ".h")) {
        editor.SetLanguageDefinition(TextEditor::LanguageDefinition::C());
    } else {
        editor.SetLanguageDefinition(TextEditor::LanguageDefinition::GLSL());
    }
    // other_uniform_buffers.clear();
    other_uniform_buffers = new_other_uniform_buffers;
    return true;
}

bool PixelShader::New(const char* filename) {
    std::ofstream fd(filename, std::ios::out | std::ios::binary);
    if (!fd.is_open()) {
        return false;
    }
    fd.write(fragment_shader_code_default, strlen(fragment_shader_code_default));
    fd.close();
    bool success = Load(strdup(filename));
    shader_locs.clear();
    name = "Shader " + std::to_string(numLoadedShadersEver);
    num = numLoadedShadersEver++;
    return success;
}

void PixelShader::Reload() {
    int w = rt_width;
    int h = rt_height;
    nlohmann::json j = DumpUniforms();
    Unload();
    Load(filename);
    Setup(w, h);
    LoadUniforms(j);
}

void PixelShader::Unload() {
    // CleanupTexture(albedo_tex);
    UnloadRenderTexture(renderTexture);
    UnloadRenderTexture(selfTexture);
    UnloadShader(pixelShader);
    renderTexture = {0};
    selfTexture = {0};
    pixelShader = {0};
    if (drawType == ShaderDrawType::MODEL) {
        if (IsModelReady(model)) {
            UnloadModel(model);
        }
        delete modelFilebuf;
        model = {0};
        modelFilebuf = nullptr;
    }
}

void PixelShader::Setup(int width, int height) {
    // albedo_tex = BlankTexture();
    rt_width = width;
    rt_height = height;
    renderTexture = LoadRenderTexture(rt_width, rt_height);
    selfTexture = LoadRenderTexture(rt_width, rt_height);
}

void PixelShader::SetRTSize(int width, int height) {
    UnloadRenderTexture(renderTexture);
    UnloadRenderTexture(selfTexture);
    Setup(width, height);
}

void PixelShader::SetClearColor(int r, int g, int b, int a) {
    clearColor.r = r;
    clearColor.g = g;
    clearColor.b = b;
    clearColor.a = a;
}

void PixelShader::LoadModel(std::string filename) {
    Model newModel;
    if (filename == "(sphere)") {
        newModel = LoadModelFromMesh(GenMeshSphere(1.0, 40.0, 20.0));
    } else if (filename == "(cube)") {
        newModel = LoadModelFromMesh(GenMeshCube(1.0, 1.0, 1.0));
    } else if (filename == "(donut)") {
        newModel = LoadModelFromMesh(GenMeshTorus(0.5, 2.0, 20.0, 40.0));
    } else {
        newModel = ::LoadModel(filename.c_str());
    }
    if (IsModelReady(newModel)) {
        if (IsModelReady(model)) {
            UnloadModel(model);
        }
        model = newModel;
        if (modelFilebuf == nullptr) {
            modelFilebuf = new char[IMAGE_NAME_BUFFER_LENGTH];
        }
        strncpy(modelFilebuf, filename.c_str(), IMAGE_NAME_BUFFER_LENGTH-1);
        modelFilebuf[IMAGE_NAME_BUFFER_LENGTH-1] = 0;
    }
}


bool PixelShader::InputTextureFields(std::string str) {
    if (image_uniform_buffers.count(str) < 1) {
        image_uniform_buffers.insert(
            std::make_pair(str, std::make_pair(new char[IMAGE_NAME_BUFFER_LENGTH] {0}, Texture2D {0})));
    }
    char* buf = image_uniform_buffers[str].first;
    auto& tex = image_uniform_buffers[str].second;
    ImGui::PushID(str.c_str());
    ImGui::InputTextWithHint(str.c_str(), "path to image", buf, IMAGE_NAME_BUFFER_LENGTH);
    static std::map<std::string, bool> browse_returned;
    if (browse_returned.count(str) < 1) {
        browse_returned.insert(std::make_pair(str, false));
    }
    bool isSet = false;
    if (ImGui::Button("Browse")) {
        std::string id = std::string("BrowseForImageInput ")+str;
        fileDialogManager.openIfNotAlready(id, "Select an Image ("+str+")",
            BrowseFilenameCB(buf, &browse_returned[str]));
    }

    ImGui::SameLine();
    if (ImGui::Button("Reload Image")) {
        SetUniform(str, SAMPLER2D, buf);
        isSet = true;
    }
    InputTextureOptions(tex);
    if (ImGui::Button("Set RT Size from Texture")) {
        if (tex.width > 0 && tex.height > 0) {
            SetRTSize(rt_width=tex.width, rt_height=tex.height);
        }
    }
    if (pixelShaderReference != nullptr) {
        if (ImGui::Button("Paste Reference")) {
            CleanupTexture(tex);
            tex = pixelShaderReference->renderTexture.texture;
            snprintf(buf, IMAGE_NAME_BUFFER_LENGTH, "(Shader Output %u)", pixelShaderReference->num);
            pixelShaderReference = nullptr;
            isSet = true;
        }
    }
    ImGui::PopID();

    if (browse_returned[str]) {
        browse_returned[str] = false;
        SetUniform(str, SAMPLER2D, buf);
        isSet = true;
    }

    return isSet;
}

void PixelShader::UpdateCamera(float dt) {
    static Vector2 cursorPos;
    if ((controlling_camera || focused) && IsMouseButtonPressed(MOUSE_BUTTON_RIGHT)) {
        // don't enable control if the mouse isn't hovering over the window and it is active
        if (!controlling_camera && !CheckCollisionPointRec(GetMousePosition(), outputArea)) {
            return;
        }
        controlling_camera = !controlling_camera;
        if (controlling_camera) {
            cursorPos = GetMousePosition();
            DisableCursor();
        } else {
            EnableCursor();
            SetMousePosition(cursorPos.x, cursorPos.y);
        }
    } else if (controlling_camera) {
        // Note: most of this code is adapted from Raylib's camera implementation
        // I extracted it in order to couple it to framerate
        float CAMERA_MOVE_SPEED = dt*1.5f;
        float CAMERA_ROTATION_SPEED = dt*1.0f;
        float CAMERA_PAN_SPEED = dt*4.0f;
        float CAMERA_MOUSE_MOVE_SENSITIVITY = dt*0.06f;
        float CAMERA_MOUSE_SCROLL_SENSITIVITY = dt*3.0f;
        Vector2 mousePositionDelta = GetMouseDelta();
        SetMousePosition(outputArea.x+outputArea.width*0.5f, outputArea.y+outputArea.height*0.5f);

        const bool moveInWorldPlane = false;
        const bool rotateAroundTarget = false;
        const bool lockView = true;
        const bool rotateUp = false;

        // Camera rotation
        if (IsKeyDown(KEY_DOWN)) CameraPitch(&camera, -CAMERA_ROTATION_SPEED, lockView, rotateAroundTarget, rotateUp);
        if (IsKeyDown(KEY_UP)) CameraPitch(&camera, CAMERA_ROTATION_SPEED, lockView, rotateAroundTarget, rotateUp);
        if (IsKeyDown(KEY_RIGHT)) CameraYaw(&camera, -CAMERA_ROTATION_SPEED, rotateAroundTarget);
        if (IsKeyDown(KEY_LEFT)) CameraYaw(&camera, CAMERA_ROTATION_SPEED, rotateAroundTarget);
        if (IsKeyDown(KEY_Q)) CameraRoll(&camera, -CAMERA_ROTATION_SPEED);
        if (IsKeyDown(KEY_E)) CameraRoll(&camera, CAMERA_ROTATION_SPEED);

        // Camera movement
        if (!IsGamepadAvailable(0))
        {
            // Mouse
            CameraYaw(&camera, -mousePositionDelta.x*CAMERA_MOUSE_MOVE_SENSITIVITY, rotateAroundTarget);
            CameraPitch(&camera, -mousePositionDelta.y*CAMERA_MOUSE_MOVE_SENSITIVITY, lockView, rotateAroundTarget, rotateUp);

            // Keyboard
            if (IsKeyDown(KEY_W)) CameraMoveForward(&camera, CAMERA_MOVE_SPEED, moveInWorldPlane);
            if (IsKeyDown(KEY_A)) CameraMoveRight(&camera, -CAMERA_MOVE_SPEED, moveInWorldPlane);
            if (IsKeyDown(KEY_S)) CameraMoveForward(&camera, -CAMERA_MOVE_SPEED, moveInWorldPlane);
            if (IsKeyDown(KEY_D)) CameraMoveRight(&camera, CAMERA_MOVE_SPEED, moveInWorldPlane);
        }
        else
        {
            // Gamepad controller
            CameraYaw(&camera, -(GetGamepadAxisMovement(0, GAMEPAD_AXIS_RIGHT_X) * 2)*CAMERA_MOUSE_MOVE_SENSITIVITY, rotateAroundTarget);
            CameraPitch(&camera, -(GetGamepadAxisMovement(0, GAMEPAD_AXIS_RIGHT_Y) * 2)*CAMERA_MOUSE_MOVE_SENSITIVITY, lockView, rotateAroundTarget, rotateUp);

            if (GetGamepadAxisMovement(0, GAMEPAD_AXIS_LEFT_Y) <= -0.25f) CameraMoveForward(&camera, CAMERA_MOVE_SPEED, moveInWorldPlane);
            if (GetGamepadAxisMovement(0, GAMEPAD_AXIS_LEFT_X) <= -0.25f) CameraMoveRight(&camera, -CAMERA_MOVE_SPEED, moveInWorldPlane);
            if (GetGamepadAxisMovement(0, GAMEPAD_AXIS_LEFT_Y) >= 0.25f) CameraMoveForward(&camera, -CAMERA_MOVE_SPEED, moveInWorldPlane);
            if (GetGamepadAxisMovement(0, GAMEPAD_AXIS_LEFT_X) >= 0.25f) CameraMoveRight(&camera, CAMERA_MOVE_SPEED, moveInWorldPlane);
        }

        if (IsKeyDown(KEY_SPACE)) CameraMoveUp(&camera, CAMERA_MOVE_SPEED);
        if (IsKeyDown(KEY_LEFT_CONTROL)) CameraMoveUp(&camera, -CAMERA_MOVE_SPEED);

        if (IsKeyPressed(KEY_ESCAPE)) {
            controlling_camera = false;
            EnableCursor();
            SetMousePosition(cursorPos.x, cursorPos.y);
        }
    }
}

void PixelShader::DrawGUI(float dt) {
    focused = false;
    ImGui::Begin((name + " Output").c_str(), &is_active);
    focused |= ImGui::IsWindowFocused();
    int w = rt_width, h = rt_height;
    int view_width = ImGui::GetWindowWidth();
    int view_height = ImGui::GetWindowHeight();
    if (w > view_width) {
        h = ((float)view_width*h)/w;
        w = view_width;
    }
    if (h > view_height) {
        w = ((float)view_height*w)/h;
        h = view_height;
    }

    rlImGuiImageRect(&renderTexture.texture, w, h, {0.0, 0.0, (float)rt_width, -(float)rt_height});

    if (drawType == ShaderDrawType::MODEL && ImGui::Button("Reset Viewport")) {
        camera.position = {-10, 0, 0};
        camera.target = {-8, 0, 0};
        camera.up = {0, 1, 0};
        camera.fovy = atanf(tanf(fovx * PI / 360.0f) * rt_height/(float)rt_width) * 360.0f / PI;
    }

    auto wpos = ImGui::GetWindowPos();
    auto wsize = ImGui::GetWindowSize();
    outputArea = {
        wpos.x, wpos.y, wsize.x, wsize.y
    };

    ImGui::End();

    ImGui::Begin((name + " Options").c_str(), &is_active);
    focused |= ImGui::IsWindowFocused();

    if (ImGui::Button("Reload Shader")) {
        Reload();
    }
    ImGui::SameLine();
    if (ImGui::Button("Reset Time")) {
        frame_counter = 0;
        runtime = 0;
    }
    ImVec4 clearColorF = ImGui::ColorConvertU32ToFloat4(*(uint32_t*)&clearColor);
    if (ImGui::ColorEdit4("Clear Color", (float*)&clearColorF)) {
        *(uint32_t*)&clearColor = ImGui::ColorConvertFloat4ToU32(clearColorF);
    }
    // InputTextureFields("Diffuse/Albedo", image_input, &other_uniform_buffers["texture0"], albedo_tex, -1);
    if (ImGui::InputTextWithHint("Image Output", "path to image to save", image_output, sizeof(image_output))) {
        saving_filename = std::string(image_output);
    }
    if (ImGui::Button("Save Image") && !saving_sequence) {
        saving_single = true;
    }
    ImGui::SameLine();
    ImGui::Checkbox("Save Sequence", &saving_sequence);
    ImGui::SameLine();
    if (ImGui::Checkbox("Save Gif", &saving_gif)) {
        if (saving_gif) {
            // started recording
            gifState = {0};
            msf_gif_begin(&gifState, rt_width, rt_height);
            TraceLog(LOG_INFO, "Started recording gif...");
        } else {
            // finished recording
            TraceLog(LOG_INFO, "Finishing recording gif...");
            MsfGifResult result = msf_gif_end(&gifState);
            if (result.data != nullptr) {
                std::ofstream fd(saving_filename, std::ios::out | std::ios::binary);
                if (fd.is_open()) {
                    fd.write((char*)result.data, result.dataSize);
                    fd.close();
                    TraceLog(LOG_INFO, "Gif created successfuly! Size: %u Kb", result.dataSize/1024);
                } else {
                    TraceLog(LOG_ERROR, "Failed to create gif file!");
                }
            } else {
                TraceLog(LOG_ERROR, "Failed to finalize gif!");
            }
        }
    }
    static int size[2] = {rt_width, rt_height};
    static float set_size_timer = -1.0f;
    if (ImGui::InputInt2("Render Texture Size", size)) {
        if (size[0] != rt_width || size[1] != rt_height) {
            set_size_timer = 0.5f;
        }
    } else if (set_size_timer > 0.0f) {
        set_size_timer -= dt;
        if (set_size_timer <= 0.0f) {
            SetRTSize(rt_width=size[0], rt_height=size[1]);
        }
    }
    InputTextureOptions(renderTexture.texture);
    if (ImGui::Button("Clone Shader")) {
        requested_clone = true;
    }
    ImGui::SameLine();
    if (ImGui::Button("Copy Reference To Output")) {
        requested_reference = true;
    }
    if (drawType == ShaderDrawType::MODEL) {
        static bool browse_returned;
        bool isSet = false;

        if (modelFilebuf == nullptr) {
            modelFilebuf = new char[IMAGE_NAME_BUFFER_LENGTH] {0};
        }
        ImGui::InputTextWithHint("Model", "path to file", modelFilebuf, IMAGE_NAME_BUFFER_LENGTH);
        if (ImGui::Button("Browse")) {
            std::string id = std::string("BrowseForModelInput ") + std::to_string(num);
            fileDialogManager.openIfNotAlready(id, "Select a model",
                BrowseFilenameCB(modelFilebuf, &browse_returned));
        }
        ImGui::SameLine();
        if (ImGui::Button("Load") || browse_returned) {
            LoadModel(modelFilebuf);
            browse_returned = false;
        }
    }
    ImGui::End();

    ImGui::Begin((name + " Uniforms").c_str(), &is_active);
    focused |= ImGui::IsWindowFocused();
    int loc_count = 0;
    for (auto p : shader_locs) {
        auto v = p.second;
        Uniform* uniform = nullptr;
        ImGui::PushID(loc_count++);
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
                InputTextureFields(p.first);
                break;
            default:
                break;
        }
        ImGui::PopID();
    }
    ImGui::End();
    UpdateCamera(dt);
    DrawTextEditor();
}

void PixelShader::DrawTextEditor() {
    auto cpos = editor.GetCursorPosition();
    ImGui::Begin((name + " Text Editor").c_str(), &is_active, ImGuiWindowFlags_HorizontalScrollbar | ImGuiWindowFlags_MenuBar);
    focused |= ImGui::IsWindowFocused();
    focused |= editor.IsHandleKeyboardInputsEnabled();
    ImGui::SetWindowSize(ImVec2(800, 600), ImGuiCond_FirstUseEver);
    if (ImGui::BeginMenuBar())
    {
        bool shift_down = IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT);
        bool control_down = IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL);
        bool save = focused && control_down && !shift_down && IsKeyPressed(KEY_S);
        bool saveas = focused && control_down && shift_down && IsKeyPressed(KEY_S);
        if (ImGui::BeginMenu("File"))
        {
            save |= ImGui::MenuItem("Save", "Ctrl-S");
            saveas |= ImGui::MenuItem("Save As...", "Ctrl-Shift-S");
            if (saveas) {
                fileDialogManager.openIfNotAlready(("Save As (" + name + ")"), TextSaveAsCB(editor.GetText()), true);
            }
            if (ImGui::MenuItem("Close")) {
                is_active = false;
            }
            ImGui::EndMenu();
        }
        if (save && !saveas)
        {
            auto textToSave = editor.GetText();
            std::ofstream fd(filename);
            if (fd.is_open()) {
                fd.write(textToSave.c_str(), textToSave.length() - 1);
                fd.close();
            }
            requested_reload = true;
        }
        if (ImGui::BeginMenu("Edit"))
        {
            bool ro = editor.IsReadOnly();
            if (ImGui::MenuItem("Read-only mode", nullptr, &ro))
                editor.SetReadOnly(ro);
            ImGui::Separator();

            if (ImGui::MenuItem("Undo", "Ctrl-Z", nullptr, !ro && editor.CanUndo()))
                editor.Undo();
            if (ImGui::MenuItem("Redo", "Ctrl-Y", nullptr, !ro && editor.CanRedo()))
                editor.Redo();

            ImGui::Separator();

            if (ImGui::MenuItem("Copy", "Ctrl-C", nullptr, editor.HasSelection()))
                editor.Copy();
            if (ImGui::MenuItem("Cut", "Ctrl-X", nullptr, !ro && editor.HasSelection()))
                editor.Cut();
            if (ImGui::MenuItem("Delete", "Del", nullptr, !ro && editor.HasSelection()))
                editor.Delete();
            if (ImGui::MenuItem("Paste", "Ctrl-V", nullptr, !ro && ImGui::GetClipboardText() != nullptr))
                editor.Paste();

            ImGui::Separator();

            if (ImGui::MenuItem("Select all", nullptr, nullptr))
                editor.SetSelection(TextEditor::Coordinates(), TextEditor::Coordinates(editor.GetTotalLines(), 0));

            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Theme"))
        {
            if (ImGui::MenuItem("Dark palette"))
                editor.SetPalette(TextEditor::GetDarkPalette());
            if (ImGui::MenuItem("Light palette"))
                editor.SetPalette(TextEditor::GetLightPalette());
            if (ImGui::MenuItem("Retro blue palette"))
                editor.SetPalette(TextEditor::GetRetroBluePalette());
            ImGui::EndMenu();
        }
        ImGui::EndMenuBar();
    }

    ImGui::Text("%6d/%-6d %6d lines  | %s | %s | %s | %s", cpos.mLine + 1, cpos.mColumn + 1, editor.GetTotalLines(),
        editor.IsOverwrite() ? "Ovr" : "Ins",
        editor.CanUndo() ? "*" : " ",
        editor.GetLanguageDefinition().mName.c_str(), filename);

    editor.Render("TextEditor");
    ImGui::End();
    if (requested_reload) {
        Reload();
        requested_reload = false;
    }
}

void PixelShader::SetUniform(std::string name, ShaderUniformType type, void* value) {
    if (shader_locs.count(name) < 1) return;
    if (type < SAMPLER2D && other_uniform_buffers.count(name) < 1) return;
    auto v = shader_locs[name];
    Uniform* uniform = type == SAMPLER2D ? nullptr : &other_uniform_buffers[name];
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
                    std::make_pair(name, std::make_pair(new char[IMAGE_NAME_BUFFER_LENGTH] {0}, Texture2D {0}))
                );
            } else {
                CleanupTexture(image_uniform_buffers[name].second);
            }
            {
                auto& buf = image_uniform_buffers[name];
                strncpy(buf.first, (char*)value, IMAGE_NAME_BUFFER_LENGTH-1);
                buf.first[IMAGE_NAME_BUFFER_LENGTH-1] = 0;
                buf.second = LoadTextureFromString(buf.first);
                // SetShaderValueTexture(pixelShader, v.first, buf.second);
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
                    if (j["v"].is_string()) {
                        std::string tex = j["v"].get<std::string>();
                        SetUniform(p.key(), SAMPLER2D, (void*)tex.c_str());
                    }
                    break;
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
        ShaderUniformType type = l.second.second;
        bool should_include = true;
        nlohmann::json j = {{"t", type}};
        if (type == SAMPLER2D) {
            if (key == "selfTexture") {
                continue;
            }
            // printf("Dumping Sampler2D\n");
            if (image_uniform_buffers.count(key) < 1) {
                should_include = false;
                // printf("Sampler2D Value not found\n");
            } else if (strlen(image_uniform_buffers[key].first) > 0) {
                j["v"] = std::string(image_uniform_buffers[key].first);
                // printf("Sampler2D Value: %s\n", image_uniform_buffers[key].first);
            } else {
                should_include = false;
                // printf("Sampler2D Value empty\n");
            }
        } else if (other_uniform_buffers.count(key) > 0) {
            Uniform uniform = other_uniform_buffers[key];
            if (uniform.isSet) {
                switch (type) {
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
                default:
                    break;
                }
            }
        }
        if (should_include) {
            json[key] = j;
        }
    }
    // printf("%s\n", json.dump().c_str());
    return json;
}
