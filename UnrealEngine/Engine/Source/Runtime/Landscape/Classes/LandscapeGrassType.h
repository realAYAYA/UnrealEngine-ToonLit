// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Object.h"
#include "Engine/EngineTypes.h"
#include "PerPlatformProperties.h"
#include "PerQualityLevelProperties.h"
#include "SceneTypes.h"
#include "LandscapeGrassType.generated.h"

class UStaticMesh;
struct FPropertyChangedEvent;

UENUM()
enum class EGrassScaling : uint8
{
	/** Grass instances will have uniform X, Y and Z scales. */
	Uniform,
	/** Grass instances will have random X, Y and Z scales. */
	Free,
	/** X and Y will be the same random scale, Z will be another */
	LockXY,
};

USTRUCT()
struct FGrassVariety
{
	GENERATED_USTRUCT_BODY()

	FGrassVariety();

	UPROPERTY(EditAnywhere, Category=Grass)
	TObjectPtr<UStaticMesh> GrassMesh;

	UPROPERTY(EditAnywhere, Category=Grass, meta = (ToolTip = "Material Overrides."))
	TArray<TObjectPtr<class UMaterialInterface>> OverrideMaterials;

	/* Instances per 10 square meters. */
	UPROPERTY(EditAnywhere, Category=Grass, meta = (UIMin = 0, ClampMin = 0, UIMax = 1000, ClampMax = 1000))
	FPerPlatformFloat GrassDensity;

	UPROPERTY(EditAnywhere, Category=Grass, meta = (UIMin = 0, ClampMin = 0, UIMax = 1000, ClampMax = 1000))
	FPerQualityLevelFloat GrassDensityQuality;

	/* If true, use a jittered grid sequence for placement, otherwise use a halton sequence. */
	UPROPERTY(EditAnywhere, Category=Grass)
	bool bUseGrid;

	UPROPERTY(EditAnywhere, Category=Grass, meta = (EditCondition = "bUseGrid", UIMin = 0, ClampMin = 0, UIMax = 1, ClampMax = 1))
	float PlacementJitter;

	/* The distance where instances will begin to fade out if using a PerInstanceFadeAmount material node. 0 disables. */
	UPROPERTY(EditAnywhere, Category=Grass, meta = (UIMin = 0, ClampMin = 0, UIMax = 1000000, ClampMax = 1000000))
	FPerPlatformInt StartCullDistance;

	UPROPERTY(EditAnywhere, Category = Grass, meta = (UIMin = 0, ClampMin = 0, UIMax = 1000000, ClampMax = 1000000))
	FPerQualityLevelInt StartCullDistanceQuality;

	/**
	 * The distance where instances will have completely faded out when using a PerInstanceFadeAmount material node. 0 disables. 
	 * When the entire cluster is beyond this distance, the cluster is completely culled and not rendered at all.
	 */
	UPROPERTY(EditAnywhere, Category = Grass, meta = (UIMin = 0, ClampMin = 0, UIMax = 1000000, ClampMax = 1000000))
	FPerPlatformInt EndCullDistance;

	UPROPERTY(EditAnywhere, Category = Grass, meta = (UIMin = 0, ClampMin = 0, UIMax = 1000000, ClampMax = 1000000))
	FPerQualityLevelInt EndCullDistanceQuality;
	/** 
	 * Specifies the smallest LOD that will be used for this component.
	 * If -1 (default), the MinLOD of the static mesh asset will be used instead.
	 */
	UPROPERTY(EditAnywhere, Category = Grass, meta = (UIMin = -1, ClampMin = -1, UIMax = 8, ClampMax = 8))
	int32 MinLOD;

	/** Specifies grass instance scaling type */
	UPROPERTY(EditAnywhere, Category=Grass)
	EGrassScaling Scaling;

	/** Specifies the range of scale, from minimum to maximum, to apply to a grass instance's X Scale property */
	UPROPERTY(EditAnywhere, Category=Grass)
	FFloatInterval ScaleX;

	/** Specifies the range of scale, from minimum to maximum, to apply to a grass instance's Y Scale property */
	UPROPERTY(EditAnywhere, Category=Grass, meta = (EditCondition = "Scaling == EGrassScaling::Free"))
	FFloatInterval ScaleY;

