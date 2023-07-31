// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Options/GLTFExportOptions.h"
#include "Json/GLTFJsonEnums.h"
#include "UObject/GCObjectScopeGuard.h"

class UMaterialInterface;
class UStaticMesh;
class UStaticMeshComponent;
class USkeletalMesh;
class USkeletalMeshComponent;

class GLTFEXPORTER_API FGLTFBuilder
{
public:

	const FString FileName;
	const bool bIsGLB;

	const UGLTFExportOptions* ExportOptions; // TODO: make ExportOptions private and expose each option via getters to ease overriding settings in future

	FGLTFBuilder(const FString& FileName, const UGLTFExportOptions* ExportOptions = nullptr);

	const UMaterialInterface* ResolveProxy(const UMaterialInterface* Material) const;
	void ResolveProxies(TArray<const UMaterialInterface*>& Materials) const;

	FIntPoint GetBakeSizeForMaterialProperty(const UMaterialInterface* Material, EGLTFMaterialPropertyGroup PropertyGroup) const;
	TextureFilter GetBakeFilterForMaterialProperty(const UMaterialInterface* Material, EGLTFMaterialPropertyGroup PropertyGroup) const;
	TextureAddress GetBakeTilingForMaterialProperty(const UMaterialInterface* Material, EGLTFMaterialPropertyGroup PropertyGroup) const;

	EGLTFJsonHDREncoding GetTextureHDREncoding() const;

	bool ShouldExportLight(EComponentMobility::Type LightMobility) const;

	int32 SanitizeLOD(const UStaticMesh* StaticMesh, const UStaticMeshComponent* StaticMeshComponent, int32 LODIndex) const;
	int32 SanitizeLOD(const USkeletalMesh* SkeletalMesh, const USkeletalMeshComponent* SkeletalMeshComponent, int32 LODIndex) const;

private:

	FGCObjectScopeGuard ExportOptionsGuard;

	static const UGLTFExportOptions* SanitizeExportOptions(const UGLTFExportOptions* Options);
	static EGLTFSceneMobility GetSceneMobility(EComponentMobility::Type Mobility);
};
