# glTF Exporter plugin

The glTF Exporter lets Unreal content creators showcase any scene or model on the web in full interactive real-time 3D with minimal effort.


## Compatibility

Currently, the plugin works with Unreal Engine 5.0 on Windows, Mac, and Linux.


## Usage

- Alt 1: Export asset via Content Browser
  - Right-click on a `StaticMesh`, `SkeletalMesh`, `AnimSequence`, `Level`, or `Material` asset in the Content Browser.
  - Select `Asset Actions -> Export...`
  - Change `Save as type` to `.gltf` (or `.glb`) and click `Save`
  - When `glTF Export Options` window is shown, click `Export`
- Alt 2: Export current level via File Menu
  - Select any number of actors in the current level
  - In the top menu, select `File -> Export Selected...`
  - Change `Save as type` to `.gltf` (or `.glb`) and click `Save`
  - When `glTF Export Options` window is shown, click `Export`
- Alt 3: Script the export using `Blueprint` in `Editor` or at `Runtime`, or use `Python` in `Editor`.


## Documentation

- [What is glTF?](Docs/what-is-gltf.md)
- [What can be exported?](Docs/what-can-be-exported.md)
- [Export options reference](Docs/export-options-reference.md)
- [Scripting the export](Docs/scripting-the-export.md)
