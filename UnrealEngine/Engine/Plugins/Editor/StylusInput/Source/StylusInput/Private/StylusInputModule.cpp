// Copyright Epic Games, Inc. All Rights Reserved.

#include "IStylusInputModule.h"
#include "CoreMinimal.h"

#include "Framework/Docking/TabManager.h"
#include "Interfaces/IMainFrameModule.h"
#include "Logging/LogMacros.h"
#include "Misc/App.h"
#include "Widgets/Docking/SDockTab.h"
#include "Widgets/SWindow.h"
#include "WorkspaceMenuStructure.h"
#include "WorkspaceMenuStructureModule.h"
#include "SStylusInputDebugWidget.h"

#define LOCTEXT_NAMESPACE "FStylusInputModule"


static const FName StylusInputDebugTabName = FName("StylusInputDebug");

class FStylusInputModule : public IModuleInterface
{
};

IMPLEMENT_MODULE(FStylusInputModule, StylusInput)

// This is the function that all platform-specific implementations are required to implement.
TSharedPtr<IStylusInputInterfaceInternal> CreateStylusInputInterface();

#if PLATFORM_WINDOWS
#include "WindowsStylusInputInterface.h"
#else
TSharedPtr<IStylusInputInterfaceInternal> CreateStylusInputInterface() { return TSharedPtr<IStylusInputInterfaceInternal>(); }
#endif

// TODO: Other platforms

void UStylusInputSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	if (FApp::IsUnattended() || IsRunningCommandlet())
	{
		return;
	}

	Super::Initialize(Collection);

	UE_LOG(LogStylusInput, Log, TEXT("Initializing StylusInput subsystem."));

	InputInterface = CreateStylusInputInterface();

	if (!InputInterface.IsValid())
	{
		UE_LOG(LogStylusInput, Log, TEXT("StylusInput not supported on this platform."));
		return;
	}

	const TSharedRef<FGlobalTabmanager>& TabManager = FGlobalTabmanager::Get();
	const IWorkspaceMenuStructure& MenuStructure = WorkspaceMenu::GetMenuStructure();

	TabManager->RegisterNomadTabSpawner(StylusInputDebugTabName,
		FOnSpawnTab::CreateUObject(this, &UStylusInputSubsystem::OnSpawnPluginTab))
		.SetDisplayName(LOCTEXT("DebugTabTitle", "Stylus Input Debug"))
		.SetTooltipText(LOCTEXT("DebugTabTooltip", "Debug panel to display current values of stylus inputs."))
		.SetIcon(FSlateIcon(FAppStyle::GetAppStyleSetName(), "StylusInputDebug.TabIcon"))
		.SetGroup(MenuStructure.GetDeveloperToolsDebugCategory());
}

void UStylusInputSubsystem::Deinitialize()
{
	Super::Deinitialize();

	FGlobalTabmanager::Get()->UnregisterTabSpawner(StylusInputDebugTabName);

	InputInterface.Reset();

	UE_LOG(LogStylusInput, Log, TEXT("Shutting down StylusInput subsystem."));
}

int32 UStylusInputSubsystem::NumInputDevices() const
{
	if (InputInterface.IsValid())
	{
		return InputInterface->NumInputDevices();
	}
	return 0;
}

const IStylusInputDevice* UStylusInputSubsystem::GetInputDevice(int32 Index) const
{
	if (InputInterface.IsValid())
	{
		return InputInterface->GetInputDevice(Index);
	}
	return nullptr;
}

void UStylusInputSubsystem::AddMessageHandler(IStylusMessageHandler& InHandler)
{
	MessageHandlers.AddUnique(&InHandler);
}

void UStylusInputSubsystem::RemoveMessageHandler(IStylusMessageHandler& InHandler)
{
	MessageHandlers.Remove(&InHandler);
}

void UStylusInputSubsystem::Tick(float DeltaTime)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(UStylusInputSubsystem::Tick);

	if (InputInterface.IsValid())
	{
		InputInterface->Tick();

		for (int32 DeviceIdx = 0; DeviceIdx < NumInputDevices(); ++DeviceIdx)
		{
			IStylusInputDevice* InputDevice = InputInterface->GetInputDevice(DeviceIdx);
			if (InputDevice->IsDirty())
			{
				InputDevice->Tick();

				for (IStylusMessageHandler* Handler : MessageHandlers)
				{
					Handler->OnStylusStateChanged(InputDevice->GetCurrentState(), DeviceIdx);
				}
			}
		}
	}
}

TSharedRef<SDockTab> UStylusInputSubsystem::OnSpawnPluginTab(const FSpawnTabArgs& Args)
{
	return SNew(SDockTab)
		.TabRole(ETabRole::NomadTab)
		[
			SNew(SStylusInputDebugWidget, *this)
		];
}



#undef LOCTEXT_NAMESPACE