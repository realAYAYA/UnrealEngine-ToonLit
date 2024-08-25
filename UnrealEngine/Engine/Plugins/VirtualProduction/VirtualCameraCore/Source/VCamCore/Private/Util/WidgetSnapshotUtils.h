// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "VCamCore.h"
#include "UI/WidgetSnapshots.h"

class UWidget;
struct FWidgetTreeSnapshot;

namespace UE::VCamCore::WidgetSnapshotUtils::Private
{
	struct FWidgetSnapshotSettings
	{
		TSet<TSubclassOf<UWidget>> AllowedWidgetClasses;
		TSet<const FProperty*> AllowedProperties;

		bool IsClassAllowed(TSubclassOf<UWidget> Widget) const;
	};
	
	/** Saves the state of the widget */
	FWidgetTreeSnapshot TakeTreeHierarchySnapshot(UUserWidget& Widget, const FWidgetSnapshotSettings& SnapshotSettings = VCamCore::Private::FVCamCoreModule::Get().GetSnapshotSettings());
	/** Applies the data saved in Snapshot to Widget*/
	bool ApplyTreeHierarchySnapshot(const FWidgetTreeSnapshot& Snapshot, UUserWidget& Widget);

	/** Updates the snapshot only for this widget in the hierarchy if a matching snapshot exists (otherwise does not add it).*/
	void RetakeSnapshotForWidgetInHierarchy(FWidgetTreeSnapshot& WidgetTreeSnapshot, UWidget& Widget, const FWidgetSnapshotSettings& SnapshotSettings = VCamCore::Private::FVCamCoreModule::Get().GetSnapshotSettings());

	FWidgetSnapshot TakeWidgetSnapshot(UWidget& Widget, const TSet<const FProperty*>& AllowedProperties);
	void ApplyWidgetSnapshot(const FWidgetSnapshot& Snapshot, UWidget& Widget);
};
