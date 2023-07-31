// Copyright Epic Games, Inc. All Rights Reserved.

#include "Components/CheckBox.h"
#include "Widgets/SNullWidget.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/Input/SCheckBox.h"
#include "Slate/SlateBrushAsset.h"
#include "Styling/UMGCoreStyle.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(CheckBox)

#define LOCTEXT_NAMESPACE "UMG"

/////////////////////////////////////////////////////
// UCheckBox

static FCheckBoxStyle* DefaultCheckboxStyle = nullptr;

#if WITH_EDITOR
static FCheckBoxStyle* EditorCheckboxStyle = nullptr;
#endif 

UCheckBox::UCheckBox(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	if (DefaultCheckboxStyle == nullptr)
	{
		DefaultCheckboxStyle = new FCheckBoxStyle(FUMGCoreStyle::Get().GetWidgetStyle<FCheckBoxStyle>("Checkbox"));

		// Unlink UMG default colors.
		DefaultCheckboxStyle->UnlinkColors();
	}

PRAGMA_DISABLE_DEPRECATION_WARNINGS
	WidgetStyle = *DefaultCheckboxStyle;

#if WITH_EDITOR 
	if (EditorCheckboxStyle == nullptr)
	{
		EditorCheckboxStyle = new FCheckBoxStyle(FCoreStyle::Get().GetWidgetStyle<FCheckBoxStyle>("Checkbox"));

		// Unlink UMG Editor colors from the editor settings colors.
		EditorCheckboxStyle->UnlinkColors();
	}

	if (IsEditorWidget())
	{
		WidgetStyle = *EditorCheckboxStyle;

		// The CDO isn't an editor widget and thus won't use the editor style, call post edit change to mark difference from CDO
		PostEditChange();
	}
#endif // WITH_EDITOR

	CheckedState = ECheckBoxState::Unchecked;

	HorizontalAlignment = HAlign_Fill;

	ClickMethod = EButtonClickMethod::DownAndUp;
	TouchMethod = EButtonTouchMethod::DownAndUp;
	PressMethod = EButtonPressMethod::DownAndUp;
PRAGMA_ENABLE_DEPRECATION_WARNINGS

	IsFocusable = true;
#if WITH_EDITORONLY_DATA
	AccessibleBehavior = ESlateAccessibleBehavior::Summary;
	bCanChildrenBeAccessible = false;
#endif
}

void UCheckBox::ReleaseSlateResources(bool bReleaseChildren)
{
	Super::ReleaseSlateResources(bReleaseChildren);

	MyCheckbox.Reset();
}

TSharedRef<SWidget> UCheckBox::RebuildWidget()
{
PRAGMA_DISABLE_DEPRECATION_WARNINGS
	MyCheckbox = SNew(SCheckBox)
		.OnCheckStateChanged( BIND_UOBJECT_DELEGATE(FOnCheckStateChanged, SlateOnCheckStateChangedCallback) )
		.Style(&WidgetStyle)
		.HAlign( HorizontalAlignment )
		.ClickMethod(ClickMethod)
		.TouchMethod(TouchMethod)
		.PressMethod(PressMethod)
		.IsFocusable(IsFocusable)
		;
PRAGMA_ENABLE_DEPRECATION_WARNINGS

	if ( GetChildrenCount() > 0 )
	{
		MyCheckbox->SetContent(GetContentSlot()->Content ? GetContentSlot()->Content->TakeWidget() : SNullWidget::NullWidget);
	}
	
	return MyCheckbox.ToSharedRef();
}

void UCheckBox::SynchronizeProperties()
{
	Super::SynchronizeProperties();


PRAGMA_DISABLE_DEPRECATION_WARNINGS
	MyCheckbox->SetStyle(&WidgetStyle);
	MyCheckbox->SetIsChecked( PROPERTY_BINDING(ECheckBoxState, CheckedState) );
	MyCheckbox->SetClickMethod(ClickMethod);
	MyCheckbox->SetTouchMethod(TouchMethod);
	MyCheckbox->SetPressMethod(PressMethod);
PRAGMA_ENABLE_DEPRECATION_WARNINGS
}

void UCheckBox::OnSlotAdded(UPanelSlot* InSlot)
{
	// Add the child to the live slot if it already exists
	if ( MyCheckbox.IsValid() )
	{
		MyCheckbox->SetContent(InSlot->Content ? InSlot->Content->TakeWidget() : SNullWidget::NullWidget);
	}
}

