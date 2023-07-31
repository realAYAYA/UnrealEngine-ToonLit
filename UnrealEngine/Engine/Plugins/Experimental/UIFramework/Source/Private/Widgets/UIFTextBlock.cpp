// Copyright Epic Games, Inc. All Rights Reserved.

#include "Widgets/UIFTextBlock.h"

#include "Blueprint/UserWidget.h"
#include "Components/TextBlock.h"
#include "Net/UnrealNetwork.h"


/**
 *
 */
UUIFrameworkTextBlock::UUIFrameworkTextBlock()
{
	WidgetClass = UTextBlock::StaticClass();
}


void UUIFrameworkTextBlock::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	FDoRepLifetimeParams Params;
	Params.bIsPushBased = true;
	DOREPLIFETIME_WITH_PARAMS_FAST(ThisClass, Text, Params);
}


void UUIFrameworkTextBlock::LocalOnUMGWidgetCreated()
{
	CastChecked<UTextBlock>(LocalGetUMGWidget())->SetText(Text);
}


void UUIFrameworkTextBlock::SetText(FText InText)
{
	Text = InText;
	MARK_PROPERTY_DIRTY_FROM_NAME(ThisClass, Text, this);
}


void UUIFrameworkTextBlock::OnRep_Text()
{
	if (LocalGetUMGWidget())
	{
		CastChecked<UTextBlock>(LocalGetUMGWidget())->SetText(Text);
	}
}
