# Episode 7: DEEP SPACE

*A misdialled beam to an alien world.* Seven levels (43 to 49) set in deep
space and on the hive planet **Vurr**, far from Station Zero's tidy orbital
base: open space, a living planet, aliens everywhere. Every level has one
twist that no other level of Gunrunners uses, its own alien prototype weapon,
two or three alien residents, and its own music. Level 49 is a boss.

Design by the "Space world" thread (2026-10-10), from Paul's ask: "a new world
in space, similar to the ones we have, with gimmicks, but completely
different gimmicks, unique overall, with alien monsters", plus "or a space
ship in space".

## How it fits the game

- **Its own episode, playable any time.** The title menu gets a DEEP SPACE
  entry that starts Episode 7 at level 43 (its levels show up in LEVEL
  SELECT once reached). The main campaign still ends at level 42, so the
  space episode never gets in the way of the 42-level story.
- **Story (told straight, no show reveal):** after a job, the exit teleporter
  locks onto a stray signal and the runners rematerialise in the cockpit of
  an old courier ship drifting toward an unknown planet. MAX's voice crackles
  over the radio: a stolen alien relic, the **Star Seed**, is on that planet,
  and the Hive Mother wants it back first. One briefing per level (the usual
  10-20 second clip), and an episode ending where the runners fly home with
  the Seed. Nothing in it touches the MaxTV twist, so it can be played before
  or after level 21.
- **Look:** a new tile family for the planet: organic, glossy, bioluminescent
  (teal, violet, acid green), drawn in Cairo like everything else. Space
  levels use a starfield, a ringed gas giant and Vurr's two moons in the
  backdrop.
- **Rules kept:** the medium contract (climbs of 3 blocks, gaps of 4,
  checkpoints every 60-90 s, telegraphs of 8+ frames), one prototype per level
  in green boxes, a Turbo box, one avoidable Virus source (alien spores, green
  as always), a rubber duck (in a space helmet) and a 42 in every level, G-U-N
  letters, two secrets. All three runners must clear every level with the bot
  before it is merged.
- **Bonus levels:** none in the first pass; each level keeps a spot for one
  if Paul wants them later.

## Twists at a glance

