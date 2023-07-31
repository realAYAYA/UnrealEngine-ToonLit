# What is glTF?

glTF™ (GL Transmission Format) is an open standard file format (developed and maintained by Khronos Group) to efficiently share rich 3D content between a wide variety of applications. glTF has been specifically designed for compact file size, fast loading, complete scene representation, and extensibility – making it ideal for (but not limited to) web development.

![glTF logo](figures/gltf.png)

## glTF 2.0

Although the initial version of glTF (1.0) was heavily intended for WebGL and OpenGL applications (due to dependence on GLSL shaders), the current version (2.0) is much more run-time independent and instead only relies on well established PBR (physically based rendering) workflows. For these and other technical reasons the exporter (and much of the software ecosystem) exclusively support glTF 2.0, hereafter referred to as just glTF.

## File format

A glTF file can have one of two possible file extensions, `.gltf` (JSON) or `.glb` (binary). When choosing the first the exporter will save the full scene description as a JSON-formatted human-readable text file (`.gltf`), while textures (e.g. `.png` or `.jpeg`) and binary data (`.bin`) will be saved as separate files in the same directory. When choosing the later, the exporter will embed all textures and binary data together with the full scene description into a single self-contained binary file (`.glb`).

![glTF file extensions and related formats](figures/files.png)

## Extensions

Due to the complexities of modern 3D graphics, an engine-independent format such as glTF cannot cover all features, especially in a powerful game engine like Unreal. The good news is that the glTF base format is specifically designed, as previously mentioned, with extensibility in mind. This extensibility is facilitated using so called glTF extensions.

It is important to note that the extensibility does come at a cost - not all applications implement every glTF extension. If an application does not support an extension, it may still be able to load and show other parts of the glTF that do not use the specific extension - unless that extension is stated as explicitly required by the glTF, in which case the application will typically fail to open the entire glTF.

Each glTF extension has a unique name, allowing any application to easily identify all extensions in a glTF file, regardless of whether all are supported by the application in question or not. The first part of the extension name (i.e., prefix) can also be used to identify how well supported the extension was generally when first conceived:

- Khronos ratified extensions use the reserved prefix `KHR` and are typically the most supported.
- Multi-vendor extensions use the reserved prefix `EXT` and are typically supported by more than one vendor (i.e., company or application).
- Vendor-specific extensions use registered prefixes (like `EPIC`) and are typically supported by at least that vendor. Applications by other vendors may also still support such an extension.

To support many of Unreal's features the exporter has implemented the following extensions (all of which can be turned off in the export options):

Extension                   | Description
----------------------------|--------------------------------------------------------
`KHR_lights_punctual`       | Point, spot, and directional lights
`KHR_materials_unlit`       | Materials with unlit shading model
`KHR_materials_clearcoat`   | Materials with clear coat shading model
`KHR_materials_variants`    | Compact multiple material variants per asset
`KHR_mesh_quantization`     | Decrease vertex data size and precision
`KHR_texture_transform`     | Tiling and mirroring texture coordinates
`EPIC_lightmap_textures`    | Lightmass baked Unreal-encoded lightmaps
`EPIC_level_variant_sets`   | Scene variants by Unreal's variant manager
`EPIC_hdri_backdrops`       | Unreal backdrop actors for HDR image projection
`EPIC_sky_spheres`          | Unreal's default sky sphere actors
`EPIC_animation_playback`   | Animation looping, auto play, start time, play rate
`EPIC_texture_hdr_encoding` | HDR encoding for image formats like PNG and JPEG
`EPIC_blend_modes`          | Additive, modulate, and alpha composite blend modes
