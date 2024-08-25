// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#if USE_USD_SDK

#include "CoreMinimal.h"
#include "Engine/TextureDefines.h"
#include "Materials/MaterialInterface.h"
#include "UObject/Package.h"
#include "UObject/WeakObjectPtr.h"

#include "UsdWrappers/SdfLayer.h"

#include "USDIncludesStart.h"
#include "pxr/base/tf/token.h"
#include "pxr/usd/usd/timeCode.h"
#include "pxr/usd/usdShade/tokens.h"
#include "USDIncludesEnd.h"

PXR_NAMESPACE_OPEN_SCOPE
	class UsdAttribute;
	class UsdGeomMesh;
	class UsdPrim;
	class UsdShadeMaterial;
PXR_NAMESPACE_CLOSE_SCOPE

class FSHAHash;
class UMaterial;
class UMaterialOptions;
class UTexture;
class UUsdAssetCache;
class UUsdAssetCache2;
enum class EFlattenMaterialProperties : uint8;
enum EMaterialProperty : int;
enum TextureAddress : int;
enum TextureCompressionSettings : int;
struct FFlattenMaterial;
struct FPropertyEntry;

namespace UE
{
	class FUsdPrim;
}

namespace UsdToUnreal
{
	/**
	 * Extracts material data from UsdShadeMaterial and places the results in Material. Note that since this is used for UMaterialInstanceDynamics at
	 * runtime as well, it will not set base property overrides (e.g. BlendMode) or the parent material, and will just assume that the caller handles
	 * that. Note that in order to receive the primvar to UV index mapping calculated within this function, the provided Material should have an
	 * UUsdMaterialAssetImportData object as its AssetImportData.
	 * @param UsdShadeMaterial - Shade material with the data to convert
	 * @param Material - Output parameter that will be filled with the converted data. Only the versions that receive a dynamic material instance will
	 * work at runtime
	 * @param TexturesCache - Cache to prevent importing a texture more than once
	 * @param RenderContext - Which render context output to read from the UsdShadeMaterial
	 * @param ReuseIdenticalAssets - Whether to reuse identical textures found in the TextureCache or to create dedicated textures for each material
	 * @return Whether the conversion was successful or not.
	 */
	USDUTILITIES_API bool ConvertMaterial(
		const pxr::UsdShadeMaterial& UsdShadeMaterial,
		UMaterialInstance& Material,
		UUsdAssetCache2* TexturesCache = nullptr,
		const TCHAR* RenderContext = nullptr,
		bool bReuseIdenticalAssets = true
	);
	USDUTILITIES_API bool ConvertMaterial(
		const pxr::UsdShadeMaterial& UsdShadeMaterial,
		UMaterial& Material,
		UUsdAssetCache2* TexturesCache = nullptr,
		const TCHAR* RenderContext = nullptr,
		bool bReuseIdenticalAssets = true
	);

	/**
	 * Attemps to assign the values of the surface shader inputs to the MaterialInstance parameters by matching the inputs display names to the
	 * parameters names.
	 * @param UsdShadeMaterial - Shade material with the data to convert
	 * @param MaterialInstance - Material instance on which we will set the parameter values
	 * @param TexturesCache - Cache to prevent importing a texture more than once
	 * @param RenderContext - The USD render context to use when fetching the surface shader
	 * @param ReuseIdenticalAssets - Whether to reuse identical textures found in the TextureCache or to create dedicated textures for each material
	 * @return Whether the conversion was successful or not.
	 *
	 */
	USDUTILITIES_API bool ConvertShadeInputsToParameters(
		const pxr::UsdShadeMaterial& UsdShadeMaterial,
		UMaterialInstance& MaterialInstance,
		UUsdAssetCache2* TexturesCache,
		const TCHAR* RenderContext = nullptr,
		bool bReuseIdenticalAssets = true
	);

	PRAGMA_DISABLE_DEPRECATION_WARNINGS
	UE_DEPRECATED(5.3, "Please use the overload that doesn't use PrimvarToUVIndex anymore")
	USDUTILITIES_API bool ConvertMaterial(
		const pxr::UsdShadeMaterial& UsdShadeMaterial,
		UMaterial& Material,
		UUsdAssetCache2* TexturesCache,
		TMap<FString, int32>& PrimvarToUVIndex,
		const TCHAR* RenderContext = nullptr
	);
	UE_DEPRECATED(5.3, "Use the other overload that receives an UUsdAssetCache2 object instead")
	USDUTILITIES_API bool ConvertMaterial(
		const pxr::UsdShadeMaterial& UsdShadeMaterial,
		UMaterialInstance& Material,
		UUsdAssetCache2* TexturesCache,
		TMap<FString, int32>& PrimvarToUVIndex,
		const TCHAR* RenderContext = nullptr
	);
	UE_DEPRECATED(5.2, "Use the other overload that receives an UUsdAssetCache2 object instead")
	USDUTILITIES_API bool ConvertMaterial(
		const pxr::UsdShadeMaterial& UsdShadeMaterial,
		UMaterialInstance& Material,
		UUsdAssetCache* TexturesCache,
		TMap<FString, int32>& PrimvarToUVIndex,
		const TCHAR* RenderContext = nullptr
	);
	UE_DEPRECATED(5.2, "Use the other overload that receives an UUsdAssetCache2 object instead")
	USDUTILITIES_API bool ConvertMaterial(
		const pxr::UsdShadeMaterial& UsdShadeMaterial,
		UMaterial& Material,
		UUsdAssetCache* TexturesCache,
		TMap<FString, int32>& PrimvarToUVIndex,
		const TCHAR* RenderContext = nullptr
	);
	UE_DEPRECATED(5.2, "Use the other overload that receives an UUsdAssetCache2 object instead")
	USDUTILITIES_API bool ConvertShadeInputsToParameters(
		const pxr::UsdShadeMaterial& UsdShadeMaterial,
		UMaterialInstance& MaterialInstance,
		UUsdAssetCache* TexturesCache,
		const TCHAR* RenderContext = nullptr
	);
	PRAGMA_ENABLE_DEPRECATION_WARNINGS
}