None of these is used by levels 1-42 or their bonus levels (checked against
LEVELS.md and SPEC.md: no wall-jumping, no transport tubes, no shot-triggered
swaps, no ziplines, no rideable creatures; the ship is the vehicles thread's).

| # | Level | Look | Twist | Prototype | Aliens | Diff. |
|---|---|---|---|---|---|---|
| 43 | Starfall | The ship over Vurr, an asteroid field | **Flying the ship** | (the ship's guns) | Void Ray, Rock Leech | 4.6 |
| 44 | Crash Garden | Glowing fungus forest at twilight | **Goo Walls** | Goo Gun | Skitter, Spitpod, Gloop | 4.8 |
| 45 | Hive Gullets | Inside the hive, pink and wet | **Gullet Tubes** | Bile Blaster | Hive Mite, Polyp, Drone Warden | 5.0 |
| 46 | Crystal Drift | Floating crystal islands, violet sky | **Swap Crystals** | Swap Rifle | Blinker, Shard Golem, Prism Bat | 5.2 |
| 47 | Silk Canyon | A canyon strung with giant webs | **Silk Lines** | Silk Shooter | Loom Spider, Cocoon Pod, Dropling | 5.2 |
| 48 | Bounder Plains | Thorny grass plains under two moons | **Bounders** | Tamer's Whip | Bounder (ridden), Thorn Hog, Sky Gulper | 5.4 |
| 49 | The Hive Mother (boss) | The hive's heart, a vast egg chamber | **The Hive Remembers** | Star Seed | Egg Guard, Spore Nurse | 5.6 |

## 43 · Starfall

- **Look:** black space, a ringed gas giant, Vurr below (violet, cloud
  bands), drifting rocks lit from one side.
- **Shape:** a horizontal flight, about 260 x 30 blocks, left to right, then
  down through Vurr's clouds to the landing pad. Three checkpoints (beacons
  floating in space).
- **Twist: flying the ship.** You start in the cockpit of the courier ship
  and fly it the whole level: eight ways, momentum, its own guns. The ship
  itself is being built by the "Rideable vehicles" thread; this level uses
  their `@` entity as it lands on main. Twist on top: **drifting asteroids**
  that shots push along (big rocks split in two), so you clear your own lane.
- **Route:** 1. open space, a few slow rocks to learn the ship; 2. the belt,
  Rock Leeches on the rocks; 3. Void Rays sweep across in formations;
  4. into Vurr's clouds (the backdrop turns violet) and onto the landing pad,
  where you climb out. The exit is the hatch to the surface.
- **Aliens:** **Void Ray** (a glowing manta: sweeps across in a wave,
  tell: its fins light up before the dive); **Rock Leech** (clings to an
  asteroid and spits a slow glob at you when you pass).
- **Secrets:** a hollow asteroid with gems; a derelict probe (the duck in its
  window). **42:** the probe's serial number.
- **Virus:** a green comet you can fly around.
- **Build note:** waits for the ship vehicle on main. Built after level 44.

## 44 · Crash Garden

- **Look:** a twilight forest of giant glowing mushrooms and ferns, Vurr's two
  moons, pools of teal light. Music: dreamy synth arpeggios over hand drums.
- **Shape:** mostly vertical: down from the crash site, across the garden and
  up a series of goo chimneys, about 180 x 50 blocks. Four checkpoints.
- **Twist: Goo Walls.** Glowing green goo coats some walls. Jump into goo
  and you stick to it, sliding slowly down; press jump to kick off it, up and
  away from the wall. Kick between two goo walls to climb a chimney, or kick
  off one wall and steer back to it to climb a single wall. Dry walls are
  just walls. Gloops leave goo where they splat.
- **Prototype: Goo Gun.** Its blobs splat on walls as goo patches that last
  10 seconds, so you can climb any wall; they also glue an alien's feet for
  3 seconds.
- **Route:** 1. the crash site: one short goo wall over a safe pit;
  2. goo chimneys, then Skitters running up the goo; 3. the Goo Gun: make your
  own patches up a dry cliff to a shortcut; 4. the garden canopy to the exit.
- **Aliens:** **Skitter** (a six-legged scuttler that runs along floors,
  walls and goo; tell: a click before it leaps at you); **Spitpod** (a plant
  that lobs acid in an arc; its bulb swells first); **Gloop** (a hopping blob
  that splits in two when shot, and leaves goo where it lands).
- **Secrets:** a goo pillar hanging from the garden's ceiling leads up into
  a hidden pocket (the rubber duck); a dry cliff over the shaft has a shelf on
  top that only Goo Gun patches reach (a gem cache and the 42).
- **Virus:** spores off a green mushroom cap above the garden path.
- **Runners:** a kick is the runner's own jump, so Nova climbs a goo wall in
  fewer kicks than Rocco.
- **Built** (2026-10-10): `tools/levels/44_crash_garden.py`, about 180 x 50
  blocks, four checkpoints; the bot clears it with all three runners.

## 45 · Hive Gullets

- **Look:** inside the hive: pink, ribbed, wet walls, veins pulsing on the
  beat of a heartbeat. Music: a slow, pulsing bassline with choir pads.
- **Shape:** a hub with three wings joined by tubes, about 160 x 60 blocks.
- **Twist: Gullet Tubes.** Translucent organic tubes run through the hive.
  Step into a mouth (a puckered ring) and the hive swallows you: you shoot
  along the tube, visible through its wall, and are spat out at the other
  end. Some tubes split at a **valve**: shoot it to choose the branch. Some
  mouths only open when the hive breathes in (a slow, shown rhythm).
- **Prototype: Bile Blaster.** Its shots travel through tubes too, and come
  out the far end, so you can hit aliens in the next room before you go.
- **Aliens:** **Hive Mite** (small, fast, comes in threes from wall pores);
  **Polyp** (a wall mouth that inhales: pulls you toward it and nips);
  **Drone Warden** (an armoured flyer that patrols a wing and calls Mites).
- **Secrets:** a tube that only one valve setting reaches; a pore you can
  crawl into. **Virus:** a green, dripping pore.
- **Built** (2026-10-10): `tools/levels/45_hive_gullets.py`, 160 x 58
  blocks: the throat (start), the tube up into the hub, the hub's floor mouth
  (its valve sits in a niche in the hub's east wall) down to the east wing,
  and the breathing mouth up to the crown (the exit). The secret pore is low
  in the throat's west wall. Polyps sit on step faces, so their pull drags
  you against the step you have to jump. The bot clears it with all six
  runners.

## 46 · Crystal Drift

- **Look:** floating islands of violet and teal crystal in a pale sky, slow
  drifting shards. Music: glassy bells and a soft waltz.
- **Shape:** horizontal, island to island, about 220 x 40 blocks.
- **Twist: Swap Crystals.** Hovering crystals ring when shot: you and the
  crystal trade places in a flash. Gaps too wide to jump are crossed by
  shooting a crystal on the far side; the crystal ends up where you stood (so
  the way back needs another). Some crystals drift on a path, so timing the
  shot matters; others sit high up, so you shoot up at them.
- **Prototype: Swap Rifle.** Its shots swap you with aliens too (a Prism Bat
  over a gap is a way across) and bounce once off walls.
- **Aliens:** **Blinker** (blinks to a spot near you, then lunges; tell: a
  shimmer where it will appear); **Shard Golem** (slow, armoured, only hurt
  in its glowing back); **Prism Bat** (flutters in a figure eight, splits a
  shot into three).
- **Secrets:** a crystal you can only see in a reflection; an island above
  the clouds. **Virus:** a green, cracked crystal (swap with it and you catch
  it).

## 47 · Silk Canyon

- **Look:** a deep amber canyon strung with giant silver webs and cocoons.
  Music: twangy plucked strings and a creeping bass.
- **Shape:** a descent, top to bottom, about 120 x 80 blocks.
- **Twist: Silk Lines.** Taut silk lines run diagonally across the canyon.
  Jump into one and you hang from it and slide down it, faster and faster;
  jump off any time (keeping your speed as a long jump). Lines going up can't
  be ridden; a Loom Spider spins new ones as you watch.
- **Prototype: Silk Shooter.** Fire diagonally up: where it hits rock within
  16 blocks, a new silk line runs from there down to you.
- **Aliens:** **Loom Spider** (walks along lines, cuts the one you are on
  after a tell); **Cocoon Pod** (hangs on a web and drops a Dropling when you
  pass under); **Dropling** (small, falls on a thread, bites, climbs back up).
- **Secrets:** a cocoon full of gems; the spider's lair. **Virus:** a green
  cocoon.

## 48 · Bounder Plains

- **Look:** open plains of thorny violet grass under two moons, rock arches,
  a herd of Bounders. Music: a galloping rhythm, fiddle-like synth.
- **Shape:** horizontal and wide, about 240 x 36 blocks.
- **Twist: Bounders.** Bounders are big, friendly, flea-like aliens. Jump on
  one's back to ride it: it jumps twice as high, stomps aliens flat and runs
  over thorns that hurt on foot. It won't fit through low tunnels and won't
  climb ladders; down + jump gets off. A lost Bounder trots back to its pen.
- **Prototype: Tamer's Whip.** A short crack that stuns aliens, and from 6
  blocks away calls a Bounder to you.
- **Aliens:** **Thorn Hog** (charges along the ground; Bounders jump it);
  **Sky Gulper** (a floating mouth that swallows you and spits you back a few
  blocks; it can't swallow a runner on a Bounder).
- **Secrets:** a Bounder-only high ledge; a tunnel only reachable on foot.
  **Virus:** a green thornbush.

## 49 · The Hive Mother (boss)

- **Look:** the hive's heart: a vast egg chamber, glowing eggs, the Hive
  Mother on her throne of resin. Music: choir and pounding drums.
- **Shape:** a short approach (about 80 x 30 blocks) and a round arena.
- **Twist: The Hive Remembers.** The fight brings back the episode's twists
  one per phase: goo walls to reach her weak spots, gullet tubes that move
  you around the arena, swap crystals to dodge her charge.
- **Boss: the Hive Mother.** Three phases, under three minutes: 1. she lays
  eggs that hatch Egg Guards while her crown pulses (shoot the crown from the
  goo walls); 2. she inhales, pulling you in (ride the gullets to her flank
  sacs); 3. she charges across the arena (swap past her with the crystals and
  shoot her back). A Turbo box drops in phase 3, a full health refill waits
  at the arena door.
- **Prototype: Star Seed.** Hold fire to charge a slow, piercing star.
- **Aliens:** **Egg Guard** (hatches from an egg, charges); **Spore Nurse**
  (drifts and heals the Mother unless shot).
- **Ending:** the Star Seed in hand, the runners take off in the ship (the
  vehicles thread's ship, if it is in by then) and the episode-end cutscene
  rolls.

## Build order

1. Episode shell: levels 43-49 in the level table, the DEEP SPACE title
   entry, the planet's tile family and backdrops, the briefing clips.
2. **Level 44 Crash Garden** first (goo walls are new player movement, and
   the bot has to learn wall-jumps).
3. Levels 45, 46, 47, 48, each merged to main once all three runners clear
   it with the bot.
4. Level 43 Starfall once the ship vehicle is on main.
5. Level 49 The Hive Mother and the episode ending.

Screenshots of each level go to /mnt/project-files/gunrunners-poc/space-world/.
