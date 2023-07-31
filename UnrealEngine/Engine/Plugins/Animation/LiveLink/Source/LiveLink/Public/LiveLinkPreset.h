// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/EngineTypes.h"
#include "Engine/LatentActionManager.h"
#include "LiveLinkPresetTypes.h"
#include "Templates/PimplPtr.h"
#include "UObject/Object.h"
#include "UObject/ObjectMacros.h"

#include "LiveLinkPreset.generated.h"

UCLASS(BlueprintType)
class LIVELINK_API ULiveLinkPreset : public UObject
{
	GENERATED_BODY()

	virtual ~ULiveLinkPreset();

private:
	UPROPERTY(VisibleAnywhere, Category = "LiveLinkSourcePresets")
	TArray<FLiveLinkSourcePreset> Sources;

	UPROPERTY(VisibleAnywhere, Category = "LiveLinkSubjectPresets")
	TArray<FLiveLinkSubjectPreset> Subjects;

public:
	/** Get the list of source presets. */
	const TArray<FLiveLinkSourcePreset>& GetSourcePresets() const { return Sources; }

	/** Get the list of subject presets. */
	const TArray<FLiveLinkSubjectPreset>& GetSubjectPresets() const { return Subjects; }

	/**
	 * Remove all previous sources and subjects and add the sources and subjects from this preset.
	 * @return True is all sources and subjects from this preset could be created and added.
	 */
	UE_DEPRECATED(5.0, "This function is deprecated, please use ApplyToClientLatent")
	UFUNCTION(BlueprintCallable, BlueprintPure = false, Category="LiveLink")
	bool ApplyToClient() const;

	/**
	 * Remove all previous sources and subjects and add the sources and subjects from this preset.
	 * @return True is all sources and subjects from this preset could be created and added.
	 */
	UFUNCTION(BlueprintCallable, BlueprintPure = false, Category="LiveLink", meta = (Latent, LatentInfo = "LatentInfo", HidePin = "WorldContextObject", DefaultToSelf = "WorldContextObject"))
	void ApplyToClientLatent(UObject* WorldContextObject, FLatentActionInfo LatentInfo);
	void ApplyToClientLatent(TFunction<void(bool)> CompletionCallback = nullptr);

	/**
	 * Add the sources and subjects from this preset, but leave any existing sources and subjects connected.
	 *
	 * @param bRecreatePresets	When true, if subjects and sources from this preset already exist, we will recreate them.
	 *
	 * @return True is all sources and subjects from this preset could be created and added.
	 */
	UFUNCTION(BlueprintCallable, BlueprintPure = false, Category = "LiveLink")
	bool AddToClient(const bool bRecreatePresets = true) const;

	/** Reset this preset and build the list of sources and subjects from the client. */
	UFUNCTION(BlueprintCallable, Category="LiveLink")
	void BuildFromClient();

private:
	/** Clear the timer registered with the current world. */
	void ClearApplyToClientTimer();
	
private:
	/** Holds a handle to the OnEndFrame delegate used to apply a preset asynchronously with ApplyToClientLatent. */
	FDelegateHandle ApplyToClientEndFrameHandle;

	/** Holds the current ApplyToClient async operation. Only one operation for all presets can be done at a time. */
	static TPimplPtr<struct FApplyToClientPollingOperation> ApplyToClientPollingOperation;
};
