"""
Writes the package dependency closure of one map (hard and soft references, /Script excluded) to a text file,
one package name per line. Run by PackageWorkshopMap.ps1 as a commandlet; it passes the map and the output path
in the HAUNT_MAP and HAUNT_DEPS_OUT environment variables.
"""
import os
import unreal

MAP = os.environ["HAUNT_MAP"]
OUT = os.environ["HAUNT_DEPS_OUT"]

registry = unreal.AssetRegistryHelpers.get_asset_registry()
registry.search_all_assets(True)
options = unreal.AssetRegistryDependencyOptions(
    include_soft_package_references=True, include_hard_package_references=True,
    include_searchable_names=False, include_soft_management_references=False,
    include_hard_management_references=False)

seen, todo = set(), [MAP]
while todo:
    package = todo.pop()
    if package in seen:
        continue
    seen.add(package)
    for dependency in registry.get_dependencies(unreal.Name(package), options) or []:
        dependency = str(dependency)
        if not dependency.startswith("/Script") and dependency not in seen:
            todo.append(dependency)

os.makedirs(os.path.dirname(OUT), exist_ok=True)
with open(OUT, "w") as f:
    f.write("\n".join(sorted(seen)) + "\n")
unreal.log_warning("[MapDependencies] %d packages -> %s" % (len(seen), OUT))
