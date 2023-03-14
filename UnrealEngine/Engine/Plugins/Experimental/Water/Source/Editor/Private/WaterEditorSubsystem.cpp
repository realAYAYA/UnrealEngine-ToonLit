// Copyright Epic Games, Inc. All Rights Reserved.

#include "WaterEditorSubsystem.h"
#include "WaterBodyActor.h"
#include "EngineUtils.h"
#include "WaterZoneActor.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Editor.h"
#include "ISettingsModule.h"
#include "WaterEditorSettings.h"
#include "HAL/IConsoleManager.h"
#include "Editor/UnrealEdEngine.h"
#include "WaterSplineComponentVisualizer.h"
#include "WaterSplineComponent.h"
#include "UnrealEdGlobals.h"
#include "WaterSubsystem.h"
#include "WaterMeshComponent.h"
#include "LevelEditorViewport.h"
#include "Materials/MaterialParameterCollection.h"
#include "WaterModule.h"
#include "Modules/ModuleManager.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(WaterEditorSubsystem)

#define LOCTEXT_NAMESPACE "WaterEditorSubsystem"

UWaterEditorSubsystem::UWaterEditorSubsystem()
{
	struct FConstructorStatics
	{
		ConstructorHelpers::FObjectFinderOptional<UTexture2D> DefaultWaterActorSprite;
		ConstructorHelpers::FObjectFinderOptional<UTexture2D> ErrorSprite;

		FConstructorStatics()
			: DefaultWaterActorSprite(TEXT("/Water/Icons/WaterSprite"))
			, ErrorSprite(TEXT("/Water/Icons/WaterErrorSprite"))
		{
		}
	};
	static FConstructorStatics ConstructorStatics;

	DefaultWaterActorSprite = ConstructorStatics.DefaultWaterActorSprite.Get();
	ErrorSprite = ConstructorStatics.ErrorSprite.Get();
}

void UWaterEditorSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	LandscapeMaterialParameterCollection = GetDefault<UWaterEditorSettings>()->LandscapeMaterialParameterCollection.LoadSynchronous();

	IWaterModuleInterface& WaterModule = FModuleManager::GetModuleChecked<IWaterModuleInterface>("Water");
	WaterModule.SetWaterEditorServices(this);
}

void UWaterEditorSubsystem::Deinitialize()
{
	IWaterModuleInterface& WaterModule = FModuleManager::GetModuleChecked<IWaterModuleInterface>("Water");
	if (IWaterEditorServices* WaterEditorServices = WaterModule.GetWaterEditorServices())
	{
		if (WaterEditorServices == this)
		{
			WaterModule.SetWaterEditorServices(nullptr);
		}
	}

	Super::Deinitialize();
}

void UWaterEditorSubsystem::RegisterWaterActorSprite(UClass* InClass, UTexture2D* Texture)
{
	WaterActorSprites.Add(InClass, Texture);
}

UTexture2D* UWaterEditorSubsystem::GetWaterActorSprite(UClass* InClass) const
{
	UClass const* Class = InClass;
	typename decltype(WaterActorSprites)::ValueType const* SpritePtr = nullptr;

	// Traverse the class hierarchy and find the first available sprite
	while (Class != nullptr && SpritePtr == nullptr)
	{
		SpritePtr = WaterActorSprites.Find(Class);
		Class = Class->GetSuperClass();
	}

	if (SpritePtr != nullptr)
	{
		return *SpritePtr;
	}

	return DefaultWaterActorSprite;
}

#undef LOCTEXT_NAMESPACE

