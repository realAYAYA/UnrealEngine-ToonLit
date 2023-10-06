// Copyright Epic Games, Inc. All Rights Reserved.

#include "VCamCore.h"

#include "LogVCamCore.h"
#include "UI/VCamWidget.h"
#include "Util/WidgetSnapshotUtils.h"

#include "EnhancedInputDeveloperSettings.h"
#include "Misc/CoreDelegates.h"

#define LOCTEXT_NAMESPACE "FVCamCoreModule"

namespace UE::VCamCore::Private
{
	void FVCamCoreModule::StartupModule()
	{
		FCoreDelegates::OnPostEngineInit.AddLambda([]()
		{
			UEnhancedInputDeveloperSettings* Settings = GetMutableDefault<UEnhancedInputDeveloperSettings>();
			UE_CLOG(!Settings->bEnableUserSettings, LogVCamCore, Log, TEXT("Overriding Settings->bEnableUserSettings = true because it is required for VCam to work properly."));
			Settings->bEnableUserSettings = true;
		});
	}

	void FVCamCoreModule::ShutdownModule()
	{
		
	}

	WidgetSnapshotUtils::Private::FWidgetSnapshotSettings FVCamCoreModule::GetSnapshotSettings() const
	{
		// In the future this could be exposed via project settings or via registration functions on IVCamCoreModule
		const TSet<TSubclassOf<UWidget>> AllowedWidgetClasses { UVCamWidget::StaticClass() };
		const TSet<const FProperty*> AllowedProperties { UVCamWidget::StaticClass()->FindPropertyByName(GET_MEMBER_NAME_CHECKED(UVCamWidget, Connections)) };
		return WidgetSnapshotUtils::Private::FWidgetSnapshotSettings{
			AllowedWidgetClasses,
			AllowedProperties
		};
	}
}

#undef LOCTEXT_NAMESPACE
	
IMPLEMENT_MODULE(UE::VCamCore::Private::FVCamCoreModule, VCamCore)