// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Engine/EngineTypes.h"
#include "PhysicsEngine/BodyInstance.h"
#include "Components/PrimitiveComponent.h"
#include "ISMComponentDescriptor.generated.h"

class UInstancedStaticMeshComponent;
class UStaticMesh;
enum class ERuntimeVirtualTextureMainPassType : uint8;
enum class ERendererStencilMask : uint8;

/** Struct that holds the relevant properties that can help decide if instances of different 
	StaticMeshComponents/InstancedStaticMeshComponents/HISM can be merged into a single component. */
USTRUCT()
struct ENGINE_API FISMComponentDescriptor
{
	GENERATED_USTRUCT_BODY()

	FISMComponentDescriptor();
#if WITH_EDITOR
	static FISMComponentDescriptor CreateFrom(const TSubclassOf<UStaticMeshComponent>& ComponentClass);
	void InitFrom(const UStaticMeshComponent* Component, bool bInitBodyInstance = true);

	uint32 ComputeHash() const;
	UInstancedStaticMeshComponent* CreateComponent(UObject* Outer, FName Name = NAME_None, EObjectFlags ObjectFlags = EObjectFlags::RF_NoFlags) const;
	void InitComponent(UInstancedStaticMeshComponent* ISMComponent) const;

	friend inline uint32 GetTypeHash(const FISMComponentDescriptor& Key)
	{
		if (Key.Hash == 0)
		{
			Key.ComputeHash();
		}
		return Key.Hash;
	}

	bool operator!=(const FISMComponentDescriptor& Other) const;
	bool operator==(const FISMComponentDescriptor& Other) const;

	friend inline bool operator<(const FISMComponentDescriptor& Lhs, const FISMComponentDescriptor& Rhs)
	{
		return Lhs.Hash < Rhs.Hash;
	}
#endif

public:

#if WITH_EDITORONLY_DATA
	UPROPERTY()
	mutable uint32 Hash = 0;

	UPROPERTY(EditAnywhere, Category = "Component Settings")
	TSubclassOf<UInstancedStaticMeshComponent> ComponentClass;
	
	UPROPERTY(EditAnywhere, Category = "Component Settings")
	TObjectPtr<UStaticMesh> StaticMesh = nullptr;
	
	UPROPERTY(EditAnywhere, Category = "Component Settings")
	TArray<TObjectPtr<UMaterialInterface>> OverrideMaterials;
	
	UPROPERTY(EditAnywhere, Category = "Component Settings")
	TArray<TObjectPtr<URuntimeVirtualTexture>> RuntimeVirtualTextures;

	UPROPERTY(EditAnywhere, Category = "Component Settings")
	TEnumAsByte<EComponentMobility::Type> Mobility;
		
	UPROPERTY(EditAnywhere, Category = "Component Settings")
	ERuntimeVirtualTextureMainPassType VirtualTextureRenderPassType;
	
	UPROPERTY(EditAnywhere, Category = "Component Settings")
	ELightmapType LightmapType;

	UPROPERTY(EditAnywhere, Category = "Component Settings")
	FLightingChannels LightingChannels;

	UPROPERTY(EditAnywhere, Category = "Component Settings")
	int32 RayTracingGroupId;

	UPROPERTY(EditAnywhere, Category = "Component Settings")
	ERayTracingGroupCullingPriority RayTracingGroupCullingPriority;

	UPROPERTY(EditAnywhere, Category = "Component Settings")
	TEnumAsByte<EHasCustomNavigableGeometry::Type> bHasCustomNavigableGeometry;

	UPROPERTY(EditAnywhere, Category = "Component Settings")
	ERendererStencilMask CustomDepthStencilWriteMask;

	UPROPERTY(EditAnywhere, Category = "Component Settings")
	FBodyInstance BodyInstance;
		
	UPROPERTY(EditAnywhere, Category = "Component Settings")
	int32 InstanceStartCullDistance;

	UPROPERTY(EditAnywhere, Category = "Component Settings")
	int32 InstanceEndCullDistance;

	UPROPERTY(EditAnywhere, Category = "Component Settings")
	int32 VirtualTextureCullMips;

	UPROPERTY(EditAnywhere, Category = "Component Settings")
	int32 TranslucencySortPriority;

	UPROPERTY(EditAnywhere, Category = "Component Settings")
	int32 OverriddenLightMapRes;

	UPROPERTY(EditAnywhere, Category = "Component Settings")
	int32 CustomDepthStencilValue;

	UPROPERTY(EditAnywhere, Category = "Component Settings")
	EHLODBatchingPolicy HLODBatchingPolicy;
	
	UPROPERTY(EditAnywhere, Category = "Component Settings")
	uint8 bCastShadow : 1;

	UPROPERTY(EditAnywhere, Category = "Component Settings")
	uint8 bCastDynamicShadow : 1;

	UPROPERTY(EditAnywhere, Category = "Component Settings")
	uint8 bCastStaticShadow : 1;

	UPROPERTY(EditAnywhere, Category = "Component Settings")
	uint8 bCastContactShadow : 1;

	UPROPERTY(EditAnywhere, Category = "Component Settings")
	uint8 bCastShadowAsTwoSided : 1;

	UPROPERTY(EditAnywhere, Category = "Component Settings")
	uint8 bAffectDynamicIndirectLighting : 1;

	UPROPERTY(EditAnywhere, Category = "Component Settings")
	uint8 bAffectDistanceFieldLighting : 1;

	UPROPERTY(EditAnywhere, Category = "Component Settings")
	uint8 bReceivesDecals : 1;

	UPROPERTY(EditAnywhere, Category = "Component Settings")
	uint8 bOverrideLightMapRes : 1;

	UPROPERTY(EditAnywhere, Category = "Component Settings")
	uint8 bUseAsOccluder : 1;

	UPROPERTY(EditAnywhere, Category = "Component Settings")
	uint8 bEnableDensityScaling : 1;

	UPROPERTY(EditAnywhere, Category = "Component Settings")
	uint8 bEnableDiscardOnLoad : 1;

	UPROPERTY(EditAnywhere, Category = "Component Settings")
	uint8 bRenderCustomDepth : 1;

	UPROPERTY(EditAnywhere, Category = "Component Settings")
	uint8 bIncludeInHLOD : 1;

	UPROPERTY(EditAnywhere, Category = "Component Settings")
	uint8 bVisibleInRayTracing : 1;

	UPROPERTY(EditAnywhere, Category = "Component Settings")
	uint8 bHiddenInGame : 1;

	UPROPERTY(EditAnywhere, Category = "Component Settings")
	uint8 bIsEditorOnly : 1;

	UPROPERTY(EditAnywhere, Category = "Component Settings")
	uint8 bVisible : 1;

	UPROPERTY(EditAnywhere, Category = "Component Settings")
	uint8 bConsiderForActorPlacementWhenHidden : 1;

	UPROPERTY(EditAnywhere, Category = "Component Settings")
	uint8 bEvaluateWorldPositionOffset : 1;

	UPROPERTY(EditAnywhere, Category = "Component Settings")
	uint8 bIsLocalToWorldDeterminantNegative : 1;
#endif
};
