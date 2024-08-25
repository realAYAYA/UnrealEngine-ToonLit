// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "HAL/Platform.h"
#include "Containers/Map.h"

class UObject;
class USkeletalMesh;
class UTexture2D;
class UTexture;
class UMaterialInterface;
class UMaterial;


class CUSTOMIZABLEOBJECTEDITOR_API FUnrealBakeHelpers
{
public:

	static UObject* BakeHelper_DuplicateAsset(UObject* Object, const FString& ObjName, const FString& PkgName, bool ResetDuplicatedFlags,
											  TMap<UObject*, UObject*>& ReplacementMap, bool OverwritePackage,
											  const bool bGenerateConstantMaterialInstances);

	/**
	 * Duplicates a texture asset. Duplicates Mutable and non Mutable textures.
	 *
	 * @param OrgTex Original source texture from which a Mutable texture has been generated. Only required when SrcTex is a Mutable texture.
	 */
	static UTexture2D* BakeHelper_CreateAssetTexture(UTexture2D* SrcTex, const FString& TexObjName, const FString& TexPkgName, const UTexture* OrgTex, bool ResetDuplicatedFlags, TMap<UObject*, UObject*>& ReplacementMap, bool OverwritePackage);

	static void CopyAllMaterialParameters(UObject* DestMaterial, UMaterialInterface* OriginMaterial, const TMap<int, UTexture*>& TextureReplacementMaps);

	/** Get associated texture parameter names for the material given as parameter */
	static TArray<FName> GetTextureParameterNames(UMaterial* Material);
};
