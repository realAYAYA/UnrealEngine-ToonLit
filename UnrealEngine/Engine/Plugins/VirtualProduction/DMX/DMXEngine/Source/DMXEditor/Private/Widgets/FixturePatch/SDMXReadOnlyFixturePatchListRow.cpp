// Copyright Epic Games, Inc. All Rights Reserved.

#include "Widgets/SDMXReadOnlyFixturePatchListRow.h"

#include "DMXEditor.h"
#include "DMXEditorStyle.h"
#include "Library/DMXEntityFixturePatch.h"
#include "Library/DMXEntityFixtureType.h"
#include "Library/DMXLibrary.h"
#include "Styling/AppStyle.h"
#include "Widgets/Input/STextComboBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/SInlineEditableTextBlock.h"


#define LOCTEXT_NAMESPACE "SDMXReadOnlyFixturePatchListRow"

void SDMXReadOnlyFixturePatchListRow::Construct(const FArguments& InArgs, const TSharedRef<STableViewBase>& InOwnerTable, const TSharedRef<FDMXEntityFixturePatchRef>& InFixturePatchRef)
{
	FixturePatchRef = InFixturePatchRef.Get();

	SMultiColumnTableRow<TSharedPtr<FDMXEntityFixturePatchRef>>::Construct(
		FSuperRowType::FArguments()
		.OnDragDetected(InArgs._OnRowDragDetected)
		.Style(&FDMXEditorStyle::Get().GetWidgetStyle<FTableRowStyle>("FixturePatchList.Row")),
		InOwnerTable);
}

TSharedRef<SWidget> SDMXReadOnlyFixturePatchListRow::GenerateWidgetForColumn(const FName& ColumnName)
{
	if (ColumnName == FDMXReadOnlyFixturePatchListCollumnIDs::EditorColor)
	{
		return GenerateEditorColorRow();
	}
	else if (ColumnName == FDMXReadOnlyFixturePatchListCollumnIDs::FixturePatchName)
	{
		return GenerateFixturePatchNameRow();
	}
	else if (ColumnName == FDMXReadOnlyFixturePatchListCollumnIDs::FixtureID)
	{
		return GenerateFixtureIDRow();
	}
	else if (ColumnName == FDMXReadOnlyFixturePatchListCollumnIDs::FixtureType)
	{
		return GenerateFixtureTypeRow();
	}
	else if (ColumnName == FDMXReadOnlyFixturePatchListCollumnIDs::Mode)
	{
		return GenerateModeRow();
	}
	else if (ColumnName == FDMXReadOnlyFixturePatchListCollumnIDs::Patch)
	{
		return GeneratePatchRow();
	}

	return SNullWidget::NullWidget;
}

TSharedRef<SWidget> SDMXReadOnlyFixturePatchListRow::GenerateEditorColorRow()
{
	return
		SNew(SBorder)
		.HAlign(HAlign_Fill)
		.Padding(5.f, 2.f)
		.BorderImage(FAppStyle::GetBrush("NoBorder"))
		[
			SNew(SImage)
			.Image(FDMXEditorStyle::Get().GetBrush("DMXEditor.WhiteRoundedPropertyBorder"))
			.ColorAndOpacity(this, &SDMXReadOnlyFixturePatchListRow::GetFixtureEditorColor)
		];
}

TSharedRef<SWidget> SDMXReadOnlyFixturePatchListRow::GenerateFixturePatchNameRow()
{
	return
		SNew(SBorder)
		.HAlign(HAlign_Fill)
		.VAlign(VAlign_Center)
		.Padding(4.f)
		.BorderImage(FAppStyle::GetBrush("NoBorder"))
		[
			SNew(STextBlock)
			.Font(FAppStyle::GetFontStyle(TEXT("PropertyWindow.NormalFont")))
			.Text(this, &SDMXReadOnlyFixturePatchListRow::GetFixturePatchNameText)
			.ColorAndOpacity(FLinearColor::White)
		];
}

TSharedRef<SWidget> SDMXReadOnlyFixturePatchListRow::GenerateFixtureIDRow()
{
	return
		SNew(SBorder)
		.HAlign(HAlign_Fill)
		.VAlign(VAlign_Center)
		.Padding(4.f)
		.BorderImage(FAppStyle::GetBrush("NoBorder"))
		[
			SNew(STextBlock)
			.Font(FAppStyle::GetFontStyle(TEXT("PropertyWindow.NormalFont")))
			.Text(this, &SDMXReadOnlyFixturePatchListRow::GetFixtureIDText)
			.ColorAndOpacity(FLinearColor::White)
		];
}