void UCheckBox::OnSlotRemoved(UPanelSlot* InSlot)
{
	// Remove the widget from the live slot if it exists.
	if ( MyCheckbox.IsValid() )
	{
		MyCheckbox->SetContent(SNullWidget::NullWidget);
	}
}

bool UCheckBox::IsPressed() const
{
	if ( MyCheckbox.IsValid() )
	{
		return MyCheckbox->IsPressed();
	}

	return false;
}


PRAGMA_DISABLE_DEPRECATION_WARNINGS
EButtonClickMethod::Type UCheckBox::GetClickMethod() const
{
	return ClickMethod;
}

void UCheckBox::SetClickMethod(EButtonClickMethod::Type InClickMethod)
{
	ClickMethod = InClickMethod;
	if (MyCheckbox.IsValid())
	{
		MyCheckbox->SetClickMethod(ClickMethod);
	}
}

EButtonTouchMethod::Type UCheckBox::GetTouchMethod() const
{
	return TouchMethod;
}

void UCheckBox::SetTouchMethod(EButtonTouchMethod::Type InTouchMethod)
{
	TouchMethod = InTouchMethod;
	if (MyCheckbox.IsValid())
	{
		MyCheckbox->SetTouchMethod(TouchMethod);
	}
}

void UCheckBox::SetPressMethod(EButtonPressMethod::Type InPressMethod)
{
	PressMethod = InPressMethod;
	if (MyCheckbox.IsValid())
	{
		MyCheckbox->SetPressMethod(PressMethod);
	}
}

EButtonPressMethod::Type UCheckBox::GetPressMethod() const
{
	return PressMethod;
}

bool UCheckBox::IsChecked() const
{
	if ( MyCheckbox.IsValid() )
	{
		return MyCheckbox->IsChecked();
	}

	return ( CheckedState == ECheckBoxState::Checked );
}

ECheckBoxState UCheckBox::GetCheckedState() const
{
	if ( MyCheckbox.IsValid() )
	{
		return MyCheckbox->GetCheckedState();
	}

	return CheckedState;
}

void UCheckBox::SetIsChecked(bool InIsChecked)
{
	ECheckBoxState NewState = InIsChecked ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
	if (NewState != CheckedState)
	{
		CheckedState = NewState;
		BroadcastFieldValueChanged(FFieldNotificationClassDescriptor::CheckedState);
	}

	if ( MyCheckbox.IsValid() )
	{
		MyCheckbox->SetIsChecked(PROPERTY_BINDING(ECheckBoxState, CheckedState));
	}
}

void UCheckBox::SetCheckedState(ECheckBoxState InCheckedState)
{
	if (CheckedState != InCheckedState)
	{
		CheckedState = InCheckedState;
		BroadcastFieldValueChanged(FFieldNotificationClassDescriptor::CheckedState);
	}

	if ( MyCheckbox.IsValid() )
	{
		MyCheckbox->SetIsChecked(PROPERTY_BINDING(ECheckBoxState, CheckedState));
	}
}

const FCheckBoxStyle& UCheckBox::GetWidgetStyle() const
{
	return WidgetStyle;
}

void UCheckBox::SetWidgetStyle(const FCheckBoxStyle& InStyle)
{
	WidgetStyle = InStyle;

	if (MyCheckbox)
	{
		MyCheckbox->SetStyle(&WidgetStyle);
	}
}
PRAGMA_ENABLE_DEPRECATION_WARNINGS

void UCheckBox::SlateOnCheckStateChangedCallback(ECheckBoxState NewState)
{
PRAGMA_DISABLE_DEPRECATION_WARNINGS
	if (CheckedState != NewState)
	{
		CheckedState = NewState;
		BroadcastFieldValueChanged(FFieldNotificationClassDescriptor::CheckedState);
	}
PRAGMA_ENABLE_DEPRECATION_WARNINGS

	const bool bWantsToBeChecked = NewState != ECheckBoxState::Unchecked;
	OnCheckStateChanged.Broadcast(bWantsToBeChecked);
}

#if WITH_ACCESSIBILITY
TSharedPtr<SWidget> UCheckBox::GetAccessibleWidget() const
{
	return MyCheckbox;
}
#endif

#if WITH_EDITOR

const FText UCheckBox::GetPaletteCategory()
{
	return LOCTEXT("Common", "Common");
}

#endif

/////////////////////////////////////////////////////

#undef LOCTEXT_NAMESPACE

