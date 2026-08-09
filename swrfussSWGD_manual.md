# swrfussSWGD Manual
*A working reference for Star Wars MUD (SWR/FUSS base + SWL v1.6.1 + SWGD content)*

This manual is written directly from the current source (`mud.h`, `build.c`,
`shops.c`, `mud_prog.c`, `commands.dat`, `space.c`) rather than from generic
ROM/SMAUG assumptions or the legacy `doc/` files, which predate a lot of what
this codebase has grown into. Every field name, value slot, and syntax string
below was pulled from the actual handler code, not guessed at - where I built a
worked example, I traced it against the real parsing logic so the example would
actually work if you typed it in.

---

## 1. Structure & Where Things Happen

### 1.1 Top-level layout

```
swrfussSWL/
  src/          C source - the actual game engine (41 .c files)
  area/         .are files - rooms, mobiles, objects, resets, help.are
  system/       commands.dat, version.txt, skills.dat, and other runtime tables
  doc/          legacy reference docs shipped with the original codebase
  player/       player files (pfiles), one per character
  clans/        clan data files
  gods/         immortal-specific data
  planets/      planet registration data used by the space system
  boards/       bulletin board data
  corpses/      saved corpse objects
  hotboot/      hotboot state (in-memory game state dumped across a recompile)
  log/          runtime logs
```

The live server runs from `/mnt/c/swr/swrfussswl/` on WSL; dev copies mirror the
same layout elsewhere.

### 1.2 Key source files (src/)

| File | What lives here |
|---|---|
| `db.c` | Area file loading/saving (`fread_fuss_*` functions), boot sequence, mob prog attachment, per-item-type post-load fixups (weapon condition defaults, food timers, etc.) |
| `comm.c` | Main game loop, input/output, descriptor handling |
| `interp.c` | Command interpreter - matches typed input against `commands.dat` |
| `build.c` | **All OLC (building) commands** - redit, mset, oset, mcreate, ocreate, aassign, area management, and every named field-shortcut table (`a_types`, `weapon_table`, `spice_table`, `crystal_table`, etc.) |
| `shops.c` | Shop logic, `do_makeshop`, `do_shopset`, buy/sell/list/repairshops |
| `mud_prog.c` | The MOBprogram (mobprog) interpreter - trigger dispatch, if-checks, variable substitution, control flow |
| `mud_comm.c` | The actual `mpXXX` commands mobprogs can call (mpecho, mpforce, mpoload, etc.) |
| `fight.c` | Combat resolution |
| `magic.c` | Spellcasting, saving throws |
| `skills.c` / `swskills.c` | Non-magic skills, crafting (all the `do_make*` functions) |
| `space.c` | The entire ship/space system - by far the largest file in the codebase; ship stat setting, module install/remove, weapons fire, sabotage, power redirection |
| `act_move.c` / `act_comm.c` / `act_obj.c` / `act_info.c` / `act_wiz.c` | Mortal movement/communication/object/info commands, and immortal ("wiz") commands |
| `handler.c` | Core data structure manipulation - stat calculation, affect application, `affect_location_name()` (the reverse lookup of `a_types[]`, used for display) |
| `save.c` | Player file (pfile) save/load |
| `update.c` | The periodic tick - regen, aggression, weather, ship update loop |
| `const.c` / `tables.c` | Static string tables (class names, subclass names, etc.) |

### 1.3 Area file format (FUSS)

Area files are plain text, tagged sections, `\r\n` line endings.

```
#FUSSAREA
#AREADATA
Name         Some Area~
...
Ranges       <mob level low soft> <hi soft> <low hard> <hi hard>
#$

#MOBILE
Vnum       200
Keywords   guard~
Short      a guard~
...
#ENDMOBILE

#OBJECT
Vnum     201
Type     weapon~
...
#ENDOBJECT

#ROOM
Vnum     202
Name     A Room~
...
Reset M 0 200 1 202
#ENDROOM
```

Quirks specific to this codebase, confirmed against source and learned across
real sessions of working in it:

- **Vnums are per-type, not global.** Room 343, mobile 343, and object 343 can
  all exist simultaneously without conflict - `get_room_index`, `get_mob_index`,
  and `get_obj_index` are separate hash tables. Only collisions *within* a type
  matter.
- **The `Ranges` field in `#AREADATA` is a mob level range, not a vnum range.**
  It does not gate vnum allocation at all. Actual vnum ownership is enforced
  live, at creation time, by the builder's `aassign`ed area (§4.1).
- **Object `Stats` line:** `Stats <weight> <cost> <rent> <level> <layers>`. The
  second field is the shop-sale cost - an item with `cost 0` is invisible to
  `list` and can't be bought (`shops.c`'s `cost > 0` gate).
- **Object `Affect` line:** `Affect <type> <duration> <modifier> <location>
  <bitvector>`. `location` is a numeric index into the `apply_types` enum in
  `mud.h`. `modifier` is the amount. See §8 for the full breakdown - this is one
  of the most commonly misread lines in the whole format.
- **Object `Values` line:** `Values <v0> <v1> <v2> <v3> <v4> <v5>` - six raw
  integers whose *meaning* depends entirely on the object's `Type`. See §7 for
  the complete per-type breakdown; this is the other line that looks like noise
  until you know what type you're looking at.
- **Resets bind spawns to rooms**, in the room's own block:
  `Reset M <arg1> <mobvnum> <max> <roomvnum>` loads a mobile,
  `Reset G <arg1> <objvnum> <max>` gives/loads an object onto the last-loaded mob
  (typically used to stock a shopkeeper), `Reset O`/`Reset E` place objects in the
  room or equip them onto the last mob.

### 1.4 Build & boot

```
cd src
make swreality          # touch files first if it says "up to date" and shouldn't be
cd ../area
./../src/swreality 4801 # boot on a test port; grep for "bug:", "fatal", "duplicate"
```

`system/version.txt` is the running changelog - every meaningful change gets an
entry there.

---

## 2. Mortal Commands

There is no single short list - `system/commands.dat` currently defines roughly
550 commands, the large majority at Level 0. Rather than reproduce the whole
table (which would go stale the moment a skill gets added), here's how they're
organized:

| Category | Source file(s) | Examples |
|---|---|---|
| Movement & exploration | `act_move.c` | north/south/etc, look, exits, recall |
| Communication | `act_comm.c` | say, tell, gossip, emote, socials |
| Object interaction | `act_obj.c` | get, drop, wear, wield, give, put |
| Character info | `act_info.c` | score, inventory, equipment, who, affects |
| Combat | `fight.c`, `skills.c` | kill, flee, and all combat skills |
| Magic | `magic.c` | cast, and all spells |
| Non-combat skills / crafting | `skills.c`, `swskills.c` | all the `do_make*` crafting commands, pick, steal, sneak, hide |
| Space / ships | `space.c` | launch, land, fire, jump, calculate, plot, install_module, and the whole ship-systems command set |
| Subclasses | `player.c` | subclass, subclasses |
| Clans | `clans.c` | clan-related commands |
| Bounty hunting | `bounty.c` | bounty-related commands |

