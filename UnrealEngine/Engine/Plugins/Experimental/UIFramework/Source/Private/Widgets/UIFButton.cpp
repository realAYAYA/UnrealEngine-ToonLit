// Copyright Epic Games, Inc. All Rights Reserved.

#include "Widgets/UIFButton.h"
#include "Types/UIFWidgetTree.h"
#include "UIFLog.h"
#include "UIFPlayerComponent.h"

#include "Components/Button.h"
#include "Components/ButtonSlot.h"

#include "Engine/ActorChannel.h"
#include "Engine/Engine.h"
#include "Engine/NetDriver.h"
#include "GameFramework/Actor.h"
#include "GameFramework/PlayerController.h"
#include "Net/UnrealNetwork.h"


/**
 * 
 */
UUIFrameworkButton::UUIFrameworkButton()
{
	WidgetClass = UButton::StaticClass();
}

void UUIFrameworkButton::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	FDoRepLifetimeParams Params;
	Params.bIsPushBased = true;
	DOREPLIFETIME_WITH_PARAMS_FAST(ThisClass, Slot, Params);
}

void UUIFrameworkButton::SetContent(FUIFrameworkSimpleSlot InEntry)
{
	bool bWidgetIsDifferent = Slot.AuthorityGetWidget() != InEntry.AuthorityGetWidget();
	if (bWidgetIsDifferent)
	{
		// Remove previous widget
		if (Slot.AuthorityGetWidget())
		{
			Slot.AuthorityGetWidget()->AuthoritySetParent(GetPlayerComponent(), FUIFrameworkParentWidget());
		}

		if (InEntry.AuthorityGetWidget())
		{
			UUIFrameworkPlayerComponent* PreviousOwner = InEntry.AuthorityGetWidget()->GetPlayerComponent();
			if (PreviousOwner != nullptr && PreviousOwner != GetPlayerComponent())
			{
				Slot.AuthoritySetWidget(nullptr);
				FFrame::KismetExecutionMessage(TEXT("The widget was created for another player. It can't be added."), ELogVerbosity::Warning, "InvalidPlayerParent");
			}
		}
	}

	Slot = InEntry;
	Slot.AuthoritySetWidget(InEntry.AuthorityGetWidget()); // to make sure the id is set

	if (bWidgetIsDifferent && Slot.AuthorityGetWidget())
	{
		Slot.AuthorityGetWidget()->AuthoritySetParent(GetPlayerComponent(), FUIFrameworkParentWidget(this));
	}

	MARK_PROPERTY_DIRTY_FROM_NAME(ThisClass, Slot, this);
}


void UUIFrameworkButton::AuthorityForEachChildren(const TFunctionRef<void(UUIFrameworkWidget*)>& Func)
{
	Super::AuthorityForEachChildren(Func);
	if (UUIFrameworkWidget* ChildWidget = Slot.AuthorityGetWidget())
	{
		Func(ChildWidget);
	}
}

void UUIFrameworkButton::AuthorityRemoveChild(UUIFrameworkWidget* Widget)
{
	Super::AuthorityRemoveChild(Widget);
	ensure(Widget == Slot.AuthorityGetWidget());

	Slot.AuthoritySetWidget(nullptr);;
	MARK_PROPERTY_DIRTY_FROM_NAME(ThisClass, Slot, this);
}

void UUIFrameworkButton::LocalOnUMGWidgetCreated()
{
	Super::LocalOnUMGWidgetCreated();
	UButton* Button = CastChecked<UButton>(LocalGetUMGWidget());
	Button->OnClicked.AddUniqueDynamic(this, &ThisClass::ServerClick);
}


void UUIFrameworkButton::LocalAddChild(FUIFrameworkWidgetId ChildId)
{
	if (ChildId == Slot.GetWidgetId())
	{
		check(GetPlayerComponent());
		if (UUIFrameworkWidget* ChildWidget = GetPlayerComponent()->GetWidgetTree().FindWidgetById(ChildId))
		{
			UWidget* ChildUMGWidget = ChildWidget->LocalGetUMGWidget();
			if (ensure(ChildUMGWidget))
			{
				Slot.LocalAquireWidget();

				UButton* Button = CastChecked<UButton>(LocalGetUMGWidget());
				Button->ClearChildren();
				UButtonSlot* ButtonSlot = CastChecked<UButtonSlot>(Button->AddChild(ChildUMGWidget));
				ButtonSlot->SetPadding(Slot.Padding);
				ButtonSlot->SetHorizontalAlignment(Slot.HorizontalAlignment);
				ButtonSlot->SetVerticalAlignment(Slot.VerticalAlignment);
			}
		}
	}
	else
	{
		UE_LOG(LogUIFramework, Verbose, TEXT("The widget '%" INT64_FMT "' was not found in the Button Slots."), ChildId.GetKey());
		Super::LocalAddChild(ChildId);
	}
}

void UUIFrameworkButton::ServerClick_Implementation()
{
	if (GetPlayerComponent())
	{
		FUIFrameworkClickEventArgument Argument;
		Argument.PlayerController = GetPlayerComponent()->GetPlayerController();
		Argument.Sender = this;
		OnClick.Broadcast(Argument);
	}
}

void UUIFrameworkButton::OnRep_Slot()
{
	if (LocalGetUMGWidget() && Slot.LocalIsAquiredWidgetValid())
	{
		if (UButtonSlot* ButtonSlot = Cast<UButtonSlot>(LocalGetUMGWidget()->Slot))
		{
			ButtonSlot->SetPadding(Slot.Padding);
			ButtonSlot->SetHorizontalAlignment(Slot.HorizontalAlignment);
			ButtonSlot->SetVerticalAlignment(Slot.VerticalAlignment);
		}
	}
	// else do not do anything, the slot was not added yet or it was modified but was not applied yet by the PlayerComponent
}
