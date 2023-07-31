// Copyright Epic Games, Inc. All Rights Reserved.

#include "GameResponsivePanelSlot.h"

#include "Components/Widget.h"
#include "SlotBase.h"
#include "UObject/ObjectPtr.h"
#include "Widgets/Responsive/SGameResponsivePanel.h"
#include "Widgets/SNullWidget.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(GameResponsivePanelSlot)

/////////////////////////////////////////////////////
// UGameResponsivePanelSlot

UGameResponsivePanelSlot::UGameResponsivePanelSlot(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	Slot = nullptr;
}

void UGameResponsivePanelSlot::ReleaseSlateResources(bool bReleaseChildren)
{
	Super::ReleaseSlateResources(bReleaseChildren);

	Slot = nullptr;
}

void UGameResponsivePanelSlot::BuildSlot(TSharedRef<SGameResponsivePanel> GameResponsivePanel)
{
	Slot = &GameResponsivePanel->AddSlot()
	[
		Content == nullptr ? SNullWidget::NullWidget : Content->TakeWidget()
	];
}

void UGameResponsivePanelSlot::SynchronizeProperties()
{
}

