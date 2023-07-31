// Copyright Epic Games, Inc. All Rights Reserved.

#include "Components/ComboBoxKey.h"

#include "Styling/UMGCoreStyle.h"
#include "UObject/ConstructorHelpers.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SNullWidget.h"
#include "Widgets/Text/STextBlock.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ComboBoxKey)

#define LOCTEXT_NAMESPACE "UMG"

/////////////////////////////////////////////////////
// UComboBoxKey

UComboBoxKey::UComboBoxKey()
{
	// We don't want to try and load fonts on the server.
	if (!IsRunningDedicatedServer())
	{
#if WITH_EDITOR 
		const ISlateStyle& SlateStyle = IsEditorWidget() ? FCoreStyle::Get() : FUMGCoreStyle::Get();
		WidgetStyle = SlateStyle.GetWidgetStyle<FComboBoxStyle>(IsEditorWidget() ? "EditorUtilityComboBox" : "ComboBox");
		ItemStyle = SlateStyle.GetWidgetStyle<FTableRowStyle>("TableView.Row");

		if (IsEditorWidget())
		{
			// The CDO isn't an editor widget and thus won't use the editor style, call post edit change to mark difference from CDO
			PostEditChange();
		}
#else
		WidgetStyle = FUMGCoreStyle::Get().GetWidgetStyle<FComboBoxStyle>("ComboBox");
		ItemStyle = FUMGCoreStyle::Get().GetWidgetStyle<FTableRowStyle>("TableView.Row");
#endif // WITH_EDITOR
	}

	ContentPadding = FMargin(4.0, 2.0);
	ForegroundColor = ItemStyle.TextColor;

	MaxListHeight = 450.0f;
	bHasDownArrow = true;
	bEnableGamepadNavigationMode = true;
	bIsFocusable = true;
}


void UComboBoxKey::ReleaseSlateResources(bool bReleaseChildren)
{
	Super::ReleaseSlateResources(bReleaseChildren);

	MyComboBox.Reset();
	ComboBoxContent.Reset();
}


TSharedRef<SWidget> UComboBoxKey::RebuildWidget()
{
	MyComboBox =
		SNew(SComboBox<FName>)
		.ComboBoxStyle(&WidgetStyle)
		.ItemStyle(&ItemStyle)
		.ForegroundColor(ForegroundColor)
		.OptionsSource(&Options)
		.InitiallySelectedItem(SelectedOption)
		.ContentPadding(ContentPadding)
		.MaxListHeight(MaxListHeight)
		.HasDownArrow(bHasDownArrow)
		.EnableGamepadNavigationMode(bEnableGamepadNavigationMode)
		.OnGenerateWidget_UObject(this, &UComboBoxKey::HandleGenerateItemWidget)
		.OnSelectionChanged_UObject(this, &UComboBoxKey::HandleSelectionChanged)
		.OnComboBoxOpening_UObject(this, &UComboBoxKey::HandleOpening)
		.IsFocusable(bIsFocusable)
		[
			SAssignNew(ComboBoxContent, SBox)
		];

	GenerateContent();

	return MyComboBox.ToSharedRef();
}


void UComboBoxKey::AddOption(FName Key)
{
	if (Options.AddUnique(Key) >= 0)
	{
		if (MyComboBox)
		{
			MyComboBox->RefreshOptions();
		}
	}
}


bool UComboBoxKey::RemoveOption(FName Key)
{
	bool bResult = Options.RemoveSingle(Key) > 0;
	if (bResult)
	{
		if (Key == SelectedOption)
		{
			ClearSelection();
		}

		if (MyComboBox)
		{
			MyComboBox->RefreshOptions();
		}
	}
	return bResult;
}


void UComboBoxKey::ClearOptions()
{
	Options.Reset();
	ClearSelection();
	if (MyComboBox)
	{
		MyComboBox->RefreshOptions();
	}
}


void UComboBoxKey::ClearSelection()
{
	if (MyComboBox)
	{
		MyComboBox->ClearSelection();
	}
}


void UComboBoxKey::SetSelectedOption(FName Option)
{
	if (SelectedOption != Option)
	{
		if (MyComboBox)
		{
			MyComboBox->SetSelectedItem(Option);
		}
		else
		{
			SelectedOption = Option;
		}
	}
}


FName UComboBoxKey::GetSelectedOption() const
{
	return SelectedOption;
}


bool UComboBoxKey::IsOpen() const
{
	return MyComboBox && MyComboBox->IsOpen();
}


void UComboBoxKey::GenerateContent()
{
	bool bSetToNull = true;
	if (IsDesignTime())
	{
		if (!SelectedOption.IsNone())
		{
			ComboBoxContent->SetContent(SNew(STextBlock)
				.Text(FText::FromName(SelectedOption)));
			bSetToNull = false;
		}
	}
	else
	{
		// Call the user's delegate to see if they want to generate a custom widget bound to the data source.
		if (OnGenerateContentWidget.IsBound())
		{
			if (UWidget* Widget = OnGenerateContentWidget.Execute(SelectedOption))
			{
				ComboBoxContent->SetContent(Widget->TakeWidget());
				bSetToNull = false;
			}
		}
		else
		{
			// Warn the user that they need to generate a widget for the item
			ComboBoxContent->SetContent(
				SNew(STextBlock)
				.Text(FText::Format(LOCTEXT("WarningMissingOnGenerateContentWidget", "{0} Missing OnGenerateContentWidget"), FText::FromName(SelectedOption)))
				);
			bSetToNull = false;
		}
	}

	if (bSetToNull)
	{
		ComboBoxContent->SetContent(SNullWidget::NullWidget);
	}
}


TSharedRef<SWidget> UComboBoxKey::HandleGenerateItemWidget(FName Item)
{
	if (IsDesignTime())
	{
		return SNew(STextBlock)
			.Text(FText::FromName(Item));
	}
	else
	{
		// Call the user's delegate to see if they want to generate a custom widget bound to the data source.
		if (OnGenerateItemWidget.IsBound())
		{
			if (UWidget* Widget = OnGenerateItemWidget.Execute(Item))
			{
				return Widget->TakeWidget();
			}
			else
			{
				return SNullWidget::NullWidget;
			}
		}
	}

	return SNew(STextBlock)
		.Text(FText::Format(LOCTEXT("WarningMissingOnGenerateItemWidget", "{0} Missing OnGenerateItemWidget"), FText::FromName(Item)));
}


void UComboBoxKey::HandleSelectionChanged(FName Item, ESelectInfo::Type SelectionType)
{
	if (SelectedOption != Item)
	{
		SelectedOption = Item;

		if (!IsDesignTime())
		{
			OnSelectionChanged.Broadcast(Item, SelectionType);
		}
		GenerateContent();
	}
}

void UComboBoxKey::HandleOpening()
{
	if (!IsDesignTime())
	{
		OnOpening.Broadcast();
	}
}

#if WITH_EDITOR

const FText UComboBoxKey::GetPaletteCategory()
{
	return LOCTEXT("Input", "Input");
}

#endif

/////////////////////////////////////////////////////

#undef LOCTEXT_NAMESPACE

