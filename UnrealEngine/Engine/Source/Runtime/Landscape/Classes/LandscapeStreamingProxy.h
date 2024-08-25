// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "LandscapeProxy.h"

#include "LandscapeStreamingProxy.generated.h"

class ALandscape;

#if WITH_EDITOR
class UMaterialInterface;
struct FActorPartitionIdentifier;
#endif

UCLASS(MinimalAPI, notplaceable)
class ALandscapeStreamingProxy : public ALandscapeProxy
{
	GENERATED_BODY()

public:
	ALandscapeStreamingProxy(const FObjectInitializer& ObjectInitializer);

#if WITH_EDITORONLY_DATA
	UPROPERTY()
	TLazyObjectPtr<ALandscape> LandscapeActor_DEPRECATED;
#endif // WITH_EDITORONLY_DATA

private:
	UPROPERTY(EditAnywhere, Category = LandscapeProxy, Meta = (DisplayName = "Landscape Actor"))
	TSoftObjectPtr<ALandscape> LandscapeActorRef;

	UPROPERTY()
	TSet<FName> OverriddenSharedProperties;

public:
	//~ Begin UObject Interface
#if WITH_EDITOR
	virtual bool ShouldExport() override { return false;  }
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
	virtual bool CanEditChange(const FProperty* InProperty) const override;
	virtual void PostRegisterAllComponents() override;
	virtual AActor* GetSceneOutlinerParent() const override;
	virtual bool CanDeleteSelectedActor(FText& OutReason) const override;
	virtual bool GetReferencedContentObjects(TArray<UObject*>& Objects) const override;
	virtual bool CanChangeIsSpatiallyLoadedFlag() const override { return AActor::CanChangeIsSpatiallyLoadedFlag(); }
	virtual bool ShouldIncludeGridSizeInName(UWorld* InWorld, const FActorPartitionIdentifier& InIdentifier) const override;
#endif
	//~ End UObject Interface

	//~ Begin ALandscapeBase Interface
	virtual ALandscape* GetLandscapeActor() override;
	virtual const ALandscape* GetLandscapeActor() const override;
	void LANDSCAPE_API SetLandscapeActor(ALandscape* InLandscape);
	virtual UMaterialInterface* GetLandscapeMaterial(int8 InLODIndex = INDEX_NONE) const override;
	virtual UMaterialInterface* GetLandscapeHoleMaterial() const override;
	//~ End ALandscapeBase Interface

	// Check input Landscape actor is match for this LandscapeProxy (by GUID)
	bool IsValidLandscapeActor(ALandscape* Landscape);

#if WITH_EDITOR
	//~ Begin ALandscapeProxy Interface
	virtual bool IsSharedPropertyOverridden(const FName& InPropertyName) const override;
	virtual void SetSharedPropertyOverride(const FName& InPropertyName, const bool bIsOverridden) override;
	//~ End ALandscapeProxy Interface

protected:
	virtual void FixupOverriddenSharedProperties() override;
#endif // WITH_EDITOR
};
