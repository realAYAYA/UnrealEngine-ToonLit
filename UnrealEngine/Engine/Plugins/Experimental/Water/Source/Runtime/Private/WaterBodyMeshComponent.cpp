// Copyright Epic Games, Inc. All Rights Reserved.

#include "WaterBodyMeshComponent.h"
#include "WaterBodyActor.h"
#include "WaterBodyComponent.h"
#include "DynamicMeshToMeshDescription.h"
#include "Engine/CollisionProfile.h"
#include "PhysicsEngine/BodySetup.h"
#include "WaterModule.h"
#include "StaticMeshResources.h"
#include "Engine/StaticMesh.h"

#if WITH_EDITOR
#include "UObject/Package.h"
#include "StaticMeshAttributes.h"
#include "DynamicMesh/DynamicMesh3.h"
#include "DynamicMeshToMeshDescription.h"
#include "DynamicMesh/DynamicMeshAttributeSet.h"
#include "DynamicMesh/MeshNormals.h"
#include "DynamicMesh/MeshTangents.h"
#endif 

#include UE_INLINE_GENERATED_CPP_BY_NAME(WaterBodyMeshComponent)

UWaterBodyMeshComponent::UWaterBodyMeshComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SetMobility(EComponentMobility::Static);

	SetCollisionProfileName(UCollisionProfile::NoCollision_ProfileName);
	SetGenerateOverlapEvents(false);
	SetCollisionEnabled(ECollisionEnabled::NoCollision);

	CanCharacterStepUpOn = ECanBeCharacterBase::ECB_No;
	DepthPriorityGroup = SDPG_World;

	bComputeFastLocalBounds = true;
	bComputeBoundsOnceForGame = true;

	AlwaysLoadOnServer = false;
	bSelectable = true;

	bCanEverAffectNavigation = false;
}

bool UWaterBodyMeshComponent::CanCreateSceneProxy() const
{
	if (GetStaticMesh() == nullptr)
	{
		UE_LOG(LogWater, Verbose, TEXT("Skipping CreateSceneProxy for WaterBodyMeshComponent %s (StaticMesh is null)"), *GetFullName());
		return false;
	}

	// Prevent accessing the RenderData during async compilation. The RenderState will be recreated when compilation finishes.
	if (GetStaticMesh()->IsCompiling())
	{
		UE_LOG(LogWater, Verbose, TEXT("Skipping CreateSceneProxy for WaterBodyMeshComponent %s (StaticMesh is not ready)"), *GetFullName());
		return false;
	}

	if (GetStaticMesh()->GetRenderData() == nullptr)
	{
		UE_LOG(LogWater, Verbose, TEXT("Skipping CreateSceneProxy for WaterBodyMeshComponent %s (RenderData is null)"), *GetFullName());
		return false;
	}

	// By now the compilation should be finished so having null render data is not valid.
	if (!GetStaticMesh()->GetRenderData()->IsInitialized())
	{
		UE_LOG(LogWater, Warning, TEXT("Skipping CreateSceneProxy for WaterBodyMeshComponent %s (RenderData is not initialized)"), *GetFullName());
		return false;
	}

	const FStaticMeshLODResourcesArray& LODResources = GetStaticMesh()->GetRenderData()->LODResources;
	const int32 SMCurrentMinLOD = GetStaticMesh()->GetMinLODIdx();
	const int32 EffectiveMinLOD = bOverrideMinLOD ? MinLOD : SMCurrentMinLOD;
	if (LODResources.Num() == 0	|| LODResources[FMath::Clamp<int32>(EffectiveMinLOD, 0, LODResources.Num()-1)].VertexBuffers.StaticMeshVertexBuffer.GetNumVertices() == 0)
	{
		UE_LOG(LogWater, Warning, TEXT("Skipping CreateSceneProxy for WaterBodyMeshComponent %s (LOD problems)"), *GetFullName());
		return false;
	}

	return true;
}
