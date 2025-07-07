#include <cstdarg>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <functional>
#include <list>
#include <vector>

#include <raylib.h>
#include <imgui.h>
#include <rlImGui.h>
#include <rlgl.h>
#include <external/glad.h>

#include "PixelShader.hpp"
#include "FileDialogs.hpp"
#include "JsonConfig.hpp"
#include "nlohmann/json.hpp"

PixelShader* pixelShaderReference = nullptr;
std::list<PixelShader*> pixelShaders;
FileDialogs::FileDialogManager fileDialogManager;
int default_rt_width = 512;
std::list<std::string> log_lines;
std::ofstream __log_fd;

void __TraceLogCallback(int level, const char* s, va_list args) {
    const int buffer_size = 512;
    const std::vector<const char*> log_levels {
        "TRACE",
        "DEBUG",
        "INFO",
        "WARNING",
        "ERROR",
        "FATAL",
    };
    TraceLogCallback a;
    std::string str;
    char buf[buffer_size];
    str.reserve(buffer_size+8);
    snprintf(buf, buffer_size, "[%s] ", log_levels[level - 1]);
    str.assign(buf);
    vsnprintf(buf, buffer_size, s, args);
    str.append(buf);
    log_lines.push_back(str);
    printf("%s\n", str.c_str());
    str.append("\n");
    __log_fd.write(str.c_str(), str.length());
}

bool LoadPixelShader(std::string file) {
    if (!file.size()) return false;
    PixelShader* ps = new PixelShader(file.c_str());
    ps->Setup(default_rt_width);
    if (ps->IsReady()) {
        pixelShaders.push_back(ps);
        return true;
    }
    return false;
}

JsonConfig LoadWorkspace(std::string fname="workspace.json") {
    JsonConfig workspaceCfg(fname);
    if (workspaceCfg.contains("shaders")) {
        auto shaders = workspaceCfg["shaders"];
        if (shaders.is_object()) {
            for (auto s : shaders.items()) {
                std::string fname = s.key();
                nlohmann::json j = s.value();
                if (!j.is_object()) continue;
                if (LoadPixelShader(fname)) {
                    if (j.contains("uniforms") && j["uniforms"].is_object()) {
                        PixelShader* ps = pixelShaders.back();
                        if (j.contains("width") && j["width"].is_number()) {
                            ps->SetRTWidth(j["width"].get<int>());
                        }
                        ps->LoadUniforms(j["uniforms"]);
                    }
                }
            }
        }
    }
    return workspaceCfg;
}

void SaveWorkspace(JsonConfig& cfg) {
    nlohmann::json json;
    for (auto ps : pixelShaders) {
        if (ps == nullptr) continue;
        nlohmann::json j = ps->DumpUniforms();
        json[std::string(ps->filename)] = {
            {"uniforms", j},
            {"width", ps->rt_width},
        };
    }
    cfg["shaders"] = json;
    cfg.save();
}

