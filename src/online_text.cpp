// ghsmashhits - the main menu's online item named as on the PlayStation 3 ("ONLINE"), not "Xbox LIVE".
//
// Online play here is through Steam between friends, not Xbox Live. The script that builds the main menu (its items
// added with add_mainmenu_item) picks the online item's text by platform:
// `if isXenon online_text = qs(2725B21F) else online_text = qs(E60E3E4A)`. Both texts are in every language's pack on
// the Xbox disc: "Xbox LIVE", and the PlayStation 3's "ONLINE" ("EN LIGNE", "EN LÍNEA"). The game turns a text's key
// into its text in the player's language with sub_823AB560 (r3 = the key; sub_822B6E50, a script structure's
// localized string, and the other callers): asked for "Xbox LIVE", it is given the key of "ONLINE". The other texts
// that name Xbox LIVE (its messages and hints, keys of their own) are left as they are.
//
// This game's main menu, though, is not built from that script line: the item asks for E54F6A42, "XBOX LIVE" in
// capitals (seen with a temporary log of the keys this function was given while the main menu was built: E54F6A42
// came between MANO A MANO's and ESTUDIO DE MUSICA's, never 2725B21F). Both keys are given "ONLINE".

#include <rex/cvar.h>
#include <rex/logging.h>
#include <rex/ppc/context.h>

#include <atomic>
#include <cstdint>

REXCVAR_DEFINE_BOOL(ghsmashhits_online_text, true, "GHSmashHits",
                    "The main menu's online item named \"ONLINE\" (the game's PlayStation 3 text, in its language), "
                    "not \"Xbox LIVE\" (false: as on the Xbox 360)");

namespace {

constexpr uint32_t kXboxLiveText = 0x2725B21F;      // "Xbox LIVE"
constexpr uint32_t kXboxLiveCapitals = 0xE54F6A42;  // "XBOX LIVE", this game's main menu item
constexpr uint32_t kOnlineText = 0xE60E3E4A;        // "ONLINE", the PlayStation 3's

std::atomic<bool> logged{false};

}  // namespace

// [[midasm_hook]] at 0x823AB56C (stw r3,116(r1)) in sub_823AB560, the text of a key: r3 = the key.
void GHSmashHitsTextKeyHook(PPCRegister& r3) {
  if ((r3.u32 != kXboxLiveText && r3.u32 != kXboxLiveCapitals) || !REXCVAR_GET(ghsmashhits_online_text)) {
    return;
  }
  r3.u64 = kOnlineText;
  if (!logged.exchange(true)) {
    REXLOG_INFO("[online_text] \"Xbox LIVE\" shown as the game's \"ONLINE\" (ghsmashhits_online_text)");
  }
}
