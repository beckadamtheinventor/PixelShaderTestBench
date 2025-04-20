#include "raylib.h"
#include "imgui.h"
#include "rlImGui.h"
#include "rlgl.h"
#include "external/glad.h"
#include <cctype>
#include <cstring>
#include <ctime>
#include <list>
#include <vector>

#include "PixelShader.hpp"


int main(int argc, char** argv) {
    char pixel_shader_file[IMAGE_NAME_BUFFER_LENGTH] = "shaders/noise.fs";
    if (argc > 1) {
        strcpy(pixel_shader_file, argv[1]);
    }
    int default_rt_width = 1024;
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

    std::list<PixelShader*> pixelShaders;
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
        if (ImGui::Button("Load Shader")) {
            //if (std::filesystem::exists(image_input)) {
                PixelShader* ps = new PixelShader(pixel_shader_file);
                ps->Setup(default_rt_width);
                frame_counter = -1;
            //} else {
                if (ps->IsReady()) {
                    pixelShaders.push_back(ps);
                } else {
                    TraceLog(LOG_WARNING, "Failed to load Pixel shader %s!", pixel_shader_file);
                }
            //}
        }
        ImGui::SliderInt("Target updates per second", &render_texture_update_rate, 1, 60);
        if (ImGui::SliderInt("Target FPS", &target_fps, 10, 500)) {
            SetTargetFPS(target_fps);
        }
        if (ImGui::Button("Unlimited FPS")) {
            SetTargetFPS((target_fps = -1));
        }
        ImGui::End();

        for (auto& ps : pixelShaders) {
            if (ps != nullptr) {
                ps->DrawGUI();
                if (!ps->is_active) {
                    ps->Unload();
                    ps = nullptr;
                } else if (ps->requested_clone) {
                    ps->requested_clone = false;
                    pixelShaders.push_back(new PixelShader(ps));
                }
            }
        }

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
