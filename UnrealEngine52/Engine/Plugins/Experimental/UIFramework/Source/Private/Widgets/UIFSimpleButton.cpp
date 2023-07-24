// Copyright Epic Games, Inc. All Rights Reserved.

#include "Widgets/UIFSimpleButton.h"

#include "Blueprint/UserWidget.h"
#include "MVVMSubsystem.h"
#include "Net/Core/PushModel/PushModel.h"
#include "Net/UnrealNetwork.h"
#include "Types/MVVMEventField.h"
#include "View/MVVMView.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(UIFSimpleButton)


/**
 * 
 */
UUIFrameworkSimpleButton::UUIFrameworkSimpleButton()
{
	WidgetClass = FSoftObjectPath(TEXT("/UIFramework/Widgets/WBP_UIFSimpleButton.WBP_UIFSimpleButton_C"));
}


void UUIFrameworkSimpleButton::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	FDoRepLifetimeParams Params;
	Params.bIsPushBased = true;
	DOREPLIFETIME_WITH_PARAMS_FAST(ThisClass, Text, Params);
}


void UUIFrameworkSimpleButton::LocalOnUMGWidgetCreated()
{
	UUserWidget* UserWidget = Cast<UUserWidget>(LocalGetUMGWidget());
	if (ensure(UserWidget))
	{
		if (UMVVMView* View = UMVVMSubsystem::GetViewFromUserWidget(UserWidget))
		{
			View->SetViewModel(TEXT("Widget"), this);
		}
	}
}


void UUIFrameworkSimpleButton::SetText(FText InText)
{
	Text = InText;
	MARK_PROPERTY_DIRTY_FROM_NAME(ThisClass, Text, this);
	ForceNetUpdate();
	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(Text);
}


void UUIFrameworkSimpleButton::OnRep_Text()
{
	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(Text);
}


void UUIFrameworkSimpleButton::OnClick(FMVVMEventField Field)
{
	// todo the click event should send the userid
	ServerClick(Cast<APlayerController>(GetOuter()));
}


void UUIFrameworkSimpleButton::ServerClick_Implementation(APlayerController* PlayerController)
{
	ClickEvent.PlayerController = PlayerController;
	ClickEvent.Sender = this;
	BroadcastFieldValueChanged(ThisClass::FFieldNotificationClassDescriptor::ClickEvent);
}
