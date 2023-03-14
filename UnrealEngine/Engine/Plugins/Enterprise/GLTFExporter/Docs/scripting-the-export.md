# Scripting the export

In addition to using the UI to do export of assets or scene it is also possible to use the plugin to export through blueprint/python in editor or at runtime.

## Editor

The main function to use is `Export to GLTF`. This function takes an object to export, a destination path, the export options and selected actors.
The object can be any of the asset that are supported cf. [What can be exported?](Docs/what-can-be-exported.md)

To export a level you should pass a reference to the current world. The `Selected Actors` set must be provided even if it is empty.
It is used in the context of a level export to define only a subset of the scene to be exported.

![The blueprint function to export an asset or a scene](figures/Blueprint-Export.png)

Here is a python code snippet to export all assets in subfolder as individual gltf files

```python
import unreal

assetPath = '/game/models/'
outputDir = 'c:/Temp/glTFExport/'
glTFExportOptions = unreal.GLTFExportOptions()
selectedActors = set()

staticMestPaths = unreal.EditorAssetLibrary.list_assets(assetPath)
for smp in staticMestPaths:
    sm = unreal.EditorAssetLibrary.load_asset(smp)
    if unreal.MathLibrary.class_is_child_of(sm.get_class(), unreal.StaticMesh):
        exportpath = outputDir+sm.get_name()+'/'+sm.get_name()+'.gltf'
        unreal.GLTFExporter.export_to_gltf(sm,exportpath,glTFExportOptions,selectedActors)
```

## Runtime

As in editor a glTF export can be triggered through the call to `Export to GLTF`. 

Material baking not being available at runtime users have two options:
- prebake materials in editor and ship them in their application. Those prebake materials will then be used on export.
- use C++ and access the GLTFBuilder API at runtime to modify the content of the glTF file before export.

![Proxy material creation and resulting changes](figures/Proxy-Material.png)