int main(int argc, char** argv) {
    char pixel_shader_file[IMAGE_NAME_BUFFER_LENGTH] = "shaders/noise.fs";
    if (argc > 1) {
        strcpy(pixel_shader_file, argv[1]);
    }
    if (argc > 2) {
        default_rt_width = atoi(argv[2]);
    }

    __log_fd.open("debug.log");
    SetTraceLogCallback(__TraceLogCallback);

    SetConfigFlags(FLAG_VSYNC_HINT | FLAG_WINDOW_RESIZABLE);
    InitWindow(1000, 600, "PixelShaderTestBench");
    SetTargetFPS(60);
    SetExitKey(-1);
    int render_texture_update_rate = 30;
    float render_texture_update_timer = 0;

    rlImGuiSetup(true);
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;

    JsonConfig pinsCfg("pins.json");
    if (pinsCfg.contains("pinned_folders")) {
        auto pinned = pinsCfg["pinned_folders"];
        if (pinned.is_array()) {
            for (auto p : pinned) {
                if (p.is_string()) {
                    FileDialogs::AddPinnedFolder(p.get<std::string>());
                }
            }
        }
    }

    JsonConfig workspaceCfg = LoadWorkspace();

    bool scroll_log_to_bottom = true;
    float dt = 0.0f;
    int frame_counter = 0;
    int target_fps = 60;
    while (!WindowShouldClose()) {
        BeginDrawing();
        ClearBackground(BLACK);
        DrawFPS(1, 1);
        if (render_texture_update_timer >= 1.0 / render_texture_update_rate) {
            render_texture_update_timer -= 1.0 / render_texture_update_rate;
            for (auto& ps : pixelShaders) {
                if (ps != nullptr && ps->IsReady()) {
                    ps->Update(dt, frame_counter);
                }
            }
        }
        rlImGuiBegin();

        ImGui::Begin("Options");
        ImGui::InputTextWithHint("Pixel Shader File", "path to fragment shader", pixel_shader_file, sizeof(pixel_shader_file));
        if (ImGui::Button("Browse")) {
            fileDialogManager.openIfNotAlready("BrowseForShaderInput", "Load Shader", [] (std::string s) { return LoadPixelShader(s); });
        }
        ImGui::SameLine();
        if (ImGui::Button("Load Shader")) {
            frame_counter = -1;
            if (!LoadPixelShader(pixel_shader_file)) {
                TraceLog(LOG_WARNING, "Failed to load Pixel shader %s!", pixel_shader_file);
            }
        }
        ImGui::SliderInt("Target updates per second", &render_texture_update_rate, 1, 60);
        if (ImGui::SliderInt("Target FPS", &target_fps, 10, 500)) {
            SetTargetFPS(target_fps);
        }
        if (ImGui::Button("Unlimited FPS")) {
            SetTargetFPS((target_fps = -1));
        }
        if (ImGui::Button("Save Workspace")) {
            SaveWorkspace(workspaceCfg);
        }
        ImGui::SameLine();
        if (ImGui::Button("Save As...")) {
            fileDialogManager.openIfNotAlready("SaveWorkspaceAs", "Save Workspace As",
                [&workspaceCfg] (std::string fname) { if (fname.size() < 1) return false; return workspaceCfg.save(fname); },
            true);
        }
        ImGui::End();

        // display pixel shader windows, handle unloading/referencing/cloning
        for (auto& ps : pixelShaders) {
            if (ps != nullptr) {
                ps->DrawGUI();
                if (!ps->is_active) { // if the window was closed
                    ps->Unload();
                    ps = nullptr;
                } else if (ps->requested_clone) { // if the clone button was pressed
                    ps->requested_clone = false;
                    pixelShaders.push_back(new PixelShader(ps));
                } else if (ps->requested_reference) { // if the copy reference button was pressed
                    ps->requested_reference = false;
                    pixelShaderReference = ps;
                }
            }
        }
        // display active file dialogs
        fileDialogManager.show();
        // display log window
        ImGui::Begin("Debug Log");
        for (std::string line : log_lines) {
            ImGui::Text("%s", line.c_str());
        }
        ImGui::Checkbox("Scroll log to bottom", &scroll_log_to_bottom);
        if (scroll_log_to_bottom) {
            ImGui::SetScrollY(ImGui::GetScrollMaxY());
        }
        ImGui::End();

        rlImGuiEnd();
        EndDrawing();
        dt = GetFrameTime();
        render_texture_update_timer += dt;
        frame_counter++;
    }

    {
        std::vector<std::filesystem::path> pins = FileDialogs::GetPinnedFolders();
        std::vector<std::string> pin_strings;
        for (auto& p : pins) {
            pin_strings.push_back(FileDialogs::NarrowString16To8(p.wstring()));
        }
        pinsCfg.set("pinned_folders", pin_strings);
        pinsCfg.save();
    }

    SaveWorkspace(workspaceCfg);
    __log_fd.close();

    rlImGuiShutdown();
    CloseWindow();
    return 0;
}
