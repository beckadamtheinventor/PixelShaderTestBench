#include "FileDialogs.hpp"
#include "JsonConfig.hpp"
#include "raylib.h"
#include "imgui.h"
#include "rlImGui.h"
#include "rlgl.h"
#include "external/glad.h"
#include <cctype>
#include <cstring>
#include <ctime>
#include <filesystem>
#include <functional>
#include <list>
#include <utility>
#include <vector>

#include "PixelShader.hpp"

PixelShader* pixelShaderReference = nullptr;
std::list<PixelShader*> pixelShaders;
std::list<std::pair<FileDialogs::FileDialog*, std::function<bool(std::string)>>> fileDialogs;
int default_rt_width = 512;

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

int main(int argc, char** argv) {
    char pixel_shader_file[IMAGE_NAME_BUFFER_LENGTH] = "shaders/noise.fs";
    if (argc > 1) {
        strcpy(pixel_shader_file, argv[1]);
    }
    if (argc > 2) {
        default_rt_width = atoi(argv[2]);
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

    PixelShader* ps = new PixelShader(pixel_shader_file);
    ps->Setup(default_rt_width);
    pixelShaders.push_back(ps);

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
            FileDialogs::FileDialog* dialog = new FileDialogs::FileDialog("Load Shader");
            fileDialogs.push_back(std::make_pair(dialog, [] (std::string s) { return LoadPixelShader(s); }));
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
        for (auto& d : fileDialogs) {
            if (d.first != nullptr) {
                std::filesystem::path selected;
                if (d.first->Show(selected)) {
                    d.first = nullptr;
                    d.second(FileDialogs::NarrowString16To8(selected.wstring()));
                }
            }
        }

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

    rlImGuiShutdown();
    CloseWindow();
    return 0;
}