#if WITH_EDITOR
namespace UnrealToUsd
{
	/**
	 * Bakes InMaterial into textures and constants, and configures OutUsdShadeMaterial to use the baked data.
	 * @param InMaterial - Source material to bake
	 * @param InMaterialProperties - Material properties to bake from InMaterial
	 * @param InDefaultTextureSize - Size of the baked texture to use for any material property that does not have a custom size set
	 * @param InTexturesDir - Directory where the baked textures will be placed
	 * @param OutUsdShadeMaterialPrim - UsdPrim with the UsdShadeMaterial schema that will be configured to use the baked textures and constants
	 * @param bInDecayTexturesToSinglePixel - Whether to use a single value directly on the material instead of writing out textures with that
											  have a single uniform color value for all pixels
	 * @return Whether the conversion was successful or not.
	 */
	USDUTILITIES_API bool ConvertMaterialToBakedSurface(
		const UMaterialInterface& InMaterial,
		const TArray<FPropertyEntry>& InMaterialProperties,
		const FIntPoint& InDefaultTextureSize,
		const FDirectoryPath& InTexturesDir,
		pxr::UsdPrim& OutUsdShadeMaterialPrim,
		bool bInDecayTexturesToSinglePixel = true
	);

	/**
	 * Converts a flattened material's data into textures placed at InTexturesDir, and configures OutUsdShadeMaterial to use the baked textures.
	 * Note that to avoid a potentially useless copy, InMaterial's samples will be modified in place to have 255 alpha before being exported to
	 * textures.
	 *
	 * @param MaterialName - Name of the material, used as prefix on the exported texture filenames
	 * @param InMaterial - Source material to bake
	 * @param InMaterialProperties - Object used *exclusively* to provide floating point constant values if necessary
	 * @param InTexturesDir - Directory where the baked textures will be placed
	 * @param OutUsdShadeMaterialPrim - UsdPrim with the UsdShadeMaterial schema that will be configured to use the baked textures and constants
	 * @return Whether the conversion was successful or not.
	 */
	USDUTILITIES_API bool ConvertFlattenMaterial(
		const FString& InMaterialName,
		FFlattenMaterial& InMaterial,
		const TArray<FPropertyEntry>& InMaterialProperties,
		const FDirectoryPath& InTexturesDir,
		UE::FUsdPrim& OutUsdShadeMaterialPrim
	);
}
#endif	  // WITH_EDITOR

namespace UsdUtils
{
	// Writes UnrealMaterialPathName as a material binding for MeshOrGeomSubsetPrim, either by reusing an existing UsdShadeMaterial
	// binding if it already has an 'unreal' render context output and the expected structure, or by creating a new Material prim
	// that fulfills those requirements.
	// Doesn't write to the 'unrealMaterial' attribute at all, as we intend on deprecating it in the future.
	USDUTILITIES_API void AuthorUnrealMaterialBinding(pxr::UsdPrim& MeshOrGeomSubsetPrim, const FString& UnrealMaterialPathName);

	/** Returns a path to an UE asset (e.g. "/Game/Assets/Red.Red") if MaterialPrim has an 'unreal' render context surface output that points at one
	 */
	USDUTILITIES_API TOptional<FString> GetUnrealSurfaceOutput(const pxr::UsdPrim& MaterialPrim);

	/**
	 * Sets which UE material asset the 'unreal' render context surface output of MaterialPrim is pointing at (creating the surface output
	 * on-demand if needed)
	 *
	 * @param MaterialPrim - pxr::UsdPrim with the pxr::UsdShadeMaterial schema to update the 'unreal' surface output of
	 * @param UnrealMaterialPathName - Path to an UE UMaterialInterface asset (e.g. "/Game/Assets/Red.Red")
	 * @return Whether we successfully set the surface output or not
	 */
	USDUTILITIES_API bool SetUnrealSurfaceOutput(pxr::UsdPrim& MaterialPrim, const FString& UnrealMaterialPathName);

