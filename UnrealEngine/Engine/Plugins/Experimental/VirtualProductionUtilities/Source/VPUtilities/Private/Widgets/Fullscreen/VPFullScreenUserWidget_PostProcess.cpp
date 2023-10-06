// Copyright Epic Games, Inc. All Rights Reserved.

#include "Widgets/Fullscreen/VPFullScreenUserWidget_PostProcess.h"

#include "Widgets/VPFullScreenUserWidget.h"

#include "Components/PostProcessComponent.h"
#include "Engine/TextureRenderTarget2D.h"
#include "HAL/PlatformApplicationMisc.h"
#include "GameFramework/WorldSettings.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/Package.h"
#include "Widgets/Layout/SConstraintCanvas.h"

#if WITH_EDITOR
#include "LevelEditor.h"
#include "SLevelViewport.h"
#endif

namespace UE::VPUtilities::Private
{
	const FName NAME_SlateUI = "SlateUI";
	const FName NAME_TintColorAndOpacity = "TintColorAndOpacity";
	const FName NAME_OpacityFromTexture = "OpacityFromTexture";
}

FVPFullScreenUserWidget_PostProcess::FVPFullScreenUserWidget_PostProcess()
	: PostProcessComponent(nullptr)
{}

void FVPFullScreenUserWidget_PostProcess::SetCustomPostProcessSettingsSource(TWeakObjectPtr<UObject> InCustomPostProcessSettingsSource)
{
	CustomPostProcessSettingsSource = InCustomPostProcessSettingsSource;
	
	const bool bIsRunning = PostProcessMaterialInstance != nullptr;
	if (bIsRunning)
	{
		// Save us from creating another struct member: PostProcessMaterialInstance is always created with UWorld as outer.
		UWorld* World = CastChecked<UWorld>(PostProcessMaterialInstance->GetOuter());
		InitPostProcessComponent(World);
	}
}

bool FVPFullScreenUserWidget_PostProcess::Display(UWorld* World, UUserWidget* Widget, bool bInRenderToTextureOnly, TAttribute<float> InDPIScale)
{
	bRenderToTextureOnly = bInRenderToTextureOnly;

	bool bOk = CreateRenderer(World, Widget, MoveTemp(InDPIScale));
	if (!bRenderToTextureOnly)
	{
		bOk &= InitPostProcessComponent(World);
	}

	return bOk;
}

void FVPFullScreenUserWidget_PostProcess::Hide(UWorld* World)
{
	if (!bRenderToTextureOnly)
	{
		ReleasePostProcessComponent();
	}

	FVPFullScreenUserWidget_PostProcessBase::Hide(World);
}

void FVPFullScreenUserWidget_PostProcess::Tick(UWorld* World, float DeltaSeconds)
{
	TickRenderer(World, DeltaSeconds);
}

bool FVPFullScreenUserWidget_PostProcess::InitPostProcessComponent(UWorld* World)
{
	ReleasePostProcessComponent();
	if (World && ensureMsgf(PostProcessMaterial, TEXT("Was supposed to have been inited by base class")))
	{
		const bool bUseExternalPostProcess = CustomPostProcessSettingsSource.IsValid();
		if (!bUseExternalPostProcess)
		{
			AWorldSettings* WorldSetting = World->GetWorldSettings();
			PostProcessComponent = NewObject<UPostProcessComponent>(WorldSetting, NAME_None, RF_Transient);
			PostProcessComponent->bEnabled = true;
			PostProcessComponent->bUnbound = true;
			PostProcessComponent->RegisterComponent();
		}

		return UpdateTargetPostProcessSettingsWithMaterial();
	}

	return false;
}

