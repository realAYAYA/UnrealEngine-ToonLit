// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h" 
#include "ViewportSettings.generated.h"

class UMaterial;

USTRUCT()
struct TEXTUREGRAPHENGINE_API FMaterialMappingInfo
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY(VisibleAnywhere, Category = "Viewport Settings")
	FName						MaterialInput;

	UPROPERTY(EditAnywhere, Category = "Viewport Settings")
	FName						Target;

	bool						HasTarget() const { return !Target.IsNone(); }
};

USTRUCT()
struct TEXTUREGRAPHENGINE_API FViewportSettings
{
	GENERATED_BODY()
	
	UPROPERTY(EditAnywhere, Category = "Viewport Settings", NoClear)
	TObjectPtr<UMaterial> Material;
	
	UPROPERTY(EditAnywhere, EditFixedSize, Category = "Viewport Settings")
	TArray<FMaterialMappingInfo> MaterialMappingInfos;

	void InitDefaultSettings(FName InitialTargetName);
	void SetDefaultTarget(FName DefaultTargetName);

	UMaterial* GetDefaultMaterial();
	
	FName GetMaterialName() const;
	FName GetMaterialMappingInfo(const FName MaterialInput);
	bool ContainsMaterialMappingInfo(const FName InMaterialInput);

	bool RemoveMaterialMappingForTarget(FName OutputNode);
	void OnMaterialUpdate();
	void OnTargetRename(const FName OldName, const FName NewName);
	
	DECLARE_MULTICAST_DELEGATE(FViewportSettingsUpdateEvent)
	FViewportSettingsUpdateEvent OnViewportMaterialChangeEvent;

	DECLARE_MULTICAST_DELEGATE(FMaterialMappingChangedEvent)
	FMaterialMappingChangedEvent OnMaterialMappingChangedEvent;
};
