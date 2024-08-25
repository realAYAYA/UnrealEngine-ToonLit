// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleInterface.h"
#include "Modules/ModuleManager.h"
#include "Interfaces/ITargetPlatform.h"
#include "Interfaces/ITargetPlatformManagerModule.h"

struct FStaticMeshBuildVertex;
struct FStaticMeshSection;
class FStaticMeshSectionArray;

class IMeshBuilderModule : public IModuleInterface
{
public:
	static inline IMeshBuilderModule& GetForPlatform(const ITargetPlatform* TargetPlatform)
	{
		check(TargetPlatform);
		return FModuleManager::LoadModuleChecked<IMeshBuilderModule>(TargetPlatform->GetMeshBuilderModuleName());
	}

	static inline IMeshBuilderModule& GetForRunningPlatform()
	{
		const ITargetPlatform* TargetPlatform = GetTargetPlatformManagerRef().GetRunningTargetPlatform();
		return GetForPlatform(TargetPlatform);
	}

	virtual void AppendToDDCKey(FString& DDCKey, bool bSkeletal) { }

	virtual bool BuildMesh(class FStaticMeshRenderData& OutRenderData, class UObject* Mesh, const class FStaticMeshLODGroup& LODGroup, bool bAllowNanite) = 0;

	virtual bool BuildMeshVertexPositions(class UObject* StaticMesh, TArray<uint32>& Indices, TArray<FVector3f>& Vertices, FStaticMeshSectionArray& Sections) = 0;

	virtual bool BuildSkeletalMesh(const struct FSkeletalMeshBuildParameters& SkeletalMeshBuildParameters) = 0;

	virtual void PostBuildSkeletalMesh(class FSkeletalMeshRenderData* SkeletalMeshRenderData, class USkinnedAsset* SkinnedAsset) { }
};
