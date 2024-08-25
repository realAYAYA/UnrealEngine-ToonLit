// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "GameFramework/Actor.h"
#include "LandscapeEditTypes.h"

#include "LandscapeBlueprintBrushBase.generated.h"

class UTextureRenderTarget2D;

USTRUCT(BlueprintType)
struct FLandscapeBrushParameters
{
	GENERATED_BODY()

	FLandscapeBrushParameters()
		: LayerType(ELandscapeToolTargetType::Invalid)
		, CombinedResult(nullptr)
		, WeightmapLayerName()
	{}

	FLandscapeBrushParameters(const ELandscapeToolTargetType& InLayerType, TObjectPtr<UTextureRenderTarget2D> InCombinedResult, const FName& InWeightmapLayerName = FName())
		: LayerType(InLayerType)
		, CombinedResult(InCombinedResult)
		, WeightmapLayerName(InWeightmapLayerName)
	{}

	UPROPERTY(Category = "Settings", EditAnywhere, BlueprintReadWrite)
	ELandscapeToolTargetType LayerType;

	UPROPERTY(Category = "Settings", EditAnywhere, BlueprintReadWrite)
	TObjectPtr<UTextureRenderTarget2D> CombinedResult;

	UPROPERTY(Category = "Settings", EditAnywhere, BlueprintReadWrite)
	FName WeightmapLayerName;
};


UCLASS(Abstract, NotBlueprintable, MinimalAPI)
class ALandscapeBlueprintBrushBase : public AActor
{
	GENERATED_UCLASS_BODY()

protected:
#if WITH_EDITORONLY_DATA
	UPROPERTY(Transient)
	TObjectPtr<class ALandscape> OwningLandscape;

	UPROPERTY(Category = "Settings", EditAnywhere, BlueprintReadWrite)
	bool UpdateOnPropertyChange;

	UPROPERTY(Category = "Settings", EditAnywhere, BlueprintReadWrite, Setter = "SetCanAffectHeightmap")
	bool AffectHeightmap;

	UPROPERTY(Category = "Settings", EditAnywhere, BlueprintReadWrite, Setter = "SetCanAffectWeightmap")
	bool AffectWeightmap;

	UPROPERTY(Category = "Settings", EditAnywhere, BlueprintReadWrite, Setter="SetCanAffectVisibilityLayer")
	bool AffectVisibilityLayer;

	UPROPERTY(Category = "Settings", EditAnywhere, BlueprintReadWrite)
	TArray<FName> AffectedWeightmapLayers;

	UPROPERTY(Transient)
	bool bIsVisible;

	uint32 LastRequestLayersContentUpdateFrameNumber;

	bool bCaptureBoundaryNormals = false;	// HACK [chris.tchou] remove once we have a better boundary normal solution
#endif

public:

	UE_DEPRECATED(5.3, "Please use RenderLayer_Native instead.")
	virtual UTextureRenderTarget2D* Render_Native(bool InIsHeightmap, UTextureRenderTarget2D* InCombinedResult, const FName& InWeightmapLayerName) {return nullptr;}
	virtual void Initialize_Native(const FTransform& InLandscapeTransform, const FIntPoint& InLandscapeSize, const FIntPoint& InLandscapeRenderTargetSize) {}

	UFUNCTION(BlueprintNativeEvent, meta = (DeprecatedFunction, DeprecationMessage = "Please use RenderLayer instead."))
	LANDSCAPE_API UTextureRenderTarget2D* Render(bool InIsHeightmap, UTextureRenderTarget2D* InCombinedResult, const FName& InWeightmapLayerName);

	UFUNCTION(BlueprintNativeEvent)
	LANDSCAPE_API UTextureRenderTarget2D* RenderLayer(const FLandscapeBrushParameters& InParameters);

	LANDSCAPE_API virtual UTextureRenderTarget2D* RenderLayer_Native(const FLandscapeBrushParameters& InParameters);