TSharedRef<SWidget> SDMXReadOnlyFixturePatchListRow::GenerateFixtureTypeRow()
{
	return
		SNew(SBorder)
		.HAlign(HAlign_Fill)
		.VAlign(VAlign_Center)
		.Padding(4.f)
		.BorderImage(FAppStyle::GetBrush("NoBorder"))
		[
			SNew(STextBlock)
			.Font(FAppStyle::GetFontStyle(TEXT("PropertyWindow.NormalFont")))
			.Text(this, &SDMXReadOnlyFixturePatchListRow::GetFixtureTypeText)
			.ColorAndOpacity(FLinearColor::White)
		];
}

TSharedRef<SWidget> SDMXReadOnlyFixturePatchListRow::GenerateModeRow()
{
	return
		SNew(SBorder)
		.HAlign(HAlign_Fill)
		.VAlign(VAlign_Center)
		.Padding(4.f)
		.BorderImage(FAppStyle::GetBrush("NoBorder"))
		[
			SNew(SInlineEditableTextBlock)
			.Font(FAppStyle::GetFontStyle(TEXT("PropertyWindow.NormalFont")))
			.Text(this, &SDMXReadOnlyFixturePatchListRow::GetModeText)
			.ColorAndOpacity(FLinearColor::White)
		];
}

TSharedRef<SWidget> SDMXReadOnlyFixturePatchListRow::GeneratePatchRow()
{
	return
		SNew(SBorder)
		.HAlign(HAlign_Fill)
		.VAlign(VAlign_Center)
		.Padding(4.f)
		.BorderImage(FAppStyle::GetBrush("NoBorder"))
		[
			SNew(STextBlock)
			.Font(FAppStyle::GetFontStyle(TEXT("PropertyWindow.NormalFont")))
			.Text(this, &SDMXReadOnlyFixturePatchListRow::GetPatchText)
			.ColorAndOpacity(FLinearColor::White)
		];
}

FSlateColor SDMXReadOnlyFixturePatchListRow::GetFixtureEditorColor() const
{
	if (const UDMXEntityFixturePatch* FixturePatch = GetFixturePatch())
	{
		return FixturePatch->EditorColor;
	}

	return FSlateColor::UseForeground();
}

FText SDMXReadOnlyFixturePatchListRow::GetFixturePatchNameText() const
{
	if (const UDMXEntityFixturePatch* FixturePatch = GetFixturePatch())
	{
		const FString FixturePatchName = FixturePatch->Name;
		return FText::FromString(FixturePatchName);
	}

	return FText::GetEmpty();
}

FText SDMXReadOnlyFixturePatchListRow::GetFixtureIDText() const
{
	if (const UDMXEntityFixturePatch* FixturePatch = GetFixturePatch())
	{
		int32 FixtureID;
		if (FixturePatch->FindFixtureID(FixtureID))
		{
			const FString FixtureIDAsString = FString::FromInt(FixtureID);
			return FText::FromString(FixtureIDAsString);
		}
	}

	return FText::GetEmpty();
}

FText SDMXReadOnlyFixturePatchListRow::GetFixtureTypeText() const
{
	if (const UDMXEntityFixturePatch* FixturePatch = GetFixturePatch())
	{
		if (FixturePatch->GetFixtureType())
		{
			const FString FixtureTypeName = FixturePatch->GetFixtureType()->Name;
			return FText::FromString(FixtureTypeName);
		}
	}

	return FText::GetEmpty();
}

FText SDMXReadOnlyFixturePatchListRow::GetModeText() const
{
	if (const UDMXEntityFixturePatch* FixturePatch = GetFixturePatch())
	{
		if (FixturePatch->GetActiveMode())
		{
			const FString FixtureModeName = FixturePatch->GetActiveMode()->ModeName;
			return FText::FromString(FixtureModeName);
		}
	}

	return FText::GetEmpty();
}

FText SDMXReadOnlyFixturePatchListRow::GetPatchText() const
{
	if (const UDMXEntityFixturePatch* FixturePatch = GetFixturePatch())
	{
		const int32 UniverseID = FixturePatch->GetUniverseID();
		const int32 StartingAddress = FixturePatch->GetStartingChannel();
		return FText::Format(LOCTEXT("AddressesText", "{0}.{1}"), UniverseID, StartingAddress);
	}

	return FText::GetEmpty();
}

#undef LOCTEXT_NAMESPACE