	/**
	 * Clears any opinions for the 'unreal' render context surface output of MaterialPrim within LayerToAuthorIn.
	 * If LayerToAuthorIn is an invalid layer (the default) it will clear opinions from all layers of the stage's layer stack.
	 *
	 * @param MaterialPrim - pxr::UsdPrim with the pxr::UsdShadeMaterial schema to update the 'unreal' surface output of
	 * @param LayerToAuthorIn - Layer to clear the opinions in, or an invalid layer (e.g. UE::FSdfLayer{}, which is the default)
	 * @return Whether we successfully cleared the opinions or not
	 */
	UE_DEPRECATED(5.2, "No longer used as UE material assignments are only visible in the 'unreal' render context anyway")
	USDUTILITIES_API bool RemoveUnrealSurfaceOutput(pxr::UsdPrim& MaterialPrim, const UE::FSdfLayer& LayerToAuthorIn = UE::FSdfLayer{});

	/**
	 * Returns whether the material needs to be rendered with the Translucent rendering mode.
	 * This function exists because we need this information *before* we pick the right parent for a material instance and properly convert it.
	 */
	USDUTILITIES_API bool IsMaterialTranslucent(const pxr::UsdShadeMaterial& UsdShadeMaterial);

	USDUTILITIES_API FSHAHash HashShadeMaterial(	//
		const pxr::UsdShadeMaterial& UsdShadeMaterial,
		const pxr::TfToken& RenderContext = pxr::UsdShadeTokens->universalRenderContext
	);
	USDUTILITIES_API void HashShadeMaterial(	//
		const pxr::UsdShadeMaterial& UsdShadeMaterial,
		FSHA1& InOutHash,
		const pxr::TfToken& RenderContext = pxr::UsdShadeTokens->universalRenderContext
	);

	/** Returns the resolved path from an pxr::SdfAssetPath attribute. For UDIMs path, returns the path to the 1001 tile. */
	USDUTILITIES_API FString GetResolvedAssetPath(const pxr::UsdAttribute& AssetPathAttr, pxr::UsdTimeCode TimeCode = pxr::UsdTimeCode::Default());

	UE_DEPRECATED(5.4, "This function has been renamed to 'GetResolvedAssetPath', as it should work for any asset type")
	USDUTILITIES_API FString GetResolvedTexturePath(const pxr::UsdAttribute& TextureAssetPathAttr);

	/**
	 * Computes and returns the hash string for the texture at the given path.
	 * Handles regular texture asset paths as well as asset paths identifying textures inside Usdz archives.
	 * Returns an empty string if the texture could not be hashed.
	 */
	USDUTILITIES_API FString GetTextureHash(
		const FString& ResolvedTexturePath,
		bool bSRGB,
		TextureCompressionSettings CompressionSettings,
		TextureAddress AddressX,
		TextureAddress AddressY
	);

	/** Creates a texture from a pxr::SdfAssetPath attribute. PrimPath is optional, and should point to the source shadematerial prim path. It will be
	 * placed in its UUsdAssetUserData */
	USDUTILITIES_API UTexture* CreateTexture(
		const pxr::UsdAttribute& TextureAssetPathAttr,
		const FString& PrimPath = FString(),
		TextureGroup LODGroup = TEXTUREGROUP_World,
		UObject* Outer = GetTransientPackage()
	);

	/** Checks if this texture needs virtual textures and emits a warning if it is disabled for the project */
	USDUTILITIES_API void NotifyIfVirtualTexturesNeeded(UTexture* Texture);

#if WITH_EDITOR
	/** Convert between the two different types used to represent material channels to bake */
	USDUTILITIES_API EFlattenMaterialProperties MaterialPropertyToFlattenProperty(EMaterialProperty MaterialProperty);

	/** Convert between the two different types used to represent material channels to bake */
	USDUTILITIES_API EMaterialProperty FlattenPropertyToMaterialProperty(EFlattenMaterialProperties FlattenProperty);
#endif	  // WITH_EDITOR

	/** Converts channels that have the same value for every pixel into a channel that only has a single pixel with that value */
	USDUTILITIES_API void CollapseConstantChannelsToSinglePixel(FFlattenMaterial& InMaterial);

	/** Temporary function until UnrealWrappers can create attributes, just adds a custom bool attribute 'worldSpaceNormals' as true */
	USDUTILITIES_API bool MarkMaterialPrimWithWorldSpaceNormals(const UE::FUsdPrim& MaterialPrim);

	/** Sets material instance parameters whether Material is a MaterialInstanceConstant or a MaterialInstanceDynamic */
	USDUTILITIES_API void SetScalarParameterValue(UMaterialInstance& Material, const TCHAR* ParameterName, float ParameterValue);
	USDUTILITIES_API void SetVectorParameterValue(UMaterialInstance& Material, const TCHAR* ParameterName, FLinearColor ParameterValue);
	USDUTILITIES_API void SetTextureParameterValue(UMaterialInstance& Material, const TCHAR* ParameterName, UTexture* ParameterValue);
	USDUTILITIES_API void SetBoolParameterValue(UMaterialInstance& Material, const TCHAR* ParameterName, bool bParameterValue);
}	 // namespace UsdUtils

#endif	  // #if USE_USD_SDK
