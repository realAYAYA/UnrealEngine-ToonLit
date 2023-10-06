// Copyright Epic Games, Inc. All Rights Reserved.

#include "XRCreativeSubsystem.h"
#include "XRCreativeSettings.h"
#include "Misc/CoreDelegates.h"
#include "UObject/Package.h"

#if WITH_EDITOR
#	include "IVREditorModule.h"
#endif


void UXRCreativeSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	EngineInitCompleteDelegate = FCoreDelegates::OnFEngineLoopInitComplete.AddUObject(this, &UXRCreativeSubsystem::OnEngineInitComplete);
}


#if WITH_EDITOR
bool UXRCreativeSubsystem::EnterVRMode()
{
	IVREditorModule& VREditorModule = IVREditorModule::Get();
	if (VREditorModule.IsVREditorAvailable())
	{
		VREditorModule.EnableVREditor(true);
		return true;
	}

	return false;
}


void UXRCreativeSubsystem::ExitVRMode()
{
	IVREditorModule& VREditorModule = IVREditorModule::Get();
	if (VREditorModule.IsVREditorEnabled())
	{
		VREditorModule.EnableVREditor(false);
	}
}
#endif // #if WITH_EDITOR


void UXRCreativeSubsystem::OnEngineInitComplete()
{
	EngineInitCompleteDelegate.Reset();

	const UXRCreativeSettings* Settings = UXRCreativeSettings::GetXRCreativeSettings();
	if (UClass* HelpersClass = Settings->SubsystemHelpersClass.LoadSynchronous())
	{
		Helpers = NewObject<UXRCreativeSubsystemHelpers>(GetTransientPackage(), HelpersClass);
	}
}
