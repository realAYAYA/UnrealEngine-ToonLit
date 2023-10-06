// Copyright Epic Games, Inc. All Rights Reserved.

#include "Widgets/UIFButton.h"
#include "Types/UIFWidgetTree.h"
#include "UIFLog.h"
#include "UIFModule.h"

#include "Components/Button.h"
#include "Components/ButtonSlot.h"

#include "Misc/RedirectCollector.h"
#include "Net/UnrealNetwork.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(UIFButton)


UUIFrameworkButtonWidget::UUIFrameworkButtonWidget()
{
	FButtonStyle TempStyle = GetStyle();
	TempStyle.NormalPadding = FMargin(0.0f);
	TempStyle.Normal.DrawAs = ESlateBrushDrawType::NoDrawType;
	TempStyle.Hovered.DrawAs = ESlateBrushDrawType::NoDrawType;
	TempStyle.Pressed.DrawAs = ESlateBrushDrawType::NoDrawType;
	SetStyle(TempStyle);
}

/**
 * 
 */
UUIFrameworkButton::UUIFrameworkButton()
{
// TODO: FIXME: Loading this asset fails. Temporary fixed by using UUIFrameworkButtonWidget::StaticClass()
// 	            Does this actually need to be an asset meant to be customizable or can it be set like above?

//	WidgetClass = FSoftObjectPath(TEXT("/UIFramework/Widgets/WBP_UIF_Button.WBP_UIF_Button"));
//#if WITH_EDITOR
//	// We need to ensure that this asset is cooked
//	GRedirectCollector.OnSoftObjectPathLoaded(WidgetClass.ToSoftObjectPath(), nullptr);
//#endif

	WidgetClass = UUIFrameworkButtonWidget::StaticClass();
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
			FUIFrameworkModule::AuthorityDetachWidgetFromParent(Slot.AuthorityGetWidget());
		}
	}

	Slot = InEntry;

	if (bWidgetIsDifferent && Slot.AuthorityGetWidget())
	{
		// Reset the widget to make sure the id is set and it may have been duplicated during the attach
		Slot.AuthoritySetWidget(FUIFrameworkModule::AuthorityAttachWidget(this, Slot.AuthorityGetWidget()));
	}

	MARK_PROPERTY_DIRTY_FROM_NAME(ThisClass, Slot, this);
	ForceNetUpdate();
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
	ForceNetUpdate();
}

void UUIFrameworkButton::LocalOnUMGWidgetCreated()
{
	Super::LocalOnUMGWidgetCreated();
	UButton* Button = CastChecked<UButton>(LocalGetUMGWidget());
	Button->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleClick);
}


void UUIFrameworkButton::LocalAddChild(FUIFrameworkWidgetId ChildId)
{
	FUIFrameworkWidgetTree* WidgetTree = GetWidgetTree();
	if (ChildId == Slot.GetWidgetId() && WidgetTree)
	{
		if (UUIFrameworkWidget* ChildWidget = WidgetTree->FindWidgetById(ChildId))
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

void UUIFrameworkButton::HandleClick()
{
	// todo the click event should send the userid
	ServerClick(Cast<APlayerController>(GetOuter()));
}

void UUIFrameworkButton::ServerClick_Implementation(APlayerController* PlayerController)
{
	FUIFrameworkClickEventArgument Argument;
	Argument.PlayerController = PlayerController;
	Argument.Sender = this;
	OnClick.Broadcast(Argument);
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
