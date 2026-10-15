"""Compare a Workshop map project's renderer settings with HAUNT's, and stamp them into the map plugin.

Material shaders are compiled with the cooking project's [/Script/Engine.RendererSettings]. A map cooked with
different settings (Substrate turned on, for example) loads in HAUNT, but its materials render as noise. HAUNT uses
UE 5.8's defaults for every setting checked here, so a blank UE 5.8 project matches.

    check   <Project.uproject> --engine <UE_5.8 dir>               list every setting that differs from HAUNT's
    stamp   <Project.uproject> --engine <UE_5.8 dir> --plugin <P>  check, then write the settings into P.uplugin
    export  <HAUNT.uproject>   --engine <UE_5.8 dir>               regenerate Templates/haunt_renderer_settings.json

HAUNT reads the "HauntRendererSettings" object from the cooked .uplugin and refuses the map if any value differs
from its own. normalize() must stay in step with WorkshopRendererSettings::Normalize in HAUNT.
"""
import argparse
import json
import re
import sys
from pathlib import Path

SECTION = "/Script/Engine.RendererSettings"
REFERENCE = Path(__file__).resolve().parent.parent / "Templates" / "haunt_renderer_settings.json"

# Settings that change how material and vertex factory shaders are compiled, or the GBuffer they write, with
# UE 5.8's built-in default for each (the value a blank project gets; BaseEngine.ini sets none of them). A setting
# a project leaves unset counts as its default. Settings that only add or remove shader permutations a map
# doesn't need (path tracing, heterogeneous volumes, static lighting) or that are runtime-only are left out.
ENGINE_DEFAULTS = {
    "r.Substrate": "0",
    "r.GBufferFormat": "1",
    "r.ForwardShading": "0",
    "r.VirtualTextures": "1",
    "r.VT.TileSize": "128",
    "r.VT.TileBorderSize": "4",
    "r.VT.AnisotropicFiltering": "0",
    "r.MeshPaintVirtualTexture.Support": "1",
    "r.StaticMesh.DefaultMeshPaintTextureSupport": "1",
    "Compat.UseDXT5NormalMaps": "0",
    "r.ClearCoatNormal": "0",
    "r.DBuffer": "1",
    "r.SelectiveBasePassOutputs": "0",
    "r.VelocityOutputPass": "0",
    "r.Velocity.EnableVertexDeformation": "2",
    "r.AllowGlobalClipPlane": "0",
    "r.EarlyZPassOnlyMaterialMasking": "1",
    "r.Material.RoughDiffuse": "0",
    "r.Material.EnergyConservation": "0",
    "r.LegacyLuminanceFactors": "0",
    "r.SkinCache.CompileShaders": "0",
    "r.GPUSkin.Limit2BoneInfluences": "0",
}

NUMERIC = re.compile(r"[+-]?(\d+\.?\d*|\.\d+)")


def normalize(value):
    value = value.strip().strip('"').lower()
    if value == "true":
        return "1"
    if value == "false":
        return "0"
    if NUMERIC.fullmatch(value):
        number = ("%.6f" % float(value)).rstrip("0").rstrip(".")
        return "0" if number == "-0" else number
    return value


def read_section(ini_path, values):
    """Applies one ini file's [RendererSettings] lines on top of values (later files win, as in UE)."""
    if not ini_path.is_file():
        return
    in_section = False
    for raw in ini_path.read_text(encoding="utf-8-sig", errors="replace").splitlines():
        line = raw.strip()
        if not line or line.startswith(";"):
            continue
        if line.startswith("[") and line.endswith("]"):
            in_section = line[1:-1] == SECTION
            continue
        if not in_section or "=" not in line:
            continue
        key, value = line.split("=", 1)
        key = key.strip()
        if key[:1] in "+-.!":
            if key[0] == "-" or key[0] == "!":
                values.pop(key[1:], None)
                continue
            key = key[1:]
        values[key] = value


def effective_settings(project, engine):
    """UE's Engine ini hierarchy for Windows, reduced to the renderer section."""
    project_dir = project.parent
    config = engine / "Engine" / "Config" if (engine / "Engine").is_dir() else engine / "Config"
    values = {}
    for ini in (config / "BaseEngine.ini",
                config / "Windows" / "BaseWindowsEngine.ini",
                config / "Windows" / "WindowsEngine.ini",
                project_dir / "Config" / "DefaultEngine.ini",
                project_dir / "Config" / "Windows" / "WindowsEngine.ini"):
        read_section(ini, values)
    return {key: normalize(values[key]) if key in values else default for key, default in ENGINE_DEFAULTS.items()}


def load_reference():
    data = json.loads(REFERENCE.read_text(encoding="utf-8"))
    return data["HauntRendererSettings"]


def differences(settings, reference):
    return [(key, settings.get(key, ""), value) for key, value in reference.items() if settings.get(key, "") != value]


def find_plugin(project, name):
    for path in (project.parent / "Plugins").rglob(name + ".uplugin"):
        return path
    return None


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("command", choices=["check", "stamp", "export"])
    parser.add_argument("project", type=Path, help="the .uproject")
    parser.add_argument("--engine", type=Path, required=True, help="the UE 5.8 install folder (contains Engine/)")
    parser.add_argument("--plugin", help="stamp: the map's content plugin name")
    args = parser.parse_args()

    settings = effective_settings(args.project.resolve(), args.engine.resolve())

    if args.command == "export":
        REFERENCE.write_text(json.dumps({"HauntRendererSettings": settings}, indent="\t") + "\n", encoding="utf-8")
        print("Wrote %d settings to %s" % (len(settings), REFERENCE))
        return 0

    diffs = differences(settings, load_reference())
    if diffs:
        print("These renderer settings differ from HAUNT's, which uses UE 5.8's defaults for all of them.")
        print("Set them back in Project Settings > Rendering (or delete them from the [%s]" % SECTION)
        print("section of Config/DefaultEngine.ini), restart the editor, and cook again.\n")
        for key, mine, haunt in diffs:
            print("  %-48s yours %-6s HAUNT %s" % (key, mine, haunt))
        return 1
    print("Renderer settings match HAUNT's (%d checked)." % len(settings))

    if args.command == "stamp":
        if not args.plugin:
            parser.error("stamp needs --plugin")
        descriptor = find_plugin(args.project.resolve(), args.plugin)
        if not descriptor:
            print("Could not find %s.uplugin under %s" % (args.plugin, args.project.parent / "Plugins"))
            return 1
        plugin = json.loads(descriptor.read_text(encoding="utf-8-sig"))
        plugin["HauntRendererSettings"] = settings
        descriptor.write_text(json.dumps(plugin, indent="\t") + "\n", encoding="utf-8")
        print("Stamped %s" % descriptor)
    return 0


if __name__ == "__main__":
    sys.exit(main())