	/** Specifies the range of scale, from minimum to maximum, to apply to a grass instance's Z Scale property */
	UPROPERTY(EditAnywhere, Category=Grass, meta = (EditCondition = "Scaling == EGrassScaling::Free || Scaling == EGrassScaling::LockXY"))
	FFloatInterval ScaleZ;

	/** Whether the grass instances should be placed at random rotation (true) or all at the same rotation (false) */
	UPROPERTY(EditAnywhere, Category = Grass)
	bool RandomRotation;

	/** Whether the grass instances should be tilted to the normal of the landscape (true), or always vertical (false) */
	UPROPERTY(EditAnywhere, Category = Grass)
	bool AlignToSurface;

	/* Whether to use the landscape's lightmap when rendering the grass. */
	UPROPERTY(EditAnywhere, Category = Grass)
	bool bUseLandscapeLightmap;

	/**
	 * Lighting channels that the grass will be assigned. Lights with matching channels will affect the grass.
	 * These channels only apply to opaque materials, direct lighting, and dynamic lighting and shadowing.
	 */
	UPROPERTY(EditAnywhere, AdvancedDisplay, Category = Grass)
	FLightingChannels LightingChannels;

	/** Whether the grass instances should receive decals. */
	UPROPERTY(EditAnywhere, Category = Grass)
	bool bReceivesDecals;

	/** Controls whether the primitive should affect dynamic distance field lighting methods. */
	UPROPERTY(EditAnywhere, Category = Grass)
	bool bAffectDistanceFieldLighting;

	/** Whether the grass should cast shadows when using non-precomputed shadowing. **/
	UPROPERTY(EditAnywhere, Category = Grass)
	bool bCastDynamicShadow;

	/** Whether the grass should cast contact shadows. **/
	UPROPERTY(EditAnywhere, Category = Grass)
	bool bCastContactShadow;

	/** Whether we should keep a cpu copy of the instance buffer. This should be set to true if you plan on using GetOverlappingXXXXCount functions of the component otherwise it won't return any data.**/
	UPROPERTY(EditAnywhere, Category = Grass)
	bool bKeepInstanceBufferCPUCopy;

	/** Distance at which to grass instances should disable WPO for performance reasons */
	UPROPERTY(EditAnywhere, Category = Grass)
	uint32 InstanceWorldPositionOffsetDisableDistance;

	/** Control shadow invalidation behavior, in particular with respect to Virtual Shadow Maps and material effects like World Position Offset. */
	UPROPERTY(EditAnywhere, Category=Grass, AdvancedDisplay)
	EShadowCacheInvalidationBehavior ShadowCacheInvalidationBehavior;

	bool IsGrassQualityLevelEnable() const;

	int32 GetStartCullDistance() const;

	int32 GetEndCullDistance() const;

	float GetDensity() const;

};

UCLASS(MinimalAPI)
class ULandscapeGrassType : public UObject
{
	GENERATED_UCLASS_BODY()

	UPROPERTY(EditAnywhere, Category = Grass)
	TArray<FGrassVariety> GrassVarieties;

	/**
	* Whether this grass type should be affected by the Engine Scalability system's Foliage grass.DensityScale setting. 
	* This is enabled by default but can be disabled should this grass type be important for gameplay reasons.
	*/
	UPROPERTY(EditAnywhere, Category = Scalability)
	uint32 bEnableDensityScaling : 1;

	UPROPERTY()
	TObjectPtr<UStaticMesh> GrassMesh_DEPRECATED;
	UPROPERTY()
	float GrassDensity_DEPRECATED;
	UPROPERTY()
	float PlacementJitter_DEPRECATED;
	UPROPERTY()
	int32 StartCullDistance_DEPRECATED;
	UPROPERTY()
	int32 EndCullDistance_DEPRECATED;
	UPROPERTY()
	bool RandomRotation_DEPRECATED;
	UPROPERTY()
	bool AlignToSurface_DEPRECATED;

	//~ Begin UObject Interface
	virtual void PostLoad() override;
#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif
	//~ End UObject Interface
};



