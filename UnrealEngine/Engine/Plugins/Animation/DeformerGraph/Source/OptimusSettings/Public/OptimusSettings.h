// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "OptimusSettings.generated.h"

class UMeshDeformer;

UENUM()
enum class EOptimusDefaultDeformerMode : uint8
{
	/** Never apply the default deformers. */
	Never,
	/** Only apply default deformers as replacement for the GPU Skin Cache. */
	SkinCacheOnly,
	/** Always apply the default deformers. */
	Always,
};

UCLASS(config = DeformerGraph, defaultconfig, meta = (DisplayName = "DeformerGraph"))
class OPTIMUSSETTINGS_API UOptimusSettings : public UDeveloperSettings
{
	GENERATED_UCLASS_BODY()

	/** Set when skinned meshes should have a default deformer applied. */
	UPROPERTY(config, EditAnywhere, Category = DeformerGraph)
	EOptimusDefaultDeformerMode DefaultMode = EOptimusDefaultDeformerMode::Never;

	/** A default deformer that will be used on a skinned mesh if no other deformer has been set. */
	UPROPERTY(config, EditAnywhere, Category = DeformerGraph, meta = (AllowedClasses = "/Script/OptimusCore.OptimusDeformer", EditCondition = "DefaultMode != EOptimusDefaultDeformerMode::Never"))
	TSoftObjectPtr<UMeshDeformer> DefaultDeformer;

	/** A default deformer that will be used on a skinned mesh if no other deformer has been set, and if the mesh has requested to recompute tangets. */
	UPROPERTY(config, EditAnywhere, Category = DeformerGraph, meta = (AllowedClasses = "/Script/OptimusCore.OptimusDeformer", EditCondition = "DefaultMode != EOptimusDefaultDeformerMode::Never"))
	TSoftObjectPtr<UMeshDeformer> DefaultRecomputeTangentDeformer;

#if WITH_EDITOR
	DECLARE_MULTICAST_DELEGATE_OneParam(FOnUpdateSettings, UOptimusSettings const*);
	static FOnUpdateSettings OnSettingsChange;

	void PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent) override;
#endif // WITH_EDITOR
};
