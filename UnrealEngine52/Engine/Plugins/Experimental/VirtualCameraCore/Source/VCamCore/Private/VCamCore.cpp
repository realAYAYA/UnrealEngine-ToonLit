// Copyright Epic Games, Inc. All Rights Reserved.

#include "VCamCore.h"

#include "UI/VCamWidget.h"
#include "Util/WidgetSnapshotUtils.h"

#define LOCTEXT_NAMESPACE "FVCamCoreModule"

namespace UE::VCamCore::Private
{
	void FVCamCoreModule::StartupModule()
	{
		
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