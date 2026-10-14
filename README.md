# HAUNT Workshop Map Guide

This repository contains the publishing rules for community maps for **HAUNT**.

## What you need

- **Unreal Engine 5.8**. Maps must be cooked with the same UE version as HAUNT to load reliably.
Do not cook a Workshop map with another Unreal Engine version. A map made with UE 5.7, UE 5.9, or an arbitrary custom engine build might not work.
- **Your own Unreal project.** A blank UE 5.8 project is enough. HAUNT does not provide a creator project, plugin pack, or asset kit.

Your map has to be self-contained. Build the level, its geometry, materials, and **its own lighting** in your project and cook everything it references into your pak. HAUNT supplies the lobby, the hunter container, and the players at runtime. Your map supplies everything else, including its sky and lighting.

## Quick start

1. Create a blank UE 5.8 project with its own name, add a content-only plugin with a unique name, and create a new, empty level inside it (see [Keep your content in its own plugin](#keep-your-content-in-its-own-plugin)).
2. Import the [reference templates](#reference-templates) and place the container template at its landmark position.
3. Build your map around world origin `(0, 0, 0)`, leaving room for the hunter container and the ghost spawn area (see [Map positioning and gameplay landmarks](#map-positioning-and-gameplay-landmarks)).
4. Light the map yourself, including any sky and sun (see [Lighting](#lighting)).
5. Apply the [packaging settings](#unreal-packaging-settings) and cook the map to a `.pak` + `.utoc` + `.ucas` set.
6. [Test it](#testing-your-map): check it in the editor, [upload it with SteamCMD](#uploading-with-steamcmd) as a private item tagged `Map` (HAUNT ignores items without this tag), subscribe, and play it in HAUNT.
7. Make the item public once it works.

## One Workshop item = one playable map

Each Workshop item must contain **exactly one `.umap`**: your playable map, and nothing else.

- You may use any valid Unreal asset name for the map. There is no required map-name or pak-file-name convention.
- Do not include a second playable map, test map, template map, or alternate level in the cooked output.
- Create the level with **File → New Level → Empty Level** (or Basic) and keep **World Partition disabled**. Open World / World Partition maps cook into extra generated level packages.
- Do not use sublevels, level streaming, or Level Instance actors that point at other `.umap` files. Build everything into the one level.
- In **Project Settings → Packaging → List of maps to include in a packaged build**, list only your map, and don't cook Starter Content or template maps.
- Keep the map and everything it made or imported in **one** content-only plugin of your own (see [Keep your content in its own plugin](#keep-your-content-in-its-own-plugin)). Maps under `/Game/` still load for now, but only in a restricted compatibility mode, and that support may be removed in a later update. Do not place the map under `/Engine/` or an Engine plugin root.
- Package all assets referenced by the map. Missing meshes, materials, textures, or Blueprint classes will prevent clients from loading it correctly.
- Use only your own assets and stock UE 5.8 engine features. HAUNT cannot load custom C++ code or code plugins from a Workshop pak, and content from an Engine plugin only works if HAUNT has that plugin enabled too.
- You do not need a Player Start, GameMode, or GameMode override. HAUNT loads your map into its lobby world as a level instance and places the hunter container, hunters, ghosts, and spectators itself. Your map's World Settings are not used.
- Do not include the HAUNT executable, base-game paks, or unrelated files. Your cook will also contain the engine assets your map references (default materials, basic shapes, and so on). That is expected, but don't add Engine content the map doesn't use.
- Don't edit stock engine assets (for example `/Engine/BasicShapes` or the default materials) and expect the edit to show up. HAUNT ships its own copies of engine content, and when your pak contains an asset HAUNT already has, HAUNT's copy is the one that loads. To change an engine asset, duplicate it into your own plugin and edit the duplicate.

HAUNT discovers the map from its cooked Asset Registry rather than guessing from a filename. If a pak contains more than one map, HAUNT rejects it instead of choosing one arbitrarily. Before uploading, confirm there is exactly one `.umap` in the pak (see [Check the cooked files](#2-check-the-cooked-files)).

### Keep your content in its own plugin

Unreal identifies every asset by its package path, and HAUNT's own content also lives under `/Game/`. If your pak contains an asset at the same path as one of HAUNT's, HAUNT's asset is used and yours is ignored, so your map shows the wrong mesh, material, or Blueprint, or fails to load. This happens easily if you start from a UE template project (`/Game/ThirdPerson`, `/Game/FirstPerson`, `/Game/LevelPrototyping`, `/Game/Characters`, `/Game/Input`) or use a Fab or Marketplace pack that installs into `/Game/<PackName>`.

To avoid it, put your map and all of its assets in a content-only plugin:

1. In **Edit → Plugins**, choose **Add**, pick **Content Only**, and give it a name nobody else is likely to use, for example `YourName_MapName`. Give your Unreal project its own name too.
2. Create your level inside the plugin's content folder, and move or migrate any packs and assets you use into that folder too.
3. Make sure nothing the map uses still points at `/Game/` or at another plugin of yours. Right-click the map in the Content Browser and open the **Reference Viewer** to see its dependencies.

Your content then cooks under `/YourName_MapName/...` and can't collide with HAUNT's assets.

HAUNT refuses a map when:

- Its own content is spread over more than one root, for example the plugin plus some leftovers in `/Game/`, or two plugins. Engine content your map references is fine and doesn't count.
- The plugin or the project has a reserved name or one HAUNT already uses. Don't use `Game`, `Engine`, `Script`, `Memory`, `Temp`, `Config`, `Extra`, `Global`, `Ghost`, or the name of any plugin HAUNT ships with. A name that includes your own name or the map's name is safest.
- It still uses `/Game/` and any of its assets has the same path as one of HAUNT's. In that case, move everything into a plugin.

### Keep the cook clean

HAUNT reads the full file list of your `.pak` and `.utoc` before loading anything, and refuses an item if it can't, or if the list has something a map should never contain:

- Don't encrypt the pak or IoStore container, and don't strip the pak's file list. The default packaging settings are correct; leave **Encrypt Pak** options off.
- Every cooked file must come from your own project folder or the engine's content. Don't add files from outside the project, and don't hand-edit the pak's mount point.
- No `Binaries` folders, executables, DLLs, or scripts (`.exe`, `.dll`, `.bat`, `.cmd`, `.ps1`, `.vbs`, `.js`, and similar) anywhere in the cook. A content-only plugin with no code module never produces these.
- Don't localize assets (`L10N` folders or asset localization). Localized asset variants and package redirects aren't supported in Workshop maps.

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
- The filename stem must not end in `_P` (for example `MyMap_P.pak`). That suffix is reserved for patches, and HAUNT rejects it. Some modding guides use `_P` out of habit; Workshop maps don't need it.
- Nothing else may be in the folder: no subfolders, executables, scripts, `Config`, `Content`, source files, logs, `.sig` files, readme files, or extra paks.
- Do not rename just one of the three files. Rename all three together if you rename them at all.

HAUNT checks this layout before mounting the Workshop item. It refuses an item with no pak, more than one pak, a pak missing its matching IoStore files, a `_P` filename, or any other file or folder next to the three.

## Uploading with SteamCMD

This repository includes [`workshop_item.vdf.example`](workshop_item.vdf.example), a SteamCMD upload template for HAUNT's Workshop App ID. Copy it to `workshop_item.vdf`, then fill in `contentfolder` with the folder containing only your `.pak`, `.utoc`, and `.ucas` files, and `previewfile` with your Workshop preview image. Leave `publishedfileid` as `0` for a new item; Steam assigns an ID that you reuse for later updates.

`visibility` controls who can see and download the item:

| Value | Visibility | Use it for |
| --- | --- | --- |
| `0` | Public | Released maps. Anyone joining a host can download it. |
| `1` | Friends-only | Testing with Steam friends. |
| `2` | Private | Your first uploads and solo testing. |
| `3` | Unlisted | Not listed in the Workshop, but anyone with the link can see it. |

The template defaults to `2` (private) so your first upload isn't public. Change it to `0` only when the map is ready to release (see [Testing your map](#testing-your-map)).

The template sets the `Map` Workshop tag. HAUNT requires it: an item without the `Map` tag won't be offered as a map, so don't remove it. Upload it with:

```powershell
steamcmd +login <SteamUser> +workshop_build_item "C:\Path\To\workshop_item.vdf" +quit
```

## Testing your map

HAUNT only loads Workshop maps from Steam, so you cannot load a local pak in the game. To test in HAUNT, upload the map as a **private** Workshop item, subscribe to it, and play it. Make it public once it works. Check as much as you can before that first upload, because every fix means another cook and upload.

### 1. Check in the editor

Play in Editor (PIE) in your own project, with the [reference templates](#reference-templates) placed, to check:

- the container template's door opens onto clear, walkable floor
- character templates fit through doors, corridors, and hiding spots, and across the ghost spawn grid, with the headroom described in [Character size](#character-size)
- lighting is readable with only your map's lights (HAUNT hides its sky for Workshop maps)
- the map is still navigable with your placed Light actors hidden, as they will be during the Haunt phase
- players cannot fall off the map toward the lobby below

### 2. Check the cooked files

- The upload folder contains only the `.pak`, `.utoc`, and `.ucas` files, all with the same filename stem.
- Optionally, list the pak's contents with `UnrealPak.exe "C:\Path\To\MyWorkshopMap.pak" -List` (UnrealPak is in `Engine\Binaries\Win64\`). The list should include `AssetRegistry.bin` and exactly one `.umap` from your project. If `AssetRegistry.bin` is missing, re-check the `[Pak]` settings above.

### 3. Upload privately and test in HAUNT

1. In `workshop_item.vdf`, keep `"visibility"` at `"2"` (private) and upload with SteamCMD as described above. Note the `publishedfileid` that Steam assigns and put it in the VDF so later uploads update the same item.
2. Subscribe to the item in the Steam Workshop and let Steam download it.
3. Start HAUNT, host a lobby, and select the item as the Workshop map.
4. Confirm the map mounts, loads, and displays all assets, materials, and lighting.
5. Confirm hunters can leave the container, ghosts spawn in front of it, and no one can fall toward the lobby.
6. Play through a Haunt phase and use a ghost EMP to check how the map looks with its lights off.
7. Fix any problems, re-cook, and upload again with the same `publishedfileid`. If Steam doesn't pick up the new version right away, restart Steam or unsubscribe and resubscribe.

To test with other players before release, set `"visibility"` to `"1"` (friends-only). Your Steam friends can then download the item when they join your lobby. Clients automatically download the host-selected Workshop map and must finish downloading and loading it before they count as ready to start the match. A player whose game can't load the map leaves the lobby and is told why on the main menu, and the map shows as failed for that player on the lobby's map screen. If the host's own game can't load it, the match is cancelled for everyone. Test with at least one other player before you publish.

### 4. Publish

When the map works, set `"visibility"` to `"0"` (public) and upload once more with the same `publishedfileid`, or change the visibility on the item's Workshop page.

## Map positioning and gameplay landmarks

HAUNT mounts your pak, then loads the map into the lobby world as a level instance at world location **`(0, 0, 0)`** with no rotation. Build and test your map around that origin and do not offset your level. The landmarks below are fixed world positions that HAUNT uses on every map.

| Landmark | World position | What HAUNT does there |
| --- | --- | --- |
| Map origin | `(0, 0, 0)` | Places your level instance. |
| Hunter container | `(0, 0, 205)`, rotation `(0, 0, 90)` | Moves the container from the lobby to this spot with the hunters inside. Its door faces the **negative X axis**. |
| Ghost spawn | around `(-700, 0, 300)` | Spawns ghosts, and players who join mid-match as spectators, on the floor in front of the container door. |
| Lobby | around `(0, 0, -8000)` | HAUNT's lobby, below your map. It stays loaded and solid during the match; it is only hidden from players outside it. |

### Hunter container

Hunters do not spawn in your map. They board HAUNT's shipping container in the lobby. When the round starts, HAUNT moves the container to **`(0, 0, 205)`** with rotation **`(0, 0, 90)`** (yaw 90°) and scale `(1.8, 1.5, 1.5)`, and moves the hunters with it, keeping each hunter's position inside the container. Any hunter who had wandered out of the container in the lobby is put back inside before it moves.

- Do not build or include a container; HAUNT brings its own. Use the [container template](#reference-templates) to see exactly where it will be.
- Keep the area around `(0, 0, 205)` free of your geometry so the container doesn't clip into walls or props and hunters aren't trapped.
- The door faces **negative X**. Keep the space in front of the opening clear, and connect it to the rest of your map with walkable floor.

### Ghost spawn

Ghosts, and players who join mid-match as spectators, are placed at one spawn point that HAUNT finds near **`(-700, 0, 300)`**. It tests that center point first, and only if the center fails does it search a **5 × 5 grid of points spaced 75 cm apart** around it. That covers X `-850` to `-550` and Y `-150` to `150`. For each point, HAUNT:

1. Traces straight down from Z `400` to Z `-700` for a floor that blocks the **WorldStatic** or **WorldDynamic** object types.
2. Checks that a capsule 70 cm wide and 180 cm tall, standing 2 cm above that floor, is clear on the **Visibility** channel.

Every ghost gets the same point, the first one that passes, so all ghosts start at or right around that spot. Players who die become spectators where they died, not here.

To make sure ghosts spawn where you intended:

- Put walkable floor with collision under that whole area, with its surface between Z `-700` and Z `400`. Keeping it close to the container floor is best.
- Leave the area open, with at least 185 cm of clear headroom above the floor, and room around the center for several ghosts side by side. Walls, props, low ceilings, and invisible blocking volumes all count as obstructions.
- If no point passes, ghosts may not spawn where you intended.

### Lobby

HAUNT's lobby sits below the map at about **`(0, 0, -8000)`**; its geometry spans roughly Z `-8600` to `-7800`. The lobby stays loaded for the whole match. While a player is outside it, HAUNT only hides the lobby's meshes and lights on that player's screen; its collision is still there.

The lobby's area is a box from X `-3200` to `4800`, Y `-5400` to `3600`, and Z `-9207` to `-7007`.

- Keep all of your geometry above Z `-7000` and do not build downward into the lobby's space.
- Make sure players cannot fall through or walk off your map. A player who falls toward the lobby can land on its invisible geometry instead of dying. Close off holes and map edges with floors, walls, or blocking volumes.

### Character size

What blocks a HAUNT character is its collision capsule: **70 cm wide and 180 cm tall** (radius 35, half-height 90). That is slightly narrower and shorter than the UE5 mannequin's capsule (84 × 192 cm).

- Doorways, corridors, and gaps players should fit through need to be wider than 70 cm. Leave a margin, for example 90 cm or more.
- Ceilings, hiding spots, and anything players walk under need more than 180 cm of headroom. Aim for 185 cm or more.

Use the [character template](#reference-templates) to check doorways, corridors, hiding spots, and the ghost spawn area. The template is 171 cm tall, 9 cm shorter than the capsule, so use the numbers above for headroom.

## Reference templates

The [`Templates/`](Templates/) folder contains two FBX blockout meshes that show the size of HAUNT's real container and characters. For character headroom and gaps, use the collision numbers in [Character size](#character-size). They are sizing references only: HAUNT brings the real container and characters at runtime, so these meshes must never ship in your map.

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

HAUNT chooses footstep sounds by **which physical material asset** is under the player's feet, not by its Surface Type. Its wood, stone, metal, grass, and carpet sounds are tied to HAUNT's own physical material assets, which a Workshop map can't reference. Setting up matching Surface Type slots in your project has no effect.

For Workshop maps this means:

- **Surfaces with no physical material** use the engine's default physical material, which HAUNT plays as the **stone** footstep. This is the setup to use.
- **Surfaces with a physical material of your own** are not recognized by HAUNT, and **footsteps on them are silent**. Do not assign your own physical materials to floors, stairs, or anything else players walk on.
- Do not try to recreate HAUNT's physical materials at the same asset paths in your project. That puts copies of HAUNT's assets in your pak (see [Keep your content in its own plugin](#keep-your-content-in-its-own-plugin)).

Surface-specific footsteps are not available to Workshop maps yet. Physical materials from Fab or Marketplace packs count as your own, so remove them from walkable surfaces (in the floor material, the mesh's **Simple Collision Physical Material**, the component's **Phys Material Override**, or the landscape layers). Walk across your map in HAUNT during testing and listen for silent spots.

## Blueprints in your map

Your map may contain Blueprint actors, for example for ambient effects, moving props, or sound. Keep these limits in mind:

- HAUNT does not yet offer a way for Workshop actors to hook into its gameplay. The interact key, weapons and other items, damage, round phases, and win conditions do not reach your Blueprints, so you can't make doors, buttons, traps, or objectives that players use through HAUNT's controls.
- Every player's game loads your map separately, and your Blueprints run separately on each of them. Anything driven by local events, such as timers, random numbers, or a player's own overlaps, can look or behave differently for different players.
- Workshop maps can't reach outside the game. Build anything your map needs out of in-world actors, components, and assets.

### Nodes and references Workshop maps can't use

HAUNT checks every Blueprint and asset in your pak before the map loads. If any of them uses one of the following, HAUNT refuses the whole map and names the asset and the node in the error:

- **HAUNT's own assets and classes.** Don't reference anything of HAUNT's, and don't recreate HAUNT's assets or classes at the same paths in your project to reference them. This applies to hard references, soft references, and Blueprint parent classes. It also covers the plugins HAUNT ships with: Steam, voice chat, Discord, and painting.
- **Code HAUNT doesn't include.** Nodes, components, or parent classes from a plugin that isn't part of HAUNT, including plugins that are only part of the Unreal Editor.
- **Leaving or controlling the game:** Open Level, Quit Game, Execute Console Command, Launch URL, and any web-browser, HTTP, online-subsystem, file-utility, or asset-registry node.
- **Reaching things by name:** Set Timer by Function Name (use **Set Timer by Event**), the **Set ... Property by Name** nodes, Make Soft Object Path or Make Soft Class Path from a string, and primary-asset lookups (Get Primary Asset Id List, Get Object from Primary Asset Id, and their async load nodes).
- **Player data and settings:** Save Game to Slot, Load Game from Slot, Delete Game in Slot, the async save and load nodes, Game User Settings nodes, and Set Current Culture, Language, or Locale.
- **Files and outside media:** Export Render Target, Export Texture 2D, Import File as Texture 2D, Finish Recording Submix Output (the submix/output recording export), string table CSV import or export, Media Player Open URL or Open File, File Media Source Set File Path, Stream Media Source assets, the Download Image node, and audio capture (microphone) components.

What you can use instead:

- Timers: **Set Timer by Event** with your own custom events.
- Loading: hard references to your own assets, or soft references you pick in the Details panel and load with **Async Load Asset** / **Load Asset**.
- Media: Media Player **Open Source** with a File Media Source asset inside your own plugin.

## Updating an existing Workshop item

Build and upload all three files together for every update. Do not upload only the pak or only the `.utoc`/`.ucas`; they are one IoStore container set.

Once released, keep the Workshop item public and available. Private, removed, or corrupted items cannot be downloaded by clients joining a host. If you want to test a risky update before players see it, upload it as a separate private item first.

## Troubleshooting

| Symptom | Likely cause | Fix |
| --- | --- | --- |
| `No .pak file found` | The upload folder is wrong or empty. | Upload the cooked pak alongside its matching IoStore files. |
| `Expected exactly one .pak` | More than one pak was uploaded. | Make a clean folder containing only one pak set. |
| `missing .ucas/.utoc` | One or both IoStore companion files are absent or misnamed. | Repackage and upload all three matching files. |
| `ends in _P, which is reserved` | The pak's filename ends in `_P`. | Rename the `.pak`, `.utoc`, and `.ucas` together to a stem without `_P`, and upload again. |
| `may only contain its .pak, .utoc and .ucas` | The upload folder had another file or a subfolder in it. | Upload from a clean folder that contains only the three files. |
| `must all be in one plugin; found N roots` | The map's assets are split between your plugin and `/Game/`, or across several plugins. | Move everything the map uses into one content-only plugin, and remove leftovers from the cook. |
| `plugin name ... is a reserved name` or `is already used by HAUNT` | The plugin's name collides with a reserved or existing name. | Rename the plugin to something unique, such as `YourName_MapName`, and cook again. |
| `cooked from a project named ...` or `must all come from one project folder` | The project is named after HAUNT or the engine, or the cook contains files from outside the project. | Give the project its own name, and package only from that project. |
| `already exists in HAUNT` | A `/Game/` asset in your map has the same path as one of HAUNT's. | Move the map and its assets into a content-only plugin. |
| `is encrypted` or `has no full file list` | Pak encryption or index stripping was enabled. | Turn encryption off and package with the default pak settings. |
| `a file type a map may not include` or `a path outside its own folders` | The cook contains executables, scripts, `Binaries` folders, or files from outside the project. | Use a content-only plugin, and package only the map and the assets it uses. |
| `redirects or localizes packages` | The map uses asset localization. | Remove localized asset variants and cook again. |
| `'<asset>' uses '<node or class>', which Workshop maps may not use` | That asset uses a node or class from [Nodes and references Workshop maps can't use](#nodes-and-references-workshop-maps-cant-use). | Replace it with one of the alternatives listed there, and cook again. |
| `references one of HAUNT's own assets` | Something in the map points at HAUNT's content, often through a stand-in asset at one of HAUNT's paths, or a soft reference. | Remove the reference. Use the Reference Viewer to find which asset holds it. |
| `has a damaged package header` | The cook is corrupt or wasn't made with UE 5.8. | Cook again with UE 5.8. |
| `Found N maps inside ...` | The pak contains more than one `.umap` (a second map, sublevels, World Partition cells, or template maps). | Keep a single level with World Partition disabled, remove other maps from the cook list, and repackage. |
| Your subscribed item doesn't appear as a map in HAUNT | The item is missing the `Map` Workshop tag. | Add `"tags" "Map"` to `workshop_item.vdf` and upload again with the same `publishedfileid`. |
| `Could not find a map` | The Asset Registry/map index was stripped, or the map was not cooked. | Add the `[Pak]` settings above and ensure the map is cooked. |
| Map loads without expected assets | A referenced dependency was not included, or it depends on a plugin or custom code HAUNT does not have. | Cook all dependencies with UE 5.8, and use only your own assets and stock engine features. |
| Map is dark or unlit | HAUNT hides its sky for Workshop maps and adds no lighting; the map has no lights of its own, or static lighting was not built. | Add your own sky/sun and local lights, and build lighting before cooking if you use static/stationary lights. |
| Materials render as the default gray material | The pak's shader library could not be found. | Keep the `[Pak]` settings above so `AssetRegistry.bin` stays in the pak, and cook with the default shader library settings. |
| Hunters are stuck or pushed out of the container | Map geometry overlaps the container at `(0, 0, 205)` or blocks its door. | Clear the container footprint and the space on its negative X side. |
| Ghosts do not spawn correctly | No valid floor or not enough room in the spawn grid around `(-700, 0, 300)`. | Add walkable floor with WorldStatic/WorldDynamic collision between Z `-700` and `400`, and keep the grid area clear on the Visibility channel, with at least 185 cm of headroom. |
| A player leaves the lobby while the map loads, or the match is cancelled | That player's (or the host's) game couldn't load the map. The message on their main menu says whether the files were damaged, couldn't be downloaded, or were refused. | Check that every asset the map uses is in the pak and that nothing depends on a plugin or custom code HAUNT does not have, then test again with another player. |
| Some of your assets look wrong or are replaced in HAUNT | Your pak contains assets at the same paths as HAUNT's, for example from a UE template project, a Fab pack, or an edited engine asset. HAUNT's copy is used. | Move your map and all of its assets into a uniquely named content-only plugin (see [Keep your content in its own plugin](#keep-your-content-in-its-own-plugin)). |
| Footsteps are silent on some surfaces | Those surfaces use a physical material of your own. | Remove the physical material from walkable surfaces so they use the engine default (see [Footstep sounds](#footstep-sounds-physical-materials)). |

## Support

- Found something wrong in this guide, or need help with a map that won't load? Open an [issue](https://github.com/ApostolosGames/HauntWorkshop/issues/new/choose).
- Report security problems privately, as described in [SECURITY.md](SECURITY.md).
- Everyone taking part is expected to follow the [Code of Conduct](CODE_OF_CONDUCT.md).

## License

The guide, the SteamCMD template, and the reference templates in this repository are released under the [MIT License](LICENSE). HAUNT itself, its game content, and its trademarks are not covered by this license.
