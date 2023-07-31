// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UserData/GLTFMaterialUserData.h"
#include "GLTFExportOptions.generated.h"

UENUM(BlueprintType)
enum class EGLTFTextureImageFormat : uint8
{
	/** Don't export any textures. */
	None,
	/** Always use PNG (lossless compression). */
	PNG,
	/** If texture does not have an alpha channel, use JPEG (lossy compression); otherwise fallback to PNG. */
	JPEG UMETA(DisplayName = "JPEG (if no alpha)")
};

UENUM(BlueprintType, Meta = (Bitflags, UseEnumValuesAsMaskValuesInEditor = "true"))
enum class EGLTFTextureType : uint8
{
	None = 0 UMETA(Hidden),

	HDR = 1 << 0,
	Normalmaps = 1 << 1,
	Lightmaps = 1 << 2,

	All = HDR | Normalmaps | Lightmaps UMETA(Hidden)
};
ENUM_CLASS_FLAGS(EGLTFTextureType);

UENUM(BlueprintType)
enum class EGLTFTextureHDREncoding : uint8
{
	/** Clamp HDR colors to standard 8-bit per channel. */
	None,
	/** Encode HDR colors to RGBM (will discard alpha). */
	RGBM
};

UENUM(BlueprintType, Meta = (Bitflags, UseEnumValuesAsMaskValuesInEditor = "true"))
enum class EGLTFSceneMobility : uint8
{
	None = 0 UMETA(Hidden),

	Static = 1 << 0,
	Stationary = 1 << 1,
	Movable = 1 << 2,

	All = Static | Stationary | Movable UMETA(Hidden)
};
ENUM_CLASS_FLAGS(EGLTFSceneMobility);

UENUM(BlueprintType)
enum class EGLTFVariantSetsMode : uint8
{
	/** Never export variants sets. */
	None,
	/** Uses the official extension KHR_materials_variants. Supports material variants only. */
	Khronos,
	/** Uses the extension EPIC_level_variant_sets, which is supported by Unreal's glTF viewer. */
	Epic
};

UENUM(BlueprintType)
enum class EGLTFMaterialVariantMode : uint8
{
	/** Never export material variants. */
	None,
	/** Export material variants but only use a simple quad if a material input needs to be baked out. */
	Simple,
	/** Export material variants and allow usage of the mesh data if a material input needs to be baked out with vertex data. */
	UseMeshData,
};

UCLASS(BlueprintType, Config=EditorPerProjectUserSettings, HideCategories=(DebugProperty))
class GLTFEXPORTER_API UGLTFExportOptions : public UObject
{
	GENERATED_UCLASS_BODY()

