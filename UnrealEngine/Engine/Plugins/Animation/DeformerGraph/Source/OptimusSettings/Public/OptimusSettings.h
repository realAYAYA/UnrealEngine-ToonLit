// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#if UE_ENABLE_INCLUDE_ORDER_DEPRECATED_IN_5_2
#include "RHIDefinitions.h"
#endif
#include "OptimusSettings.generated.h"

class UMeshDeformer;
enum EShaderPlatform : uint16;

UENUM()
enum class EOptimusDefaultDeformerMode : uint8
{
	/** Never apply the default deformers. */
	Never,
	/** Only apply default deformers if requested. */
	OptIn,
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

namespace Optimus
{
	/** Returns true if DeformerGraph is supported on a platform. */
	OPTIMUSSETTINGS_API bool IsSupported(EShaderPlatform Platform);

	/** Returns true if DeformerGraph is currently enabled. */
	OPTIMUSSETTINGS_API bool IsEnabled();
}
