#pragma once

namespace gr
{

// Every sound effect the game can trigger. The audio module synthesizes them
// at startup; the game logic only emits these ids.
enum class Sfx
{
  Jump,
  Land,
  Shot,
  LaserShot,
  RocketShot,
  FlameShot,
  EnemyShot,
  Hit,
  Explosion,
  SmallExplosion,
  BoxBreak,
  Gem,
  Item,
  Health,
  WeaponPickup,
  Letter,
  LettersComplete,
  Hurt,
  Death,
  AttachClimbable,
  Key,
  ForceFieldOff,
  Checkpoint,
  Teleport,
  MenuMove,
  MenuSelect,
  Tally,
  TurboOn,
  VirusOn,
  EffectEnd,
  Siren,     // the tide is about to rise
  Splash,    // into sludge, a gator surfacing
  Bubble,    // a bubble shot, a bubble popping
  Bloop,     // gone down the /dev/null pipe
  Klaxon,    // a Valve Keeper turning its wheel
  Rattle,    // a rat pipe about to burst
  Quack,     // the Duck Rapids duck
  Warn,      // a gantry's warning lights
  Chime,     // the maglev's station chime, the passing train
  Clunk,     // a coupling, a caltrop bay
  Zap,       // the Arc Caster
  Beep,      // the hunter has a lock
  Whistle,   // a gunship rocket coming down
  Scream,    // Black Halo's engine as it lines up a ram
  Rev,       // a Hover Biker revving
  Creak,     // a rope bridge about to go
  Chop,      // a Bridge Cutter's machete, a rope cut
  Hoot,      // a Howler winding up a throw
  Rustle,    // a Canopy Viper about to strike
  Whoosh,    // the Boomerang in the air
  Yell,      // the jungle yell (ten swings on one vine)
  Boing,     // Bounce House: a trampoline landing
  Rumble,    // a rolling stone about to go
  Slice,     // a trap blade's sweep
  Click,     // a pressure plate
  Drum,      // the Hall of Traps' drum egg
  Dart,      // a Dart Face's dart
  Skitter,   // a Scarab Tide on the move
  Bell,      // the mine elevator setting off
  Fuse,      // a Blasting Cap's fuse
  Flap,      // a Bat Cloud's wingbeats
  Dig,       // a Rock Mole breaking out of the rock
  Rail,      // a cart's wheels hitting the track
  Sizzle,    // something hits the lava
  Magma,     // a lava bubble's plop, a Magma Toad leaping out
  Sink,      // a basalt stone grinding down into the lava
  Croak,     // a Magma Toad landing
  Snap,      // a Basalt Crab's claws
  Squelch,   // goo: sticking to a goo wall, a Goo Gun splat, a Gloop splitting
  Chitter,   // a Skitter's click before it leaps
  Spit,      // a Spitpod lobbing acid
  Gulp,      // a Gullet Tube swallowing or spitting out
  Inhale,    // a Polyp breathing in
  Screech,   // a Drone Warden calling its mites
  Swap,      // a Swap Crystal ringing as you trade places with it
  Blink,     // a Blinker's shimmer before it appears
  Zip,       // grabbing a Silk Line and sliding off down it
  Snip,      // a Loom Spider cutting a Silk Line: a twang and a snap
  Crack,     // the Tamer's Whip
  Crash,     // a boulder landing, a chute's flaps slamming
  Hiss,      // a Pit Snake about to rear
  EngineOn,  // climbing into a vehicle
  EngineOff, // climbing out
  Cannon,    // the tank's gun
  Torpedo,   // a submarine's torpedo
  Stomp,     // a mech landing hard
  Crunch,    // something crushed under a tank
  Freeze,    // the Freeze Ray icing something over
  Tink,      // a shot off a block of ice, a frozen block shattering
  Whine,     // a Puck Drone spinning up
  Cheer,     // a faint crowd far away (the goal vent, Air Hockey goals)
  Trim,      // the Hedge Trimmer's blades (a short buzz, repeated while held)
  Puff,      // a Spore Puffer letting go of a cloud
  Squish,    // a Glob landing, splitting
  LampOn,    // a grow lamp buzzing on
  Grow,      // Growth Spurt: one size up (down is the same, played lower)
  Recoil,    // the Recoil Cannon: a deep thump with a whoosh of exhaust
  Thrust,    // an EVA Ram's thruster firing (its ram)
  Spikes,    // a Space Barnacle's shell snapping open, spikes out
  Rivet,     // a rivet working loose and popping out (a metallic ping)
  Hop,       // a Rivet Mite or the runner on a planetoid leaping (a tiny chirp)
  FlagUp,    // a flag planted: a flap of cloth and a little fanfare
  GravFlip,  // a chamber turning over: a rising whoosh with a klaxon blip
  Thud,      // landing after a flip (a soft body thump)
  Vortex,    // the Grav Grenade's vortex opening (a deep swirling hum)
  ProbeShot, // a Gravity Probe's aimed shot (a hollow electronic blip)
  CoreHum,   // the reactor core winding up for a pulse (a rising hum, 3 s)
  CorePulse, // a Core Pulse ring going off (a deep electric boom with a ring)
  BracerUp,  // the Deflector Bracer's shield raised (a quick shimmering hum)
  Deflect,   // a shot bounced off the Bracer, a pulse soaked, a drone's bubble taking a hit
  ValveTurn, // a coolant valve squeaking round
  ValveShut, // a valve shut tight (a heavy clank and a hiss)
  Rewind,    // DO NOT PRESS: the victory fanfare played backwards (6 s)
  Bulkhead,  // a bulkhead sliding (a heavy hydraulic grind ending in a clunk)
  Monitor,   // the monitors switching to the next schematic (a CRT blip)
  PhaseShot, // the Phase Rifle (a thin rising zip that goes hollow through a wall)
  Glint,     // a fake wall or hidden pocket glinting (a tiny chime)
  Rebuild,   // a Repair Swarm finishing a rebuild (a rising ratchet and a ping)
  EchoIn,    // an Echo stepping out of its pad (a reversed shimmer)
  EyeCharge, // ZERO's eye charging (a rising whine, 1.5 s)
  EyeBeam,   // its beam on the grille (a crackling buzz)
  Shutter,   // its shutter closing or opening (a camera-iris clack)
  WallFall,  // the painted flat tipping over (a creak and a huge flat slap)
  LightClunk, // a bank of studio lights switching on
  Applause,  // the studio audience cheering
  FuseLit,   // a fuse catching (a match strike and a fizz)
  BarrelHiss, // a TNT barrel about to blow (a sharp hiss, 12 frames)
  Ricochet,  // a Six-Shooter bullet off metal (a whining ping)
  Reload,    // the Six-Shooter's cylinder spinning (a ratchet and a click)
  Piano1,    // the saloon piano's four keys (honky-tonk, rising)
  Piano2,
  Piano3,
  Piano4,
  HonkyTonk, // the first bar of the theme on the saloon piano
  PosterSpin, // a wanted poster spinning on its nail
  Gust,      // a gust down the mine tunnel
  DuelDraw,  // a Duelist drawing (a holster slap and a hammer click)
  MirrorFlip, // stepping through a mirror (a glassy shimmer, inverted)
  Lever,     // a heavy iron lever thrown (a creak and a clunk)
  GhostHiss, // a Portrait Ghost leaving its frame or lunging (a cold hiss)
  ArmorClank, // a Haunted Armor waking (a rattle of plate)
  Swoosh,    // a halberd swing
  Throw,     // a Poltergeist throwing something
  BoltThunk, // a Silver Crossbow bolt pinning something
  GlassChime, // the broken mirror shot (a cracked chime)
  CoffinSlam, // the coffin lid lifting and slamming
  Thunder,   // lightning over the manor
  BellDing,  // a chef's bell rung (Level 24's belts flip after it)
  Sneeze,    // an enemy sneezing from the Pepper Grinder
  EggRing,   // an Egg Timer ringing out its last frames
  GrinderWhir, // the Pepper Grinder spinning up
  Toaster,   // the toaster popping
  CleaverThunk, // a cleaver sticking in a wall
  Mist,      // the Chrono Mister's spray (a soft, airy hiss)
  AntAlarm,  // a Soldier Ant column alarmed (a rattle of chittering clicks)
  BeetleJet, // a Bombardier Beetle firing (a boiling pop and a scalding hiss)
  Narrator,  // a narrator's subtitle (breathy blips around 200 Hz)
  Roar,      // Gorgazilla's roar (a growl through a megaphone, rising and cracking)
  BuildingCrumble, // a model building collapsing (a balsa crunch and a rattle of bits)
  BreathBeam, // the Atomic Breath (a rising crackling electric whoosh)
  MaserHum,  // a Maser dish charging (a pulsing hum climbing in pitch)
  SlideWhistle, // Level 28: a cartoon slide whistle falling (the "uh-oh" drop, Buddy into the ink)
  Bonk,      // running into painted rock (a wood block knock with a wobbling boing)
  HoleGulp,  // a Portable Hole opening (a cartoon gulp: a hollow bloop swallowed down)
  PianoCrash, // a piano landing (a smash of wood with every string jangling)
  PieSplat,  // a cream pie in the face (a wet splat and a soft plop)
  PencilScratch, // the giant pencil drawing (a dry graphite scratch)
  EraserRub, // the eraser rubbing a floor out (a squeaking rubber scrub)
  Honk,      // Buddy Boom hit (a squeeze-bulb horn honk)
  RubberBoing, // a flat Rubber Goon bouncing a runner (a long twanging boing)
  DialTurn,  // Level 29: an RGB dial clicking round, a signal change on its way (a knob notch, a blip of static, two rising notes)
  ChannelFlip, // the change landing, every bar swapping (a CRT's thump with a crackle)
  TintTurn,  // the Tri-Tone turning its tint (a selector's detent and a pure blip)
  SnowHiss,  // a Static Swarm about to latch on (TV snow swelling up)
  SnowLatch, // a Static Swarm latching on (a burst of static clamping down)
  TestTone,  // the Test Card's tell (the 1 kHz test tone)
  CardBeam,  // the Test Card's beam (a hard bright buzz falling away)
  TuneIn,    // tuned in: the static narrowing into a clean tone that resolves into a chord
  SwimStroke, // Level 30: a swim stroke (a soft underwater whoosh and a burble of bubbles)
  AbyssSplash, // spat back out of the abyss (a deep muffled boom, bubbles rushing up)
  HarpoonPlant, // a harpoon planted in the floor (a dull thud and a metal ring, muffled)
  HarpoonStick, // a harpoon sticking in a wall or an anchor ring (a sharp thunk and a clang)
  LineTaut,  // the harpoon line pulled taut (a rope creak and a low twang)
  EelSpark,  // a Cable Eel's box sparking (crackling arcs and a rising hum)
  EelSnap,   // a Cable Eel lunging out (a zap and a wet snap of jaws)
  AnglerLunge, // an Anglerfish lunging (a rushing gulp and a clap of teeth)
  JellyPulse, // a Box Jelly contracting before a rise (a soft glassy pulse)
  BubblePop, // the Bubble Up bubble popping (a bright pop and a spray)
  RingPass,  // a Bubble Up ring passed (a rising bubbly chime)
  StormStrike, // Level 31: lightning striking close (a tearing crack, a boom and a rolling rumble)
  StormGlow, // metal glowing before a strike (a buzz of static climbing, crackling harder)
  RodThunk,  // a Tesla rod sticking in rock or a machine (a hard thunk, a copper ring, a zing)
  RodArc,    // a struck rod arcing to the enemies round it (a sharp zap and a sizzle)
  MachinePower, // a dead machine powered by a strike (a relay's clack, a motor spinning up to a hum)
  GullCry,   // a Storm Gull's cry before its dive (a harsh falling kee-yow)
  GullDive,  // a Storm Gull diving (wind rushing past folded wings)
  KiteWhine, // a Kite Drone's rotors climbing in pitch as it turns at you
  FogWheeze, // a Foghorn's bellows filling (a reedy wheezing inhale)
  FogBlast,  // a Foghorn's blast (a deep brassy diaphone groan)
  BungeeStretch, // Bungee's rope stretching (a rubbery creak)
  BungeeTwang, // let go: flung on the rope (a deep elastic twang)
  StormRing, // a storm ring taken (a crackle and a bright electric chime)
  DishGroan, // Level 32: a big dish about to turn (a deep steel groan, creaking bearings)
  DishTurn,  // a dish turning (its drive motors spinning up, gears ticking)
  JamBurst,  // the Signal Jammer firing (a warbling burst of radio noise)
  RobotJammed, // a robot jammed (garbled bleeps tumbling down, a fizz)
  SandBulge, // a Sand Shark about to breach (a low rumble, the sand hissing up)
  SharkBreach, // a Sand Shark breaching (a spray of sand and a snap of jaws)
  ScrapDrop, // a Vulture Drone letting go of scrap (a falling whistle)
  ScrapClank, // scrap landing (a tinny clank and a rattle)
  MechAim,   // a Scorpion Mech's stinger aiming (a charging whine climbing)
  MechBeam,  // a Scorpion Mech's laser (a searing zap, a hum)
  MiniDishTurn, // a minidish knocked round (a little servo whirr and a click)
  CactusHat, // the cactus's hat popping up (a cork pop and a slide whistle)
  SwapWarn,  // Heatstroke: left and right about to swap (a wobbling two-tone siren)
  EngineDrone, // Level 34: the fleet's engines (a low chugging drone, propellers beating through it)
  GrappleFire, // the Grapple Gun fired (a pneumatic thunk, the line whizzing off its drum)
  GrappleLatch, // the hook catching a ring (a brass clank and the ring's chime)
  RopeCreak,  // a line taking the runner's weight (a hemp creak)
  GrappleYank, // a light enemy yanked in (the line cracking taut, the reel whirring)
  PirateJet,  // a Sky Pirate jetting in (a jet pack's roar sputtering)
  CutlassDraw, // a Sky Pirate's cutlass raised (a steel shing)
  CutlassSlash, // the cutlass swung (a heavy swish, a ring of steel)
  FlintCock,  // a Sky Pirate's pistol aimed (the flintlock's hammer clicking back)
  FlintShot,  // the flintlock fired (a crack and a puff)
  ParrotSquawk, // a Peck Parrot shooed off (an angry rising squawk)
  ParrotDive, // a Peck Parrot diving (a falling whistle and a shriek)
  ParrotHop,  // the carrier hopping (a flap and a chirp)
  BalloonHiss, // a parrot pecking a balloon down (taps and air hissing out)
  MineFuse,   // a Balloon Mine lit (quick ticking beeps over a fizz)
  MinePop,    // a Balloon Mine bursting (a balloon's bang, shrapnel rattling)
  CannonBoom, // Human Cannonball fired (a circus cannon's boom and a whistle up)
  NetBoing,   // landing in a net (a springy boing)
  LooterSink, // the Looter going down (a groaning hull, an engine coughing out)
  BayHatch,   // the bomb bay's hatch opening (a trapdoor's creak and clunk)
  DeckDip,    // a small deck dipping under Rocco (a wooden thump and creak)
  CurtainSwish, // Level 37: a booth's curtain drawn (a velvet swish, the rings clattering on the rail)
  BoothSwap,  // out of a booth as someone else (the curtain whipped open, a brass ta-da)
  PlateClunk, // a heavy plate going down (a deep thump, a brass clank, the latch)
  ShutterRoll, // a shutter rolling up (slats clacking round the drum, a motor whine)
  TableLower, // a cutting table let down (chains clinking through pulleys, a winch ticking)
  TableLand,  // the table down flush (a big wooden thump, things on it jumping)
  SpringBoing, // a spring rack throwing the runner (a big coil's boing, a steel twang)
  SpikeTumble, // into the ceiling needles (a sharp tink, a wobbling whoosh down)
  PushRackScrape, // the archive's rack shoved (casters grinding, one squeaking)
  VaultDoor,  // through the vault door (the wheel's ratchet, a hinge groan, a boom)
  DoorBlocked, // the vault door blocked (two dull knocks)
  DecoyLaunch, // the Decoy Launcher (a pneumatic thoonk, air, a spring's twang)
  DecoyLand,  // a decoy landing upright (wooden limbs clattering)
  DecoyBonk,  // something walloping a decoy (a hollow wooden bonk)
  DecoyPop,   // a decoy giving up (a pop, a poof of sawdust, parts clattering)
  MannequinCreak, // a Mannequin about to move (wooden joints creaking, a crick)
  PufferSqueeze, // a Powder Puffer's tell (a rubber bulb's squeak, air drawn in)
  PufferPuff, // its puff (a soft powdery pfff, a whump of air)
  RackRattle, // a Rack Runner's tell (every hanger rattling)
  RackRoll,   // a Rack Runner setting off (casters rumbling faster, hangers swinging)
  RackRam,    // rammed by a Rack Runner (a chrome crash, a clank, hangers jangling)
  MirrorSit,  // sitting at the bulb mirror (a stool's creak, three powder pats)
  Mustache,   // the mustache on (a twirling slide up, a sparkling ding)
  SuitTouch,  // the Gorgazilla suit grabbed (a rubbery squeak, the zip run up)
  SuitRoar,   // Gorgazilla steps out (a hammy, muffled rubber-monster roar)
  TotemThrow, // Totem: the top runner thrown (a hup whoosh)
  TotemStack, // back on the stack (three quick bops climbing)
  PenStrike,  // Level 38: a red marker struck through a word (a felt swipe, the nib squeaking)
  WordFade,   // the thing a struck word named fading out (an airy whoosh, paper rustling apart)
  WordBack,   // a word written back, its thing back (a pencil flourish, a wooden two-note knock)
  Scribble,   // a Typo scribbling (graphite zigzagging over paper)
  PencilSketch, // a load-bearing word or a pen box sketched back (long pencil strokes)
  TypoSqueak, // a Typo springing up (a rubbery "eek!", a wet blip of ink)
  KeyClack,   // a typewriter key landed on (its click, the type bar slapping the roller)
  KeyWrong,   // the wrong key (type bars jamming: a dull clunk and a rattle)
  CarriageBell, // the sentence typed (the typewriter's bell, the carriage ratcheting back)
  CrusherCreak, // the press about to drop (a steel groan, air hissing)
  CrusherSlam, // the rubber stamp press slamming (a huge thump, a rubber slap, the frame clanking)
  DeadlineTick, // a Deadline's minute hand (a small dry tick)
  DeadlineBell, // a Deadline's alarm (the hammer rattling between two bells)
  BlockScrape, // a Writer's Block about to slide (concrete grinding, grit popping)
  BlockThud,  // a Writer's Block coming to rest (a deep concrete thud, grit crumbling)
  BlockTink,  // a shot off a Writer's Block (a hard ping, a flick of grit)
  CardSpin,   // a pitch card spinning on its pin (paper flapping round, slowing)
  EggCrack,   // the Easter egg hatching (the shell crackling, a pop)
  EggCheep,   // the chick (two quick high cheeps)
  PaddleBoing, // Breakout: the ball off the ruler (a ruler's twang)
  BrickBreak, // a letter brick knocked out (a card crunch, a typed clack)
  BrickCrack, // a red-ink brick cracking (a dry snap)
  BallLost,   // the ball past the ruler (a sad muted horn, two notes down)
  InkPlop,    // a Typo crawling out of its inkwell (a liquid plop, a bubble)
  MonitorHop, // Level 39: through a monitor (a burst of TV snow, the CRT's thunk and whine)
  EchoRepeat, // an Echo Mic shot's echo (the shot again, fainter, a slapback tail)
  FloorBeep,  // a Floor Manager counting down (a walkie's short beep)
  CountOne,   // "one": the whole room lets fly (a clapperboard clack, a volley of cards)
  CableWhip,  // a Cable Wrangler yanking his cable (a rubbery whip crack, the slack slapping)
  TripFall,   // tripped over a cable (a stumble, a body thump, the cable twanging)
  BoomWhoosh, // a Boom Op's pass (a long airy swoosh, the furry mic's rumble)
  PressHiss,  // a studio press about to drop (a hydraulic groan, air hissing)
  PressSlam,  // the press slamming (a heavy steel slam, a clank, a hiss of relief)
  VhsRewind,  // the tape rewinding (a clunk, the spools whirring up, the heads chattering)
  PyroBurst,  // Level 40: a pyro cue (the igniter's tick, a gas whoomp, a roaring column)
  TrussMotor, // a truss flying (a chain motor's clunk, its geared whine, the chain rattling)
  ConfettiBlast, // a confetti cannon (a CO2 thump and hiss, paper fluttering, a far cheer)
  SpotStrike, // a followspot striking up (a heavy switch, the arc's fizz, the mains hum)
  SpotDouse,  // a followspot going out (the dowser clacking shut, the hum sagging)
  SpotLock,   // a followspot locked on (the iris snapping in, two rising pings)
  CannonRoar, // the Q99 cannon (a deep whump, a long roaring jet, sputtering out)
  SandbagThump, // a sandbag landing (a dull heavy thud, grit, the catwalk ringing)
  PyroStream, // the Pyro Rig lighting (the pilot's tick, a gas whoosh into a flutter)
  BreakerSnap, // the Q99 fuse box hit (a crackle of sparks, a relay snapping)
  DiscPing,   // a disc down in Trigger Happy (a bell ping, a wooden clack)
  LaserBeep,  // a Rigger's sight (a quick double beep)
  RiggerShot, // a Rigger's line launcher (a pneumatic thwip, the line zinging out)
  RopeCut,    // a Sandbag Drop's cut (the knife's snick, the rope whipping up the blocks)
  LavaRumble, // Level 33: the lava rising (a deep magma rumble, slow bubbles glugging)
  SteamBurst, // a coolant purge venting (a blast of steam, a hissing roar tailing off)
  PurgeSiren, // the purge running out (a klaxon whooping)
  LavaPop,    // popped out of the lava (a searing sizzle, a fat pop up and away)
  GutterSizzle, // wading in the magma gutter (a short searing sizzle)
  SinkSuck,   // the Heat Sink held (a vacuum whoosh pulling in)
  SinkGulp,   // a shot swallowed by the Heat Sink (a deep glug and a ping)
  SinkThrow,  // the Heat Sink's fireball thrown (a whump and a rolling roar of flame)
  FirewallSputter, // a Firewall about to dip (flames sputtering, a glitchy buzz)
  FirewallRoar, // a Firewall flaring back up (a roaring whoosh)
  DaemonWindup, // a Lava Daemon winding up (a crackling charge rising)
  DaemonSpit, // a Lava Daemon's fireball lobbed (a fwoomp and a hiss)
  DaemonHop,  // a Lava Daemon leaping (a rocky scrape and a sizzle)
  FanWhine,   // a Fan Bot spinning up (a rising turbine whine)
  FanGust,    // a Fan Bot's gust (a rush of wind)
  RackGlitch, // the corrupted rack spinning up (digital chatter over a sick whine)
  RebootChime, // the DO NOT REBOOT rack shot (a power-down whir, then a startup chime)
  RatingsDing, // the RATINGS server shot (a rising arpeggio and a till's ding)
  CraneBell,  // Level 36: a crane's warning bell (an electric bell's clapper ringing fast)
  MagnetClamp, // the magnet down on steel (a heavy clank, its hum taking hold)
  MagnetTap,  // the magnet down on nothing (a hollow clonk)
  MagnetRelease, // a load let go (the hum cut off, the release's clack)
  CraneStop,  // a ride crane at the end of its rail (a buffer's clank and a spring's boing)
  CrateScrape, // a metal crate shoved along the floor (steel scraping on concrete)
  CrateLand,  // a metal crate (or a dropped robot) landing (a steel boom, a rattle)
  CrateBump,  // a sliding crate coming to rest (a dull clunk)
  CrateBack,  // a crate back at its start (a winch's whir and a clunk)
  CrateCrunch, // a crate hitting an enemy (a metal bonk)
  FoamFlump,  // a crate into the foam pit (a soft whump)
  LeverThrow, // a crane lever thrown (a knife switch's clack, a spark's crackle)
  ForkRev,    // a Forklift Bot's tell (its engine revving, the forks whining down)
  ForkCharge, // a Forklift Bot charging (tyres squealing, the engine roaring)
  ForkLift,   // the runner lifted on the forks (hydraulics hissing up, a clank)
  ForkDrop,   // set down off the forks (the hydraulics letting go, a clunk)
  ForkBeep,   // a Forklift Bot turning round (a reversing beeper)
  FlatDown,   // a scenery flat set down (a wooden slap, the stage weight's thud)
  FlatHit,    // a shot into a flat or a wooden crate (a hollow knock)
  FlatBreak,  // a flat torn down (timber splintering, canvas ripping)
  TapeThrow,  // a Stagehand's throw (a whoosh, the tape roll fluttering)
  MimicCreak, // a Prop Mimic's lid opening (a creaking hinge, a wet growl)
  MimicSnap,  // a Prop Mimic's bite (the lid slammed shut on its teeth)
  MagnetWhine, // the Magnet Gun pulling (an electric hum rising, warbling)
  MagnetPulse, // the Magnet Gun's push (a deep thoom and a crackle)
  LiftOverhead, // Throw Everything: something lifted overhead (a scrape and a heave)
  ThrowToss,  // something thrown (a tumbling whoosh)
  ThrowLand,  // it lands as a block (a crate's wooden crash)
  OldTitle,   // the TURBODUDES crate shot open (the old prototype's chiptune title jingle)
  MastWind,   // Level 35: wind round the mast (gusts rising and falling, a whistle in the lattice)
  HatchClank, // through a hatch (a latch thrown, the steel door swinging, a clank)
  ClimberSetOff, // a Mast Climber setting off up a ladder (boots ringing on rungs)
  WrenchRaise, // a Mast Climber's wrench raised (a ratchet's clicks and a steel shing)
  WrenchSwing, // the wrench swung (a heavy swish and a clang)
  RotorWhine, // a Relay Drone coming round the mast (a rotor whine rising)
  DroneCharge, // a Relay Drone's muzzle charging (two quick beeps over a rising whine)
  DroneShot,  // a Relay Drone's shot (a buzzy zap)
  NodeHum,    // a Feedback Node charging (a hum rising, a squeal of feedback creeping in)
  NodePulse,  // a Feedback Node's ring going out (a feedback shriek and a whump of air)
  SwarmVolley, // the Satellite Swarm firing (three quick chirps)
  SwarmBlock, // a swarm drone soaking up a shot (a zap and a power-down)
  AirshipDrone, // the cargo airship arriving (big engines droning, props beating)
  HypeSting,  // a Halcyon holo banner (a bright corporate fanfare sting)
  TitanStomp, // the Halcyon Titan stomping (a huge metal boom, the shockwave rushing)
  TitanLaser, // its laser sweep (a charging hum snapping into a searing beam)
  TitanFlame, // its flamethrower (a whoosh lighting into a roar)
  MissileLock, // missile crosshairs locking on (fast lock-on beeps)
  MissileRain, // the missiles coming down (whistles falling into booms)
  CoreOpen,   // its power core opening (a pneumatic hiss and a hum)
  LatticeShot, // a lattice shot coming round the mast (a ringing metal zing)
  VentHiss,   // a vent panel about to open (a rising hiss)
  TitanTopple, // the Titan toppling off the mast (a long metal groan and a falling whistle)
  BrochureJingle, // the sales brochure found (a cheesy jingle)
  AlienAnthem, // the alien's anthem under the flag (four warbling notes)
  DummyBump,  // Copycat: a dummy bumping the runner (a rubbery boing)
  DummyDrop,  // a dummy dropping into a pit (a slide whistle down and a thud)
  Count,
};

} // namespace gr
