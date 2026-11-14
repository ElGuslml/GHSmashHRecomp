// ghsmashhits - the app: the SDK's virtual hooks (rex::ReXApp).

#pragma once

#include <rex/rex_app.h>

#include <rex/cvar.h>
#include <rex/filesystem.h>
#include <rex/kernel/xam/module.h>
#include <rex/logging.h>
#include <rex/system/kernel_state.h>
#include <rex/system/online/session_service.h>
#include <rex/system/xthread.h>
#include <rex/ui/imgui_dialog.h>

#include <native_x_renderer/native_x_renderer.h>

#include "native_x_renderer_ghsmashhits.h"
#include "keyboard.h"
#include "options.h"
#include "project.h"
#include "startup.h"
#include "widescreen.h"

class GhsmashhitsApp : public rex::ReXApp {
 public:
  using rex::ReXApp::ReXApp;

  static std::unique_ptr<rex::ui::WindowedApp> Create(
      rex::ui::WindowedAppContext& ctx) {
    return std::unique_ptr<GhsmashhitsApp>(new GhsmashhitsApp(ctx, "ghsmashhits",
        PPCImageConfig));
  }

  // Other hooks the SDK offers (rex/rex_app.h): OnPostInitLogging, OnLoadXexImage, OnPostLoadXexImage,
  // OnCreateDialogs, OnShutdown.
  // Use the "xenos" GPU emulation (rexgpu-xenos.dll) unless another one was given.
  void OnPreSetup(rex::RuntimeConfig& config) override {
    if (config.gpu_plugin.empty()) {
      config.gpu_plugin = "xenos";
    }
  }

  // Game files: without --game_data_root, assets/ next to GHSmashHitsRecomp.exe or the repository's assets/. User data
  // (shader cache, downloadable content; the saves and achievements before the profiles): userdata/ next to the
  // executable; the saves, settings and achievements in the launchers' current profile (src/startup.cpp).
  void OnConfigurePaths(rex::PathConfig& paths) override {
    if (paths.game_data_root.empty()) {
      paths.game_data_root = ghsmashhits::DefaultGameRoot();
    }
    ghsmashhits::ConfigureUserData(paths.user_data_root, paths.cache_root);
    ghsmashhits::ConfigureProfile(paths.user_data_root);
  }

  // ghsmashhits.toml with the built-in defaults if it is missing, then (after the SDK loads it) the game files check
  // and GH Smash Hits Launcher's options (separate executable, launcher/): ghsmashhits_options.toml, before the window and the GPU
  // are created. The game shows no window of its own. No game files check when the launcher asks for the
  // achievements' list (--achievements_cache_only: no window, a question would wait unseen).
  bool SetupEnvironment() override {
    auto exe_dir = rex::filesystem::GetExecutableFolder();
    ghsmashhits::EnsureDefaultConfig(exe_dir / GAME_CONFIG);
    if (!rex::ReXApp::SetupEnvironment()) return false;
    REXLOG_INFO("[startup] User data: {}", user_data_root().string());
    REXLOG_INFO("[startup] {}", native_x_renderer::Describe());
    if (!rex::cvar::Query<bool>("achievements_cache_only") && !native_x_renderer::ToolModeRequested() &&
        !ghsmashhits::CheckGameFiles(game_data_root())) {
      return false;
    }
    ghsmashhits::LoadLauncherOptions(exe_dir / GAME_OPTIONS, ghsmashhits::GpuPluginPath());
    return true;
  }

  // NativeXRenderer's tool modes (D129), instead of the game: the launcher's "Compiling shaders"
  // (--native_x_renderer_prepare_pipelines) and the developers' pipeline list (--native_x_renderer_make_pipeline_list),
  // with the launcher's options already applied, so the pipelines are the ones the game will ask for.
  bool OnToolMode() override {
    if (!native_x_renderer::ToolModeRequested()) return false;
    native_x_renderer::RunToolMode(ghsmashhits::NativeXRendererProfile(), game_data_root(), cache_root());
    return true;
  }

  // Window title (the SDK sets "<app name> <SDK build stamp>"), then NativeXRenderer, before the guest starts: its
  // hooks (src/native_x_renderer_ghsmashhits.cpp) are pass-throughs until it is initialized; then ultrawide
  // (src/widescreen.cpp): the cameras' aspect ratio follows the window.
  void OnPostSetup() override {
    if (window()) {
      window()->SetTitle(GAME_TITLE);
    }
    native_x_renderer::Initialize(ghsmashhits::NativeXRendererProfile(),
                                  runtime() ? runtime()->graphics_system() : nullptr, window(),
                                  runtime() ? runtime()->cache_root() : std::filesystem::path());
    if (window()) {
      ghsmashhits::SetOutputSize(window()->GetActualPhysicalWidth(), window()->GetActualPhysicalHeight());
    }
    ghsmashhits::InitializeKeyboard();
    // Online play: a friend's invite shows at once (a card, START held to join; rex/ui/overlay/invite_prompt.h), and
    // an accepted one is told to the game only in the menus: never over the highway, nor while a loading screen is
    // up (GH Metallica's: told while it loaded its menus after leaving a song, the game joined the party as the lobby
    // menu was being built and crashed).
    rex::system::online::SessionService::SetDefaultInviteGate(
        [] { return !ghsmashhits::SongShown() && !ghsmashhits::LoadingShown(); });
  }
  void OnWindowPixelSizeChanged(uint32_t pixel_width, uint32_t pixel_height) override {
    ghsmashhits::SetOutputSize(pixel_width, pixel_height);
  }

  // Ultrawide's black bars outside a song (src/widescreen.cpp): a dialog of the SDK's ImGui layer, gone before it.
  void OnCreateDialogs(rex::ui::ImGuiDrawer* drawer) override { menu_bars_ = ghsmashhits::CreateMenuBars(drawer); }
  void OnShutdown() override { menu_bars_.reset(); }

  // On exit, log whether the game asked to launch another executable.
  void OnGuestThreadExit(rex::system::XThread* thread) override {
    (void)thread;
    auto* rt = runtime();
    if (!rt || !rt->kernel_state()) return;
    auto xam = rt->kernel_state()->GetKernelModule<rex::kernel::xam::XamModule>("xam.xex");
    if (!xam) return;
    auto& ld = xam->loader_data();
    REXLOG_INFO("[ghsmashhits] Title exited. launch_path='{}' flags=0x{:X} launch_data={} bytes",
                ld.launch_path, ld.launch_flags, ld.launch_data.size());
  }

 private:
  std::unique_ptr<rex::ui::ImGuiDialog> menu_bars_;
};
