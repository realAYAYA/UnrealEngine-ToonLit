// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Stats/Stats.h"
#include "UObject/GCObject.h"
#include "AssetTypeActions_Base.h"
#include "GameFramework/ForceFeedbackEffect.h"
#include "TickableEditorObject.h"

struct FAssetData;

struct FPreviewForceFeedbackEffect : public FActiveForceFeedbackEffect, public FTickableEditorObject, public FGCObject
{
	// FTickableEditorObject Implementation
	virtual bool IsTickable() const override;
	virtual void Tick( float DeltaTime ) override;
	virtual TStatId GetStatId() const override;

	// FGCObject Implementation
	virtual void AddReferencedObjects( FReferenceCollector& Collector ) override;
	virtual FString GetReferencerName() const override
	{
		return TEXT("FPreviewForceFeedbackEffect");
	}
};

class FAssetTypeActions_ForceFeedbackEffect : public FAssetTypeActions_Base
{
public:
	FAssetTypeActions_ForceFeedbackEffect(EAssetTypeCategories::Type InAssetCategoryBit)
		: AssetCategoryBit(InAssetCategoryBit)
	{ }

	// IAssetTypeActions Implementation
	virtual FText GetName() const override { return NSLOCTEXT("AssetTypeActions", "AssetTypeActions_ForceFeedbackEffect", "Force Feedback Effect"); }
	virtual FColor GetTypeColor() const override { return FColor(175, 0, 0); }
	virtual UClass* GetSupportedClass() const override;
	virtual uint32 GetCategories() override { return AssetCategoryBit; }
	virtual void GetActions(const TArray<UObject*>& InObjects, struct FToolMenuSection& Section) override;
	virtual bool AssetsActivatedOverride( const TArray<UObject*>& InObjects, EAssetTypeActivationMethod::Type ActivationType ) override;
	virtual TSharedPtr<SWidget> GetThumbnailOverlay(const FAssetData& AssetData) const override;

	/** Return if the specified effect is playing*/
	bool IsEffectPlaying(const TArray<TWeakObjectPtr<UForceFeedbackEffect>>& Objects) const;

	/** Return if the specified effect is playing*/
	bool IsEffectPlaying(const UForceFeedbackEffect* ForceFeedbackEffect) const;

	/** Return if the specified asset is playing an effect*/
	bool IsEffectPlaying(const FAssetData& AssetData) const;

	/** Handler for when PlayEffect is selected */
	void ExecutePlayEffect(TArray<TWeakObjectPtr<UForceFeedbackEffect>> Objects);

	/** Handler for when StopEffect is selected */
	void ExecuteStopEffect(TArray<TWeakObjectPtr<UForceFeedbackEffect>> Objects);

	/** Returns true if only one effect is selected to play */
	bool CanExecutePlayCommand(TArray<TWeakObjectPtr<UForceFeedbackEffect>> Objects) const;

	/** Plays the specified effect */
	void PlayEffect(UForceFeedbackEffect* Effect);

	/** Stops any currently playing effect */
	void StopEffect();

private:
	FPreviewForceFeedbackEffect PreviewForceFeedbackEffect;

	EAssetTypeCategories::Type AssetCategoryBit;
};
