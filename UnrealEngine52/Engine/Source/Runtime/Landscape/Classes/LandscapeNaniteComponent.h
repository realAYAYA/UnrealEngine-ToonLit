// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Misc/Guid.h"
#include "Components/StaticMeshComponent.h"

#include "LandscapeNaniteComponent.generated.h"

class ALandscape;
class ALandscapeProxy;
class ULandscapeComponent;
class FPrimitiveSceneProxy;

UCLASS(hidecategories = (Display, Attachment, Physics, Debug, Collision, Movement, Rendering, PrimitiveComponent, Object, Transform, Mobility, VirtualTexture), showcategories = ("Rendering|Material"), MinimalAPI, Within = LandscapeProxy)
class ULandscapeNaniteComponent : public UStaticMeshComponent
{
	GENERATED_BODY()

public:
	ULandscapeNaniteComponent(const FObjectInitializer& ObjectInitializer);

	virtual void PostLoad() override;

	/** Gets the landscape proxy actor which owns this component */
	LANDSCAPE_API ALandscapeProxy* GetLandscapeProxy() const;
						
	/** Get the landscape actor associated with this component. */
	LANDSCAPE_API ALandscape* GetLandscapeActor() const;

	inline const FGuid& GetProxyContentId() const
	{
		return ProxyContentId;
	}

	void UpdatedSharedPropertiesFromActor();

	void SetEnabled(bool bValue);

	inline bool IsEnabled() const
	{
		return bEnabled;
	}

private:
	
	/** Collect all the PSO precache data used by the static mesh component */
	virtual void CollectPSOPrecacheData(const FPSOPrecacheParams& BasePrecachePSOParams, FComponentPSOPrecacheParamsList& OutParams) override;

	/* The landscape proxy identity this Nanite representation was generated for */
	UPROPERTY()
	FGuid ProxyContentId;

	UPROPERTY(Transient)
	bool bEnabled;

public:
#if WITH_EDITOR
	/**
	 * Generates the Nanite static mesh, stores the content Id.
	 * @param Landscape Proxy to generate the mesh for
	 * @param NewProxyContentId Hash of the content that this mesh corresponds to
	 * 
	 * @return true if the Nanite mesh creation was successful
	 */
	LANDSCAPE_API bool InitializeForLandscape(ALandscapeProxy* Landscape, const FGuid& NewProxyContentId);

	/**
	 * Ensures the cooked cached platform data of the Nanite static mesh is finished. It is necessary to ensure that StreamablePages are loaded from DDC
	 * @param Landscape Proxy for this Nanite landscape component
	 * @param TargetPlatform info about the platform being cooked
	 * 
	 * @return true if the Nanite mesh creation was successful
	 */
	LANDSCAPE_API bool InitializePlatformForLandscape(ALandscapeProxy* Landscape, const ITargetPlatform* TargetPlatform);
#endif

	virtual FPrimitiveSceneProxy* CreateSceneProxy() override;

	virtual bool IsHLODRelevant() const override;
};
