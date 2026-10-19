# Harbour builder

Turns the harbour that the world generator baked into a world's heightmap into proper 3D
land objects: rubble-mound breakwaters with rock armour, crest road, crown wall and round
heads; vertical quay walls with a concrete apron, fenders and bollards; and red/green beacon
towers on the breakwater heads. It also levels the port platform, lowers the baked-in
breakwater blobs under the new meshes, and paints a desert/port terrain texture.

Everything is generated from data already in the world folder, so no internet access or
3D modelling package is needed.

## Use

```
pip install -r tools/harbour_builder/requirements.txt
python3 tools/harbour_builder/build_harbour.py bin/World/BOUJDOUR/harbour.json --debug
```

The run writes these into the world folder:

| Output | What it is |
|---|---|
| `Models/LandObject/<prefix>_breakwaters/` | Breakwaters, round heads and beacons (OBJ + MTL + textures) |
| `Models/LandObject/<prefix>_quays/` | Quay walls, apron, fenders and bollards |
| `height_harbour.f32` | Heightmap with the port platform levelled and structures cut down |
| `terrain_harbour.jpg` | Terrain texture: sand, rock on slopes, seabed, concrete port platform |
| `landobject.ini` | Places both objects (absolute height, collision and radar on) |
| `terrain.ini` | Updated to use the two files above |
| `harbour_detection.png` | With `--debug`: what was detected (yellow breakwaters, blue basin, red quay lines) |

Source files (`height.f32`, the original texture) are left untouched, so you can change the
config and re-run. The output is deterministic.

Models live in the world folder (`World/<name>/Models/LandObject/...`), which the simulator
searches after the user folder, so the world is self-contained and tracked in git.

## Adding another port

1. Copy `bin/World/BOUJDOUR/harbour.json` into the world folder.
2. Set `window` to a lat/long box that surrounds the harbour with some margin.
3. Run with `--debug` and check `harbour_detection.png`. The yellow areas must be the
   breakwaters, and the blue area must be the inner basin only.
   * Breakwaters missing, or coast marked yellow: change `structure_max_width_m`, which
     should be a little wider than the widest breakwater, and `structure_min_area_m2`.
   * Wrong side treated as the basin: the entrance is sealed by joining the breakwater heads
     (or the single head to the nearest shore). Check the heads are where you expect.
4. Set `beacons` to the approximate lat/long of each head with `"red"` or `"green"`. Each
   beacon snaps to the nearest detected head. Under IALA region A, which includes Morocco,
   the head on a ship's port side when entering is red.
5. Load the world in the simulator and adjust `levels` (quay height, crest height and so on)
   if needed.

To keep other land objects (lighthouses, cranes, buildings), list them under
`extra_land_objects` in the config using the usual `landobject.ini` keys, because
`landobject.ini` is rewritten on every run.

## Limits

* It can only rebuild what the heightmap shows. Structures narrower than about two heightmap
  cells (small piers, pontoons) are not detected.
* Levels, slopes and beacon colours are typical values set in the config, not surveyed
  data. Light characteristics belong in `light.ini` and must come from official sources.
* Buildings are not generated.
