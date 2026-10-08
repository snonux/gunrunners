# Level format

Levels are plain text, one character per 16x16 tile. Lines starting with
`;` are comments, `name=...` sets the title shown in the stage banner.

| Char | Meaning |
|------|---------|
| `#`  | solid block |
| `=`  | one-way platform (jump up through it, land on top) |
| `^`  | spikes (1 damage, bounces you up) |
| `C`  | crate: solid, takes 3 shots, drops a gem |
| `P`  | player start (feet on the tile below) |
| `X`  | exit teleporter (bottom tile; it is two tiles tall) |
| `g`  | gem (+100) |
| `h`  | health (+1 heart) |
| `w`  | walker bot: patrols, turns at walls and ledges |
| `f`  | flyer drone: hovers and drifts towards you |
| `t`  | turret: shoots at you when you are in range |
| `*`  | theme decoration (neon sign, torch, beacon) |

The left and right map edges are walls; falling off the bottom costs a life.
With the current tuning every dude can clear gaps of 2 tiles and climb steps
of 2 tiles, which `level1.txt` sticks to.
