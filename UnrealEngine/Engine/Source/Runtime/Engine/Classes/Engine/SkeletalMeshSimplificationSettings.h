// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Engine/DeveloperSettings.h"
#include "SkeletalMeshSimplificationSettings.generated.h"

/**
* Controls the selection of the system used to simplify skeletal meshes.
*/
UCLASS(config = Engine, defaultconfig, meta = (DisplayName = "Skeletal Mesh Simplification"))
class USkeletalMeshSimplificationSettings : public UDeveloperSettings
{
	GENERATED_UCLASS_BODY()

public:
	virtual FName GetContainerName() const override;
	virtual FName GetCategoryName() const override;
public:
	/** Mesh reduction plugin to use when simplifying skeletal meshes */
	UPROPERTY(config, EditAnywhere, Category = General, meta = (ConsoleVariable = "r.SkeletalMeshReductionModule", DisplayName = "Skeletal Mesh Reduction Plugin", ConfigRestartRequired = true))
	FName SkeletalMeshReductionModuleName;

	virtual void PostInitProperties() override;
#if WITH_EDITOR
	virtual void PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

};