**In-game references:** `help <topic>` (pulls from `area/help.are`, the
authoritative and continuously-corrected reference) and `commands` (lists
everything the character's current level can use).

### 2.1 Worked examples: mortal commands that take multiple, order-dependent arguments

Most mortal commands take zero or one argument. A handful genuinely chain
several values together and are worth knowing cold, since getting the argument
order wrong just silently fails or targets the wrong thing.

**`calculate <starsystem> <entry x> <entry y> <entry z>`** - plots a hyperspace
jump. All four arguments are required together; there's no way to set the
destination and coordinates separately. Example:
```
calculate Corellia 12 -4 30
```
(Note: as of v1.29 this refuses to work at all while the ship is in simulator
mode - see §2.2 - since a simulated flight shouldn't expose the real starmap.)

**`redirect <system> <target>`** - reroutes ship power between subsystems
(`space.c`, `do_redirect`). `<system>` is the source being drained (`engine`,
`shield`/`shields`, `laser`/`lasers`), `<target>` is where the power goes, and
the valid targets differ per source:
```
redirect engine shield      Divert engine power to shields (ship slows down)
redirect shield laser       Divert shield power to lasers (recharges slower)
redirect laser engine       Divert laser power to engines (speed up)
redirect engine default     Return engine power to normal
```
Internally this sets/clears `SHIPFLAG_*RENGINE`/`*RLASER`/`*RSHIELD` combinations
and recalculates `ship->currspeed` on the spot - it's genuinely a two-token
command where both tokens matter and the second token's valid values depend on
the first.

**`sabotage <system>`** (from the engine room, requires being inside the ship
you're sabotaging) - single argument, but from a fixed vocabulary that maps to
specific `SHIPFLAG_SABOTAGED*` bits:
```
sabotage lasers | ions | drive | turret1 | turret2 | launcher | tlauncher | rlauncher
```
Each of these degrades a *different* subsystem and is cleared independently by
the matching `repairship` branch - `sabotage drive` doesn't get fixed by
`repairship hull`.

**Crafting commands** (`do_make*` in `swskills.c`) generally take the item name
you want to produce as a single argument, but success depends on standing in a
room with the right flags (`ROOM_FACTORY`, or `ROOM_SAFE`+`ROOM_SILENCE` for
lightsaber work) *and* having the raw materials in inventory *and* a high enough
skill - so while the command itself is simple, the precondition list is the
"multiple values" that actually matters. See §7's `oset` walkthroughs for what
a crafted item's underlying values look like once it exists.

---

## 3. Immortal Commands

Levels are computed from `MAX_LEVEL` (currently **105**) in `mud.h`. Practical
tiers, low to high:

| Level | Name(s) | Rough meaning |
|---|---|---|
| 100 | `LEVEL_HERO` | Top mortal level / apprentice-immortal boundary |
| 101 | `LEVEL_IMMORTAL`, `LEVEL_ACOLYTE`, `LEVEL_NEOPHYTE` | New immortal |
| 102 | `LEVEL_LESSER`, `LEVEL_TRUEIMM`, `LEVEL_DEMI`, `LEVEL_SAVIOR`, `LEVEL_CREATOR` | Working builder tier - most OLC commands live here |
| 103 | `LEVEL_GOD`, `LEVEL_GREATER`, `LEVEL_ASCENDANT` | Senior immortal |
| 104 | `LEVEL_IMPLEMENTOR`, `LEVEL_ETERNAL`, `LEVEL_INFINITE`, `LEVEL_SUB_IMPLEM` | Near-top |
| 105 | `LEVEL_SUPREME` | Absolute top (you) |

Verified per-command levels from `commands.dat` for everything covered in this
manual:

| Command | Level | Purpose |
|---|---|---|
| `goto` | 100 | Teleport to a room by vnum/name - **also creates rooms** (see §4.2) |
| `redit` | 100 | Edit the room you're standing in |
| `rlist` | 100 | List room vnums in an area |
| `savearea` / `loadarea` | 100 | Save/reload an area's data to/from disk |
| `rset` | 102 | Set individual room fields without entering redit mode |
| `mset` | 102 | Set fields on a mobile |
| `oset` | 102 | Set fields on an object prototype (§7-8) |
| `mcreate` / `ocreate` | 102 | Create a new mobile/object prototype at a vnum |
| `mpedit` / `opedit` / `rpedit` | 102 | Attach/edit mobprogs on mobiles/objects/rooms |
| `aassign` | 102 | Assign yourself a working area (governs which vnums you can create in) |
| `makeshop` / `shopset` | 102 | Turn a mobile into a shopkeeper and configure it |
| `olist` / `mlist` | 102 | List object/mobile vnums in an area |
| `aset` | 103 | Area-level settings (name, low/high vnum ranges, etc.) |

Other notable immortal commands in `build.c`: `astat` (area info), `foldarea` /
`unfoldarea` / `installarea` (area packaging), `rdelete` / `odelete` / `mdelete`
(delete a prototype).

---

## 4. Building

### 4.1 Claim an area before creating anything

```
aassign <area filename.are>
```

Sets `ch->pcdata->area`. Below `LEVEL_GREATER`, every room/object/mobile creation
command checks the new vnum against that area's declared vnum ranges
(`pArea->low_r_vnum`/`hi_r_vnum`, `low_o_vnum`/`hi_o_vnum`, etc.) and refuses if
it's outside them. `aassign none` (or `null`/`clear`) drops back to whatever's
automatically assigned to you.

### 4.2 Creating rooms - there is no separate "dig" command

Room creation is folded into `goto`. If you `goto <vnum>` and that vnum doesn't
exist yet, `do_goto`:

1. Checks the vnum isn't already a room, is positive, and is within your
   assigned area's room-vnum range (or you're `LEVEL_GREATER`+).
2. Validates it against the global valid-vnum table.
3. Calls `make_room()` and drops you into the freshly created room.

Workflow: `aassign` your area, then `goto` the vnum you want.

### 4.3 redit fields - full walkthrough with a multi-value example

`redit` has two modes: one-shot (`redit <field> <value>`) and a toggle mode
(`redit on`) where subsequent raw input is captured as `<field> <value>` directly
- useful for a rapid sequence of edits. `redit done`/`off` exits toggle mode.

Full field list:
```
name desc ed rmed
exit bexit exdesc exflags exname exkey exdistance
flags sector teledelay televnum tunnel
rlist
```

**The `exit` field is the best example of a single command carrying several
positional values that all matter**, straight from `do_redit`'s own usage line:

```
redit exit <dir> [room] [flags] [key] [keywords]
```

- `<dir>` - a direction word (`north`, `south`, etc.), OR prefixed with `+` to
  *add* a second exit in that slot rather than overwrite, OR prefixed with `#`
  to address an exit by its numeric slot instead of a compass direction (useful
  for non-standard directions).
- `[room]` - destination vnum. Omitting this (or passing `0`) *deletes* the
  exit in that direction rather than creating one - the same field name does
  double duty as both "create/edit" and "remove" depending on whether a vnum
  follows it.
- `[flags]`, `[key]`, `[keywords]` - door flags (locked/closed/pickproof),
  the key object's vnum, and the door's typed name, all optional trailing
  values.

Worked example - opening a locked door east to room 455 that needs a key:
```
redit exit east 455 door,closed,locked 500 blast door
```
And deleting an exit entirely:
```
redit exit east
```
(no room vnum after the direction = remove).

Everything else:
- `name` - one-line room summary
- `desc` - opens the full description in the line editor
- `ed` / `rmed` - add/remove an extra description (examine-able room feature)
- `flags` - room flags (indoors, nomob, safe, etc.)
- `sector` - terrain type
- `tunnel` - max occupants
- `teledelay` / `televnum` - random-teleport hazard configuration

### 4.4 Saving your work

`savearea` writes the currently-assigned area back to its `.are` file.
`foldarea`/`unfoldarea`/`installarea` handle packaging when moving an area
between dev and live - check `astat` on an area before assuming it needs folding.

---

## 5. Creating and Editing Mobiles

### 5.1 Create the prototype

```
mcreate <vnum> [copy-vnum] <mobile name>
```

`vnum` must not already exist, must fall inside your `aassign`ed area's mob-vnum
range (below `LEVEL_MODIFY_PROTO`), and must pass `is_valid_vnum`. The optional
`copy-vnum` clones an existing mobile's full stat block as a starting point -
useful for a run of similar shopkeepers or guards.

### 5.2 Edit it with mset

```
mset <mob> <field> <value>
```

Full field list, as currently implemented in `do_mset`:
```
str int wis dex con cha lck frc
sav1 sav2 sav3 sav4 sav5
sex race armor level numattacks credits
hitroll damroll hp force move align
name short long description title
spec spec_fun spec2 spec_fun2
flags wanted vip affected
minsnoop clan
mentalstate emotion thirst drunk full blood
r i s ri  (roleplay-related flags)
```

**Worked example - building a functional guard from scratch:**
```
mcreate 500 guard captain
mset guard captain short a stern-looking guard captain
mset guard captain long A guard captain stands here, watching the crowd closely.
mset guard captain level 60
mset guard captain hp 400
mset guard captain hitroll 25
mset guard captain damroll 20
mset guard captain armor -50
mset guard captain flags sentinel aggressive
```
Note `flags` (like `oset`'s `affect resistant`/`immune`, §8.2) accepts multiple
flag tokens in one call rather than needing one `mset` per flag.

- `short` / `long` / `description` - the three description strings (short = "a
  protocol droid", long = the room-glance line, description = what `look <mob>`
  shows)
- `spec` / `spec_fun` (and `spec2`) - attaches a hardcoded C special-procedure to
  the mob (distinct from mobprogs, which are scripted rather than compiled)
- `flags` - ActFlags (npc, sentinel, aggressive, etc.)

This is a field-setter, not a menu editor - there's no "medit" walking you
through a screen.

### 5.3 Delete

`mdelete <vnum>` removes a mobile prototype. This doesn't warn you about resets
or shops still referencing it - check `mlist`/`astat` first.

---

## 6. Creating Shops

Shops attach to a mobile - there is no separate "shop object."

### 6.1 `makeshop <mobvnum>`

Allocates the `SHOP_DATA` struct with defaults. Does **not** set what the shop
buys:
```
profit_buy  = 120   (player pays 120% of item cost)
profit_sell = 90    (player receives 90% of item cost when selling to shop)
open_hour   = 0
close_hour  = 23
```
Fails if the mob already has a shop.

### 6.2 `shopset <mobvnum> <field> <value>`

```
Field being one of: buy0 buy1 buy2 buy3 buy4 buy sell open close keeper
```
- `buy0`-`buy4` - the five item types this shop will purchase/carry (accepts a
  type name like `fightercomp` or its numeric `ITEM_*` value)
- `sell` / `buy` - the sell/buy profit percentages
- `open` / `close` - operating hours
- `keeper` - reassign which mob vnum owns this shop

### 6.3 Stocking it

Shops don't carry their own inventory list - what a shopkeeper "has" is whatever
is in that mob's inventory when the room resets, via `Reset G` lines under the
mob's `Reset M` line:
```
Reset M 0 <mobvnum> 1 <roomvnum>
  Reset G 1 <objvnum> 1
  Reset G 1 <objvnum> 1
  ...
```
Anything with a `list`-visible cost > 0 (the `Stats` cost field, §1.3) and a
matching `buy_type` shows up in `list` and is purchasable.

### 6.4 Full worked example - building a shop end to end

This mirrors the real pattern used for Menari Spaceport's module vendors
(`area/menari_spaceport`, rooms 452-456, mobs 363-367) - five single-purpose
shops, each carrying exactly one item type:

```
aassign menari_spaceport
mcreate 363 protocol droid starfighter parts vendor
mset 363 short a starfighter parts droid
mset 363 long A protocol droid stands behind the counter, restocking overdrive boosters.
makeshop 363
shopset 363 buy0 fightercomp
shopset 363 sell 90
shopset 363 buy 120
```
Then, in the room the droid will occupy (via `redit` or directly in the reset
block):
```
Reset M 0 363 1 452
  Reset G 1 227 1
  Reset G 1 232 1
  ...
```
`buy_type` accepting a name string (`fightercomp`) rather than forcing you to
look up the numeric `ITEM_*` constant is a deliberate convenience in
`do_shopset` - it calls `get_otype()` internally when the argument isn't
already a number.

---

## 7. Object Values In Depth (value0-value5)

Every object has six raw integer slots - `value[0]` through `value[5]` - whose
meaning is entirely determined by the object's `Type`. This is the single most
"multiple values, order matters, and the meaning shifts under you" system in the
codebase, so it gets its own section rather than a one-line mention.

You can set these two ways: raw (`oset <obj> value3 6`) or via a named shortcut
that `do_oset` maps to the correct slot for that object's current type
(`oset <obj> weapontype blaster` - same effect, much harder to get wrong).
**The named shortcuts only work if the object's `Type` is already set correctly**
- `do_oset` switches on `obj->item_type` to decide which shortcut table applies.

### 7.1 Weapons (`ITEM_WEAPON`)

| Slot | Shortcut | Meaning |
|---|---|---|
| value0 | `condition` | current condition (0 = auto-defaults to `INIT_WEAPON_CONDITION` on load) |
| value1 | `numdamdie` | number of damage dice |
| value2 | `sizedamdie` | size of each damage die |
| value3 | `weapontype` | weapon type, by name (table below) or number |
| value4 | `charges`/`charge` | current ammo/charge |
| value5 | `maxcharges`/`maxcharge` | max ammo/charge |

Weapon type names (`weapon_table[]`, index = value3):
```
0 none          4 whip           8 bludgeon
1 vibro-axe     5 claw           9 bowcaster
2 vibro-blade   6 blaster       10 (unused)
3 lightsaber    7 vibro-sword   11 force pike
                                12 (unused)
```
Blaster, lightsaber, vibro-blade, force pike, and bowcaster all auto-default
`value5` (max charge) to a fuzzy ~1000 on load if it's not set - representing
power cell/ammo capacity, not just literal "ammo" for melee weapons like the
lightsaber and vibro-blade.

**Worked example - a blaster pistol:**
```
ocreate 5001 a heavy blaster pistol
oset 5001 type weapon
oset 5001 weapontype blaster
oset 5001 numdamdie 3
oset 5001 sizedamdie 6
oset 5001 condition 100
oset 5001 wear wield
oset 5001 cost 800
```
This produces a 3d6 blaster with full condition, sellable for 800, worn wielded.

### 7.2 Armor (`ITEM_ARMOR`)

| Slot | Shortcut | Meaning |
|---|---|---|
| value0 | `condition` | current condition (defaults to value1 if 0 on load) |
| value1 | (no shortcut - set via `value1` directly) | max condition |
| value3 | - | also drives `obj->timer` on load |
| `ac` shortcut | → value1 | armor class value itself |

Note the overlap: `value1` is both "max condition" *and* what the `ac`
shortcut writes to. Set AC first, then condition, to avoid confusing yourself
about which write landed last.

**Worked example:**
```
ocreate 5002 a durasteel chestplate
oset 5002 type armor
oset 5002 ac 40
oset 5002 condition 100
oset 5002 wear body
```

### 7.3 Containers (`ITEM_CONTAINER`)

| Slot | Shortcut | Meaning |
|---|---|---|
| value0 | `capacity` | max weight/volume it can hold |
| value1 | `cflags` | container flags (closeable, locked, pickproof, etc.) |
| value2 | `key` | vnum of the key object that opens it, if locked |

**Worked example - a locked footlocker:**
```
ocreate 5003 a durasteel footlocker
oset 5003 type container
oset 5003 capacity 200
oset 5003 cflags closeable locked
oset 5003 key 5004
```
(where 5004 is a separately-created key object).

### 7.4 Potions and pills (`ITEM_POTION` / `ITEM_PILL`)

| Slot | Shortcut | Meaning |
|---|---|---|
| value0 | `slevel` | caster level the spells fire at |
| value1 | `spell1` | first spell (name, resolved via `skill_lookup`) |
| value2 | `spell2` | second spell |
| value3 | `spell3` | third spell |

Up to three spells fire off a single potion. **Worked example:**
```
ocreate 5005 a vial of bacta serum
oset 5005 type potion
oset 5005 slevel 30
oset 5005 spell1 cure light
oset 5005 spell2 refresh
```
(spell3 left unset - not every slot needs to be filled).

### 7.5 Salves (`ITEM_SALVE`)

| Slot | Shortcut | Meaning |
|---|---|---|
| value0 | `slevel` | caster level |
| value1 | `maxdoses` | doses when full |
| value2 | `doses` | current doses |
| value3 | `delay` | reuse delay - **keep this low**, per the in-code comment ("delay is anoying") |
| value4 | `spell1` | first spell |
| value5 | `spell2` | second spell |

**Worked example:**
```
ocreate 5006 a tube of kolto gel
oset 5006 type salve
oset 5006 slevel 25
oset 5006 maxdoses 5
oset 5006 doses 5
oset 5006 delay 2
oset 5006 spell1 cure light
```

### 7.6 Devices (`ITEM_DEVICE`, wand/staff-equivalent)

| Slot | Shortcut | Meaning |
|---|---|---|
| value0 | `slevel` | caster level |
| value1 | `maxcharges` | max charges |
| value2 | `charges` | current charges |
| value3 | `spell` | the single spell this device casts |

**Worked example:**
```
ocreate 5007 an ion charge emitter
oset 5007 type device
oset 5007 slevel 20
oset 5007 maxcharges 10
oset 5007 charges 10
oset 5007 spell ion field
```

### 7.7 Spice (`ITEM_RAWSPICE` / `ITEM_SPICE`)

| Slot | Shortcut | Meaning |
|---|---|---|
| value0 | `spicetype` | by name (table below) |
| value1 | `grade` | potency/quality |

Spice types (`spice_table[]`): `glitterstim, carsanum, ryll, andris` (plus
unused filler slots s4-s9).

### 7.8 Crystals (`ITEM_CRYSTAL`, lightsaber focusing crystals)

| Slot | Shortcut | Meaning |
|---|---|---|
| value0 | `gemtype` | by name (table below) |

Crystal types (`crystal_table[]`): `non-adegan, kathracite, relacite, danite,
mephite, ponite, illum, corusca`.

### 7.9 Ammo and batteries (`ITEM_AMMO`, `ITEM_BOLT`, `ITEM_BATTERY`)

| Slot | Shortcut | Meaning |
|---|---|---|
| value0 | `charges`/`charge` | current charge count - auto-fuzzy-defaults on load if left at 0 (battery/bolt ~95, ammo ~495) |

### 7.10 Switches, levers, buttons (`ITEM_SWITCH` / `ITEM_LEVER` / `ITEM_BUTTON`)

| Slot | Shortcut | Meaning |
|---|---|---|
| value0 | `tflags` | trigger flags - what this switch is wired to do when thrown (resolved via `get_trigflag`) |

### 7.11 Food (`ITEM_FOOD`) - the one with load-time behavior, not just storage

Food doesn't use named `oset` shortcuts at all - it's set with raw `value1`/
`value4` - but it's worth documenting because of what happens on load
(`db.c`):
- `value1` is the max condition (how long before it rots)
- `value4`, if nonzero, is used as the *initial* condition instead of value1 -
  i.e. you can spawn already-half-rotten food by setting value4 lower than value1.
```
oset 5008 value1 100
oset 5008 value4 20
```
produces food that rots as if it's already 80% of the way to spoiled.

---

## 8. Affects In Depth (the `oset affect` system)

This is the mechanism behind every stat-granting item in the game, including
every ship module. It's the same underlying `AFFECT_DATA` structure whether
it's applied via raw area-file text or via command.

### 8.1 Syntax and what it actually creates

```
oset <object> affect <field> <value>
```

Internally, `do_oset` looks `<field>` up via `get_atype()` against the `a_types[]`
string table (an exact-match lookup across all `MAX_APPLY_TYPE` entries), then
builds an `AFFECT_DATA` with `type = -1`, `duration = -1` (i.e. permanent, not
adjustable through this path), `location = <resolved field>`, `modifier =
<value>`. This is the exact same five-field structure as a raw `Affect` line in
the area file (`type duration modifier location bitvector`) - `oset affect` is
just a friendlier way to write that line without needing to know the numeric
location index.

The full list of stat fields `a_types[]` recognizes (this is longer than what
`oset`'s own built-in help text shows - the help text is a stale partial list
that was never updated when the ship/space apply types were added):

```
Core stats:     strength dexterity intelligence wisdom constitution charisma luck
Vitals:         hit move force credits experience
Combat:         armor hitroll damroll
Saves:          save_poison save_rod save_para save_breath save_spell
Skills:         backstab pick track steal sneak hide palm detrap dodge peek
                scan gouge search mount disarm kick parry bash stun punch
                climb grip scribe brew
Misc:           sex level age height weight affected resistant immune
                susceptible weaponspell wearspell removespell emotion
                mentalstate stripsn remove dig full thirst drunk blood
Ship/space:     hassembly engine generator tractorbeam overdrive laserbattery
                simulator cloak mlauncher tlauncher rlauncher hyperspeed
                realspeed lasers maxshield maxenergy maxmissiles maxrockets
                maxtorpedos comm sensor astro_array chaff manuever
                laserdamage ions armor cargo
```

### 8.2 Multi-flag affects: resistant / immune / susceptible / affected

These four fields don't take a single number - `do_oset`'s affect handler
detects them specifically (`loc >= APPLY_AFFECT && loc < APPLY_WEAPONSPELL`) and
switches into a loop that reads **as many flag tokens as you give it on one
line**, OR-ing each into a single bitvector:
```
oset <object> affect resistant fire cold electricity
```
sets resistance to all three damage types in one command - not three separate
`oset` calls. This is the object-affect equivalent of `mset`'s `flags` field
(§5.2) and `oset`'s own `flags`/`wear` fields, all of which accept multiple
space-separated tokens per call rather than one-token-per-call.

### 8.3 Worked example: recreating a real ship module from scratch via commands

Every module object in Menari Spaceport (§6.4) was actually built by hand-editing
the area file directly during that session - but the exact same result is
achievable purely through `ocreate`/`oset`, and this is worth knowing since it's
faster for one-off items. Recreating vnum 227, the fighter-class overdrive
booster module:

```
ocreate 227 a fighter-class overdrive booster module
oset 227 type fightercomp
oset 227 short a fighter-class overdrive booster module
oset 227 long A fighter-class overdrive booster module is here.
oset 227 flags inventory
oset 227 wear take
oset 227 cost 5000
oset 227 affect engine 1
oset 227 affect overdrive 1
```

Two `affect` calls, matching the object's two real `Affect` lines:
- `affect engine 1` - the `APPLY_ENGINE`-family exclusivity marker used purely
  by `module_type_install()` to block a second engine-type component from being
  installed on the same ship; the modifier value here is meaningless noise.
- `affect overdrive 1` - the actual functional grant; `update_ship_modules()`
  reads this and sets `ship->overdrive = 1`, unlocking the `overdrive` command
  for that ship.

Compare against a stat-granting module, the fighter engine (vnum 236, which
actually adds +250 realspeed rather than just flipping a flag):
```
ocreate 236 a fighter-class engine module
oset 236 type fightercomp
oset 236 affect engine 1
oset 236 affect realspeed 250
```
Here the second affect's modifier (250) is the real number - `update_ship_modules`
does `ship->realspeed += paf->modifier` for every installed module carrying an
`APPLY_REALSPEED` affect, which is also exactly why a ship can carry *multiple*
components that all add to `realspeed` (a secondary "speed coil" booster, say)
without conflicting - only `engine`/`hassembly`/`generator`/`tractorbeam`/
`overdrive`/`laserbattery`/`simulator`/`cloak` are checked for one-per-ship
exclusivity; the raw stat locations like `realspeed`/`hyperspeed` are not, by
design.

### 8.4 Removing an affect

```
oset <object> rmaffect <affect#>
```
where `<affect#>` is the position in the object's affect list (check with `oset
<object>` bare, or look at the object), not the location code itself.

---

## 9. Using Mobprogs

Mobprogs let a mobile react to things happening around it - a trigger fires, and
a short list of commands (which can include conditional logic) runs "as" that
mobile.

### 9.1 Attaching a mobprog

**In-file**, appended after the mobile's block, terminated by a line starting
with a single `|`:
```
>trigger_type argument_list~
program_command_1
program_command_2
...
~
```

**Via `#MOBPROGS` section**, referencing external files:
```
#MOBPROGS
M <vnum> <mobprogram_filename>
S
```

In-game, `mpedit <mob>` is the live editor for attaching/editing these.

### 9.2 Trigger types

Full current list (`mud.h` + `mprog_name_to_type` in `db.c` - notably larger
than what the legacy `doc/mudprogs/DOC` describes):

```
act_prog        speech_prog     rand_prog       fight_prog / rfight_prog
death_prog / rdeath_prog        hitprcnt_prog
entry_prog / enter_prog         greet_prog / rgreet_prog / ogreet_prog
all_greet_prog  give_prog       bribe_prog      hour_prog       time_prog
wear_prog       remove_prog     sac_prog        look_prog       exa_prog
zap_prog        get_prog        drop_prog       damage_prog     repair_prog
randiw_prog     speechiw_prog   pull_prog       push_prog
sleep_prog      rest_prog       leave_prog      script_prog     use_prog
```

### 9.3 Control flow

```
if <ifcheck>
   ...commands...
or <ifcheck>
   ...commands (runs if the preceding if/or was false and this check is true)...
else
   ...commands...
endif
break
```
No loop construct - `or` chains an additional condition onto the same if/else
block rather than being a general boolean operator; `break` aborts the rest of
the current prog.

### 9.4 If-checks

Verified against `mprog_do_ifcheck`:
```
rand<N>            random chance
level, sex, position, goldamt, str, wis, int, dex, con, cha, lck
race, clan, clantype, council / senator
doingquest, ishelled, norecall
name, number
objtype, objval0 .. objval5   (when the check target is an object, not a char)
```
Numeric checks take a comparison operator and value (`if level > 50`). String
checks compare against a literal (`if race > wookiee` uses string comparison
semantics for equality-style checks, not numeric ordering).

### 9.5 Variable substitution

```
$n / $N   actor's name / actor's short description (NPC) or name+title (PC)
$t / $T   secondary target's name / short description+title
$i / $I   the mobile's own name / short description
$r        a random character in the room
```

### 9.6 The mpXXX commands

Full current list (`commands.dat` + `mud_comm.c`):
```
mpecho            mpechoat          mea (alias)       mpat
mpforce           mpechoaround      mer (alias)       mpasound
mpoload           mpjunk            mpgoto            mpdamage
mpdeposit         mprestore         mpkill            mptransfer
mpmload           mpnothing         mppurge           mpinvis
mpadvance         mpapply           mpapplyb          mppkset
mpclosepassage    mpopenpassage     mpdream           mpslay
mppractice        mpwithdraw        mpgain
```

### 9.7 Worked example: a multi-condition combat trigger

This combines a numeric threshold check, a chained `or`, and several different
mpcommands in one prog - closer to what a real "interesting" mob actually needs
than the single-line examples above:

```
#MOBILE
Vnum       501
Keywords   bounty hunter~
Short      a grizzled bounty hunter~
...
>hitprcnt_prog 25~
if hitprcnt < 25
   mpecho The bounty hunter snarls and reaches for something at his belt.
   if rand60
      mpoload 5010
      mpechoat $n You'll regret that.
      mpforce $n flee
   or rand40
      mpecho He grits his teeth and fights on.
   endif
endif
~
|

>speech_prog surrender~
if position > 3
   mpecho The bounty hunter lowers his weapon. Smart choice.
   mppurge
endif
~
|
#ENDMOBILE
```

Walking through it: `hitprcnt_prog 25` fires once the mob drops to 25% HP or
below. Inside, a nested `if`/`or` picks one of two behaviors with a 60/40 split
- loading a smoke-grenade object (`mpoload 5010`) and forcing the attacker to
flee, or just continuing the fight. The second prog block is a completely
separate trigger (`speech_prog`, keyed on the word "surrender") with its own
independent `if` gating on the mob's position, ending the encounter with
`mppurge` if the check passes. Two prog blocks on one mobile, each with their
own trigger type and independent logic, is the normal way multiple behaviors
get layered onto a single mob.

---

## 10. Ships In Depth

Ships are the single largest system in the codebase - `space.c` alone is bigger
than most MUD codebases in their entirety. This section covers how a ship
prototype gets built from nothing, the full `setship` numeric field table (which
nothing in-game prints in one place), and how it connects back to the module
system in §7-8.

### 10.1 Creating a ship

```
makeship <filename> <ship name>
```

`do_makeship` allocates a brand-new `SHIP_DATA`, defaults its type to
`SHIP_CIVILIAN`, zeroes out owner/pilot/copilot/home/description to empty
strings, sets `energy = maxenergy` and `hull = maxhull` (both of which are 0 at
this point - see §10.3, you set the real numbers with `setship` immediately
after), and saves it under `<filename>` in the ship data directory. The ship
exists after this but is entirely unconfigured - no rooms, no stats, no class.
Everything else is `setship`.

`copyship <source> <new filename> <new name>` clones an existing ship's full
stat block (including its module slot count, per v1.27) as a faster starting
point than building from zero - useful for a fleet of identical fighters.

### 10.2 Two different "type" concepts - don't confuse them

- **`setship <ship> type <republic|imperial|civilian|mob>`** - faction/ownership
  category. Purely narrative/political, doesn't touch any combat stat.
- **`setship <ship> class <0-11>`** - the actual hull class, which is what
  drives every numeric cap in §10.3. Set as a raw number matching the
  `ship_class` enum order: `0=FIGHTER_SHIP, 1=MIDSIZE_SHIP, 2=FRIGATE_SHIP,
  3=CAPITAL_SHIP, 4=SUPERCAPITAL_SHIP, 5=SHIP_PLATFORM, 6=CLOUD_CAR,
  7=OCEAN_SHIP, 8=LAND_SPEEDER, 9=WHEELED, 10=LAND_CRAWLER, 11=WALKER`.

**Set `class` before setting any numeric stat field** - every numeric `setship`
field below branches on `ship->ship_class` to decide the cap it enforces, so
setting speed/shields/lasers/etc. before the class is correct will cap against
whatever class the ship happens to default to, not the one you intend.

### 10.3 The full setship field table (5 core starship classes)

Every numeric field below is a `URANGE(0, atoi(argument), <cap>)`, and the cap
is different per hull class - verified directly against every branch in
`do_setship`. This table covers the five actual *starship* classes (fighter
through supercapital); the six ground/atmospheric vehicle classes (platform,
cloud car, ocean ship, land speeder, wheeled, land crawler, walker) mostly have
their own separate, generally much smaller caps set in the same field blocks -
several fields (`tractorbeam`, `hyperspeed`) explicitly refuse to be set at all
on some of them ("Impossible.") since a land speeder has no hyperdrive to speak
of. Check the relevant field's block in `space.c` directly if you're building a
ground vehicle - the pattern is identical, just with different numbers appended
after the five starship branches.

| Field | Fighter | Midship | Frigate | Capital | Supercapital |
|---|---|---|---|---|---|
| `speed` (realspeed) | 250 | 150 | 75 | 25 | 15 |
| `hyperspeed` | 100 | 125 | 75 | 50 | 50 |
| `manuever` | 200 | 150 | 75 | 50 | 15 |
| `lasers` | 4 | 6 | 10 | 15 | 25 |
| `laserdamage` | 5 | 10 | 15 | 20 | 25 |
| `ions` | 2 | 3 | 5 | 10 | 15 |
| `armor` (& maxarmor) | 5 | 10 | 25 | 50 | 75 |
| `shield` (maxshield) | 500 | 1000 | 2500 | 5000 | 7500 |
| `hull` (& maxhull) | 1000 | 5000 | 10000 | 17500 | 25000 |
| `energy` (& maxenergy) | 5000 | 10000 | 25000 | 30000 | 32000 |
| `sensor` | 50 | 100 | 150 | 175 | 200 |
| `chaff` (& maxchaff) | 5 | 15 | 25 | 50 | 50 |
| `missiles` (& maxmissiles) | 12 | 18 | 32 | 64 | 128 |
| `torpedos` (& maxtorpedos) | 6 | 12 | 24 | 48 | 96 |
| `rockets` (& maxrockets) | 1 | 3 | 9 | 27 | 81 |
| `tractorbeam` | *impossible* | 5 | 25 | 100 | 150 |

Fields with **no class variance** - flat range regardless of hull class:
```
comm         0-255
astroarray   0-255
autodamage   0-255   (head-imm only)
autoammo     0-255   (head-imm only, sets both current and max)
maxmodules   10-99   (v1.27; per-class DEFAULTS of 15/30/50/75/99 apply only
                       when bulk-backfilling via resetmaxshipmodules with no
                       explicit value - v1.28 - not a hard per-class cap)
```

**Notably absent from `setship` entirely: cargo capacity.** There's no
`setship <ship> cargo` field at all - `maxcargo` can *only* be granted through
the module system (`oset <cargo-pod-obj> affect cargo <n>`, §7-8), never set
directly by an admin. If a ship needs cargo space, it needs a cargo pod module
installed - there's no shortcut.

Non-numeric / structural fields, briefly:
```
filename name owner copilot pilot description home
cockpit entrance turret1 turret2 hanger engineroom firstroom lastroom shipyard
pilotseat coseat gunseat navseat
flags
```
The seat/room fields (`cockpit`, `entrance`, `turret1`/`turret2`, `hanger`,
`engineroom`, `pilotseat`, `coseat`, `gunseat`, `navseat`, `firstroom`,
`lastroom`, `shipyard`) all take a room vnum - these wire the ship's physical
layout to actual rooms in your area, so the rooms need to exist (via `redit`,
§4.2-4.3) before you assign them here.

### 10.4 Worked example: building a starfighter from nothing

```
aassign my_shipyard
makeship starfighter_01 the Void Runner
setship "Void Runner" type republic
setship "Void Runner" class 0
setship "Void Runner" speed 250
setship "Void Runner" hyperspeed 100
setship "Void Runner" manuever 200
setship "Void Runner" lasers 4
setship "Void Runner" laserdamage 5
setship "Void Runner" armor 5
setship "Void Runner" shield 500
setship "Void Runner" hull 1000
setship "Void Runner" energy 5000
setship "Void Runner" sensor 50
setship "Void Runner" missiles 12
setship "Void Runner" cockpit 6001
setship "Void Runner" entrance 6002
setship "Void Runner" engineroom 6003
setship "Void Runner" pilotseat 6001
setship "Void Runner" firstroom 6000
setship "Void Runner" lastroom 6010
setship "Void Runner" shipyard 6000
setship "Void Runner" owner Public
```
This maxes every stat at the fighter's own cap (setting a value above the cap
just silently clamps to it, so `speed 9999` and `speed 250` produce an
identical result for a fighter) and wires the room wiring last, once the rooms
(6000-6010, built via §4.2-4.3 first) actually exist.

### 10.5 Ships and the module system, tied together

Everything in §10.3 is what a ship has with **zero modules installed** - the
moment even one module gets installed via `install_module`, `update_ship_modules()`
zeroes out most of these same stats and rebuilds them entirely from the sum of
installed components (§8.3 walks through exactly which apply types are additive
vs. best-of vs. one-time flags). A ship built purely with `setship` and never
given any modules keeps its `setship` numbers permanently and behaves like a
classic non-modular ship; a ship with even one module installed is from that
point on only as good as what's bolted onto it. This is why `maxmodules`
defaults to 0 (§1.3, §6) - opting into the module system is a deliberate,
per-ship choice, not automatic.

---

## 11. Subclasses In Depth

Subclasses are a five-file feature - adding or auditing one means touching the
same five places every time, in the same order, whether you're adding a brand
new subclass or fixing a gap in an existing one. This section documents the
pattern directly from the real work of auditing all 27 SWGD subclasses against
this codebase and porting the four that were missing (Survivor, Jury Rigger,
Certified Genius, Mercenary).

### 11.1 The five files, in the order you touch them

**1. `mud.h`** - the enum. Currently:
```c
#define SUBCLASS_NONE            0
#define SUBCLASS_SNIPER          1
...
#define SUBCLASS_PARAMEDIC      23
#define SUBCLASS_SURVIVOR       24
#define SUBCLASS_JURYRIGGER     25
#define SUBCLASS_GENIUS         26
#define SUBCLASS_MERCENARY      27
#define SUBCLASS_MAX             28
```
**Always append at the end and bump `SUBCLASS_MAX`. Never renumber an existing
value** - the number is what's stored in every player's pfile, and renumbering
retroactively reassigns everyone's subclass to whatever now occupies that slot.

**2. `const.c`** - the display-name table, `subclasses[]`, one string per enum
value in the same order:
```c
"Prodigy", "False Prophet", "Doctor", "Paramedic",
"Survivor", "Jury Rigger", "Certified Genius", "Mercenary"
```
This is what `subclasses[ch->subclass]` resolves to anywhere the game prints a
subclass name - `do_subclasses`, `score`, etc.

**3. Effect files** - wherever the subclass actually *does* something. This is
scattered by nature of what the subclass grants, not one file:

- **`magic.c`** - saving throw bonuses. This codebase's five `saves_*` functions
  (`saves_poison_death`, `saves_wands`, `saves_para_petri`, `saves_breath`,
  `saves_spell_staff`) each end in a fixed cap line; a subclass save bonus goes
  in immediately before it:
  ```c
  if( victim->subclass == SUBCLASS_SURVIVOR || victim->subclass == SUBCLASS_FORCE_SENSITIVE )
     save += 5;
  save = URANGE( 5, save, 95 );
  ```
  Note this codebase's saves use an older SWR-style formula (`50 + (top_level -
  level - saving) * 2`), not SWGD's percent-based version - a ported bonus needs
  adapting to land before the existing cap line, not replacing the formula.

- **`update.c`** - regen and skill-cap bonuses. `hit_gain`/`mana_gain`/`move_gain`
  each have their own `COND_FULL`/`COND_THIRST` penalty block; a flat regen bonus
  goes in immediately before that block:
  ```c
  if( ch->subclass == SUBCLASS_SURVIVOR )
     gain += 20;
  ```
  A *multiplier* (rather than flat bonus) goes in immediately *after* the same
  block, since the COND penalties are themselves multiplicative and should apply
  first:
  ```c
  if( ch->subclass == SUBCLASS_JEDI || ch->subclass == SUBCLASS_SITH_HUNTER )
     gain *= 4;
  if( ch->subclass == SUBCLASS_PRODIGY )
     gain *= 10;
  ```
  Separately, `update.c` also holds the max-skill-level switch (a subclass that
  lets a player train a skill past the normal cap):
  ```c
  case SUBCLASS_GENIUS:
     if( ability == ENGINEERING_ABILITY || ability == MEDICAL_ABILITY )
        level += 25;
     break;
  ```

- **`skills.c`** - adept-level overrides (a subclass letting a skill train past
  the normal 100% adept ceiling) and any subclass-specific mechanical change to
  a skill's own math:
  ```c
  if( !IS_NPC( ch ) && ch->subclass == SUBCLASS_MERCENARY && sn == gsn_spray )
     adept = UMAX( adept, 150 );
  ```
  and, further down in the relevant skill's own function body, a straight
  behavior change:
  ```c
  if( !IS_NPC( ch ) && ch->subclass == SUBCLASS_MARTIST )
     dam *= 2;   /* inside do_gouge */
  ```

- **`skills.c` / `swskills.c`** - location-gate exemptions, for a subclass that
  bypasses a normal restriction (e.g. Jury Rigger crafting without a factory):
  ```c
  if( !IS_SET( ch->in_room->room_flags, ROOM_FACTORY ) && ch->subclass != SUBCLASS_JURYRIGGER )
  ```
  This pattern repeats once per crafting command that has the gate - in the
  Jury Rigger case, 25+ separate `do_make*` functions each needed the same
  `&& ch->subclass != SUBCLASS_JURYRIGGER` appended to their existing
  `ROOM_FACTORY` check, plus the two `ROOM_SILENCE` lightsaber-crafting gates.
  There's no central "crafting gate" function to patch once - it's genuinely
  per-command.

**4. `player.c`** - the grant block and the listing.

The grant block, one `if` per subclass, all living in the same function
(`do_subclass`) before its final fallthrough ("That is not a valid subclass"):
```c
if( !str_cmp( argument, "sniper" ) )
{
    if( ch->skill_level[HUNTING_ABILITY] >= 100 )
    {
        send_to_char( "Congratulations, you are now a Sniper.\n\r", ch );
        ch->subclass = SUBCLASS_SNIPER;
        return;
    }
    send_to_char( "You need at least level 100 in bounty hunting to become a Sniper.\n\r", ch );
    return;
}
```
That's the real, current Sniper block, verbatim - the shape every new subclass
follows: check the requirement(s), grant on success with a flavor message, or
explain what's missing and return without granting. Requirements are typically
`ch->top_level` (character level) plus one or more `ch->skill_level[X_ABILITY]`
thresholds. **New grant blocks go in before the function's final fallthrough
case, not after** - anything appended after the "not a valid subclass" message
never runs.

The listing (further down in the same file, grouped into categories like
"General Subclasses:", "Combat Subclasses:", "Bounty Hunting Subclasses:") is a
series of `ch_printf` calls each printing a `subclasses[SUBCLASS_X]` name into
the appropriate category. A new subclass needs its own name added to whichever
category line fits it, or it grants successfully but never shows up in
`subclasses` output - an easy thing to forget since the grant and the listing
are two separate, non-adjacent edits in the same file.

### 11.2 Pfile safety

Because the enum value is what's actually stored per-player, the two hard rules
are: **only append, never renumber**, and **`SUBCLASS_MAX` must always match the
true count** (it's used as a bound check elsewhere - an out-of-date `MAX` after
an append can let an invalid subclass value slip through validation).

### 11.3 Worked example: adding a new subclass end to end

Say you want a `SUBCLASS_INFILTRATOR` granted at level 100 + Espionage/Hunting
100, that gives +15% to steal amounts and lets the player pick locks silently
(no room-noise check). Following the same five-file pattern:

**mud.h:**
```c
#define SUBCLASS_MERCENARY   27
#define SUBCLASS_INFILTRATOR 28
#define SUBCLASS_MAX         29
```

**const.c:**
```c
"Mercenary", "Infiltrator"
```

**skills.c** (steal bonus, next to the existing Sneak steal bonus pattern):
```c
if( ch->subclass == SUBCLASS_INFILTRATOR )
   amount = ( int )( victim->gold * number_range( 8, 20 ) / 100 );
```
and wherever the lockpicking noise-check lives, an exemption in the same style
as the Jury Rigger crafting-gate pattern.

**player.c** grant block, inserted before the fallthrough:
```c
if( !str_cmp( argument, "infiltrator" ) )
{
    if( ch->top_level < LEVEL_HERO )
    {
        send_to_char( "You need to be at least level 100 to become an Infiltrator.\n\r", ch );
        return;
    }
    if( ch->skill_level[HUNTING_ABILITY] < 100 )
    {
        send_to_char( "You need at least level 100 in bounty hunting first.\n\r", ch );
        return;
    }
    send_to_char( "You melt into the shadows - you are now an Infiltrator.\n\r", ch );
    ch->subclass = SUBCLASS_INFILTRATOR;
    return;
}
```
...plus adding "Infiltrator" to the appropriate category in the `do_subclasses`
listing, and a `help.are` entry for it (§6-9 conventions apply the same way
here as everywhere else - a subclass without a help entry is just as easy to
forget as one without a listing line).

---

## 12. Combat Mechanics In Depth

Combat resolution lives almost entirely in `fight.c`, split across two layers:
`multi_hit()` decides *how many* attacks happen in a round, and `one_hit()`
resolves each individual attack's to-hit roll and damage.

### 12.1 How many attacks happen in a round

`multi_hit()` is a chain of independent chance rolls, each one potentially
adding another call to `one_hit()` - it stops the moment any hit ends the fight
or the target changes:

1. **The base attack always happens.**
2. **Dark Frenzy bonus attacks** - if the attacker has the `AFF_DARK_FRENZY`
   affect, its stored modifier value is how many *guaranteed* extra attacks fire
   before anything chance-based is checked.
3. **Berserk** - `IS_AFFECTED(ch, AFF_BERSERK)` rolls against
   `learned[gsn_berserk] * 5 / 2` (NPCs always succeed).
4. **Dual wield** - if something's equipped in `WEAR_DUAL_WIELD`, rolls against
   `learned[gsn_dual_wield]` for an extra attack, and separately computes a
   `dual_bonus` (`learned[gsn_dual_wield] / 10` for players, skill/10 for NPCs)
   that feeds into every attack-chance roll below it.
5. **Martial Artist unarmed dual-wield** - if the subclass is `SUBCLASS_MARTIST`
   and nothing is wielded, this grants *two* guaranteed extra attacks outright
   (documented in-code as "one more attack for their speed bonus") - no roll
   needed, unlike the dual-wield case above.
6. **Low movement penalty** - if `ch->move < 10`, `dual_bonus` is forced to -20,
   dragging down every subsequent chance roll.
7. **NPC fixed attack count** - if the attacker is an NPC with `numattacks > 0`
   set (`mset numattacks`, §5.2), it simply loops that many guaranteed attacks
   and returns - none of the skill-based chances below apply to such mobs at all.
8. **Second / third / fourth / fifth attack** - four sequential independent
   rolls, each against a skill (`gsn_second_attack` through `gsn_fifth_attack`)
   modified by `dual_bonus` at increasing weight (full bonus for second, ×1.5
   for third/fourth/fifth), each divided down (÷1.5, then ÷2, ÷2, ÷2). Every one
   of these can succeed or fail independently and each calls `learn_from_success`
   / `learn_from_failure` on its own skill, meaning it's a real skill to train,
   not a fixed class perk.
9. **NPC extra bonus attack** - a flat `top_level / 4` percent chance of one more
   attack, checked only for NPCs, after all the named-skill attacks.

Move point cost (`encumbrance()`, scaled by terrain `sector_type`) is only
deducted if the *entire* round ends without a hit landing.

### 12.2 To-hit resolution (inside one_hit)

```
thac0 = interpolate(COMBAT_ABILITY_skill, 20, 10) - hitroll
victim_ac = victim's AC / 10
```
- `interpolate()` slides thac0 from 20 (untrained) down to 10 (fully trained
  Combat Ability) - lower thac0 is better, matching classic THAC0 conventions.
- `victim_ac` is adjusted before the roll: +1 if the attacker can't see the
  victim's weapon, -4 (better AC, i.e. harder to hit) if the attacker can't see
  the victim at all, +2 for a Defel victim's natural camouflage, +5 if the
  victim isn't awake (asleep/stunned/etc. - meaning *worse* AC, easier to hit),
  plus a weapon-proficiency-derived adjustment (`prof_bonus / 20`).
- The roll: `d20`, where a natural 1 always misses, a natural 20 always hits,
  and anything else needs `diceroll >= thac0 - victim_ac`.

### 12.3 Damage resolution

- **Unarmed:** `number_range(barenumdie, baresizedie * barenumdie) + damplus`
  (all per-character fields, typically race/class-derived).
- **Martial Artist unarmed override:** flat `number_range(280, 380)`, replacing
  the barenumdie formula entirely when unarmed and no weapon wielded - a huge
  jump over the generic unarmed formula, which is the mechanical core of why
  the subclass exists.
- **Wielded weapon:** `number_range(wield->value[1], wield->value[2])` - the
  exact same `numdamdie`/`sizedamdie` slots documented in §7.1, used here as a
  literal min/max range rather than "roll N dice of size M" (this codebase's
  weapon damage is a flat random range between the two stored values, not
  dice-notation multiplication).
- **Damroll** adds flat on top of whichever base applied.
- **Weapon proficiency bonus** scales damage multiplicatively:
  `dam *= (1 + prof_bonus / 100)`.
- **Enhanced Damage skill** (`gsn_enhanced_damage`) adds
  `dam * learned[gsn_enhanced_damage] / 120` on success, and is itself a
  trainable skill checked on every hit.
- **Sleeping/incapacitated victim:** flat ×2.
- **Backstab:** `dam *= (2 + URANGE(2, HUNTING_ABILITY - victim's COMBAT_ABILITY/4, 30) / 8)`
  - the attacker's Hunting skill relative to the victim's Combat skill drives
    the multiplier, clamped 2-30 before being folded in.
- **Circle** (a flanking/positional backstab variant) uses the identical formula
  with `/16` instead of `/8` - roughly half the multiplier ceiling of a
  straight backstab.
- **RIS (resistant/immune/susceptible) processing** runs last, checking each
  damage-type bit (`RIS_FIRE`, `RIS_COLD`, `RIS_ELECTRICITY`, `RIS_ACID`,
  `RIS_ENERGY`, `RIS_DRAIN`, `RIS_POISON`, `RIS_BLUNT`, `RIS_PIERCE`,
  `RIS_SLASH`, `RIS_MAGIC`/`RIS_NONMAGIC`) against the victim's resistances -
  this is the exact same `oset affect resistant/immune/susceptible` system
  documented in §8.2, read back out during combat.

### 12.4 Where this connects to earlier sections

A crafted or `oset`-built weapon's `value1`/`value2` (§7.1) are read here as the
literal damage range - there's no dice notation to reason about, just "this
weapon hits for somewhere in this range, every time, before bonuses." A
subclass's combat-relevant effects (Martial Artist's unarmed override and
fist-based dual-wield, Mercenary's still-unported two-handed-as-dual-wield
effect from §11) hook in at exactly the decision points listed in §12.1 -
knowing this flow is what makes it possible to find the right insertion point
for a new subclass combat perk instead of guessing.

---

## 13. Crafting In Depth

Every `do_make*` crafting command in `swskills.c`/`skills.c` follows the same
two-phase shape: an instant precondition/material check that *starts* a timed
process, and a second pass, ticks later, that actually consumes materials and
creates the item. Understanding this shape is what makes it possible to add a
new craftable item without just copy-pasting an existing one blindly.

### 13.1 Phase one: the instant check (starting the craft)

On the initial call (`ch->substate` at its default), a crafting command:

1. **Checks the room** - `ROOM_FACTORY` (or, for lightsaber-family crafting,
   `ROOM_SAFE` + `ROOM_SILENCE`), with the Jury Rigger subclass exemption
   (§11.1) appended to every one of these checks individually.
2. **Checks for required tools and materials by *presence only*** - walking the
   character's inventory (`ch->last_carrying`) looking for specific
   `item_type`s (e.g. `ITEM_TOOLKIT`, `ITEM_DURAPLAST`, `ITEM_BATTERY`,
   `ITEM_OVEN`, `ITEM_CIRCUIT`, `ITEM_SUPERCONDUCTOR` for a blaster). Nothing is
   consumed yet - this pass only sets boolean flags for "do I have one of
   these."
3. **Rolls the initial skill check** - `number_percent() < learned[gsn_X]` (or
   always succeeds for NPCs). Failure sends a flavor "can't figure out how to
   fit the parts together" message and calls `learn_from_failure` - the attempt
   is over, nothing consumed, nothing scheduled.
4. **On success, schedules the completion** via `add_timer(ch, TIMER_DO_FUN,
   <ticks>, do_makeX, 1)` and stashes the original argument in `ch->dest_buf`
   for phase two to recover. **The tick count itself is subclass-dependent** -
   the real `do_makeblaster` example: Quickwork subclass gets 10 ticks,
   Weaponsmith gets 15, everyone else gets 25. This is the crafting system's
   own version of the "subclass changes a numeric parameter" pattern from §11.

If the character is interrupted (moves, gets attacked, disconnects) before the
timer fires, `SUB_TIMER_DO_ABORT` fires instead, freeing the stashed buffer and
telling the player they failed to finish - no item, no consumed materials,
since none were touched yet.

### 13.2 Phase two: completion (materials consumed, item created)

When the timer actually fires (`ch->substate == 1`), the function:

1. Recovers the original argument from `ch->dest_buf`.
2. **Re-checks materials, and this time actually consumes them** - the same
   inventory walk as phase one, but now every matching item gets
   `separate_obj()` → `obj_from_char()` → `extract_obj()`'d out of existence.
   Reusable tools (the toolkit, the oven) are checked again for their presence
   flag but never extracted - they're equipment, not consumables. Some
   materials are capped mid-loop (e.g. superconductors: `power < 2` caps
   consumption at two, no matter how many are carried).
3. **Rolls a second, harsher skill check** -
   `number_percent() > schance * 2` (note: this is a *failure* condition,
   inverted from phase one) combined with a re-verification that every required
   material was actually found this time. Failing here destroys nothing extra
   (materials are already consumed) but produces no item - a "the parts existed
   but you still botched it" failure state, distinct from phase one's "you
   never had the parts" failure.
4. **Creates the item** via `create_object(pObjIndex, level)` off a *template
   vnum baked into the crafting function itself* (`vnum = 10420` for blasters) -
   the crafted item is always a clone of one specific prototype object, not
   built field-by-field from raw values. `level` here is the character's own
   skill level in the relevant ability, which then drives the item's own
   `obj->level` and its computed stat values.
5. **Sets the object's fields programmatically** - and this is where §7 and §8
   connect directly: the crafted blaster's `value[0..5]` are set exactly per
   the weapon layout in §7.1 (condition, min/max damage as a flat range rather
   than dice, weapon type constant, current/max ammo), and two `AFFECT_DATA`
   structs (hitroll and damroll bonuses, scaled off character level) are built
   and linked in exactly the same way `oset affect` builds one (§8.1) - crafting
   is, under the hood, just a script doing `oset`-equivalent operations on a
   freshly cloned object instead of an admin typing them by hand.
6. **A subclass can adjust the final numbers too** - Weaponsmith gives a flat
   10% bump to both ends of the damage range (`value[1] *= 1.1; value[2] *= 1.1`)
   on top of everything else.

### 13.3 What this means for adding a new craftable item

To add a new `do_make*` command:
- Pick (or create, §5) a template object at a fixed vnum for `create_object`
  to clone.
- Decide the tool/consumable `item_type`s it needs, matching existing types
  where sensible (so a builder stocking a shop for it can reuse existing raw
  material objects) or creating new ones (§5, §7) where not.
- Follow the two-phase substate/timer shape exactly - this isn't optional
  scaffolding, it's what makes the craft interruptible and prevents duplicating
  the item if the timer fires twice.
- Set the resulting object's values and affects using the same slot layout
  documented in §7 for whatever `item_type` you're producing - a crafted potion
  would use the `slevel`/`spell1-3` slots (§7.4), a crafted piece of armor the
  `condition`/`ac` slots (§7.2), and so on.

---

## 14. Clans In Depth

Clans are entirely admin-created - there's no live player-facing "found your
own clan" command (`do_newclan` is explicitly stubbed: *"This command is being
recycled to conserve thought."*). Everything below is either an immortal
command or a leader/officer-only mortal command gated on `ch->pcdata->clan`.

### 14.1 Creating a clan

```
makeclan <clan name>
```
Refuses if a clan by that name already exists (case-sensitive-ish match via
`get_clan`). Allocates a `CLAN_DATA` with empty leader/officer/description
strings - there's no starting membership, treasury, or ship count; all of that
gets configured afterward via `setclan`.

### 14.2 setclan - the full field list

```
setclan <clan> <field> <value>
```
```
leader number1 number2 subclan
members board recall storage
funds trooper1 trooper2 jail
guard1 guard2 patrol1 patrol2
```
plus, restricted to `LEVEL_SUB_IMPLEM`+: `name filename desc`.

- `leader` / `number1` / `number2` - player names for the top three ranks
  (leader, first officer, second officer). Only the leader (or an immortal) can
  reassign these via the mortal-facing `appoint`/`demote` commands (§14.3) -
  `setclan` itself is the admin-level equivalent, no rank-of-the-caller check.
- `subclan` - links this clan as a subclan of another (`clan->mainclan`), for
  faction hierarchies.
- `members` / `board` / `recall` / `storage` / `jail` / `guard1`/`guard2` /
  `patrol1`/`patrol2` - vnums wiring the clan to physical rooms/mobs (member
  roster room, clan bulletin board, clan-specific recall point, clan storage,
  a jail room for captured enemies, guard mobs, patrol routes).
- `funds` - the clan's shared treasury (a `long`), fed by the mortal-facing
  `clandonate`/`clanwithdraw` commands (§14.3) and separately by clan-owned
  ship sales (`clansellship`).
- `trooper1` / `trooper2` - likely mob vnums for clan-affiliated troop types
  (guard reinforcements); check `clans.c` directly before relying on exact
  semantics if you haven't used these fields before - they're set the same way
  as the room-vnum fields but I haven't traced their consumers as closely as
  the others in this table.

### 14.3 Mortal-facing clan commands

- `clanrank <player> <rank>` (or `clanrank <player> none` to clear) - restricted
  to the clan's leader or first officer (`number1`), or any immortal. Writes to
  `victim->pcdata->clan_rank` and updates the clan's roster entry.
- `appoint <name> <first|second>` - leader-only, fills `number1`/`number2`,
  refuses if that slot is already occupied (must `demote` first).
- `clandonate <amount>` - must be standing in a `ROOM_BANK` room; adds straight
  to `clan->funds`, deducts from the player's own gold.
- `clanbuyship` / `clansellship` - clan-funded ship purchases, incrementing
  `clan->spacecraft` (for actual starships, `ship_class <= SHIP_PLATFORM`) or
  `clan->vehicles` (for ground/atmospheric classes) as a fleet-size counter
  distinct from the ship roster itself.

### 14.4 Where clans intersect other systems

A clan can `governed_by` a planet (§16.1 - set from the *planet's* side via
`setplanet <planet> governed_by <clan>`, not from the clan's side), and clan
membership gates a few unrelated systems elsewhere in the codebase - e.g.
Hunters Guild members are explicitly blocked from posting bounties (§15.1),
checked by clan name string comparison rather than any dedicated flag.

---

## 15. Bounty Hunting In Depth

A small, self-contained system (`bounty.c`, ~300 lines) built around a single
persistent list of player-targeted bounties, independent of any specific room
or mob.

### 15.1 Posting a bounty

```
addbounty <target player> <amount>
```

Hard requirements, all checked in order:
- Must be physically standing in room vnum **6604** (the Hunters Guild on
  Tatooine) - hardcoded, not configurable via any field.
- Members of the clan literally named "the hunters guild" are blocked outright
  ("Your job is to collect bounties not post them") - checked by exact clan
  name string match.
- `<amount>` must be at least 5000 credits.
- Target must be an online, non-NPC player (`get_char_world`, then an
  `IS_NPC` check) - **bounties cannot be placed on mobiles**, only on players,
  and the target must currently be logged in to be found at all.

Bounties **stack** - posting a second bounty on an already-targeted player adds
to the existing total (`bounty->amount += amount`) rather than replacing it or
requiring a fresh entry, and the whole event is broadcast game-wide via
`echo_to_all`.

### 15.2 Data model

Bounties are stored as a flat linked list (`first_disintigration` /
`last_disintigration`) keyed purely by the **target's name as a string** - not
a pfile pointer, not a vnum, just the name. This means a bounty persists across
that player's login sessions (saved to `system/disintigration.lst` via
`save_disintigrations()`) and doesn't care whether the target is currently
online when the bounty is posted or paid out.

### 15.3 Payout

`do_bounties` (no argument) lists every active bounty and its amount - this is
also what `addbounty` falls back to when called with no arguments at all, so
it doubles as both the "list" and "add" command depending on whether you gave
it arguments.

The actual payout function, `disintigration(ch, victim, amount)`, is invoked
from `fight.c`'s death-handling path via `claim_disintigration()` when a bounty
target is killed - not from within `bounty.c` itself. If you're trying to
change *how* or *when* a bounty gets paid out (rather than how it's posted),
`claim_disintigration` in `fight.c` is where to look, not `bounty.c`.

---

## 16. Planets & Starsystems In Depth

Planets and starsystems are two separate data structures (`PLANET_DATA` /
`SPACE_DATA`) that get explicitly linked together, and planets are also where
the ship-jump destination list (§2.1's `calculate`) and the cargo economy
(mentioned in passing in earlier session notes) both actually live.

### 16.1 Creating and configuring a planet

```
makeplanet <planet name>
setplanet <planet> <field> [value]
```
Fields:
```
base_value flags
name filename starsystem governed_by
resource produces consumes
```
- `starsystem` - attaches the planet to a starsystem by name
  (`starsystem_from_name`), linking it into that system's own planet list
  (`first_planet`/`last_planet`, via `next_in_system`/`prev_in_system`). A
  planet with no starsystem set exists as a data object but isn't reachable
  through normal space navigation.
- `governed_by` - assigns a clan (by name, via `get_clan`) as the planet's
  governing faction - this is the *planet-side* half of the clan/planet
  relationship (§14.4); there's no equivalent field set from the clan's own
  record.
- `base_value` - a flat economic baseline for the planet.
- `flags` - currently just `nocapture` (`PLANET_NOCAPTURE`), exempting a planet
  from the area-capturing mechanic mentioned in `bounty.c`'s own file header
  comment ("Bounty Hunter Module (and area capturing as well)").
- `resource` / `produces` / `consumes` - each takes a cargo type name (resolved
  via `get_cargo_num`, falling back to a raw number) plus an amount, writing
  into a per-cargo-type array on the planet (`planet->resource[]`,
  `->produces[]`, `->consumes[]`). Setting `resource` or `consumes` triggers
  `update_cargo_costs(planet)` immediately - this is the planetary economy
  system that presumably drives buy/sell pricing for the cargo/trading system
  elsewhere in `space.c`; `produces` alone does not recompute costs, only the
  other two do, which is a genuine asymmetry worth knowing about rather than
  assuming all three behave identically.

### 16.2 Creating and configuring a starsystem

```
makestarsystem <name>
setstarsystem <starsystem> <field> [value]
```
A starsystem is the object that ship jumps (`calculate`/`plot`, §2.1) actually
target - it's what shows up in the destination list `calculate` prints, and
what a ship's `ship->starsystem` pointer refers to while docked in realspace.
Planets attach to a starsystem (§16.1) but a starsystem can exist with zero
planets - it's still a valid jump destination on its own.

### 16.3 How this ties back to ships and simulators

The "Simulator" starsystem referenced throughout the ship-simulator work
(§9's mobprog examples aside, and the earlier `do_calculate`/`do_plot` simulator
guard) is just an ordinary `SPACE_DATA` entry created the same way any other
starsystem is - it isn't a special hardcoded case in the starsystem system
itself, only in the ship-jump commands that specifically check for it by name
to decide whether to reveal the real starmap.

---

## 17. Help File Authoring (hedit / hset)

Everything documented so far that touches `help.are` (§6, §11.3) was written by
directly editing the area file's text. There's also a live in-game path, and
it behaves differently enough from what you might expect that it's worth
covering explicitly.

### 17.1 hedit - create or open a help entry in the line editor

```
hedit <keyword>
```

If `<keyword>` doesn't already exist as a help topic, `hedit` **creates it on
the spot** - there's no separate "new help entry" command. It then drops you
straight into the line editor (`start_editing`) on that entry's body text; when
you finish editing normally, `stop_editing` writes your buffer back into the
in-memory `HELP_DATA->text` field.

**The new entry's level defaults to your own current trust level** unless you
prefix the keyword with a number:
```
hedit 0 mynewcommand      Create/open at level 0 (everyone)
hedit 50 gmroom           Create/open at level 50
hedit mynewcommand        Create/open at whatever level you currently are
```
This means an immortal casually testing `hedit somehelp` without a level prefix
creates that entry gated to their own (typically very high) level by default -
worth remembering if a help entry you just wrote isn't showing up for a mortal
tester.

### 17.2 hset - everything else about an existing entry

```
hset <field> [value] [help page]
```
Fields: `level keyword remove save`.

- `level <n> <keyword>` - change an existing entry's minimum level.
- `keyword <new keyword> <old keyword>` - rename a topic.
- `remove <keyword>` - delete an entry outright.
- **`save`** - writes the entire in-memory help table back out to
  `help.are` on disk, first renaming the existing file to `help.are.bak`.

**This is the detail most likely to bite someone new to the live-editing
path: `hedit` only edits the in-memory copy. Nothing is written to disk until
you separately run `hset save`.** A crash or reboot before that point loses
whatever was edited live - this is exactly why every help-file change made
directly in this project's sessions has gone through hand-editing `help.are`
followed by a boot-test instead: it's more auditable and doesn't depend on
remembering the separate save step. Both paths are legitimate, but if you or
anyone else uses `hedit` live in-game, **`hset save` immediately afterward is
not optional.**

---

## 18. Object and Room Progs (opedit / rpedit)

§9 covers mobprogs in full depth. Object progs and room progs share the same
underlying interpreter (`mud_prog.c` - if-checks, control flow, variable
substitution, all identical) but trigger on different events and are edited
with their own commands, since an object or a room needs a different attach
point than a mobile does.

### 18.1 Syntax

```
opedit <object> <command> [number] <program> <value>
rpedit <command> [number] <program> <value>
```
(`rpedit`, like `redit`, operates on the room you're currently standing in -
no target argument.) Both share `mpedit`'s command vocabulary: `add delete
insert edit list`.

### 18.2 Object trigger types - a different set than mobprogs

```
act speech rand wear remove sac zap get
drop damage repair greet exa use
pull push   (for levers, pullchains, buttons)
```
These map to things happening *to* or *with* an object rather than to a
mobile: `wear`/`remove` fire when the object is worn/removed, `sac` on
sacrifice, `zap` when hit by a zap-type effect, `get`/`drop` on pickup/drop,
`use` for a generic use-trigger, and `pull`/`push` specifically for
lever/button/pullchain-type objects (tying back to the `ITEM_LEVER`/
`ITEM_BUTTON`/`ITEM_SWITCH` value system in §7.10 - a switch's `tflags` value
decides what it's wired to *do* mechanically, while a `pull`/`push` prog on
that same object is how you'd script custom narrative/side-effect behavior on
top of that).

### 18.3 Room prog trigger types

Rooms get their own smaller set (check `rpedit`'s own syntax listing directly
if you need the exact current list - it wasn't captured in as much depth here
as the mob and object lists above, but the editing shape is identical to both).

### 18.4 Practical note

Since all three prog systems (mob/object/room) share the same interpreter, an
if-check or mpcommand documented in §9.4/§9.6 works identically inside an
object or room prog too - the only things that differ across the three are
*which trigger keywords exist* (§9.2 vs §18.2) and *what `$n`/`$t`/`$i`
resolve to* in context (e.g. `$i` inside an object prog refers to the object
itself, not a mobile).

---

## 19. Socials (sedit)

Worth documenting precisely because it's easy to mis-guess from the name alone
- `sedit` is the **social command editor** (things like `smile`, `wave`, `bow`),
entirely unrelated to shops (which are `makeshop`/`shopset`, §6) or ships
(`setship`, §10). If you're looking for a shop-editing command called
something like "sedit," it doesn't exist - `shopset` is the actual name.

### 19.1 Syntax

```
sedit <social> [field]
sedit <social> create
sedit <social> delete      (LEVEL_GOD+)
sedit save                 (LEVEL_LESSER+)
```
Fields:
```
cnoarg onoarg cfound ofound vfound cauto oauto
```
- `cnoarg` - message shown to the **c**haracter when the social is used with
  **no arg**ument (target).
- `onoarg` - message shown to **o**thers in the room, no-argument case.
- `cfound` / `ofound` / `vfound` - character's message / others' message /
  **v**ictim's own message, when the social is used *with* a target and that
  target is found.
- `cauto` / `oauto` - character's / others' message when the social's target
  resolves to the character themself (e.g. `smile self`).

`sedit <social> create` seeds all of these with sensible defaults (e.g.
`char_no_arg` auto-filled to `"You <social>."`) that you then overwrite field
by field. `sedit save` writes the whole social table to disk - the same
separate-save-step pattern as `hset save` (§17.2), and worth the same caution:
edits made via `sedit` aren't persisted until you explicitly save.

---

## Closing note

This manual reflects the codebase as directly inspected in source, not the
original SWR/FUSS documentation in `doc/`. Where the two disagree, trust this
manual and the source.

Sections most likely to drift as the codebase evolves: the mortal command
category table (§2, intentionally non-exhaustive), the level-gate table (§3, if
command levels get retuned), the `a_types`/value-slot tables in §7-8 if new item
types or apply types get added later, the §10.3 setship cap table if class
balance ever gets retuned, and the §11 subclass enum/listing if new subclasses
get added after this was written (§11's enum snapshot will be stale the moment
another subclass is appended, by design - the pattern it documents stays valid
even when the specific list doesn't). Two smaller notes on the newer sections:
§14's `trooper1`/`trooper2` clan fields are documented from their setter syntax
only, not their consumers - verify against `clans.c` directly before depending
on exact behavior there; and §18.3's room prog trigger list is intentionally
left thin (check `rpedit`'s own in-game syntax listing for the current set)
rather than guessed at. Everything else in §12-19 was traced against the actual
function bodies, not inferred. Those are the ones worth re-verifying against
source directly if something here stops matching what you see in-game.
