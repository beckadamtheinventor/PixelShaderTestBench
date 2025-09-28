#include <cstdarg>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <utility>
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

#define AUTO_SAVE_INTERVAL 60
PixelShader* pixelShaderReference = nullptr;
std::map<int, PixelShader*> pixelShaders;
FileDialogs::FileDialogManager fileDialogManager;
int default_rt_width = 512;
std::vector<std::string> log_lines;
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

int LoadPixelShader(std::string file, int id=-1) {
    if (!file.size()) return false;
    PixelShader* ps;
    if (id == -1) {
        ps = new PixelShader(file.c_str());
    } else {
        ps = new PixelShader(file.c_str(), id);
    }
    ps->Setup(default_rt_width, default_rt_width);
    if (ps->IsReady()) {
        pixelShaders.insert(std::make_pair(ps->num, ps));
        return ps->num;
    }
    return -1;
}

bool NewPixelShader(std::string file) {
    if (!file.size()) return false;
    PixelShader* ps = new PixelShader();
    if (ps->New(file.c_str())) {
        ps->Setup(default_rt_width, default_rt_width);
        if (ps->IsReady()) {
            pixelShaders.insert(std::make_pair(ps->num, ps));
            return true;
        }
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
                int id = LoadPixelShader(fname);
                if (id != -1) {
                    if (j.contains("clear_color") && j["clear_color"].is_array()) {
                        auto arr = j["clear_color"];
                        int clearColorI[4] = {0};
                        for (int i=0; i<arr.size(); i++) {
                            if (arr[i].is_number_integer()) {
                                clearColorI[i] = arr[i].get<int>();
                            }
                        }
                    }
                    if (j.contains("uniforms") && j["uniforms"].is_object()) {
                        PixelShader* ps = pixelShaders[id];
                        if (j.contains("width") && j["width"].is_number()) {
                            if (j.contains("height") && j["height"].is_number()) {
                                ps->SetRTSize(j["width"].get<int>(), j["height"].get<int>());
                            } else {
                                ps->SetRTSize(j["width"].get<int>(), j["width"].get<int>());
                            }
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
    for (auto p : pixelShaders) {
        auto ps = p.second;
        if (ps == nullptr) continue;
        nlohmann::json j = ps->DumpUniforms();
        json[std::string(ps->filename)] = {
            {"uniforms", j},
            {"width", ps->rt_width},
            {"height", ps->rt_height},
            {"id", ps->num},
            {"clear_color", nlohmann::json::array({ps->clearColor.r, ps->clearColor.g, ps->clearColor.b, ps->clearColor.a})},
        };
    }
    cfg["shaders"] = json;
    cfg.save();
}

class WorkspaceSaveAsCB : public FileDialogs::Callback {
    JsonConfig workspaceCfg;
    public:
    WorkspaceSaveAsCB(JsonConfig workspaceCfg) : workspaceCfg(workspaceCfg) {}
    bool operator()(std::string fname) {
        if (fname.size() < 1) return false;
        return workspaceCfg.save(fname);
    }
};

class LoadPixelShaderCB : public FileDialogs::Callback {
    public:
    bool operator()(std::string fname) {
        return LoadPixelShader(fname);
    }
};

class NewPixelShaderCB : public FileDialogs::Callback {
    public:
    bool operator()(std::string fname) {
        return NewPixelShader(fname);
    }
};



int main(int argc, char** argv) {
    bool debug = false;
    char pixel_shader_file[IMAGE_NAME_BUFFER_LENGTH] = "shaders/noise.fs";
    for (int i=1; i<argc; i++) {
        if (!strcmp(argv[i], "--debug")) {
            debug = true;
        } else if (!strcmp(argv[i], "-w") || !strcmp(argv[i], "--width")) {
            if (i+1 < argc)
                default_rt_width = atoi(argv[i+1]);
        } else if (!strcmp(argv[i], "-f") || !strcmp(argv[i], "--file")) {
            if (i+1 < argc)
                strcpy(pixel_shader_file, argv[i+1]);
        }
    }

    __log_fd.open("debug.log");
    SetTraceLogCallback(__TraceLogCallback);
    if (debug) {
        SetTraceLogLevel(LOG_TRACE);
    }

    SetConfigFlags(FLAG_VSYNC_HINT | FLAG_WINDOW_RESIZABLE);
    InitWindow(1000, 600, "PixelShaderTestBench");
    SetTargetFPS(60);
    SetExitKey(-1);
    int render_texture_update_rate = 30;
    float render_texture_update_timer = 0;
    bool autosave_workspace = true;
    float auto_save_timer = 0;
    float auto_save_interval = AUTO_SAVE_INTERVAL;

    rlImGuiSetup(true);
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    io.ConfigFlags |= ImGuiConfigFlags_NoMouseCursorChange;

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

    JsonConfig preferencesCfg("preferences.json");
    if (preferencesCfg.contains("autosave")) {
        autosave_workspace = preferencesCfg.get<bool>("autosave");
    }
    if (preferencesCfg.contains("autosave_interval")) {
        auto_save_interval = preferencesCfg.get<float>("autosave_interval");
    }

    JsonConfig workspaceCfg = LoadWorkspace();

    bool scroll_log_to_bottom = true;
    float dt = 0.0f;
    int frame_counter = 0;
    int target_fps = 60;
    while (!WindowShouldClose()) {
        BeginDrawing();
        if (render_texture_update_timer >= 1.0 / render_texture_update_rate) {
            render_texture_update_timer -= 1.0 / render_texture_update_rate;
            for (auto p : pixelShaders) {
                auto ps = p.second;
                if (ps != nullptr && ps->IsReady()) {
                    ps->Update(dt * target_fps / (float)render_texture_update_rate);
                }
            }
        }
        ClearBackground(BLACK);
        DrawFPS(1, 1);
        rlImGuiBegin();

        ImGui::Begin("Options");
        ImGui::InputTextWithHint("Pixel Shader File", "path to fragment shader", pixel_shader_file, sizeof(pixel_shader_file));
        if (ImGui::Button("Browse")) {
            fileDialogManager.openIfNotAlready("BrowseForShaderInput", "Load Shader", LoadPixelShaderCB());
        }
        ImGui::SameLine();
        if (ImGui::Button("Load")) {
            frame_counter = -1;
            if (!LoadPixelShader(pixel_shader_file)) {
                TraceLog(LOG_WARNING, "Failed to load Pixel shader %s!", pixel_shader_file);
            }
        }
        ImGui::SameLine();
        if (ImGui::Button("New")) {
            fileDialogManager.openIfNotAlready("BrowseForNewShader", "New Shader", NewPixelShaderCB(), true);
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
            fileDialogManager.openIfNotAlready("SaveWorkspaceAs", "Save Workspace As", WorkspaceSaveAsCB(workspaceCfg), true);
        }
        ImGui::SameLine();
        if (ImGui::Checkbox("Autosave", &autosave_workspace)) {}
        if (ImGui::InputFloat("Autosave Interval", &auto_save_interval)) {}
        ImGui::End();

        // display pixel shader windows, handle unloading/referencing/cloning
        for (auto& p : pixelShaders) {
            auto& ps = p.second;
            if (ps != nullptr) {
                ps->DrawGUI(dt);
                if (!ps->is_active) { // if the window was closed
                    ps->Unload();
                    ps = nullptr;
                } else if (ps->requested_clone) { // if the clone button was pressed
                    ps->requested_clone = false;
                    auto ps2 = new PixelShader(ps);
                    pixelShaders.insert(std::make_pair(ps2->num, ps2));
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

        if (autosave_workspace) {
            auto_save_timer += dt;
            if (auto_save_timer >= auto_save_interval) {
                auto_save_timer -= auto_save_interval;
                SaveWorkspace(workspaceCfg);
            }
        }
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

    {
        preferencesCfg.set("autosave", autosave_workspace);
        preferencesCfg.set("autosave_interval", auto_save_interval);
        preferencesCfg.save();
    }

    SaveWorkspace(workspaceCfg);
    __log_fd.close();

    rlImGuiShutdown();
    CloseWindow();
    return 0;
}
