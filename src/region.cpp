// ghsmashhits - the game's territory: the name on its title screen, and its legal screens.
//
// One disc for every region. The game takes its territory from XGetGameRegion's high byte (sub_82382C20: 0 America,
// 1 Asia, 2 Europe, anything else the rest of the world), and the script 0xC618A402 turns Europe and the rest of the
// world into the European edition: "Guitar Hero: Greatest Hits" for "Guitar Hero: Smash Hits" on the title screen and
// three others (the textures 0x6436BF12 and 0xC55D1AB2 for 0xA3632413 and 0x289BD15C), and the European legal screens
// ('boot_legal_EU'). Its other uses change nothing here: in English outside America the script 0xC8864B71 asks for
// songlist_b.pak instead of songlist.pak, and neither is on the disc (its caller goes on without them); 0xB7D3B9E1
// reads the territory on the PS3 only; 0xCAA10E1B compares with a limit that is 20 in either case.
//
// The SDK answers 0xFFFF for every game, the rest of the world, so this game called itself Greatest Hits. It now
// answers as a console in America does, Smash Hits being the name of the game and of this project;
// ghsmashhits_greatest_hits = true answers as one in Europe.

#include <rex/cvar.h>
#include <rex/ppc/context.h>

REXCVAR_DEFINE_BOOL(ghsmashhits_greatest_hits, false, "GHSmashHits",
                    "The game as sold in Europe and Australia: Guitar Hero: Greatest Hits on its title screen, with "
                    "its European legal screens; false: Guitar Hero: Smash Hits, as sold in America");

namespace {

constexpr uint32_t kRegionAmerica = 0x00FF;  // XC_GAME_REGION_NA_ALL
constexpr uint32_t kRegionEurope = 0x02FF;   // XC_GAME_REGION_EUROPE_ALL

}  // namespace

// [[midasm_hook]] 0x82382CEC (sub_82382C20, the game's start: rlwinm r11,r3,24,24,31, right after the call to
// XGetGameRegion): r3 = its answer.
void GHSmashHitsGameRegionHook(PPCRegister& r3) {
  r3.u64 = REXCVAR_GET(ghsmashhits_greatest_hits) ? kRegionEurope : kRegionAmerica;
}