	UFUNCTION(BlueprintNativeEvent)
	LANDSCAPE_API void Initialize(const FTransform& InLandscapeTransform, const FIntPoint& InLandscapeSize, const FIntPoint& InLandscapeRenderTargetSize);

	UFUNCTION(BlueprintCallable, Category = "Landscape")
	LANDSCAPE_API void RequestLandscapeUpdate(bool bInUserTriggered = false);

	UFUNCTION(BlueprintImplementableEvent, CallInEditor)
	LANDSCAPE_API void GetBlueprintRenderDependencies(TArray<UObject*>& OutStreamableAssets);

	LANDSCAPE_API void SetCanAffectHeightmap(bool bInCanAffectHeightmap);
	LANDSCAPE_API void SetCanAffectWeightmap(bool bInCanAffectWeightmap);
	LANDSCAPE_API void SetCanAffectVisibilityLayer(bool bInCanAffectVisibilityLayer);

#if WITH_EDITOR
	LANDSCAPE_API virtual void CheckForErrors() override;

	LANDSCAPE_API virtual void GetRenderDependencies(TSet<UObject*>& OutDependencies);

	LANDSCAPE_API virtual void SetOwningLandscape(class ALandscape* InOwningLandscape);
	LANDSCAPE_API class ALandscape* GetOwningLandscape() const;

	UE_DEPRECATED(5.3, "Renamed CanAffectHeightmap")
	bool IsAffectingHeightmap() const { return AffectHeightmap; }
	UE_DEPRECATED(5.3, "Renamed CanAffectWeightmap")
	bool IsAffectingWeightmap() const { return AffectWeightmap; }
	UE_DEPRECATED(5.3, "Renamed AffectsVisibilityLayer")
	LANDSCAPE_API virtual bool IsAffectingWeightmapLayer(const FName& InLayerName) const;
	UE_DEPRECATED(5.3, "Renamed CanAffectVisibilityLayer")
	bool IsAffectingVisibilityLayer() const { return AffectVisibilityLayer; }

	bool CanAffectHeightmap() const { return AffectHeightmap; }
	bool CanAffectWeightmap() const { return AffectWeightmap; }
	bool CanAffectVisibilityLayer() const { return AffectVisibilityLayer; }

	virtual bool AffectsHeightmap() const { return CanAffectHeightmap(); }
	virtual bool AffectsWeightmap() const { return CanAffectWeightmap(); }
	LANDSCAPE_API virtual bool AffectsWeightmapLayer(const FName& InLayerName) const;
	virtual bool AffectsVisibilityLayer() const { return CanAffectVisibilityLayer(); }

	bool IsVisible() const { return bIsVisible; }
	LANDSCAPE_API bool IsLayerUpdatePending() const;

	LANDSCAPE_API void SetIsVisible(bool bInIsVisible);

	UE_DEPRECATED(5.3, "Renamed SetCanAffectHeightmap")
	LANDSCAPE_API void SetAffectsHeightmap(bool bInAffectsHeightmap);
	UE_DEPRECATED(5.3, "Renamed SetCanAffectWeightmap")
	LANDSCAPE_API void SetAffectsWeightmap(bool bInAffectsWeightmap);
	UE_DEPRECATED(5.3, "Renamed SetCanAffectVisibilityLayer")
	LANDSCAPE_API void SetAffectsVisibilityLayer(bool bInAffectsVisibilityLayer);

	LANDSCAPE_API virtual bool ShouldTickIfViewportsOnly() const override;
	LANDSCAPE_API virtual void Tick(float DeltaSeconds) override;
	LANDSCAPE_API virtual void PostEditMove(bool bFinished) override;
	LANDSCAPE_API virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
	LANDSCAPE_API virtual void Destroyed() override;

	LANDSCAPE_API virtual void PushDeferredLayersContentUpdate();

	virtual bool CanChangeIsSpatiallyLoadedFlag() const override { return false; }

	bool GetCaptureBoundaryNormals() { return bCaptureBoundaryNormals; }	// HACK [chris.tchou] remove once we have a better boundary normal solution
#endif
};
