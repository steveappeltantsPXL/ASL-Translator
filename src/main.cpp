#include <SDL3/SDL.h>
#include <imgui.h>
#include <imgui_impl_opengl3.h>
#include <imgui_impl_sdl3.h>
#include "avatar/AvatarRenderer.h"

#ifdef _WIN32
#include <dwmapi.h>
#include <windows.h>
#endif
#include <GL/gl.h>

#include <spdlog/spdlog.h>

// Responsive breakpoint: switch to compact mode below this width
constexpr float COMPACT_WIDTH = 640.0f;

// Accent color (teal) used for the avatar placeholder background
constexpr ImU32 ACCENT_TEAL_TR = IM_COL32(26, 188, 156, 200);

void apply_custom_style() {
    ImGuiStyle& style = ImGui::GetStyle();
    style.Colors[ImGuiCol_Button] = ImVec4(0.10f, 0.10f, 0.10f, 1.00f);
    style.Colors[ImGuiCol_ButtonHovered] = ImVec4(0.12f, 0.50f, 0.46f, 1.00f);
    style.Colors[ImGuiCol_ButtonActive] = ImVec4(0.10f, 0.73f, 0.61f, 1.00f);
    style.Colors[ImGuiCol_FrameBg] = ImVec4(0.10f, 0.10f, 0.10f, 1.00f);
    style.Colors[ImGuiCol_FrameBgHovered] = ImVec4(0.12f, 0.50f, 0.46f, 1.00f);
    style.Colors[ImGuiCol_FrameBgActive] = ImVec4(0.10f, 0.73f, 0.61f, 1.00f);
}

void render_avatar_placeholder() {
    const ImVec2 avail = ImGui::GetContentRegionAvail();
    const ImVec2 p = ImGui::GetCursorScreenPos();
    ImDrawList* dl = ImGui::GetWindowDrawList();

    // Teal background rect
    dl->AddRectFilled(p, ImVec2(p.x + avail.x, p.y + avail.y), ACCENT_TEAL_TR, 8.f);

    const char* label = "Avatar available soon";
    ImVec2 text_size = ImGui::CalcTextSize(label);
    dl->AddText(ImVec2(p.x + avail.x / 2 - text_size.x / 2, p.y + avail.y / 2 - text_size.y / 2),
                IM_COL32(160, 160, 160, 255),
                label);
}

int main(int, char**) {
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        spdlog::error("SDL_Init failed: {}", SDL_GetError());
        return 1;
    }

    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);

    SDL_Window* window =
        SDL_CreateWindow("Visear Translator", 1280, 720, SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE);
    if (!window) {
        spdlog::error("SDL_CreateWindow failed: {}", SDL_GetError());
        SDL_Quit();
        return 1;
    }

    // Read the Windows accent color once (used for toolbar button tint).
    // Falls back to #0078D4 if DWM query fails or on non-Windows.
#ifdef _WIN32
    ImVec4 accent_color = ImVec4(0.f, 0.471f, 0.831f, 1.f);
    {
        DWORD dwm_color = 0;
        BOOL opaque = FALSE;
        if (SUCCEEDED(DwmGetColorizationColor(&dwm_color, &opaque)))
            accent_color = ImVec4(((dwm_color >> 16) & 0xFF) / 255.f,
                                  ((dwm_color >> 8) & 0xFF) / 255.f,
                                  (dwm_color & 0xFF) / 255.f,
                                  1.f);
    }
#else
    const ImVec4 accent_color(0.f, 0.471f, 0.831f, 1.f);
