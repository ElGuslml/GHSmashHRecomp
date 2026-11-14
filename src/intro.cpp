// ghsmashhits - the boot's videos skipped: the game starts at its title screen ("press a button to rock").
//
// The boot is a chain of UI states (scripts\guitar\menu\states\layouts\ui_boot.qb): the legal notice
// (UIstate_boot_legal: ui_boot_legal_wait waits 6 seconds on the disc, after the loading behind the same picture),
// then four videos, each state's script running ui_boot_movie_wait {movie, state}: PlayMovieAndWait, then the next
// state: 'atvi' (Activision), 'ro_logo' (RedOctane), 'bx_logo' (Beenox, this game's developer), 'ns_logo' (Neversoft),
// then uistate_boot_iis, the title screen (read from the decoded scripts; the intro's state, 'band_intro', is
// registered but no state leads to it). PlayMovieAndWait (guitar_movies.qb) starts the video with the script function
// PlayMovie (sub_823CE830) and waits while IsMoviePlaying, as when a button skips a video. So PlayMovie returns false
// for those four, the first time each (a later request plays): the chain goes on at once. The legal notice's 6 seconds
// are not waited (the notice is shown while the game loads). The development path (skip_boot_menu) is not used: it
// also assigns the primary controller and leaves the loading screen up. GH Metallica's and GH Van Halen's fix.

#include <rex/cvar.h>
#include <rex/logging.h>
#include <rex/ppc/context.h>
#include <rex/system/kernel_state.h>
#include <rex/system/xmemory.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <cctype>
#include <cstdint>
#include <cstring>
#include <string>
#include <string_view>

REXCVAR_DEFINE_BOOL(ghsmashhits_skip_intro, false, "GHSmashHits",
                    "Skip the videos at the game's start (Activision, RedOctane, Beenox, Neversoft) and the legal "
                    "notice's wait: the game starts at its title screen; false = as on the console");

namespace {

constexpr uint32_t kLegalWaitScript = 0x87F1EF1B;  // ui_boot_legal_wait
constexpr std::array<std::string_view, 4> kBootMovies = {"atvi", "ro_logo", "bx_logo", "ns_logo"};
std::array<std::atomic<bool>, kBootMovies.size()> g_skipped{};

bool Enabled() {
  static const bool enabled = REXCVAR_GET(ghsmashhits_skip_intro);
  return enabled;
}

const uint8_t* GuestBase() {
  auto* kernel = rex::system::kernel_state();
  return kernel && kernel->memory() ? kernel->memory()->virtual_membase() : nullptr;
}

uint32_t Load32(const uint8_t* base, uint32_t address) {
  uint32_t value;
  std::memcpy(&value, base + address, 4);
  return __builtin_bswap32(value);
}

// A string of guest memory, lowercase, at most 64 characters.
std::string GuestString(const uint8_t* base, uint32_t address) {
  std::string text;
  for (uint32_t i = 0; address && i < 64; ++i) {
    char c = char(base[address + i]);
    if (!c) {
      break;
    }
    text += char(std::tolower(static_cast<unsigned char>(c)));
  }
  return text;
}

}  // namespace

// [[midasm_hook]] 0x82393BEC (the script function Wait, sub_823939B8, its seconds branch): r4 = the wait in
// milliseconds, r30 = the script (its name at +216). The boot's legal notice is not waited for while the intro is
// skipped. GH Metallica's and GH Van Halen's hook on the same instruction also shorten their crowd's claps by the
// calibration, which this game's scripts already do (docs/fixes-from-gh-metallica.md): here it only serves the intro.
void GHSmashHitsScriptWaitSecondsHook(PPCRegister& r4, PPCRegister& r30) {
  if (!Enabled() || !r30.u32) {
    return;
  }
  const uint8_t* base = GuestBase();
  if (!base || Load32(base, r30.u32 + 216) != kLegalWaitScript) {
    return;
  }
  static std::atomic<bool> logged{false};
  if (!logged.exchange(true)) {
    REXLOG_INFO("[intro] the legal notice's wait ({} ms) skipped", r4.u32);
  }
  r4.u64 = 0;
}

// [[midasm_hook]] 0x823CE8B0 (PlayMovie, sub_823CE830, after it read its string parameter `movie`, QBKey 0xE2A10D90,
// into 84(r1)): r3 = whether it was found. A boot video the first time it is asked for: not found, so PlayMovie
// returns false and plays nothing.
void GHSmashHitsPlayMovieHook(PPCRegister& r1, PPCRegister& r3) {
  if (!Enabled() || !(r3.u32 & 0xFF)) {
    return;
  }
  const uint8_t* base = GuestBase();
  if (!base) {
    return;
  }
  const std::string movie = GuestString(base, Load32(base, r1.u32 + 84));
  for (size_t i = 0; i < kBootMovies.size(); ++i) {
    if (movie == kBootMovies[i] && !g_skipped[i].exchange(true)) {
      REXLOG_INFO("[intro] boot video '{}' skipped", movie);
      r3.u64 = 0;
      return;
    }
  }
}
