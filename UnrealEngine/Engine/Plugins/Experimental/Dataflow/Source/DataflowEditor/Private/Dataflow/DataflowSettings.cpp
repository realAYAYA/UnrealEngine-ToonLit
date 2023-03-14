// Copyright Epic Games, Inc. All Rights Reserved.

#include "Dataflow/DataflowSettings.h"

UDataflowSettings::UDataflowSettings(const FObjectInitializer& ObjectInitlaizer)
	: Super(ObjectInitlaizer)
{
	ManagedArrayCollectionPinTypeColor = FLinearColor(0.353393f, 0.454175f, 1.0f, 1.0f);
	ArrayPinTypeColor = FLinearColor(1.0f, 0.172585f, 0.0f, 1.0f);
	BoxPinTypeColor = FLinearColor(0.013575f, 0.770000f, 0.429609f, 1.0f);

	GeometryCollectionCategoryNodeTitleColor = FLinearColor(0.55f, 0.0f, 1.f);
	GeometryCollectionCategoryNodeBodyTintColor = FLinearColor(0.55f, 0.0f, 1.f);
}

FName UDataflowSettings::GetCategoryName() const
{
	return TEXT("Plugins");
}

FText UDataflowSettings::GetSectionText() const
{
	return NSLOCTEXT("DataflowPlugin", "DataflowSettingsSection", "Dataflow");
}

void UDataflowSettings::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	if (PropertyChangedEvent.Property != nullptr)
	{
		SettingsChangedDelegate.Broadcast(PropertyChangedEvent.Property->GetFName(), this);
	}
}

UDataflowSettings::FOnDataflowSettingsChanged& UDataflowSettings::OnSettingsChanged()
{
	return SettingsChangedDelegate;
}

UDataflowSettings::FOnDataflowSettingsChanged UDataflowSettings::SettingsChangedDelegate;