#endif

    const SDL_GLContext gl = SDL_GL_CreateContext(window);
    SDL_GL_MakeCurrent(window, gl);
    SDL_GL_SetSwapInterval(1);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;

    // Primary font: Calibri — designed for screen readability with ClearType.
    // Symbol glyphs (► ■ ⚙ ●) are merged from Segoe UI since Calibri lacks them.
    {
        ImFontConfig cfg;
        cfg.OversampleH = 3;
        cfg.OversampleV = 2;
        cfg.PixelSnapH = true;
        static const ImWchar text_ranges[] = {
            0x0020,
            0x00FF,  // Basic Latin + Latin Supplement
            0x2000,
            0x206F,  // General Punctuation
            0,
        };
        if (!io.Fonts->AddFontFromFileTTF("C:/Windows/Fonts/calibri.ttf", 16.0f, &cfg, text_ranges))
            io.Fonts->AddFontDefault();

        // Merge Segoe UI for symbol-only ranges that Calibri does not cover.
        ImFontConfig merge;
        merge.MergeMode = true;
        merge.OversampleH = 3;
        merge.OversampleV = 2;
        static const ImWchar symbol_ranges[] = {
            0x2190,
            0x23FF,  // Arrows + Misc Technical (⏹)
            0x25A0,
            0x26FF,  // Geometric Shapes (▶ ●) + Misc Symbols (⚙)
            0,
        };
        io.Fonts->AddFontFromFileTTF("C:/Windows/Fonts/segoeui.ttf", 16.0f, &merge, symbol_ranges);
    }

    ImGui::StyleColorsDark();
    apply_custom_style();

    ImGui_ImplSDL3_InitForOpenGL(window, gl);
    ImGui_ImplOpenGL3_Init("#version 330 core");

    avatar::AvatarRenderer avatarRenderer;
    const bool avatarOk = avatarRenderer.init("resources/models/avatar/avatar.glb");
    if (!avatarOk) {
        spdlog::info("Avatar init failed — placeholder will be shown");
    }

    int selectedAnim = 0;  // current animation index for the combo box

    bool running = true;
    while (running) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            ImGui_ImplSDL3_ProcessEvent(&event);
            if (event.type == SDL_EVENT_QUIT)
                running = false;
        }

        // Render avatar to its FBO before starting the ImGui frame.
        if (avatarOk) {
            const float aw = io.DisplaySize.x < COMPACT_WIDTH
                ? io.DisplaySize.x - 16.f
                : io.DisplaySize.x * 0.40f - 16.f;
            const float ah = io.DisplaySize.x < COMPACT_WIDTH
                ? io.DisplaySize.y - 60.f - 35.f
                : io.DisplaySize.y - 40.f - 80.f - 35.f;
            avatarRenderer.render(aw, ah, io.DeltaTime);
        }

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplSDL3_NewFrame();
        ImGui::NewFrame();

        if (io.DisplaySize.x < COMPACT_WIDTH) {
            // === MOBILE: Avatar (top) + Captions strip (bottom 60 px) ===
            constexpr float captions_height = 60.f;
            const float avatar_height = io.DisplaySize.y - captions_height;

            constexpr ImGuiWindowFlags panel_flags =
                ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
                ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoScrollbar;

            ImGui::SetNextWindowPos(ImVec2(0, 0));
            ImGui::SetNextWindowSize(ImVec2(io.DisplaySize.x, avatar_height));
            ImGui::Begin("##ASL_Avatar_Mobile", nullptr, panel_flags);
            if (avatarOk) {
                ImGui::Image(avatarRenderer.getTexture(),
                             ImGui::GetContentRegionAvail(),
                             ImVec2(0, 1), ImVec2(1, 0));
            } else {
                render_avatar_placeholder();
            }
            ImGui::End();

            ImGui::SetNextWindowPos(ImVec2(0, avatar_height));
            ImGui::SetNextWindowSize(ImVec2(io.DisplaySize.x, captions_height));
            ImGui::Begin("##Captions_Mobile", nullptr, panel_flags);
            ImGui::Text("Recognized text will appear here...");
            ImGui::SameLine(ImGui::GetContentRegionAvail().x - 52.f);
            ImGui::TextColored(ImVec4(0.1f, 0.73f, 0.61f, 1.0f), "● %.0fps", io.Framerate);
            ImGui::End();

        } else {
            // === DESKTOP: Toolbar + 3 coupled panels + Captions strip ===
            constexpr ImGuiWindowFlags toolbar_flags =
                ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
                ImGuiWindowFlags_NoBringToFrontOnFocus;
            ImGui::SetNextWindowPos(ImVec2(0, 0));
            ImGui::SetNextWindowSize(ImVec2(io.DisplaySize.x, 40.f));
            ImGui::Begin("##Toolbar", nullptr, toolbar_flags);
            ImGui::PushStyleColor(ImGuiCol_Button, accent_color);
            if (ImGui::Button("► Start Capture")) { /* ... */
            }
            ImGui::SameLine();
            if (ImGui::Button("■ Stop")) { /* ... */
            }
            ImGui::PopStyleColor();
            ImGui::SameLine();
            ImGui::Spacing();
            ImGui::SameLine();
            ImGui::TextColored(ImVec4(0.1f, 0.73f, 0.61f, 1.0f), "● Running");
            ImGui::SameLine();
            ImGui::Text("  %.0f fps", io.Framerate);
            ImGui::End();

            const float width = io.DisplaySize.x;
            constexpr float toolbar_height = 40.f;
            constexpr float captions_height = 80.f;
            const float top_panels_height = io.DisplaySize.y - toolbar_height - captions_height;
            const float cam_width = width * 0.35f;
            const float avatar_width = width * 0.40f;
            const float controls_width = width * 0.25f;

            constexpr ImGuiWindowFlags locked =
                ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse;

            // Camera Feed
            ImGui::SetNextWindowPos(ImVec2(0, toolbar_height), ImGuiCond_Always);
            ImGui::SetNextWindowSize(ImVec2(cam_width, top_panels_height), ImGuiCond_Always);
            ImGui::Begin("Camera Feed", nullptr, locked);
            const ImVec2 cam_avail = ImGui::GetContentRegionAvail();
            const ImVec2 cam_p = ImGui::GetCursorScreenPos();
            ImDrawList* dl = ImGui::GetWindowDrawList();
            dl->AddRectFilled(cam_p,
                              ImVec2(cam_p.x + cam_avail.x, cam_p.y + cam_avail.y),
                              IM_COL32(30, 30, 30, 255),
                              4.f);
            dl->AddRect(cam_p,
                        ImVec2(cam_p.x + cam_avail.x, cam_p.y + cam_avail.y),
                        IM_COL32(80, 80, 80, 255),
                        4.f);
            dl->AddText(ImVec2(cam_p.x + cam_avail.x / 2 - 20, cam_p.y + cam_avail.y / 2 - 7),
                        IM_COL32(160, 160, 160, 255),
                        "No Camera");
            ImGui::Dummy(cam_avail);
            ImGui::End();

            // ASL Avatar
            ImGui::SetNextWindowPos(ImVec2(cam_width, toolbar_height), ImGuiCond_Always);
            ImGui::SetNextWindowSize(ImVec2(avatar_width, top_panels_height), ImGuiCond_Always);
            ImGui::Begin("ASL Avatar", nullptr, locked);
            if (avatarOk) {
                ImGui::Image(avatarRenderer.getTexture(),
                             ImGui::GetContentRegionAvail(),
                             ImVec2(0, 1), ImVec2(1, 0));
            } else {
                render_avatar_placeholder();
            }
            ImGui::End();

            // Controls — full height so it reaches the bottom edge
            ImGui::SetNextWindowPos(ImVec2(cam_width + avatar_width, toolbar_height),
                                    ImGuiCond_Always);
            ImGui::SetNextWindowSize(ImVec2(controls_width, io.DisplaySize.y - toolbar_height),
                                     ImGuiCond_Always);
            ImGui::Begin("Controls", nullptr, locked);
            ImGui::Text("Language: EN");
            ImGui::Text("Mode: ASL → TX");
            ImGui::Spacing();
            ImGui::Text("Confidence:");
            ImGui::ProgressBar(0.8f, ImVec2(-1, 0), "80%");
            // Animation selector (shown only when model has multiple animations)
            if (avatarRenderer.animationCount() > 1) {
                ImGui::Spacing();
                ImGui::Separator();
                ImGui::Text("Avatar Animation:");
                if (ImGui::BeginCombo("##AnimCombo",
                        avatarRenderer.animationName(selectedAnim).c_str())) {
                    for (int i = 0; i < avatarRenderer.animationCount(); ++i) {
                        const bool selected = (i == selectedAnim);
                        if (ImGui::Selectable(
                                avatarRenderer.animationName(i).c_str(), selected)) {
                            selectedAnim = i;
                            avatarRenderer.selectAnimation(i);
                        }
                        if (selected) ImGui::SetItemDefaultFocus();
                    }
                    ImGui::EndCombo();
                }
            }
            ImGui::End();

            // Captions
            ImGui::SetNextWindowPos(ImVec2(0, toolbar_height + top_panels_height),
                                    ImGuiCond_Always);
            ImGui::SetNextWindowSize(ImVec2(cam_width + avatar_width, captions_height),
                                     ImGuiCond_Always);
            ImGui::Begin("Captions", nullptr, locked);
            ImGui::TextWrapped("Recognized text will appear here...");
            ImGui::End();
        }

        ImGui::Render();
        glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        SDL_GL_SwapWindow(window);
    }

    avatarRenderer.shutdown();  // must happen while GL context is still current
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplSDL3_Shutdown();
    ImGui::DestroyContext();
    SDL_GL_DestroyContext(gl);
    SDL_DestroyWindow(window);
    SDL_Quit();

    return 0;
}