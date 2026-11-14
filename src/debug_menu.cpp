// ghsmashhits - the game's own debug menu, offered in the main menu and the pause menu.
//
// The game ships Neversoft's debug menu (scripts\guitar\guitar_debug_menu.qb, state uistate_debug: Unlock All, Skip
// Into Song, Settings with Toggle Bot / Show Song Time / Change Venue / Select Slomo, Toggle FPS Display, ...). The
// main menu (script create_main_menu_elements) and the pause menu (ui_create_pausemenu, and the older
// create_pause_menu) add an item that opens it when the script global enable_button_cheats is 1. This game's guitar.qb
// sets it to 0 (GH Metallica's and GH Van Halen's set it to 1, and their guitar_startup sets it to 0 on the retail
// disc: "if cd", the script function CD, here sub_823ECFB8, the byte at 0x82A94081). The same global also turns on
// development shortcuts that change the save: in the setlist, left or L3 unlocks every song (setlist_debug_unlockall)
// and right completes one (setlist_debug_completesong); in the career's gig posters LB, a guitar's orange fret,
// completes a song with its stars (ui_gig_posters_debug_complete).
//
// The main menu asks one more global, enable_debug_menus (show_debug_menus = 0 when it is 0): 1 in this game's
// guitar.qb, never changed (GH Van Halen's guitar_startup sets it to 0 on the disc); only the main menu reads it, and
// it reads as 1 there too.
//
// So the globals keep their values and only those three scripts read them as 1. A "$name" in a script is a component
// of type 13 (or 26, a global naming another) with the name's key, which sub_822BE090 replaces by the global's value
// (an integer: type 1 at +2, the value at +8). The script interpreter's statement (sub_822D06E0, r31 = the script)
// evaluates its expressions with sub_823AF960 at two places; hooks there keep, per thread, the scripts whose
// expressions are being evaluated, and the running script's name is at +216 of it (as src/framerate.cpp reads
// it). Analysis: GH Metallica's local_docs/43-debug-menu.md.

#include <rex/cvar.h>
#include <rex/logging.h>
#include <rex/ppc/context.h>
#include <rex/system/kernel_state.h>
#include <rex/system/xmemory.h>

#include <array>
#include <cstdint>
#include <cstring>
#include <mutex>
#include <set>

REXCVAR_DEFINE_BOOL(ghsmashhits_debug_menu, false, "GHSmashHits",
                    "The game's own debug menu (Neversoft's): an item in the main menu and the pause menu that opens "
                    "it, without the development shortcuts that change the save; false = as on the console");

namespace {

constexpr uint32_t kEnableButtonCheats = 0x2AF92804;     // the script global enable_button_cheats
constexpr uint32_t kEnableDebugMenus = 0x69E6470E;       // the script global enable_debug_menus (the main menu's)
constexpr uint32_t kCreateMainMenuElements = 0x01B68F5B;  // the main menu's items
constexpr uint32_t kUiCreatePauseMenu = 0x09E116BF;       // the pause menu's items
constexpr uint32_t kCreatePauseMenu = 0x3562432C;         // the older pause menu's items
constexpr uint8_t kTypeInteger = 1;
constexpr uint8_t kTypeName = 13;
constexpr uint8_t kTypeGlobalAlias = 26;
constexpr uint32_t kScriptName = 216;  // the running script's name in the interpreter's script object

bool Enabled() {
  static const bool enabled = REXCVAR_GET(ghsmashhits_debug_menu);
  return enabled;
}

uint8_t* GuestBase() {
  auto* kernel = rex::system::kernel_state();
  return kernel && kernel->memory() ? kernel->memory()->virtual_membase() : nullptr;
}

uint32_t Load32(const uint8_t* base, uint32_t address) {
  uint32_t value;
  std::memcpy(&value, base + address, 4);
  return __builtin_bswap32(value);
}

void Store32(uint8_t* base, uint32_t address, uint32_t value) {
  value = __builtin_bswap32(value);
  std::memcpy(base + address, &value, 4);
}

// The scripts whose expressions this thread is evaluating, innermost last (an expression can run another script).
struct Evaluations {
  std::array<uint32_t, 64> scripts{};
  uint32_t depth = 0;
};
thread_local Evaluations t_evaluations;

void LogOnce(uint32_t script, bool shown) {
  static std::mutex mutex;
  static std::set<uint32_t> logged;
  std::lock_guard lock(mutex);
  if (logged.size() < 32 && logged.insert(script).second) {
    if (shown) {
      REXLOG_INFO("[debug menu] script {:08X} offers the debug menu (enable_button_cheats read as 1)", script);
    } else {
      REXLOG_INFO("[debug menu] script {:08X} reads enable_button_cheats: left at 0 (a development shortcut)", script);
    }
  }
}

}  // namespace

// [[midasm_hook]] 0x822D08B0 and 0x822D0BC4 (sub_822D06E0, before each bl sub_823AF960): r31 = the script.
void GHSmashHitsScriptEvalEnterHook(PPCRegister& r31) {
  if (!Enabled()) {
    return;
  }
  Evaluations& e = t_evaluations;
  if (e.depth < e.scripts.size()) {
    e.scripts[e.depth] = r31.u32;
  }
  ++e.depth;
}

// [[midasm_hook]] 0x822D08B8 and 0x822D0BC8 (sub_822D06E0, after each bl sub_823AF960).
void GHSmashHitsScriptEvalLeaveHook() {
  if (!Enabled()) {
    return;
  }
  Evaluations& e = t_evaluations;
  if (e.depth > 0) {
    --e.depth;
  }
}

// [[midasm_hook]] 0x822BE090 (the entry of sub_822BE090: a "$name" component resolved to the global's value),
// return_on_true: r3 = the component. enable_button_cheats read by the menus' scripts, and enable_debug_menus read by
// the main menu's, become the integer 1.
bool GHSmashHitsGlobalResolveHook(PPCRegister& r3) {
  if (!Enabled()) {
    return false;
  }
  uint8_t* base = GuestBase();
  const uint32_t component = r3.u32;
  if (!base || !component) {
    return false;
  }
  const uint8_t type = base[component + 2];
  const uint32_t global = Load32(base, component + 8);
  if ((type != kTypeName && type != kTypeGlobalAlias) || (global != kEnableButtonCheats && global != kEnableDebugMenus)) {
    return false;
  }
  const Evaluations& e = t_evaluations;
  if (e.depth == 0 || e.depth > e.scripts.size() || !e.scripts[e.depth - 1]) {
    return false;
  }
  const uint32_t script = Load32(base, e.scripts[e.depth - 1] + kScriptName);
  const bool menu = global == kEnableDebugMenus
                        ? script == kCreateMainMenuElements
                        : script == kCreateMainMenuElements || script == kUiCreatePauseMenu || script == kCreatePauseMenu;
  if (global == kEnableButtonCheats) {
    LogOnce(script, menu);
  }
  if (!menu) {
    return false;
  }
  base[component + 2] = kTypeInteger;
  Store32(base, component + 8, 1);
  return true;
}