bool FVPFullScreenUserWidget_PostProcess::UpdateTargetPostProcessSettingsWithMaterial()
{
	UMaterialInstanceDynamic* MaterialInstance = PostProcessMaterialInstance;
	if (FPostProcessSettings* const PostProcessSettings = GetPostProcessSettings()
		; PostProcessSettings && ensure(MaterialInstance))
	{
		// User added blend material should not affect the widget so insert the material at the beginning
		const FWeightedBlendable Blendable{ 1.f, MaterialInstance };
			
		// Use case: Virtual Camera specifies an external post process settings
		// 1. Virtual Camera is activated > creates this widget
		// 2. Save Map
		// 3. Reload map > we'll have an empty slot in the blendables because PostProcessMaterialInstance is transient
		const bool bReuseOldEmptySlot = PostProcessSettings->WeightedBlendables.Array.Num() > 0 && !PostProcessSettings->WeightedBlendables.Array[0].Object;
		if (bReuseOldEmptySlot)
		{
			PostProcessSettings->WeightedBlendables.Array[0].Object = MaterialInstance;
		}
		else
		{
			PostProcessSettings->WeightedBlendables.Array.Insert(Blendable, 0);
		}
		return true;
	}

	return false;
}

void FVPFullScreenUserWidget_PostProcess::ReleasePostProcessComponent()
{
	const bool bIsRunning = PostProcessMaterialInstance != nullptr;
	if (!bIsRunning)
	{
		return;
	}
	
	const bool bNeedsToResetExternalSettings = CustomPostProcessSettingsSource.IsValid();
	if (FPostProcessSettings* Settings = GetPostProcessSettings()
		; bNeedsToResetExternalSettings && Settings)
	{
		const int32 Index = Settings->WeightedBlendables.Array.IndexOfByPredicate([this](const FWeightedBlendable& Blendable){ return Blendable.Object == PostProcessMaterialInstance; });
		if (Index != INDEX_NONE)
		{
			Settings->WeightedBlendables.Array.RemoveAt(Index);
		}
	}
	// CustomPostProcessSettingsSource may have gone stale
	else if (PostProcessComponent)
	{
		PostProcessComponent->UnregisterComponent();
	}
	
	PostProcessComponent = nullptr;
}

bool FVPFullScreenUserWidget_PostProcess::OnRenderTargetInited()
{
	// Outer needs to be transient package: otherwise we cause a world memory leak using "Save Current Level As" due to reference not getting replaced correctly
	PostProcessMaterialInstance = UMaterialInstanceDynamic::Create(PostProcessMaterial, GetTransientPackage());
	PostProcessMaterialInstance->SetFlags(RF_Transient);
	if (ensure(PostProcessMaterialInstance))
	{
		using namespace UE::VPUtilities::Private;
		PostProcessMaterialInstance->SetTextureParameterValue(NAME_SlateUI, WidgetRenderTarget);
		PostProcessMaterialInstance->SetVectorParameterValue(NAME_TintColorAndOpacity, PostProcessTintColorAndOpacity);
		PostProcessMaterialInstance->SetScalarParameterValue(NAME_OpacityFromTexture, PostProcessOpacityFromTexture);
		return true;
	}
	return false;
}

FPostProcessSettings* FVPFullScreenUserWidget_PostProcess::GetPostProcessSettings() const
{
	if (PostProcessComponent)
	{
		return &PostProcessComponent->Settings;
	}

	if (!CustomPostProcessSettingsSource.IsValid())
	{
		return nullptr;
	}

	// The easiest way without overcomplicating the API with an additional callback would be to look for the first struct property.
	// We could always extend our API to accept a callback that extracts the FPostProcessSettings from the UObject instead.
	for (TFieldIterator<FStructProperty> StructIt(CustomPostProcessSettingsSource.Get()->GetClass()); StructIt; ++StructIt)
	{
		if (StructIt->Struct == FPostProcessSettings::StaticStruct())
		{
			return StructIt->ContainerPtrToValuePtr<FPostProcessSettings>(CustomPostProcessSettingsSource.Get());
		}
	}

	return nullptr;
}
