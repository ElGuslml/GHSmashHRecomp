// ghsmashhits - NativeXRenderer for Guitar Hero: Smash Hits: the game's D3D entry points and its D3D device and
// object layouts.
//
// The XDK Direct3D library is linked into default.xex (XDK 2.0.7978) at 0x821C02E8-0x821DE5D0, the draws included
// (0x821C1E78-0x821C3158), with XGraphics-style helpers below it (0x821BE7C0-0x821C02E0). The device layout is
// Dante's Inferno's up to the register shadow (POTCAWE's + 128), the current objects POTCAWE's + 144; the resource
// objects are POTCAWE's. Every address and offset here, with its evidence, is in
// docs/native-x-renderer-ghsmashhits.md. The hooks only run the original function unless a NativeXRenderer feature
// is on (--native_x_renderer_trace, --native_x_renderer_verify, --native_x_renderer).

#include "native_x_renderer_ghsmashhits.h"

#include <native_x_renderer/hooks.h>

#include <algorithm>
#include <vector>

// Draws. DrawVerticesUP and DrawIndexedVerticesUP (sub_821C2880, 4 engine sites, no call of its own) commit their
// packets with an inlined EndVertices ([dev+48] = [dev+13444]); the engine's only BeginVertices / EndVertices pair
// (sub_821B3DB8) calls EndVertices (sub_821C2310), which is that same store. EndVertices is not hooked:
// inline_draw_end commits all of them.
NATIVE_X_RENDERER_HOOK(821C2D10, DrawIndexedVertices)
NATIVE_X_RENDERER_HOOK(821C2920, DrawVertices)
NATIVE_X_RENDERER_HOOK(821C2320, DrawVerticesUP)  // calls BeginVertices, inlines the end
NATIVE_X_RENDERER_HOOK(821C1E78, BeginVertices)
NATIVE_X_RENDERER_HOOK(821C2368, BeginIndexedVertices)  // only from DrawIndexedVerticesUP

// Render targets, clears, resolves, tiling. The engine sets targets through sub_821C6810, an alias that branches to
// SetRenderTarget. EndTiling resolves the tiles itself (the engine passes flags 0x300 and a destination).
NATIVE_X_RENDERER_HOOK(821C61E8, SetRenderTarget)
NATIVE_X_RENDERER_HOOK(821C6538, SetDepthStencilSurface)
NATIVE_X_RENDERER_HOOK(821DCCF8, Clear)  // calls ClearF
NATIVE_X_RENDERER_HOOK(821DCBD0, ClearF)
NATIVE_X_RENDERER_HOOK(821DD520, Resolve)
NATIVE_X_RENDERER_HOOK(821D12C0, BeginTiling)
NATIVE_X_RENDERER_HOOK(821D1630, EndTiling)
NATIVE_X_RENDERER_HOOK(821D1150, SetPredication)
NATIVE_X_RENDERER_HOOK(821C60B8, SetViewport)  // D3DVIEWPORT9 (integers), stored as floats at +12656
NATIVE_X_RENDERER_HOOK(821C5718, SetScissorRect)

// Shaders, vertex input, textures, constants. The engine writes constants straight into the device too (VS in 18
// functions, PS in 33) and patches sampler states in the fetch constants, without a call. SetTexture, SetStreamSource
// and the constant setters take the dirty bit or mask from their caller.
NATIVE_X_RENDERER_HOOK(821C1878, SetVertexShader)
NATIVE_X_RENDERER_HOOK(821C16B8, SetPixelShader)
NATIVE_X_RENDERER_HOOK(821C1A48, SetVertexDeclaration)
NATIVE_X_RENDERER_HOOK(821C5888, SetStreamSource)
NATIVE_X_RENDERER_HOOK(821C5A30, SetIndices)
NATIVE_X_RENDERER_HOOK(821C08F0, SetTexture)
NATIVE_X_RENDERER_HOOK(821C1430, SetVertexShaderConstantF)
NATIVE_X_RENDERER_HOOK(821C1508, SetPixelShaderConstantF)

// Resources: typed thunks over the common Lock (sub_821C7F28) and Unlock (sub_821C7058), and the cache flush over a
// range. The locks return the pointer (no data argument) and ignore the offset and size.
NATIVE_X_RENDERER_HOOK(821C82F0, VertexBufferLock)
NATIVE_X_RENDERER_HOOK(821C73A0, VertexBufferUnlock)
NATIVE_X_RENDERER_HOOK(821C8340, IndexBufferLock)
NATIVE_X_RENDERER_HOOK(821C73B0, IndexBufferUnlock)
NATIVE_X_RENDERER_HOOK(821BE8F8, TextureUnlock)
NATIVE_X_RENDERER_HOOK(821D0F88, FlushCpuRange)

