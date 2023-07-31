// Copyright Epic Games, Inc. All Rights Reserved.

#include "Components/InputKeySelector.h"
#include "Engine/Font.h"
#include "UObject/ConstructorHelpers.h"
#include "UObject/FrameworkObjectVersion.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/Input/SInputKeySelector.h"
#include "Internationalization/Internationalization.h"
#include "Styling/UMGCoreStyle.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(InputKeySelector)

#define LOCTEXT_NAMESPACE "UMG"

static FButtonStyle* DefaultInputKeySelectorButtonStyle = nullptr;
static FTextBlockStyle* DefaultInputKeySelectorTextStyle = nullptr;

#if WITH_EDITOR 
static FButtonStyle* EditorInputKeySelectorButtonStyle = nullptr;
static FTextBlockStyle* EditorInputKeySelectorTextStyle = nullptr;
#endif

UInputKeySelector::UInputKeySelector( const FObjectInitializer& ObjectInitializer )
	: Super(ObjectInitializer)
{
	if (DefaultInputKeySelectorButtonStyle == nullptr)
	{
		DefaultInputKeySelectorButtonStyle = new FButtonStyle(FUMGCoreStyle::Get().GetWidgetStyle<FButtonStyle>("Button"));

		// Unlink UMG default colors.
		DefaultInputKeySelectorButtonStyle->UnlinkColors();
	}

	if (DefaultInputKeySelectorTextStyle == nullptr)
	{
		DefaultInputKeySelectorTextStyle = new FTextBlockStyle(FUMGCoreStyle::Get().GetWidgetStyle<FTextBlockStyle>("NormalText"));

		// Unlink UMG default colors.
		DefaultInputKeySelectorTextStyle->UnlinkColors();
	}

	WidgetStyle = *DefaultInputKeySelectorButtonStyle;
	TextStyle = *DefaultInputKeySelectorTextStyle;

#if WITH_EDITOR 
	if (EditorInputKeySelectorButtonStyle == nullptr)
	{
		EditorInputKeySelectorButtonStyle = new FButtonStyle(FCoreStyle::Get().GetWidgetStyle<FButtonStyle>("Button"));

		// Unlink UMG Editor colors from the editor settings colors.
		EditorInputKeySelectorButtonStyle->UnlinkColors();
	}

	if (EditorInputKeySelectorTextStyle == nullptr)
	{
		EditorInputKeySelectorTextStyle = new FTextBlockStyle(FCoreStyle::Get().GetWidgetStyle<FTextBlockStyle>("NormalText"));

		// Unlink UMG Editor colors from the editor settings colors.
		EditorInputKeySelectorTextStyle->UnlinkColors();
	}

	if (IsEditorWidget())
	{
		WidgetStyle = *EditorInputKeySelectorButtonStyle;
		TextStyle = *EditorInputKeySelectorTextStyle;

		// The CDO isn't an editor widget and thus won't use the editor style, call post edit change to mark difference from CDO
		PostEditChange();
	}
#endif // WITH_EDITOR

	KeySelectionText = NSLOCTEXT("InputKeySelector", "DefaultKeySelectionText", "...");
	NoKeySpecifiedText = NSLOCTEXT("InputKeySelector", "DefaultEmptyText", "Empty");
	SelectedKey = FInputChord(EKeys::Invalid);
	bAllowModifierKeys = true;
	bAllowGamepadKeys = false;

	EscapeKeys.AddUnique(EKeys::Gamepad_Special_Right); // In most (if not all) cases this is going to be the menu button

	if (!IsRunningDedicatedServer())
	{
		static ConstructorHelpers::FObjectFinder<UFont> RobotoFontObj(*UWidget::GetDefaultFontName());
		TextStyle.Font = FSlateFontInfo(RobotoFontObj.Object, 24, FName("Bold"));
	}
}

void UInputKeySelector::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);

	Ar.UsingCustomVersion(FFrameworkObjectVersion::GUID);
}

void UInputKeySelector::SetSelectedKey( const FInputChord& InSelectedKey )
{
	if ( MyInputKeySelector.IsValid() )
	{
		MyInputKeySelector->SetSelectedKey( InSelectedKey );
	}
	SelectedKey = InSelectedKey;
}

void UInputKeySelector::SetKeySelectionText( FText InKeySelectionText )
{
	if ( MyInputKeySelector.IsValid() )
	{
		MyInputKeySelector->SetKeySelectionText( InKeySelectionText );
	}
	KeySelectionText = MoveTemp(InKeySelectionText);
}

