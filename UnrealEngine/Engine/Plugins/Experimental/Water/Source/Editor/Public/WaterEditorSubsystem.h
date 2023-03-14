// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Stats/Stats.h"
#include "EditorSubsystem.h"
#include "Engine/EngineTypes.h"
#include "TickableEditorObject.h"
#include "WaterModule.h"
#include "WaterEditorSubsystem.generated.h"

class AWaterZone;
class UTexture2D;
class UTextureRenderTarget2D;
class UWorld;
class UWaterSubsystem;
class UMaterialParameterCollection;

UCLASS()
class UWaterEditorSubsystem : public UEditorSubsystem, public IWaterEditorServices
{
	GENERATED_BODY()

	UWaterEditorSubsystem();
public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	UMaterialParameterCollection* GetLandscapeMaterialParameterCollection() const {	return LandscapeMaterialParameterCollection; }

	// ~ IWaterEditorServices interface
	virtual void RegisterWaterActorSprite(UClass* InClass, UTexture2D* Texture) override;
	virtual UTexture2D* GetWaterActorSprite(UClass* InClass) const override;
	virtual UTexture2D* GetErrorSprite() const override { return ErrorSprite; }
private:
	UPROPERTY()
	TObjectPtr<UMaterialParameterCollection> LandscapeMaterialParameterCollection;

	UPROPERTY()
	TMap<TObjectPtr<UClass>, TObjectPtr<UTexture2D>> WaterActorSprites;
	UPROPERTY()
	TObjectPtr<UTexture2D> DefaultWaterActorSprite;
	UPROPERTY()
	TObjectPtr<UTexture2D> ErrorSprite;
};