// Command buffer, queries, frame. The kick submits the segment and continues in the same memory; NewSegment moves the
// write pointer to a newly allocated segment (from the kick, the reservation sub_821CA2D8 and Present). The engine
// calls Synchronize (sub_821D22F0, Swap with no front buffer), then Present, which calls VdSwap with the front buffer
// in r4; the library's worker thread sub_821D3E08 can present again.
NATIVE_X_RENDERER_HOOK(821CA0A0, KickSegment)
NATIVE_X_RENDERER_HOOK(821C9CE8, NewSegment)
NATIVE_X_RENDERER_HOOK(821C1BD0, SetShaderGPRAllocation)
NATIVE_X_RENDERER_HOOK(821CD248, QueryIssue)
NATIVE_X_RENDERER_HOOK(821CDEE8, QueryGetData)
NATIVE_X_RENDERER_HOOK(821D1E20, Swap)
NATIVE_X_RENDERER_HOOK(821D22F8, Present)

namespace ghsmashhits {

namespace {

// Frame generation's motion of the note highway's board (NativeXRenderer D124). Its grid of quads (1600 vertices, its
// vertex shader's hash below) stands still while the texture it shows, the board drawn each frame, scrolls toward the
// player. The beat lines (quads across the highway) move with that texture: matched with the last frame's, they give at
// each height of the screen where the board's surface was a frame ago (it moves faster lower down, and spreads sideways
// from the highway's middle). Each grid vertex's last position: its height as the beat lines' moved at that height, its
// place across the highway kept between their ends.
constexpr uint64_t kBoardShader = 0x2083ccdca7046959ull;
constexpr uint64_t kBeatLineShader = 0x73eff373b3e69dbdull;

void AdjustHighwayMotion(native_x_renderer::OverlayDraw* draws, size_t count) {
  struct Line {
    float y, y_last, left, right;
  };
  static std::vector<Line> lines;
  lines.clear();
  for (size_t d = 0; d < count; ++d) {
    const native_x_renderer::OverlayDraw& draw = draws[d];
    if (draw.vertex_shader != kBeatLineShader || !draw.matched || draw.vertex_count != 4) continue;
    Line line{0.0f, 0.0f, draw.positions[0], draw.positions[0]};
    for (uint32_t i = 0; i < 4; ++i) {
      line.y += draw.positions[2 * i + 1] * 0.25f;
      line.y_last += draw.previous[2 * i + 1] * 0.25f;
      line.left = std::min(line.left, draw.positions[2 * i]);
      line.right = std::max(line.right, draw.positions[2 * i]);
    }
    // A line comes down the screen; one matched with another (a new line at the top) is left out.
    if (line.y_last < line.y && line.right - line.left > 1.0f) lines.push_back(line);
  }
  if (lines.size() < 2) return;
  std::sort(lines.begin(), lines.end(), [](const Line& a, const Line& b) { return a.y < b.y; });
  // A field of the lines at height y: between the two around it, or along the nearest two past the ends.
  auto at = [&](float y, float Line::*field) {
    size_t i = 1;
    while (i + 1 < lines.size() && lines[i].y < y) ++i;
    const Line& a = lines[i - 1];
    const Line& b = lines[i];
    float t = b.y != a.y ? (y - a.y) / (b.y - a.y) : 0.0f;
    return a.*field + t * (b.*field - a.*field);
  };
  for (size_t d = 0; d < count; ++d) {
    native_x_renderer::OverlayDraw& draw = draws[d];
    if (draw.vertex_shader != kBoardShader) continue;
    for (uint32_t i = 0; i < draw.vertex_count; ++i) {
      float x = draw.positions[2 * i], y = draw.positions[2 * i + 1];
      float y_last = at(y, &Line::y_last);
      float left = at(y, &Line::left), right = at(y, &Line::right);
      float across = right - left > 1.0f ? (x - left) / (right - left) : 0.5f;
      float left_last = at(y_last, &Line::left), right_last = at(y_last, &Line::right);
      draw.previous[2 * i] = left_last + across * (right_last - left_last);
      draw.previous[2 * i + 1] = y_last;
    }
  }
}

}  // namespace

const native_x_renderer::Profile& NativeXRendererProfile() {
  static const native_x_renderer::Profile profile = [] {
    native_x_renderer::Profile p;
    p.game = "Guitar Hero: Smash Hits (4156083E)";
    p.xdk_version = "2.0.7978";
    p.output_width = 1280;  // the presentation parameters come from a video-mode table at run time
    p.output_height = 720;
    // The finished scene, before the HUD and the note highway: after the post-processing (the 640x360 4x MSAA
    // downsample of the scene, the 320x180 blurs), the engine copies the 1280x720 scene into 0x40008BE0 (Resolve's
    // return address 0x8251FE2C, called from 0x824B9088, once a frame) and draws it back with the bloom, then the
    // HUD over it. In the menus the same copy is their 3D background, before the 2D layer.
    p.scene_resolve_caller = 0x8251FE2C;
    // DLSS and FSR 3.1 without view hooks (D119): the main 3D view from its first depth-writing draw of 1280x720 with a
    // perspective matrix (the stage and the band) to that same copy, the scene's only one: the note highway and the HUD
    // are drawn after it, outside the view (no jitter, no temporal history for the notes). The camera from the vote of
    // the draws (D118).
    p.upscale_resolve_caller = 0x8251FE2C;
    // Frame generation (D123): the 2D layer after the view, the note highway and its notes included, is drawn in pixels
    // of the 1280x720 canvas (src/widescreen.cpp narrows it there): its motion comes from its vertices (D124).
    p.overlay_motion = true;
    p.overlay_motion_adjust = AdjustHighwayMotion;
    // The ultrawide hooks (src/widescreen.cpp) widen the 16:9 cameras by AspectFactor() and keep the 2D layer at
    // 16:9 in the center: NativeXRenderer may fill a wider output (native_x_renderer_fill_screen).
    p.widens_cameras = true;
    // The post-processing works on reduced copies of the scene (D127), GH Metallica's: the 640x360 4x MSAA resolve of
    // the 1280x720 scene, then 320x180 targets for the glow pass, the bright pass, and the bloom's and the depth of
    // field's separable 16-tap blurs (offsets and weights in c64-c95 for 320x180 texels). At the render scale those
    // offsets skip host texels (GH Metallica's bloom showed a lattice): they stay at 1x, as on the console.
    p.unscaled_screen_fractions = true;
    // The pipeline list (D129): every shader the game has is in DATA/FXFILES/MaterialLibrary.bin.xen, as in GH
    // Metallica (NativeXRenderer's D129, D133).
    static const char* const kShaderFiles[] = {"DATA/FXFILES/MaterialLibrary.bin.xen"};
    p.title_id = 0x4156083E;
    p.shader_files = kShaderFiles;
    p.shader_file_count = 1;
    native_x_renderer::DeviceLayout& d = p.device;
    // Six dirty masks from +0: VS constants, PS constants, register groups and the program, fetch slots (low half,
    // slot 0 in bit 31) and the 0x2280 group, the 0x2300/0x2380 groups with the clip planes and bool/loop constants,
    // the tile-dependent state.
    d.vs_constants_dirty = 0;
    d.ps_constants_dirty = 8;
    d.fetch_constants_dirty = 24;
    d.fetch_dirty_slot0_bit = 31;
    d.command_buffer_write = 48;
    d.fetch_constants = 1152;
    d.vs_float_constants = 1920;
    d.ps_float_constants = 6016;
    d.bool_loop_constants = 10112;
    d.register_groups = {{
        {10368, 0x2000, 16},
        {10444, 0x2100, 21},
        {10528, 0x2180, 5},
        {10548, 0x2200, 12},
        {10596, 0x2280, 21},
        {10680, 0x2300, 38},
        {10832, 0x2380, 8},
    }};
    d.fence_counter = 10908;
    d.write_back_block = 10896;
    d.flags = 10940;
    d.vertex_declaration = 11824;
    d.index_buffer = 12444;
    d.render_targets = 12448;
    d.depth_stencil = 12464;
    d.vertex_buffers = 12468;  // one pointer per stream
    d.textures = 12552;
    d.viewport = 12656;  // x, y, width, height, min z, max z (floats)
    d.pixel_shader = 12700;
    d.vertex_shader = 12704;
    d.predication_mask = 12716;
    d.tile_count = 12756;
    d.tile_rects = 12760;
    // DrawVerticesUP, DrawIndexedVerticesUP and EndVertices commit BeginVertices' / BeginIndexedVertices' packets
    // with [dev+48] = [dev+13444].
    d.inline_draw_end = 13444;
    native_x_renderer::ObjectLayout& o = p.objects;
    o.texture_fetch = 28;
    o.surface_info = 24;
    o.surface_color_depth_info = 28;
    o.surface_size = 36;
    o.surface_width_minus_one = {18, 14};
    o.surface_height_minus_one = {3, 15};
    o.buffer_address = 24;
    o.buffer_size = 28;
    return p;
  }();
  return profile;
}

}  // namespace ghsmashhits