	/** Scale factor used for exporting all assets (0.01 by default) for conversion from centimeters (Unreal default) to meters (glTF). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Config, Category = General)
	float ExportUniformScale;

	/** If enabled, the preview mesh for a standalone animation or material asset will also be exported. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Config, Category = General)
	bool bExportPreviewMesh;

	/** If enabled, certain values (like HDR colors and light angles) will be truncated during export to strictly conform to the formal glTF specification. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Config, Category = General)
	bool bStrictCompliance;

	/** If enabled, floating-point-based JSON properties that are nearly equal to their default value will not be exported and thus regarded as exactly default, reducing JSON size. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Config, Category = General)
	bool bSkipNearDefaultValues;

	/** If enabled, version info for Unreal Engine and exporter plugin will be included as metadata in the glTF asset, which is useful when reporting issues. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Config, Category = General)
	bool bIncludeGeneratorVersion;

	/** If enabled, materials that have a proxy defined in their user data, will be exported using that proxy instead. This setting won't affect proxy materials exported or referenced directly. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Config, Category = Material)
	bool bExportProxyMaterials;

	/** If enabled, materials with shading model unlit will be properly exported. Uses extension KHR_materials_unlit. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Config, Category = Material)
	bool bExportUnlitMaterials;

	/** If enabled, materials with shading model clear coat will be properly exported. Uses extension KHR_materials_clearcoat, which is not supported by all glTF viewers. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Config, Category = Material)
	bool bExportClearCoatMaterials;

	/** If enabled, materials with blend modes additive, modulate, and alpha composite will be properly exported. Uses extension EPIC_blend_modes, which is supported by Unreal's glTF viewer. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Config, Category = Material)
	bool bExportExtraBlendModes;

	/** Bake mode determining if and how a material input is baked out to a texture. Baking is only used for non-trivial material inputs (i.e. not simple texture or constant expressions). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Config, Category = Material)
	EGLTFMaterialBakeMode BakeMaterialInputs;

	/** Default size of the baked out texture (containing the material input). Can be overridden by material- and input-specific bake settings, see GLTFMaterialExportOptions. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Config, Category = Material, Meta = (EditCondition = "BakeMaterialInputs != EGLTFMaterialBakeMode::Disabled"))
	EGLTFMaterialBakeSizePOT DefaultMaterialBakeSize;

	/** Default filtering mode used when sampling the baked out texture. Can be overridden by material- and input-specific bake settings, see GLTFMaterialExportOptions. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Config, Category = Material, Meta = (EditCondition = "BakeMaterialInputs != EGLTFMaterialBakeMode::Disabled", ValidEnumValues = "TF_Nearest, TF_Bilinear, TF_Trilinear"))
	TEnumAsByte<TextureFilter> DefaultMaterialBakeFilter;

	/** Default addressing mode used when sampling the baked out texture. Can be overridden by material- and input-specific bake settings, see GLTFMaterialExportOptions. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Config, Category = Material, Meta = (EditCondition = "BakeMaterialInputs != EGLTFMaterialBakeMode::Disabled"))
	TEnumAsByte<TextureAddress> DefaultMaterialBakeTiling;

	/** Input-specific default bake settings that override the general defaults above. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Config, Category = Material, Meta = (EditCondition = "BakeMaterialInputs != EGLTFMaterialBakeMode::Disabled"))
	TMap<EGLTFMaterialPropertyGroup, FGLTFOverrideMaterialBakeSettings> DefaultInputBakeSettings;

	/** Default LOD level used for exporting a mesh. Can be overridden by component or asset settings (e.g. minimum or forced LOD level). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Config, Category = Mesh, Meta = (ClampMin = "0"))
	int32 DefaultLevelOfDetail;

	/** If enabled, export vertex color. Not recommended due to vertex colors always being used as a base color multiplier in glTF, regardless of material. Often producing undesirable results. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Config, Category = Mesh)
	bool bExportVertexColors;

	/** If enabled, export vertex bone weights and indices in skeletal meshes. Necessary for animation sequences. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Config, Category = Mesh)
	bool bExportVertexSkinWeights;

	/** If enabled, use quantization for vertex tangents and normals, reducing size. Requires extension KHR_mesh_quantization, which may result in the mesh not loading in some glTF viewers. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Config, Category = Mesh)
	bool bUseMeshQuantization;

	/** If enabled, export level sequences. Only transform tracks are currently supported. The level sequence will be played at the assigned display rate. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Config, Category = Animation)
	bool bExportLevelSequences;

	/** If enabled, export single animation asset used by a skeletal mesh component. Export of vertex skin weights must be enabled. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Config, Category = Animation, Meta = (EditCondition = "bExportVertexSkinWeights"))
	bool bExportAnimationSequences;

	/** If enabled, export play rate, start time, looping, and auto play for an animation or level sequence. Uses extension EPIC_animation_playback, which is supported by Unreal's glTF viewer. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Config, Category = Animation)
	bool bExportPlaybackSettings;

	/** Desired image format used for exported textures. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Config, Category = Texture)
	EGLTFTextureImageFormat TextureImageFormat;

	/** Level of compression used for textures exported with lossy image formats, 0 (default) or value between 1 (worst quality, best compression) and 100 (best quality, worst compression). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Config, Category = Texture, Meta = (ClampMin = "0", ClampMax = "100", EditCondition = "TextureImageFormat == EGLTFTextureImageFormat::JPEG"))
	int32 TextureImageQuality;

	/** Texture types that will always use lossless formats (e.g. PNG) because of sensitivity to compression artifacts. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Config, Category = Texture, Meta = (Bitmask, BitmaskEnum = "/Script/GLTFExporter.EGLTFTextureType", EditCondition = "TextureImageFormat == EGLTFTextureImageFormat::JPEG"))
	int32 NoLossyImageFormatFor; // Bitmask combined from EGLTFTextureType

	/** If enabled, export UV tiling and un-mirroring settings in a texture coordinate expression node for simple material input expressions. Uses extension KHR_texture_transform. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Config, Category = Texture, Meta = (EditCondition = "TextureImageFormat != EGLTFTextureImageFormat::None"))
	bool bExportTextureTransforms;

	/** If enabled, export lightmaps (created by Lightmass) when exporting a level. Uses extension EPIC_lightmap_textures, which is supported by Unreal's glTF viewer. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Config, Category = Texture, Meta = (EditCondition = "TextureImageFormat != EGLTFTextureImageFormat::None"))
	bool bExportLightmaps;

	/** Encoding used to store textures that have pixel colors with more than 8-bit per channel. Uses extension EPIC_texture_hdr_encoding, which is supported by Unreal's glTF viewer. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Config, Category = Texture, Meta = (DisplayName = "Texture HDR Encoding", EditCondition = "TextureImageFormat != EGLTFTextureImageFormat::None"))
	EGLTFTextureHDREncoding TextureHDREncoding;

	/** If enabled, exported normalmaps will be adjusted from Unreal to glTF convention (i.e. the green channel is flipped). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Config, Category = Texture, Meta = (EditCondition = "TextureImageFormat != EGLTFTextureImageFormat::None"))
	bool bAdjustNormalmaps;

	/** If enabled, export actors and components that are flagged as hidden in-game. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Config, Category = Scene)
	bool bExportHiddenInGame;

	/** Mobility of directional, point, and spot light components that will be exported. Uses extension KHR_lights_punctual. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Config, Category = Scene, Meta = (Bitmask, BitmaskEnum = "/Script/GLTFExporter.EGLTFSceneMobility"))
	int32 ExportLights; // Bitmask combined from EGLTFSceneMobility

	/** If enabled, export camera components. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Config, Category = Scene)
	bool bExportCameras;

	/** If enabled, export HDRIBackdrop blueprints. Uses extension EPIC_hdri_backdrops, which is supported by Unreal's glTF viewer. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Config, Category = Scene, Meta = (DisplayName = "Export HDRI Backdrops"))
	bool bExportHDRIBackdrops;

	/** If enabled, export SkySphere blueprints. Uses extension EPIC_sky_spheres, which is supported by Unreal's glTF viewer. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Config, Category = Scene)
	bool bExportSkySpheres;

	/** Mode determining if and how to export LevelVariantSetsActors. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Config, Category = Scene)
	EGLTFVariantSetsMode VariantSetsMode;

	/** Mode determining if and how to export material variants that change the materials property on a static or skeletal mesh component. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Config, Category = VariantSets, Meta = (EditCondition = "VariantSetsMode != EGLTFVariantSetsMode::None"))
	EGLTFMaterialVariantMode ExportMaterialVariants;

	/** If enabled, export variants that change the mesh property on a static or skeletal mesh component. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Config, Category = VariantSets, Meta = (EditCondition = "VariantSetsMode == EGLTFVariantSetsMode::Epic"))
	bool bExportMeshVariants;

	/** If enabled, export variants that change the visible property on a scene component. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Config, Category = VariantSets, Meta = (EditCondition = "VariantSetsMode == EGLTFVariantSetsMode::Epic"))
	bool bExportVisibilityVariants;

	UFUNCTION(BlueprintCallable, Category = General)
	void ResetToDefault();
};
