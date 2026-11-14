# Online play (experimental)

The game's online modes between friends (ported from GH Metallica on 2026-11-13, the same engine). There are no Xbox
Live or game servers: the players' games connect to each other, each with the same version of this port. The game's
own parts here: the invite gate (`src/ghsmashhits_app.h`: never during a song nor a loading screen), the matchmaking's
wait (`src/matchmaking.cpp`, hook 0x823F8A7C in the matchmaker's constructor sub_823F89D8) and the main menu's item
(`src/online_text.cpp`, hook 0x823AB56C in sub_823AB560); the rest is the SDK's (changes 89-102).

Checked here (2026-11-13): two instances on one PC, by address (not Steam): the second was invited into the first's
party at once, the matchmaking (3 s), the setlist and a Pro Face-Off song with both players, each named on the other's
highway; logs without errors. Not yet between two PCs nor through Steam in this game (both tried in GH Metallica).

The main menu's item here asks for another text than GH Metallica's: E54F6A42, "XBOX LIVE" in capitals (found with a
temporary log of the keys the text function was given), so `src/online_text.cpp` gives "ONLINE" for both keys.

## Setting it up

The launcher's **Settings > Online** (launcher 3.21): **Online play** on, on every PC; **Through Steam** turns on with
it. Steam must be running and signed in on every PC, the players must be Steam friends, and Valve's `steam_api64.dll`
must be next to the game (the release has it). Steam shows the game as Spacewar (app 480). Steam carries the game
between the players (its relays), so no port is opened.

Playing by address (one PC hosts, the others type its address and port) is in the game, but the launcher does not
offer it for now (`game::kOnlineByAddress` false, Gustavo's choice while online play is tried between homes); it
writes `online_steam = online_play`, an empty `online_host` and `online_upnp = false`. By hand, in the options file:
`online_host = "a.b.c.d:port"` on the friends' PCs, the host's router forwarding UDP 3074 (`online_port`), which the
host's game asks of it by UPnP (`online_upnp`, lines `online: UPnP:` in its log). An internet provider that shares one
address among its customers (carrier-grade NAT: the router's own internet address is a private one) makes hosting by
address impossible; Steam or a virtual network (ZeroTier, Tailscale, Radmin VPN) works there.

## In the game

The main menu's online item says "ONLINE" ("EN LÍNEA", "EN LIGNE"): the game's own text for its PlayStation 3
version, in every language's pack on the Xbox disc, in place of "Xbox LIVE" (`ghsmashhits_online_text`, true; false
brings "Xbox LIVE" back; the game's other texts that name Xbox LIVE are unchanged). The online modes are in the
game's own menus, as with Xbox Live on the console: the party, the friends list, invites,
Pro Face-Off, Battle, Band Quickplay and the rest. The friends list shows the players connected and the Steam friends
with the game open. An invite shows as a card at the top of the screen for 15 seconds: hold START on a controller (or
Enter) for 2 seconds to join. A friend can also be invited from Steam's friends list (Invite to game), or joined
there (Join game): the game takes the player to the friend's party. That happens once per join: when the host later leaves
its party or goes online again (the game makes a new party), the friend stays in its own lobby until invited, as on
the console (SDK change 102; before it, the friend followed the host into every new party).

Steam does not show a friend's game when the friend's profile keeps its game details private (the friend looks
"online", not "in game"). The games then tell each other directly that they are running, so one of the two seeing the
other is enough for both friends lists (SDK change 101; before it, only one of the two listed the other, and the other
had to invite from Steam).

The game's matchmaking also waits for strangers to fill a match's empty places, as on Xbox Live: up to 45 seconds
after the party starts a Band Quickplay or a duel. There are no strangers here, so the match waits 3 seconds
(`ghsmashhits_online_host_wait`; 0 = the game's own times, for two separate groups of friends looking for each other,
Band vs Band). A slow connection plays fine: tested in GH Metallica with 150-200 ms of delay and 5 % of the packets
lost each way.

## The options

The launcher writes them to `ghsmashhits_options.toml`; they can also go in `ghsmashhits.toml` or on the command line:
`online_play`, `online_steam`, `online_host` (`"a.b.c.d:port"`; empty: this PC hosts), `online_port`, `online_upnp`
(the host's router asked to forward its port), and `online_xuid` (two instances on one PC with copies of one profile).
`ghsmashhits_online_host_wait` (seconds, 3) is the matchmaking's wait above.
`ghsmashhits_online_text` (true): the main menu's item named "ONLINE" instead of "Xbox LIVE".

## Turning it off

**Online play** off (the default): the game is offline, as before.
