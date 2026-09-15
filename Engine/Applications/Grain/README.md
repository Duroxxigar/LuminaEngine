# Grain: Ember Vale

A standalone RHI showcase. A micro-voxel island held as a sparse brickmap in GPU memory, raymarched
with one bounce of global illumination, and played as a small action RPG. No engine renderer, no
`CWorld`, no assets on disk. Every shader is embedded in the binary and every model is built from
boxes at startup.

Build and run:

```bash
./LuminaBuild.bat Build Grain -TargetType=Program -Configuration=Development
```

```bash
./Binaries/Windows64/Grain-Program-Development.exe -novalidation -unlocked
```

## Playing

You are a delver on an island of three unlit beacons. Dig crystal, survive what the night brings,
and carry four shards to each beacon to light it.

| Input | Action |
| --- | --- |
| `W A S D` | Move |
| `Space` | Jump, or swim up |
| `Left Shift` | Sprint, spends stamina |
| Mouse | Look |
| Left mouse | Swing. Hits anything in the arc, otherwise digs the terrain |
| Right mouse | Cast an ember bolt. Arcs, detonates, craters |
| `E` | Light a beacon you are standing at |
| `Tab` | Release the mouse |
| `Escape` | Quit |

Husks close and hit. Wisps hover out of melee range and only fire on a clear line, so cover works.
Golems are slow, hit hard and drop three shards. Kills and shards give experience, and a level raises
health, stamina, focus and the damage a swing does. The sun crosses in eight minutes and the vale
spawns faster the darker it gets.

## Debug keys

`H` toggles the overlay, `T` the bounce accumulation, `F` the spatial filter, `G` the temporal
resolve, `V` a free flying camera.

## Flags

| Flag | Effect |
| --- | --- |
| `-seed N` | World and placement seed |
| `-time N` | Start at N percent through the day, 25 is noon and 75 is midnight |
| `-daylength N` | Seconds for a full cycle |
| `-screenshot -frames N` | Run a scripted demo at a fixed sixty steps and write `Grain.png` at frame N |
| `-unlocked` | Mailbox present, since the default pins every measurement to the refresh rate |
| `-gputimes` | Per pass GPU breakdown for the last frame |
| `-nosim` | Skip the dense water volume |
| `-nohud` `-nofilter` `-notemporal` `-noaa` | Turn one stage off |
| `-debugmat` `-debugnormal` | Return right after the primary hit |
| `-freecam` | Fly the scene instead of following the delver |

Measure with `-gputimes` and `-unlocked` together, over 200 frames or more. The per pass breakdown
covers only the last frame and is often unrepresentative, so trust the averaged frame time and read
the breakdown for ratios.
