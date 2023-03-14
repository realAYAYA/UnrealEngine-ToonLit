// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "GameFramework/Actor.h"

#include "LandscapeBlueprintBrushBase.generated.h"

class UTextureRenderTarget2D;

UCLASS(Abstract, NotBlueprintable)
class LANDSCAPE_API ALandscapeBlueprintBrushBase : public AActor
{
	GENERATED_UCLASS_BODY()

protected:
#if WITH_EDITORONLY_DATA
	UPROPERTY(Transient)
	TObjectPtr<class ALandscape> OwningLandscape;

	UPROPERTY(Category = "Settings", EditAnywhere, BlueprintReadWrite)
	bool UpdateOnPropertyChange;

	UPROPERTY(Category = "Settings", EditAnywhere, BlueprintReadWrite)
	bool AffectHeightmap;

	UPROPERTY(Category = "Settings", EditAnywhere, BlueprintReadWrite)
	bool AffectWeightmap;

	UPROPERTY(Category = "Settings", EditAnywhere, BlueprintReadWrite)
	TArray<FName> AffectedWeightmapLayers;

	UPROPERTY(Transient)
	bool bIsVisible;

	uint32 LastRequestLayersContentUpdateFrameNumber;
#endif

public:
	virtual UTextureRenderTarget2D* Render_Native(bool InIsHeightmap, UTextureRenderTarget2D* InCombinedResult, const FName& InWeightmapLayerName) {return nullptr;}
	virtual void Initialize_Native(const FTransform& InLandscapeTransform, const FIntPoint& InLandscapeSize, const FIntPoint& InLandscapeRenderTargetSize) {}

	UFUNCTION(BlueprintNativeEvent)
	UTextureRenderTarget2D* Render(bool InIsHeightmap, UTextureRenderTarget2D* InCombinedResult, const FName& InWeightmapLayerName);

	UFUNCTION(BlueprintNativeEvent)
	void Initialize(const FTransform& InLandscapeTransform, const FIntPoint& InLandscapeSize, const FIntPoint& InLandscapeRenderTargetSize);

	UFUNCTION(BlueprintCallable, Category = "Landscape")
	void RequestLandscapeUpdate();

	UFUNCTION(BlueprintImplementableEvent, CallInEditor)
	void GetBlueprintRenderDependencies(TArray<UObject*>& OutStreamableAssets);

#if WITH_EDITOR
	virtual void CheckForErrors() override;

	virtual void GetRenderDependencies(TSet<UObject*>& OutDependencies);

	virtual void SetOwningLandscape(class ALandscape* InOwningLandscape);
	class ALandscape* GetOwningLandscape() const;

	bool IsAffectingHeightmap() const { return AffectHeightmap; }
	bool IsAffectingWeightmap() const { return AffectWeightmap; }
	virtual bool IsAffectingWeightmapLayer(const FName& InLayerName) const;
	bool IsVisible() const { return bIsVisible; }
	bool IsLayerUpdatePending() const;

	void SetIsVisible(bool bInIsVisible);
	void SetAffectsHeightmap(bool bInAffectsHeightmap);
	void SetAffectsWeightmap(bool bInAffectsWeightmap);

	virtual bool ShouldTickIfViewportsOnly() const override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void PostEditMove(bool bFinished) override;
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
	virtual void Destroyed() override;

	virtual void PushDeferredLayersContentUpdate();

	virtual bool CanChangeIsSpatiallyLoadedFlag() const override { return false; }
#endif
};