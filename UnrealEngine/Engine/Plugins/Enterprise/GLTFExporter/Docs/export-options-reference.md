# Export options reference

Option                         | Description
-------------------------------| ----------------------------------------------------------------------------------------------------------------------------
`Export Uniform Scale`         | Scale factor used for exporting all assets (0.01 by default) for conversion from centimeters (Unreal default) to meters (glTF).
`Export Preview Mesh`          | If enabled, the preview mesh for a standalone animation or material asset will also be exported.
`Strict Compliance`            | If enabled, certain values (like HDR colors and light angles) will be truncated during export to strictly conform to the formal glTF specification.
`Skip Near Default Values`     | If enabled, floating-point-based JSON properties that are nearly equal to their default value will not be exported and thus regarded as exactly default, reducing size of JSON data.
`Include Generator Version`    | If enabled, version info for Unreal Engine and exporter plugin will be included as metadata in the glTF asset, which is useful when reporting issues.
`Export Proxy Materials`       | If enabled, materials that have a proxy defined in their user data, will be exported using that proxy instead. This setting won't affect proxy materials exported or referenced directly.
`Export Unlit Materials`       | If enabled, materials with shading model unlit will be properly exported. Uses extension KHR_materials_unlit.
`Export Clear Coat Materials`  | If enabled, materials with shading model clear coat will be properly exported. Uses extension KHR_materials_clearcoat, which is not supported by all glTF viewers.
`Export Extra Blend Modes`     | If enabled, materials with blend modes additive, modulate, and alpha composite will be properly exported. Uses extension EPIC_blend_modes, which is supported by Unreal's glTF viewer.
`Bake Material Inputs`         | Bake mode determining if and how a material input is baked out to a texture. Baking is only used for non-trivial material inputs (i.e. not simple texture or constant expressions).
`Default Material Bake Size`   | Default size of the baked out texture (containing the material input). Can be overridden by material- and input-specific bake settings, see GLTFMaterialExportOptions.
`Default Material Bake Filter` | Default filtering mode used when sampling the baked out texture. Can be overridden by material- and input-specific bake settings, see GLTFMaterialExportOptions.
`Default Material Bake Tiling` | Default addressing mode used when sampling the baked out texture. Can be overridden by material- and input-specific bake settings, see GLTFMaterialExportOptions.
`Default Input Bake Settings`  | Input-specific default bake settings that override the general defaults above.
`Default Level Of Detail`      | Default LOD level used for exporting a mesh. Can be overridden by component or asset settings (e.g. minimum or forced LOD level).
`Export Vertex Colors`         | If enabled, export vertex color. Not recommended due to vertex colors always being used as a base color multiplier in glTF, regardless of material. Often producing undesirable results.
`Export Vertex Skin Weights`   | If enabled, export vertex bone weights and indices in skeletal meshes. Necessary for animation sequences.
`Use Mesh Quantization`        | If enabled, use quantization for vertex tangents and normals, reducing size. Requires extension KHR_mesh_quantization, which may result in the mesh not loading in some glTF viewers.
`Export Level Sequences`       | If enabled, export level sequences. Only transform tracks are currently supported. The level sequence will be played at the assigned display rate.
`Export Animation Sequences`   | If enabled, export single animation asset used by a skeletal mesh component. Export of vertex skin weights must be enabled.
`Export Playback Settings`     | If enabled, export play rate, start time, looping, and auto play for an animation or level sequence. Uses extension EPIC_animation_playback, which is supported by Unreal's glTF viewer.
`Texture Image Format`         | Desired image format used for exported textures.
`Texture Image Quality`        | Level of compression used for textures exported with lossy image formats, 0 (default) or value between 1 (worst quality, best compression) and 100 (best quality, worst compression).
`No Lossy Image Format For`    | Texture types that will always use lossless formats (e.g. PNG) because of sensitivity to compression artifacts.
`Export Texture Transforms`    | If enabled, export UV tiling and un-mirroring settings in a texture coordinate expression node for simple material input expressions. Uses extension KHR_texture_transform.
`Export Lightmaps`             | If enabled, export lightmaps (created by Lightmass) when exporting a level. Uses extension EPIC_lightmap_textures, which is supported by Unreal's glTF viewer.
`Texture HDR Encoding`         | Encoding used to store textures that have pixel colors with more than 8-bit per channel. Uses extension EPIC_texture_hdr_encoding, which is supported by Unreal's glTF viewer.
`Adjust Normalmaps`            | If enabled, exported normalmaps will be adjusted from Unreal to glTF convention (i.e. the green channel is flipped).
`Export Hidden In Game`        | If enabled, export actors and components that are flagged as hidden in-game.
`Export Lights`                | Mobility of directional, point, and spot light components that will be exported. Uses extension KHR_lights_punctual.
`Export Cameras`               | If enabled, export camera components.
`Export HDRI Backdrops`        | If enabled, export HDRIBackdrop blueprints. Uses extension EPIC_hdri_backdrops, which is supported by Unreal's glTF viewer.
`Export Sky Spheres`           | If enabled, export SkySphere blueprints. Uses extension EPIC_sky_spheres, which is supported by Unreal's glTF viewer.
`Variant Sets Mode`            | Mode determining if and how to export LevelVariantSetsActors.
`Export Material Variants`     | Mode determining if and how to export material variants that change the materials property on a static or skeletal mesh component.
`Export Mesh Variants`         | If enabled, export variants that change the mesh property on a static or skeletal mesh component.
`Export Visibility Variants`   | If enabled, export variants that change the visible property on a scene component.
