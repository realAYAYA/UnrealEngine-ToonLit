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
	/* The landscape proxy identity this Nanite representation was generated for */
	UPROPERTY()
	FGuid ProxyContentId;

	UPROPERTY(Transient)
	bool bEnabled;

public:
#if WITH_EDITOR
	LANDSCAPE_API void InitializeForLandscape(ALandscapeProxy* Landscape, const FGuid& NewProxyContentId);
	LANDSCAPE_API void InitializePlatformForLandscape(ALandscapeProxy* Landscape, const ITargetPlatform* TargetPlatform);
#endif

	virtual FPrimitiveSceneProxy* CreateSceneProxy() override;
};
