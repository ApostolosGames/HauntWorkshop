# HAUNT Workshop Map Guide

This repository contains the publishing rules for community maps for **HAUNT**.

## What you need

- **Unreal Engine 5.8**. Maps must be cooked with the same UE version as HAUNT to load reliably.
Do not cook a Workshop map with another Unreal Engine version. A map made with UE 5.7, UE 5.9, or an arbitrary custom engine build might not work.
- **Your own Unreal project.** A blank UE 5.8 project is enough. HAUNT does not provide a creator project, plugin pack, or asset kit.

Your map has to be self-contained. Build the level, its geometry, materials, and **its own lighting** in your project and cook everything it references into your pak. HAUNT supplies the lobby, the hunter container, and the players at runtime. Your map supplies everything else, including its sky and lighting.

## Quick start

1. Create a blank UE 5.8 project and a new, empty level.
2. Import the [reference templates](#reference-templates) and place the container template at its landmark position.
3. Build your map around world origin `(0, 0, 0)`, leaving room for the hunter container and the ghost spawn area (see [Map positioning and gameplay landmarks](#map-positioning-and-gameplay-landmarks)).
4. Light the map yourself, including any sky and sun (see [Lighting](#lighting)).
5. Apply the [packaging settings](#unreal-packaging-settings) and cook the map to a `.pak` + `.utoc` + `.ucas` set.
6. [Test it](#testing-your-map): check it in the editor, [upload it with SteamCMD](#uploading-with-steamcmd) as a private item, subscribe, and play it in HAUNT.
7. Make the item public once it works.

## One Workshop item = one playable map

Each Workshop item must contain **exactly one `.umap`**: your playable map, and nothing else.

- You may use any valid Unreal asset name for the map. There is no required map-name or pak-file-name convention.
- Do not include a second playable map, test map, template map, or alternate level in the cooked output.
- Create the level with **File → New Level → Empty Level** (or Basic) and keep **World Partition disabled**. Open World / World Partition maps cook into extra generated level packages.
- Do not use sublevels, level streaming, or Level Instance actors that point at other `.umap` files. Build everything into the one level.
- In **Project Settings → Packaging → List of maps to include in a packaged build**, list only your map, and don't cook Starter Content or template maps.
- Keep the map under your project's `/Game/` content or a content-only plugin of your own; do not place it under `/Engine/` or an Engine plugin root. HAUNT ignores maps under those roots.
- Package all assets referenced by the map. Missing meshes, materials, textures, or Blueprint classes will prevent clients from loading it correctly.
- Use only your own assets and stock UE 5.8 engine features. HAUNT cannot load custom C++ code or code plugins from a Workshop pak, and content from an Engine plugin only works if HAUNT has that plugin enabled too.
- You do not need a Player Start, GameMode, or GameMode override. HAUNT loads your map into its lobby world as a level instance and places the hunter container, hunters, ghosts, and spectators itself. Your map's World Settings are not used.
- Do not include the HAUNT executable, base-game paks, Engine content, or unrelated files.

HAUNT discovers the map from its cooked Asset Registry rather than guessing from a filename. If a pak contains more than one map, HAUNT rejects it instead of choosing one arbitrarily. Before uploading, confirm there is exactly one `.umap` in the pak (see [Check the cooked files](#2-check-the-cooked-files)).

## Unreal packaging settings

In **Project Settings → Packaging**, configure the Workshop project as follows:

- Enable **Use Pak File**.
- Enable **Use Io Store**.
- Cook only the map and the content it references. Do not cook the whole engine or unrelated sample/template content.
- Ensure the map is included in the cook list (or otherwise included by your project’s packaging rules).

The cooked pak must retain the Asset Registry and map directory entries so HAUNT can discover the `UWorld`. In `Config/DefaultGame.ini`, include:

```ini
[Pak]
+DirectoryIndexKeepFiles="*.umap"
+DirectoryIndexKeepFiles="*AssetRegistry.bin"
```

Cook/package for the same target platform as the game build you intend to play. Windows Workshop uploads need Windows cooked output.

## Upload folder layout

The folder submitted to Steam Workshop must contain **only** the three matching files for one map build:

```text
MyWorkshopMap/
├── MyWorkshopMap.pak
├── MyWorkshopMap.utoc
└── MyWorkshopMap.ucas
```

Rules:

- Exactly one `.pak` file is allowed.
- Its `.utoc` and `.ucas` files must have the same filename stem as the `.pak`.
- Do not upload folders, executables, `Config`, `Content`, source files, logs, `.sig` files, or extra paks.
- Do not rename just one of the three files. Rename all three together if you rename them at all.

HAUNT validates this layout before mounting the Workshop item. It rejects uploads with no pak, multiple paks, or a pak missing its matching IoStore files.

## Uploading with SteamCMD

This repository includes [`workshop_item.vdf.example`](workshop_item.vdf.example), a SteamCMD upload template for HAUNT's Workshop App ID. Copy it to `workshop_item.vdf`, then fill in `contentfolder` with the folder containing only your `.pak`, `.utoc`, and `.ucas` files, and `previewfile` with your Workshop preview image. Leave `publishedfileid` as `0` for a new item; Steam assigns an ID that you reuse for later updates.

`visibility` controls who can see and download the item:

| Value | Visibility | Use it for |
| --- | --- | --- |
| `0` | Public | Released maps. Anyone joining a host can download it. |
| `1` | Friends-only | Testing with Steam friends. |
| `2` | Private | Your first uploads and solo testing. |
| `3` | Unlisted | Not listed in the Workshop, but anyone with the link can see it. |

The template defaults to `0`. Set it to `2` for your first upload (see [Testing your map](#testing-your-map)).

The template adds the required `Map` Workshop tag. Upload it with:

```powershell
steamcmd +login <SteamUser> +workshop_build_item "C:\Path\To\workshop_item.vdf" +quit
```

## Testing your map

HAUNT only loads Workshop maps from Steam, so you cannot load a local pak in the game. To test in HAUNT, upload the map as a **private** Workshop item, subscribe to it, and play it. Make it public once it works. Check as much as you can before that first upload, because every fix means another cook and upload.

### 1. Check in the editor

Play in Editor (PIE) in your own project, with the [reference templates](#reference-templates) placed, to check:

- the container template's door opens onto clear, walkable floor
- character templates fit through doors, corridors, and hiding spots, and across the ghost spawn grid
- lighting is readable with only your map's lights (HAUNT hides its sky for Workshop maps)
- the map is still navigable with your placed Light actors hidden, as they will be during the Haunt phase
- players cannot fall off the map toward the lobby below

### 2. Check the cooked files

- The upload folder contains only the `.pak`, `.utoc`, and `.ucas` files, all with the same filename stem.
- Optionally, list the pak's contents with `UnrealPak.exe "C:\Path\To\MyWorkshopMap.pak" -List` (UnrealPak is in `Engine\Binaries\Win64\`). The list should include `AssetRegistry.bin` and exactly one `.umap` from your project. If `AssetRegistry.bin` is missing, re-check the `[Pak]` settings above.

### 3. Upload privately and test in HAUNT

1. In `workshop_item.vdf`, set `"visibility"` to `"2"` (private) and upload with SteamCMD as described above. Note the `publishedfileid` that Steam assigns and put it in the VDF so later uploads update the same item.
2. Subscribe to the item in the Steam Workshop and let Steam download it.
3. Start HAUNT, host a lobby, and select the item as the Workshop map.
4. Confirm the map mounts, loads, and displays all assets, materials, and lighting.
5. Confirm hunters can leave the container, ghosts spawn in front of it, and no one can fall toward the lobby.
6. Play through a Haunt phase and use a ghost EMP to check how the map looks with its lights off.
7. Fix any problems, re-cook, and upload again with the same `publishedfileid`. If Steam doesn't pick up the new version right away, restart Steam or unsubscribe and resubscribe.

To test with other players before release, set `"visibility"` to `"1"` (friends-only). Your Steam friends can then download the item when they join your lobby. Clients automatically download the host-selected Workshop map and must finish downloading and loading it before they count as ready to start the match.

### 4. Publish

When the map works, set `"visibility"` to `"0"` (public) and upload once more with the same `publishedfileid`, or change the visibility on the item's Workshop page.

## Map positioning and gameplay landmarks

HAUNT mounts your pak, then loads the map into the lobby world as a level instance at world location **`(0, 0, 0)`** with no rotation. Build and test your map around that origin and do not offset your level. The landmarks below are fixed world positions that HAUNT uses on every map.

| Landmark | World position | What HAUNT does there |
| --- | --- | --- |
| Map origin | `(0, 0, 0)` | Places your level instance. |
| Hunter container | `(0, 0, 205)`, rotation `(0, 0, 90)` | Moves the container from the lobby to this spot with the hunters inside. Its door faces the **negative X axis**. |
| Ghost spawn | around `(-700, 0, 300)` | Spawns ghosts, and spectators, on the floor in front of the container door. |
| Lobby | around `(0, 0, -8000)` | HAUNT's lobby, below your map. It is only loaded for players inside the lobby area. |

### Hunter container

Hunters do not spawn in your map. They board HAUNT's shipping container in the lobby. When the round starts, HAUNT moves the container to **`(0, 0, 205)`** with rotation **`(0, 0, 90)`** (yaw 90°) and scale `(1.8, 1.5, 1.5)`, then teleports every hunter to the same spot inside it.

- Do not build or include a container; HAUNT brings its own. Use the [container template](#reference-templates) to see exactly where it will be.
- Keep the area around `(0, 0, 205)` free of your geometry so the container doesn't clip into walls or props and hunters aren't trapped.
- The door faces **negative X**. Keep the space in front of the opening clear, and connect it to the rest of your map with walkable floor.

### Ghost spawn

Ghosts and spectators are placed by searching a **5 × 5 grid of points spaced 75 cm apart**, centered on **`(-700, 0, 300)`**. That covers X `-850` to `-550` and Y `-150` to `150`. For each point, HAUNT:

1. Traces straight down from Z `400` to Z `-700` for a floor that blocks the **WorldStatic** or **WorldDynamic** object types.
2. Checks that a character-sized capsule standing on that floor is clear on the **Visibility** channel.

To make sure every ghost gets a valid spot:

- Put walkable floor with collision under that whole area, with its surface between Z `-700` and Z `400`. Keeping it close to the container floor is best.
- Leave the area open and at least a character's height clear above the floor. Walls, props, low ceilings, and invisible blocking volumes all count as obstructions.
- If no grid point passes, ghosts may not spawn where you intended.

### Lobby

HAUNT's lobby sits below the map at about **`(0, 0, -8000)`**; its geometry spans roughly Z `-8600` to `-7800`. Each player's client only loads the lobby while that player is inside the lobby area, so during a match it is unloaded for everyone on your map.

- Keep all of your geometry well above it and do not build downward into that space.
- Make sure players cannot fall through or walk off your map toward the lobby. Close off holes and map edges with floors, walls, or blocking volumes.

### Character size

HAUNT characters are about **95% of the UE5 mannequin's height** and about **twice its width**. Size corridors, doors, props, hiding spaces, and ghost spawn clearance for this wider character shape.

Use the [character template](#reference-templates) to check doorways, corridors, hiding spots, and the ghost spawn area.

## Reference templates

The [`Templates/`](Templates/) folder contains two FBX blockout meshes that match the size of HAUNT's real container and characters. They are sizing references only: HAUNT brings the real container and characters at runtime, so these meshes must never ship in your map.

| File | Size (X × Y × Z) | Pivot | Marked face |
| --- | --- | --- | --- |
| [`SM_HAUNT_Container_Template.fbx`](Templates/SM_HAUNT_Container_Template.fbx) | about 424 × 981 × 396 cm | Center of the container, 6 cm above its floor | Door end, on the mesh's local **+Y** side, uses a separate material |
| [`SM_HAUNT_Character_Template.fbx`](Templates/SM_HAUNT_Character_Template.fbx) | about 74 × 74 × 171 cm | Center of the feet | Front, on the mesh's local **+X** side, uses a separate material |

### Importing

Drag both files into your project's Content Browser (for example, into a `/Game/HAUNT_Templates/` folder) and import them with the default static mesh settings at an import scale of `1.0`. Both are already in centimeters. The container includes a simple box collision (`UCX_`), so it blocks the character when you test in the editor.

### Placing them

- **Container:** place `SM_HAUNT_Container_Template` at location **`(0, 0, 205)`**, rotation **`(0, 0, 90)`**, scale **`(1, 1, 1)`**. The size is already baked in, so do not apply HAUNT's container scale again. The door end then faces **negative X**, at about X `-490`, and the container floor sits at about Z `199`. That is where HAUNT's real container appears.
- **Character:** place `SM_HAUNT_Character_Template` wherever you want to check clearance, with its feet on the floor. Its front faces the actor's forward (+X) direction. A few copies across the ghost spawn grid (X `-850` to `-550`, Y `-150` to `150`) are an easy way to check that ghosts fit.

### Keep them out of your cooked map

For every template you place in the level, select the actor and enable **Is Editor Only Actor** (in the Details panel under **Actor → Advanced**). Editor-only actors are stripped when the map is cooked. Alternatively, delete the template actors before cooking. Do not reference the template meshes from any real gameplay actor, or they will be packaged with your map.

## Lighting

### Your map provides all of its lighting

The lobby's Ultra Dynamic Sky is hidden while a Workshop map is loaded, so HAUNT adds no sun, sky, or ambient light to your map. Whatever lighting your level contains is all players will see. Without it, your map renders dark.

- **Sky and sun:** add your own Directional Light, Sky Light, Sky Atmosphere, Exponential Height Fog, and Volumetric Cloud if you want them. Use no more than one of each.
- **Local lighting:** use point, spot, and rect lights, emissive materials, and lit props to make the space readable.
- **Build settings:** movable lighting (Lumen) is the simplest option. If you use static or stationary lights, build lighting in your project before cooking so the lightmaps ship in your pak.

### Lights go out during the Haunt phase and on EMP

HAUNT switches lights off as part of gameplay:

- **Haunt phase:** every visible **standalone** Point Light, Spot Light, and Rect Light actor in your level is turned off, then turned back on when the phase ends.
- **Ghost EMP:** standalone Point, Spot, and Rect Light actors within the EMP's radius are turned off temporarily.

Only standalone light actors placed directly in the level are affected. **A light inside a Blueprint will not turn off** for the Haunt phase or an EMP, so don't put lights you want to go dark inside Blueprint actors. Directional Lights and Sky Lights are never switched off, and emissive materials don't change either. The hunter container's own lights are handled separately by its light switch.

Place the lights that should go dark during a haunt as standalone light actors, keep your always-on lighting (moonlight, sky, emissive) dim enough that the haunt still feels dark, and make sure the map is still navigable with those lights off.

## Footstep sounds (physical materials)

HAUNT picks footstep sounds from the **Surface Type** of the physical material under the player's feet. HAUNT's surface types are matched by slot number, so your project must use the same slots:

| Surface Type slot | Name | Footstep sound |
| --- | --- | --- |
| `SurfaceType1` | `WOOD` | Wood |
| `SurfaceType2` | `STONE` | Stone |
| `SurfaceType3` | `METAL` | Metal |
| `SurfaceType4` | `GRASS` | Grass |
| `SurfaceType5` | `CARPET` | Carpet |

Surfaces with no physical material, or any other surface type, use the default footstep sound.

1. **Define the surface types.** In **Project Settings → Engine → Physics → Physical Surface**, name `SurfaceType1` to `SurfaceType5` exactly as in the table. Only the slot number is used in game; the names just make them easy to pick in the editor. This writes the following to `Config/DefaultEngine.ini`:

   ```ini
   [/Script/Engine.PhysicsSettings]
   +PhysicalSurfaces=(Type=SurfaceType1,Name="WOOD")
   +PhysicalSurfaces=(Type=SurfaceType2,Name="STONE")
   +PhysicalSurfaces=(Type=SurfaceType3,Name="METAL")
   +PhysicalSurfaces=(Type=SurfaceType4,Name="GRASS")
   +PhysicalSurfaces=(Type=SurfaceType5,Name="CARPET")
   ```

2. **Create physical materials.** In the Content Browser, choose **Add → Physics → Physical Material** and create one per surface, for example `PM_Wood`, `PM_Stone`, `PM_Metal`, `PM_Grass`, and `PM_Carpet`. Set each one's **Surface Type** to the matching entry.
3. **Assign them to your floors.** Set **Phys Material** in the details of each floor material or material instance. For meshes with more than one material, the first material slot decides unless you set **Simple Collision Physical Material** on the static mesh or **Phys Material Override** on the placed component. For landscapes, assign physical materials to the landscape layers.

The physical materials are cooked into your pak like any other asset your map references, so HAUNT doesn't need to know their names. Walk across each surface in HAUNT during testing to confirm the right sound plays.

## Updating an existing Workshop item

Build and upload all three files together for every update. Do not upload only the pak or only the `.utoc`/`.ucas`; they are one IoStore container set.

Once released, keep the Workshop item public and available. Private, removed, or corrupted items cannot be downloaded by clients joining a host. If you want to test a risky update before players see it, upload it as a separate private item first.

## Troubleshooting

| Symptom | Likely cause | Fix |
| --- | --- | --- |
| `No .pak file found` | The upload folder is wrong or empty. | Upload the cooked pak alongside its matching IoStore files. |
| `Expected exactly one .pak` | More than one pak was uploaded. | Make a clean folder containing only one pak set. |
| `missing .ucas/.utoc` | One or both IoStore companion files are absent or misnamed. | Repackage and upload all three matching files. |
| `Found N maps inside ...` | The pak contains more than one `.umap` (a second map, sublevels, World Partition cells, or template maps). | Keep a single level with World Partition disabled, remove other maps from the cook list, and repackage. |
| `Could not find a map` | The Asset Registry/map index was stripped, or the map was not cooked. | Add the `[Pak]` settings above and ensure the map is cooked. |
| Map loads without expected assets | A referenced dependency was not included, or it depends on a plugin or custom code HAUNT does not have. | Cook all dependencies with UE 5.8, and use only your own assets and stock engine features. |
| Map is dark or unlit | HAUNT hides its sky for Workshop maps and adds no lighting; the map has no lights of its own, or static lighting was not built. | Add your own sky/sun and local lights, and build lighting before cooking if you use static/stationary lights. |
| Materials render as the default gray material | The pak's shader library could not be found. | Keep the `[Pak]` settings above so `AssetRegistry.bin` stays in the pak, and cook with the default shader library settings. |
| Hunters are stuck or pushed out of the container | Map geometry overlaps the container at `(0, 0, 205)` or blocks its door. | Clear the container footprint and the space on its negative X side. |
| Ghosts do not spawn correctly | No valid floor or not enough room in the spawn grid around `(-700, 0, 300)`. | Add walkable floor with WorldStatic/WorldDynamic collision between Z `-700` and `400`, and keep the grid area clear on the Visibility channel. |

## Contributing and support

- Found something wrong in this guide, or need help with a map that won't load? Open an [issue](https://github.com/ApostolosGames/HauntWorkshop/issues/new/choose). See [CONTRIBUTING.md](CONTRIBUTING.md) for pull requests.
- Report security problems privately, as described in [SECURITY.md](SECURITY.md).
- Everyone taking part is expected to follow the [Code of Conduct](CODE_OF_CONDUCT.md).

## License

The guide, the SteamCMD template, and the reference templates in this repository are released under the [MIT License](LICENSE). HAUNT itself, its game content, and its trademarks are not covered by this license.