void UInputKeySelector::SetNoKeySpecifiedText(FText InNoKeySpecifiedText)
{
	if (MyInputKeySelector.IsValid())
	{
		MyInputKeySelector->SetNoKeySpecifiedText(InNoKeySpecifiedText);
	}
	NoKeySpecifiedText = MoveTemp(InNoKeySpecifiedText);
}

void UInputKeySelector::SetAllowModifierKeys( const bool bInAllowModifierKeys )
{
	if ( MyInputKeySelector.IsValid() )
	{
		MyInputKeySelector->SetAllowModifierKeys( bInAllowModifierKeys );
	}
	bAllowModifierKeys = bInAllowModifierKeys;
}

void UInputKeySelector::SetAllowGamepadKeys(const bool bInAllowGamepadKeys)
{
	if (MyInputKeySelector.IsValid())
	{
		MyInputKeySelector->SetAllowGamepadKeys(bInAllowGamepadKeys);
	}
	bAllowGamepadKeys = bInAllowGamepadKeys;
}

bool UInputKeySelector::GetIsSelectingKey() const
{
	return MyInputKeySelector.IsValid() ? MyInputKeySelector->GetIsSelectingKey() : false;
}

void UInputKeySelector::SetButtonStyle( const FButtonStyle* InButtonStyle )
{
	if ( MyInputKeySelector.IsValid() )
	{
		MyInputKeySelector->SetButtonStyle(InButtonStyle);
	}
	WidgetStyle = *InButtonStyle;
}

void UInputKeySelector::SetEscapeKeys(const TArray<FKey>& InKeys)
{
	if (MyInputKeySelector.IsValid())
	{
		MyInputKeySelector->SetEscapeKeys(InKeys);
	}
	EscapeKeys = InKeys;
}
#if WITH_EDITOR
const FText UInputKeySelector::GetPaletteCategory()
{
	return LOCTEXT("Advanced", "Advanced");
}
#endif

void UInputKeySelector::SynchronizeProperties()
{
	Super::SynchronizeProperties();

	MyInputKeySelector->SetSelectedKey( SelectedKey );
	MyInputKeySelector->SetMargin( Margin );
	MyInputKeySelector->SetButtonStyle( &WidgetStyle );
	MyInputKeySelector->SetTextStyle( &TextStyle );
	MyInputKeySelector->SetKeySelectionText( KeySelectionText );
	MyInputKeySelector->SetNoKeySpecifiedText(NoKeySpecifiedText);
	MyInputKeySelector->SetAllowModifierKeys( bAllowModifierKeys );
	MyInputKeySelector->SetAllowGamepadKeys(bAllowGamepadKeys);
	MyInputKeySelector->SetEscapeKeys(EscapeKeys);
}

void UInputKeySelector::ReleaseSlateResources(bool bReleaseChildren)
{
	Super::ReleaseSlateResources(bReleaseChildren);

	MyInputKeySelector.Reset();
}

TSharedRef<SWidget> UInputKeySelector::RebuildWidget()
{
	MyInputKeySelector = SNew(SInputKeySelector)
		.SelectedKey(SelectedKey)
		.Margin(Margin)
		.ButtonStyle(&WidgetStyle)
		.TextStyle(&TextStyle)
		.KeySelectionText(KeySelectionText)
		.NoKeySpecifiedText(NoKeySpecifiedText)
		.AllowModifierKeys(bAllowModifierKeys)
		.AllowGamepadKeys(bAllowGamepadKeys)
		.EscapeKeys(EscapeKeys)
		.OnKeySelected( BIND_UOBJECT_DELEGATE( SInputKeySelector::FOnKeySelected, HandleKeySelected ) )
		.OnIsSelectingKeyChanged( BIND_UOBJECT_DELEGATE( SInputKeySelector::FOnIsSelectingKeyChanged, HandleIsSelectingKeyChanged ) );
	return MyInputKeySelector.ToSharedRef();
}

void UInputKeySelector::HandleKeySelected(const FInputChord& InSelectedKey)
{
	SelectedKey = InSelectedKey;
	OnKeySelected.Broadcast(SelectedKey);
}

void UInputKeySelector::HandleIsSelectingKeyChanged()
{
	OnIsSelectingKeyChanged.Broadcast();
}

void UInputKeySelector::SetTextBlockVisibility(const ESlateVisibility InVisibility)
{
	if (MyInputKeySelector.IsValid())
	{
		EVisibility SlateVisibility = UWidget::ConvertSerializedVisibilityToRuntime(InVisibility);
		MyInputKeySelector->SetTextBlockVisibility(SlateVisibility);
	}
}

#undef LOCTEXT_NAMESPACE
