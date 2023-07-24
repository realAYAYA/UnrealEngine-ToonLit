// Copyright Epic Games, Inc. All Rights Reserved.

#include "STextPropertyEditableTextBox.h"
#include "AssetRegistry/AssetData.h"
#include "Internationalization/TextNamespaceUtil.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SGridPanel.h"
#include "Widgets/Layout/SUniformGridPanel.h"
#include "Widgets/Layout/SWidgetSwitcher.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SMultiLineEditableTextBox.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Input/SComboButton.h"
#include "Widgets/Input/SComboBox.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Input/SSearchBox.h"
#include "Styling/AppStyle.h"
#include "Misc/PackageName.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Internationalization/StringTable.h"
#include "Internationalization/TextPackageNamespaceUtil.h"
#include "Internationalization/StringTableCore.h"
#include "Internationalization/StringTableRegistry.h"
#include "Serialization/TextReferenceCollector.h"
#include "Styling/StyleColors.h"
#include "Widgets/Layout/SLinkedBox.h"
#include "SSimpleComboButton.h"

#define LOCTEXT_NAMESPACE "STextPropertyEditableTextBox"

FText STextPropertyEditableTextBox::MultipleValuesText(NSLOCTEXT("PropertyEditor", "MultipleValues", "Multiple Values"));

#if USE_STABLE_LOCALIZATION_KEYS

void IEditableTextProperty::StaticStableTextId(UObject* InObject, const ETextPropertyEditAction InEditAction, const FString& InTextSource, const FString& InProposedNamespace, const FString& InProposedKey, FString& OutStableNamespace, FString& OutStableKey)
{
	UPackage* Package = InObject ? InObject->GetOutermost() : nullptr;
	StaticStableTextId(Package, InEditAction, InTextSource, InProposedNamespace, InProposedKey, OutStableNamespace, OutStableKey);
}

void IEditableTextProperty::StaticStableTextId(UPackage* InPackage, const ETextPropertyEditAction InEditAction, const FString& InTextSource, const FString& InProposedNamespace, const FString& InProposedKey, FString& OutStableNamespace, FString& OutStableKey)
{
	bool bPersistKey = false;

	const FString PackageNamespace = TextNamespaceUtil::EnsurePackageNamespace(InPackage);
	if (!PackageNamespace.IsEmpty())
	{
		// Make sure the proposed namespace is using the correct namespace for this package
		OutStableNamespace = TextNamespaceUtil::BuildFullNamespace(InProposedNamespace, PackageNamespace, /*bAlwaysApplyPackageNamespace*/true);

		if (InProposedNamespace.Equals(OutStableNamespace, ESearchCase::CaseSensitive) || InEditAction == ETextPropertyEditAction::EditedNamespace)
		{
			// If the proposal was already using the correct namespace (or we just set the namespace), attempt to persist the proposed key too
			if (!InProposedKey.IsEmpty())
			{
				// If we changed the source text, then we can persist the key if this text is the *only* reference using that ID
				// If we changed the identifier, then we can persist the key only if doing so won't cause an identify conflict
				const FTextReferenceCollector::EComparisonMode ReferenceComparisonMode = InEditAction == ETextPropertyEditAction::EditedSource ? FTextReferenceCollector::EComparisonMode::MatchId : FTextReferenceCollector::EComparisonMode::MismatchSource;
				const int32 RequiredReferenceCount = InEditAction == ETextPropertyEditAction::EditedSource ? 1 : 0;

				int32 ReferenceCount = 0;
				FTextReferenceCollector(InPackage, ReferenceComparisonMode, OutStableNamespace, InProposedKey, InTextSource, ReferenceCount);

				if (ReferenceCount == RequiredReferenceCount)
				{
					bPersistKey = true;
					OutStableKey = InProposedKey;
				}
			}
		}
		else if (InEditAction != ETextPropertyEditAction::EditedNamespace)
		{
			// If our proposed namespace wasn't correct for our package, and we didn't just set it (which doesn't include the package namespace)
			// then we should clear out any user specified part of it
			OutStableNamespace = TextNamespaceUtil::BuildFullNamespace(FString(), PackageNamespace, /*bAlwaysApplyPackageNamespace*/true);
		}
	}

	if (!bPersistKey)
	{
		OutStableKey = FGuid::NewGuid().ToString();
	}
}

#endif // USE_STABLE_LOCALIZATION_KEYS

void STextPropertyEditableStringTableReference::Construct(const FArguments& InArgs, const TSharedRef<IEditableTextProperty>& InEditableTextProperty)
{
	EditableTextProperty = InEditableTextProperty;

	OptionTextFilter = MakeShareable(new FOptionTextFilter(FOptionTextFilter::FItemToStringArray::CreateLambda([](const TSharedPtr<FAvailableStringTable>& InItem, OUT TArray< FString >& StringArray) {
		StringArray.Add(InItem->DisplayName.ToString());
	})));
	KeyTextFilter = MakeShareable(new FKeyTextFilter(FKeyTextFilter::FItemToStringArray::CreateLambda([](const TSharedPtr<FString>& InItem, OUT TArray< FString >& StringArray) {
		StringArray.Add(*InItem);
	})));

	TSharedRef<SHorizontalBox> HorizontalBox = SNew(SHorizontalBox);

	HorizontalBox->AddSlot()
		.Padding(0)
		[
			SAssignNew(StringTableOptionsCombo, SComboButton)
			.ComboButtonStyle(&InArgs._ComboStyle->ComboButtonStyle)
			.ContentPadding(FMargin(4.0, 2.0))
			.OnGetMenuContent(this, &STextPropertyEditableStringTableReference::OnGetStringTableComboOptions)
			.OnComboBoxOpened(this, &STextPropertyEditableStringTableReference::UpdateStringTableComboOptions)
			.CollapseMenuOnParentFocus(true)
			.ButtonContent()
			[
				SNew(STextBlock)
				.Text(this, &STextPropertyEditableStringTableReference::GetStringTableComboContent)
				.ToolTipText(this, &STextPropertyEditableStringTableReference::GetStringTableComboToolTip)
				.Font(InArgs._Font)
			]
		];

	HorizontalBox->AddSlot()
		.Padding(10, 0)
		[
			SAssignNew(StringTableKeysCombo, SComboButton)
			.ComboButtonStyle(&InArgs._ComboStyle->ComboButtonStyle)
			.ContentPadding(FMargin(4.0, 2.0))
			.IsEnabled(this, &STextPropertyEditableStringTableReference::IsUnlinkEnabled)
			.OnGetMenuContent(this, &STextPropertyEditableStringTableReference::OnGetStringTableKeyOptions)
			.OnComboBoxOpened(this, &STextPropertyEditableStringTableReference::UpdateStringTableKeyOptions)
			.CollapseMenuOnParentFocus(true)
			.ButtonContent()
			[
				SNew(STextBlock)
				.Text(this, &STextPropertyEditableStringTableReference::GetKeyComboContent)
				.ToolTipText(this, &STextPropertyEditableStringTableReference::GetKeyComboToolTip)
				.Font(InArgs._Font)
			]
		];

	if (InArgs._AllowUnlink)
	{
		HorizontalBox->AddSlot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			[
				SNew(SBox)
				.HAlign(HAlign_Center)
				.VAlign(VAlign_Center)
				.HeightOverride(22)
				.WidthOverride(22)
				[
					SNew(SButton)
					.ButtonStyle(InArgs._ButtonStyle)
					.ContentPadding(0)
					.ToolTipText(LOCTEXT("UnlinkStringTable", "Unlink"))
					.IsEnabled(this, &STextPropertyEditableStringTableReference::IsUnlinkEnabled)
					.OnClicked(this, &STextPropertyEditableStringTableReference::OnUnlinkClicked)
					[
						SNew(SImage)
						.Image(FAppStyle::GetBrush("Icons.Delete"))
						.ColorAndOpacity(FSlateColor::UseForeground())
					]
				]
			];
	}

	ChildSlot
	[
		HorizontalBox
	];
}

void STextPropertyEditableStringTableReference::OnOptionsFilterTextChanged(const FText& InNewText)
{
	OptionTextFilter->SetRawFilterText(InNewText);
	OptionsSearchBox->SetError(OptionTextFilter->GetFilterErrorText());

	UpdateStringTableComboOptions();
}

void STextPropertyEditableStringTableReference::OnKeysFilterTextChanged(const FText& InNewText)
{
	KeyTextFilter->SetRawFilterText(InNewText);
	KeysSearchBox->SetError(KeyTextFilter->GetFilterErrorText());

	UpdateStringTableKeyOptions();
}

TSharedRef<SWidget> STextPropertyEditableStringTableReference::OnGetStringTableComboOptions()
{
	const FComboButtonStyle& ComboButtonStyle = FCoreStyle::Get().GetWidgetStyle< FComboButtonStyle >("ComboButton");
	return SNew(SBorder)
		.BorderImage(&ComboButtonStyle.MenuBorderBrush)
		.Padding(ComboButtonStyle.MenuBorderPadding)
		[
			SNew(SVerticalBox)
			+SVerticalBox::Slot()
			.AutoHeight()
			[
				SAssignNew(OptionsSearchBox, SSearchBox)
				.OnTextChanged(this, &STextPropertyEditableStringTableReference::OnOptionsFilterTextChanged)
			]
			+SVerticalBox::Slot()
			.AutoHeight()
			[
			SNew(SWidgetSwitcher)
			.WidgetIndex_Lambda([this]() { return StringTableComboOptions.IsEmpty() ? 0 : 1; })

				+SWidgetSwitcher::Slot() // Appears when there are no string tables with keys
				.Padding(12)
				[
					SNew(STextBlock).Text(LOCTEXT("EmptyStringTableList", "No string tables available"))
				]

				+SWidgetSwitcher::Slot() // Appears when there's a string table with at least a key
				[
					SNew(SBox)
					.Padding(4)
					.WidthOverride(280)
					.MaxDesiredHeight(600)
					[
						SNew(SVerticalBox)
						+SVerticalBox::Slot()
						.FillHeight(1.f)
						.Padding(0, 5, 0, 0)
						[
							SAssignNew(StringTableOptionsList, SListView<TSharedPtr<FAvailableStringTable>>)
							.ListItemsSource(&StringTableComboOptions)
							.SelectionMode(ESelectionMode::Single)
							.OnGenerateRow(this, &STextPropertyEditableStringTableReference::OnGenerateStringTableComboOption)
							.OnSelectionChanged(this, &STextPropertyEditableStringTableReference::OnStringTableComboChanged)
						]
					]
				]
			]
		];
}

TSharedRef<ITableRow> STextPropertyEditableStringTableReference::OnGenerateStringTableComboOption(TSharedPtr<FAvailableStringTable> InItem, const TSharedRef<STableViewBase>& OwnerTable)
{
	return SNew(STableRow<TSharedPtr<FString>>, OwnerTable)
		[
			SNew(STextBlock)
			.Text(InItem->DisplayName)
			.ToolTipText(FText::FromName(InItem->TableId))
		];
}

TSharedRef<SWidget> STextPropertyEditableStringTableReference::OnGetStringTableKeyOptions()
{
	const FComboButtonStyle& ComboButtonStyle = FCoreStyle::Get().GetWidgetStyle< FComboButtonStyle >("ComboButton");
	return SNew(SBorder)
		.BorderImage(&ComboButtonStyle.MenuBorderBrush)
		.Padding(ComboButtonStyle.MenuBorderPadding)
		[
			SNew(SBox)
			.Padding(4)
			.WidthOverride(280)
			.MaxDesiredHeight(600)
			[
				SNew(SVerticalBox)
				+SVerticalBox::Slot()
				.AutoHeight()
				[
					SAssignNew(KeysSearchBox, SSearchBox)
					.OnTextChanged(this, &STextPropertyEditableStringTableReference::OnKeysFilterTextChanged)
				]
				+SVerticalBox::Slot()
				.FillHeight(1.f)
				.Padding(0, 5, 0, 0)
				[
					SAssignNew(StringTableKeysList, SListView<TSharedPtr<FString>>)
					.ListItemsSource(&KeyComboOptions)
					.SelectionMode(ESelectionMode::Single)
					.OnGenerateRow(this, &STextPropertyEditableStringTableReference::OnGenerateStringTableKeyOption)
					.OnSelectionChanged(this, &STextPropertyEditableStringTableReference::OnKeyComboChanged)
				]
			]
		];
}

TSharedRef<ITableRow> STextPropertyEditableStringTableReference::OnGenerateStringTableKeyOption(TSharedPtr<FString> InItem, const TSharedRef<STableViewBase>& OwnerTable)
{
	return SNew(STableRow<TSharedPtr<FString>>, OwnerTable)
		[
			SNew(STextBlock)
			.Text(FText::FromString(*InItem))
			.ToolTipText(FText::FromString(*InItem))
		];
}

void STextPropertyEditableStringTableReference::GetTableIdAndKey(FName& OutTableId, FString& OutKey) const
{
	const int32 NumTexts = EditableTextProperty->GetNumTexts();
	if (NumTexts > 0)
	{
		const FText PropertyValue = EditableTextProperty->GetText(0);
		FTextInspector::GetTableIdAndKey(PropertyValue, OutTableId, OutKey);

		// Verify that all texts are using the same string table and key
		for (int32 TextIndex = 1; TextIndex < NumTexts; ++TextIndex)
		{
			FName TmpTableId;
			FString TmpKey;
			if (FTextInspector::GetTableIdAndKey(PropertyValue, TmpTableId, TmpKey) && OutTableId == TmpTableId)
			{
				if (!OutKey.Equals(TmpKey, ESearchCase::CaseSensitive))
				{
					// Not using the same key - clear the key but keep the table and keep enumerating to verify the table on the remaining texts
					OutKey.Reset();
				}
			}
			else
			{
				// Not using a string table, or using a different string table - clear both table ID and key
				OutTableId = FName();
				OutKey.Reset();
				break;
			}
		}
	}
}

void STextPropertyEditableStringTableReference::SetTableIdAndKey(const FName InTableId, const FString& InKey)
{
	const FText TextToSet = FText::FromStringTable(InTableId, InKey);
	if (TextToSet.IsFromStringTable())
	{
		const int32 NumTexts = EditableTextProperty->GetNumTexts();
		for (int32 TextIndex = 0; TextIndex < NumTexts; ++TextIndex)
		{
			EditableTextProperty->SetText(TextIndex, TextToSet);
		}
	}
}

void STextPropertyEditableStringTableReference::OnStringTableComboChanged(TSharedPtr<FAvailableStringTable> NewSelection, ESelectInfo::Type SelectInfo)
{
	// If it's set from code, we did that on purpose
	if (SelectInfo != ESelectInfo::Direct && NewSelection.IsValid())
	{
		// Make sure any selected string table asset is loaded
		FName TableId = NewSelection->TableId;
		IStringTableEngineBridge::FullyLoadStringTableAsset(TableId);

		FStringTableConstPtr StringTable = FStringTableRegistry::Get().FindStringTable(TableId);
		if (StringTable.IsValid())
		{
			// Just use the first key when changing the string table
			StringTable->EnumerateSourceStrings([&](const FString& InKey, const FString& InSourceString) -> bool
			{
				SetTableIdAndKey(TableId, InKey);
				return false; // stop enumeration
			});

			StringTableOptionsCombo->SetIsOpen(false);

			OptionsSearchBox->SetText(FText::GetEmpty());
		}
	}
}

void STextPropertyEditableStringTableReference::UpdateStringTableComboOptions()
{
	FName CurrentTableId;
	{
		FString TmpKey;
		GetTableIdAndKey(CurrentTableId, TmpKey);
	}

	TSharedPtr<FAvailableStringTable> SelectedStringTableComboEntry;
	StringTableComboOptions.Reset();

	// Process assets first (as they may currently be unloaded)
	{
		FAssetRegistryModule& AssetRegistryModule = FModuleManager::Get().LoadModuleChecked<FAssetRegistryModule>(AssetRegistryConstants::ModuleName);

		TArray<FAssetData> StringTableAssets;
		AssetRegistryModule.Get().GetAssetsByClass(UStringTable::StaticClass()->GetClassPathName(), StringTableAssets);

		for (const FAssetData& StringTableAsset : StringTableAssets)
		{
			FName StringTableId = *StringTableAsset.GetObjectPathString();
			// Only allow string tables assets that have entries to be visible otherwise unexpected behavior happens for the user
			bool HasEntries = false;
			FStringTableConstPtr StringTable = FStringTableRegistry::Get().FindStringTable(StringTableId);
			if (StringTable.IsValid())
			{
				StringTable->EnumerateSourceStrings([&](const FString& InKey, const FString& InSourceString) -> bool
				{
					HasEntries = true;
					return false; // stop enumeration
				});
			}
			else
			{
				// Asset is currently unloaded, so just assume it has entries
				HasEntries = true;
			}

			if (!HasEntries)
			{
				continue; // continue on to the next string table asset
			}

			TSharedRef<FAvailableStringTable> AvailableStringTableEntry = MakeShared<FAvailableStringTable>();
			AvailableStringTableEntry->TableId = StringTableId;
			AvailableStringTableEntry->DisplayName = FText::FromName(StringTableAsset.AssetName);
			if (StringTableId == CurrentTableId)
			{
				SelectedStringTableComboEntry = AvailableStringTableEntry;
			}
			if (OptionTextFilter->PassesFilter(AvailableStringTableEntry))
			{
				StringTableComboOptions.Add(AvailableStringTableEntry);
			}
		}
	}

	// Process the remaining non-asset string tables now
	FStringTableRegistry::Get().EnumerateStringTables([&](const FName& InTableId, const FStringTableConstRef& InStringTable) -> bool
	{
		const bool bAlreadyAdded = StringTableComboOptions.ContainsByPredicate([InTableId](const TSharedPtr<FAvailableStringTable>& InAvailableStringTable)
		{
			return InAvailableStringTable->TableId == InTableId;
		});

		bool bHasEntries = false;
		InStringTable->EnumerateSourceStrings([&bHasEntries](const FString& InKey, const FString& InSourceString) -> bool
		{
			bHasEntries = true;
			return false; // stop enumeration
		});

		if (!bAlreadyAdded && bHasEntries)
		{
			TSharedRef<FAvailableStringTable> AvailableStringTableEntry = MakeShared<FAvailableStringTable>();
			AvailableStringTableEntry->TableId = InTableId;
			AvailableStringTableEntry->DisplayName = FText::FromName(InTableId);
			if (InTableId == CurrentTableId)
			{
				SelectedStringTableComboEntry = AvailableStringTableEntry;
			}
			if (OptionTextFilter->PassesFilter(AvailableStringTableEntry))
			{
				StringTableComboOptions.Add(AvailableStringTableEntry);
			}
		}

		return true; // continue enumeration
	});

	StringTableComboOptions.Sort([](const TSharedPtr<FAvailableStringTable>& InOne, const TSharedPtr<FAvailableStringTable>& InTwo)
	{
		return InOne->DisplayName.ToString() < InTwo->DisplayName.ToString();
	});

	StringTableOptionsList->RebuildList();

	if (SelectedStringTableComboEntry.IsValid())
	{
		StringTableOptionsList->SetItemSelection(SelectedStringTableComboEntry, true);
	}
	else
	{
		StringTableOptionsList->ClearSelection();
	}
}

FText STextPropertyEditableStringTableReference::GetStringTableComboContent() const
{
	FName CurrentTableId;
	{
		FString TmpKey;
		GetTableIdAndKey(CurrentTableId, TmpKey);
	}

	return FText::FromString(FPackageName::GetLongPackageAssetName(CurrentTableId.ToString()));
}

FText STextPropertyEditableStringTableReference::GetStringTableComboToolTip() const
{
	FName CurrentTableId;
	{
		FString TmpKey;
		GetTableIdAndKey(CurrentTableId, TmpKey);
	}

	return FText::FromName(CurrentTableId);
}

void STextPropertyEditableStringTableReference::OnKeyComboChanged(TSharedPtr<FString> NewSelection, ESelectInfo::Type SelectInfo)
{
	// If it's set from code, we did that on purpose
	if (SelectInfo != ESelectInfo::Direct && NewSelection.IsValid())
	{
		FName CurrentTableId;
		{
			FString TmpKey;
			GetTableIdAndKey(CurrentTableId, TmpKey);
		}

		SetTableIdAndKey(CurrentTableId, *NewSelection);

		StringTableKeysCombo->SetIsOpen(false);

		KeysSearchBox->SetText(FText::GetEmpty());
	}
}

void STextPropertyEditableStringTableReference::UpdateStringTableKeyOptions()
{
	FName CurrentTableId;
	FString CurrentKey;
	GetTableIdAndKey(CurrentTableId, CurrentKey);

	TSharedPtr<FString> SelectedKeyComboEntry;
	KeyComboOptions.Reset();

	if (!CurrentTableId.IsNone())
	{
		FStringTableConstPtr StringTable = FStringTableRegistry::Get().FindStringTable(CurrentTableId);
		if (StringTable.IsValid())
		{
			StringTable->EnumerateSourceStrings([&](const FString& InKey, const FString& InSourceString) -> bool
			{
				TSharedRef<FString> KeyComboEntry = MakeShared<FString>(InKey);
				if (InKey.Equals(CurrentKey, ESearchCase::CaseSensitive))
				{
					SelectedKeyComboEntry = KeyComboEntry;
				}
				if (KeyTextFilter->PassesFilter(KeyComboEntry))
				{
					KeyComboOptions.Add(KeyComboEntry);
				}
				return true; // continue enumeration
			});
		}
	}

	KeyComboOptions.Sort([](const TSharedPtr<FString>& InOne, const TSharedPtr<FString>& InTwo)
	{
		return *InOne < *InTwo;
	});

	StringTableKeysList->RebuildList();

	if (SelectedKeyComboEntry.IsValid())
	{
		StringTableKeysList->SetItemSelection(SelectedKeyComboEntry, true);
	}
	else
	{
		StringTableKeysList->ClearSelection();
	}
}

FText STextPropertyEditableStringTableReference::GetKeyComboContent() const
{
	FString CurrentKey;
	{
		FName TmpTableId;
		GetTableIdAndKey(TmpTableId, CurrentKey);
	}

	if (CurrentKey.IsEmpty())
	{
		return LOCTEXT("NoKeyLabel", "No Key");
	}

	return FText::FromString(MoveTemp(CurrentKey));
}

FText STextPropertyEditableStringTableReference::GetKeyComboToolTip() const
{
	return GetKeyComboContent();
}

bool STextPropertyEditableStringTableReference::IsUnlinkEnabled() const
{
	bool bEnabled = false;

	const int32 NumTexts = EditableTextProperty->GetNumTexts();
	for (int32 TextIndex = 0; TextIndex < NumTexts; ++TextIndex)
	{
		const FText CurrentText = EditableTextProperty->GetText(TextIndex);
		if (CurrentText.IsFromStringTable())
		{
			bEnabled = true;
			break;
		}
	}

	return bEnabled;
}

FReply STextPropertyEditableStringTableReference::OnUnlinkClicked()
{
	const int32 NumTexts = EditableTextProperty->GetNumTexts();
	for (int32 TextIndex = 0; TextIndex < NumTexts; ++TextIndex)
	{
		const FText CurrentText = EditableTextProperty->GetText(TextIndex);
		if (CurrentText.IsFromStringTable())
		{
			// Make a copy of the FText separate from the string table but generate a new stable namespace and key
			// This prevents problems with properties that disallow empty text (e.g. enum display name)
			FString NewNamespace;
			FString NewKey;
			EditableTextProperty->GetStableTextId(
				TextIndex,
				IEditableTextProperty::ETextPropertyEditAction::EditedKey,
				CurrentText.ToString(),
				FString(),
				FString(),
				NewNamespace,
				NewKey
			);
			
			EditableTextProperty->SetText(TextIndex, FText::ChangeKey(NewNamespace, NewKey, CurrentText));
		}
	}

	return FReply::Handled();
}

/** Single row in the advanced text settings/localization menu. Has a similar appearance to a details row in the property editor. */
class STextPropertyEditableOptionRow : public SCompoundWidget
{
	SLATE_BEGIN_ARGS(STextPropertyEditableOptionRow)
		: _IsHeader(false)
		, _ContentHAlign(HAlign_Fill)
		{}
		SLATE_ARGUMENT(bool, IsHeader)
		SLATE_ARGUMENT(EHorizontalAlignment, ContentHAlign)
		SLATE_ATTRIBUTE(FText, Text)
		SLATE_DEFAULT_SLOT(FArguments, Content)
	SLATE_END_ARGS()

public:
	void Construct(const FArguments& InArgs, TSharedRef<FLinkedBoxManager> InManager)
	{
		InArgs._Content.Widget->SetToolTip(GetToolTip());

		if (InArgs._IsHeader)
		{
			// Header row, text only, fills entire row
			ChildSlot
			[
				SNew(SBorder)
				.BorderImage(FAppStyle::Get().GetBrush("DetailsView.GridLine"))
				.Padding(FMargin(0, 0, 0, 1))
				[
					SNew(SBorder)
					.BorderImage(FAppStyle::Get().GetBrush("DetailsView.CategoryTop"))
					.BorderBackgroundColor(FSlateColor(FLinearColor::White))
					.Padding(FMargin(12, 8, 0, 8))
					.VAlign(VAlign_Center)
					[
						SNew(STextBlock)
						.TextStyle(FAppStyle::Get(), "DetailsView.CategoryTextStyle")
						.Font(FAppStyle::Get().GetFontStyle("PropertyWindow.BoldFont"))
						.Text(InArgs._Text)
						.ToolTip(GetToolTip())
					]
				]
			];
		}
		else
		{
			// Non-header row, has a name column followed by a value widget
			ChildSlot
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.AutoWidth()
				[
					SNew(SLinkedBox, InManager)
					[
						SNew(SBorder)
						.BorderImage(FAppStyle::Get().GetBrush("DetailsView.GridLine"))
						.Padding(FMargin(0, 0, 0, 1))
						[
							SNew(SBorder)
							.BorderImage(FAppStyle::Get().GetBrush("DetailsView.CategoryMiddle"))
							.BorderBackgroundColor(this, &STextPropertyEditableOptionRow::GetBackgroundColor)
							.Padding(FMargin(20, 3.5, 0, 3.5))
							.HAlign(HAlign_Left)
							.VAlign(VAlign_Center)
							[
								SNew(STextBlock)
								.Font(FAppStyle::Get().GetFontStyle("PropertyWindow.NormalFont"))
								.Text(InArgs._Text)
								.ToolTip(GetToolTip())
							]
						]
					]
				]
				+ SHorizontalBox::Slot()
				[
					SNew(SBorder)
					.BorderImage(FAppStyle::Get().GetBrush("DetailsView.GridLine"))
					.Padding(FMargin(0, 0, 0, 1))
					[
						SNew(SBorder)
						.BorderImage(FAppStyle::Get().GetBrush("DetailsView.CategoryMiddle"))
						.BorderBackgroundColor(this, &STextPropertyEditableOptionRow::GetBackgroundColor)
						.Padding(FMargin(14, 3.5, 4, 3.5))
						.HAlign(InArgs._ContentHAlign)
						.VAlign(VAlign_Center)
						[
							InArgs._Content.Widget
						]
					]
				]
			];

			// Clear the tooltip from this widget since it's set on the name/value widgets now
			SetToolTip(nullptr);
		}
	}

private:
	FSlateColor GetBackgroundColor() const
	{
		if (IsHovered())
		{
			return FStyleColors::Header;
		}

		return FStyleColors::Panel;
	}
};

void STextPropertyEditableTextBox::Construct(const FArguments& InArgs, const TSharedRef<IEditableTextProperty>& InEditableTextProperty)
{
	EditableTextProperty = InEditableTextProperty;

	TSharedPtr<SHorizontalBox> HorizontalBox;

	const bool bIsPassword = EditableTextProperty->IsPassword();
	bIsMultiLine = EditableTextProperty->IsMultiLineText();
	if (bIsMultiLine)
	{
		ChildSlot
		[
			SAssignNew(HorizontalBox, SHorizontalBox)
			+SHorizontalBox::Slot()
			.VAlign(VAlign_Center)
			.FillWidth(1.0f)
			[
				SNew(SBox)
				.MinDesiredWidth(InArgs._MinDesiredWidth)
				.MaxDesiredHeight(InArgs._MaxDesiredHeight)
				[
					SAssignNew(MultiLineWidget, SMultiLineEditableTextBox)
					.Text(this, &STextPropertyEditableTextBox::GetTextValue)
					.ToolTipText(this, &STextPropertyEditableTextBox::GetToolTipText)
					.Style(InArgs._Style)
					.Font(InArgs._Font)
					.ForegroundColor(InArgs._ForegroundColor)
					.SelectAllTextWhenFocused(false)
					.ClearKeyboardFocusOnCommit(false)
					.OnTextChanged(this, &STextPropertyEditableTextBox::OnTextChanged)
					.OnTextCommitted(this, &STextPropertyEditableTextBox::OnTextCommitted)
					.SelectAllTextOnCommit(false)
					.IsReadOnly(this, &STextPropertyEditableTextBox::IsSourceTextReadOnly)
					.AutoWrapText(InArgs._AutoWrapText)
					.WrapTextAt(InArgs._WrapTextAt)
					.ModiferKeyForNewLine(EModifierKey::Shift)
					//.IsPassword(bIsPassword)
				]
			]
		];

		PrimaryWidget = MultiLineWidget;
	}
	else
	{
		ChildSlot
		[
			SAssignNew(HorizontalBox, SHorizontalBox)
			+SHorizontalBox::Slot()
			.VAlign(VAlign_Center)
			[
				SNew(SBox)
				.MinDesiredWidth(InArgs._MinDesiredWidth)
				[
					SAssignNew(SingleLineWidget, SEditableTextBox)
					.Text(this, &STextPropertyEditableTextBox::GetTextValue)
					.ToolTipText(this, &STextPropertyEditableTextBox::GetToolTipText)
					.Style(InArgs._Style)
					.Font(InArgs._Font)
					.ForegroundColor(InArgs._ForegroundColor)
					.SelectAllTextWhenFocused(true)
					.ClearKeyboardFocusOnCommit(false)
					.OnTextChanged(this, &STextPropertyEditableTextBox::OnTextChanged)
					.OnTextCommitted(this, &STextPropertyEditableTextBox::OnTextCommitted)
					.SelectAllTextOnCommit(true)
					.IsReadOnly(this, &STextPropertyEditableTextBox::IsSourceTextReadOnly)
					.IsPassword(bIsPassword)
				]
			]
		];

		PrimaryWidget = SingleLineWidget;
	}

	const TSharedRef<FLinkedBoxManager> LinkedBoxManager = MakeShared<FLinkedBoxManager>();
	const FSlateFontInfo PropertyNormalFont = FAppStyle::Get().GetFontStyle("PropertyWindow.NormalFont");

	HorizontalBox->AddSlot()
		.AutoWidth()
		[
			SNew(SSimpleComboButton)
			.Icon(this, &STextPropertyEditableTextBox::GetAdvancedTextSettingsComboImage)
			.MenuContent()
			[
				SNew(SBox)
				.WidthOverride(340)
				.Padding(1)
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot()
					[
						SNew(STextPropertyEditableOptionRow, LinkedBoxManager)
						.Text(LOCTEXT("TextLocalizableLabel", "Localize"))
						.ToolTipText(LOCTEXT("TextLocalizableCheckBoxToolTip", "Whether to assign this text a key and allow it to be gathered for localization.\nIf set to false, marks this text as 'culture invariant' to prevent it being gathered for localization."))
						.ContentHAlign(HAlign_Left)
						[
							SNew(SCheckBox)
							.IsEnabled(this, &STextPropertyEditableTextBox::IsCultureInvariantFlagEnabled)
							.IsChecked(this, &STextPropertyEditableTextBox::GetLocalizableCheckState)
							.OnCheckStateChanged(this, &STextPropertyEditableTextBox::HandleLocalizableCheckStateChanged)
						]
					]
					+ SVerticalBox::Slot()
					[
						SNew(STextPropertyEditableOptionRow, LinkedBoxManager)
						.IsHeader(true)
						.Text(LOCTEXT("TextReferencedTextLabel", "Referenced Text"))
					]
					+ SVerticalBox::Slot()
					[
						SNew(STextPropertyEditableOptionRow, LinkedBoxManager)
						.Text(LOCTEXT("TextStringTableLabel", "String Table"))
						.IsEnabled(this, &STextPropertyEditableTextBox::IsTextLocalizable)
						[
							SNew(STextPropertyEditableStringTableReference, InEditableTextProperty)
							.AllowUnlink(true)
							.Font(PropertyNormalFont)
							.IsEnabled(this, &STextPropertyEditableTextBox::CanEdit)
						]
					]
					+ SVerticalBox::Slot()
					[
						SNew(STextPropertyEditableOptionRow, LinkedBoxManager)
						.IsHeader(true)
						.Text(LOCTEXT("TextInlineTextLabel", "Inline Text"))
					]

#if USE_STABLE_LOCALIZATION_KEYS
					+ SVerticalBox::Slot()
					[
						SNew(STextPropertyEditableOptionRow, LinkedBoxManager)
						.Text(LOCTEXT("TextPackageLabel", "Package"))
						.IsEnabled(this, &STextPropertyEditableTextBox::IsTextLocalizable)
						[
							SNew(SEditableTextBox)
							.Text(this, &STextPropertyEditableTextBox::GetPackageValue)
							.Font(PropertyNormalFont)
							.IsReadOnly(true)
						]
					]
#endif // USE_STABLE_LOCALIZATION_KEYS

					+ SVerticalBox::Slot()
					[
						SNew(STextPropertyEditableOptionRow, LinkedBoxManager)
						.Text(LOCTEXT("TextNamespaceLabel", "Namespace"))
						.IsEnabled(this, &STextPropertyEditableTextBox::IsTextLocalizable)
						[
							SAssignNew(NamespaceEditableTextBox, SEditableTextBox)
							.Text(this, &STextPropertyEditableTextBox::GetNamespaceValue)
							.Font(PropertyNormalFont)
							.SelectAllTextWhenFocused(true)
							.ClearKeyboardFocusOnCommit(false)
							.OnTextChanged(this, &STextPropertyEditableTextBox::OnNamespaceChanged)
							.OnTextCommitted(this, &STextPropertyEditableTextBox::OnNamespaceCommitted)
							.SelectAllTextOnCommit(true)
							.IsReadOnly(this, &STextPropertyEditableTextBox::IsIdentityReadOnly)
						]
					]
					+ SVerticalBox::Slot()
					[
						SNew(STextPropertyEditableOptionRow, LinkedBoxManager)
						.Text(LOCTEXT("TextKeyLabel", "Key"))
						.IsEnabled(this, &STextPropertyEditableTextBox::IsTextLocalizable)
						[
							SAssignNew(KeyEditableTextBox, SEditableTextBox)
							.Text(this, &STextPropertyEditableTextBox::GetKeyValue)
							.Font(PropertyNormalFont)
#if USE_STABLE_LOCALIZATION_KEYS
							.SelectAllTextWhenFocused(true)
							.ClearKeyboardFocusOnCommit(false)
							.OnTextChanged(this, &STextPropertyEditableTextBox::OnKeyChanged)
							.OnTextCommitted(this, &STextPropertyEditableTextBox::OnKeyCommitted)
							.SelectAllTextOnCommit(true)
							.IsReadOnly(this, &STextPropertyEditableTextBox::IsIdentityReadOnly)
#else	// USE_STABLE_LOCALIZATION_KEYS
							.IsReadOnly(true)
#endif	// USE_STABLE_LOCALIZATION_KEYS
						]
					]
				]
			]
		];

	SetEnabled(TAttribute<bool>(this, &STextPropertyEditableTextBox::CanEdit));
}

bool STextPropertyEditableTextBox::IsTextLocalizable() const
{
	const int32 NumTexts = EditableTextProperty->GetNumTexts();
	if (NumTexts == 1)
	{
		const FText PropertyValue = EditableTextProperty->GetText(0);
		return !PropertyValue.IsCultureInvariant();
	}
	return true;
}

void STextPropertyEditableTextBox::GetDesiredWidth(float& OutMinDesiredWidth, float& OutMaxDesiredWidth)
{
	if (bIsMultiLine)
	{
		OutMinDesiredWidth = 250.0f;
	}
	else
	{
		OutMinDesiredWidth = 125.0f;
	}

	OutMaxDesiredWidth = 600.0f;
}

bool STextPropertyEditableTextBox::SupportsKeyboardFocus() const
{
	return PrimaryWidget.IsValid() && PrimaryWidget->SupportsKeyboardFocus();
}

FReply STextPropertyEditableTextBox::OnFocusReceived(const FGeometry& MyGeometry, const FFocusEvent& InFocusEvent)
{
	// Forward keyboard focus to our editable text widget
	return FReply::Handled().SetUserFocus(PrimaryWidget.ToSharedRef(), InFocusEvent.GetCause());
}

bool STextPropertyEditableTextBox::CanEdit() const
{
	const bool bIsReadOnly = FTextLocalizationManager::Get().IsLocalizationLocked() || EditableTextProperty->IsReadOnly();
	return !bIsReadOnly;
}

bool STextPropertyEditableTextBox::IsCultureInvariantFlagEnabled() const
{
	return !IsSourceTextReadOnly();
}

bool STextPropertyEditableTextBox::IsSourceTextReadOnly() const
{
	if (!CanEdit())
	{
		return true;
	}

	// We can't edit the source string of string table references
	const int32 NumTexts = EditableTextProperty->GetNumTexts();
	if (NumTexts == 1)
	{
		const FText TextValue = EditableTextProperty->GetText(0);
		if (TextValue.IsFromStringTable())
		{
			return true;
		}
	}

	return false;
}

bool STextPropertyEditableTextBox::IsIdentityReadOnly() const
{
	if (!CanEdit())
	{
		return true;
	}

	// We can't edit the identity of texts that don't gather for localization
	const int32 NumTexts = EditableTextProperty->GetNumTexts();
	if (NumTexts == 1)
	{
		const FText TextValue = EditableTextProperty->GetText(0);
		if (!TextValue.ShouldGatherForLocalization())
		{
			return true;
		}
	}

	return false;
}

FText STextPropertyEditableTextBox::GetToolTipText() const
{
	FText LocalizedTextToolTip;
	const int32 NumTexts = EditableTextProperty->GetNumTexts();
	if (NumTexts == 1)
	{
		const FText TextValue = EditableTextProperty->GetText(0);

		if (TextValue.IsFromStringTable())
		{
			FName TableId;
			FString Key;
			FTextInspector::GetTableIdAndKey(TextValue, TableId, Key);

			LocalizedTextToolTip = FText::Format(
				LOCTEXT("StringTableTextToolTipFmt", "--- String Table Reference ---\nTable ID: {0}\nKey: {1}"), 
				FText::FromName(TableId), FText::FromString(Key)
				);
		}
		else
		{
			FTextId TextId;
			const FString* SourceString = FTextInspector::GetSourceString(TextValue);

			if (SourceString && TextValue.ShouldGatherForLocalization())
			{
				TextId = FTextInspector::GetTextId(TextValue);
			}

			if (!TextId.IsEmpty())
			{
				check(SourceString);

				const FString Namespace = TextId.GetNamespace().GetChars();
				const FString Key = TextId.GetKey().GetChars();

				const FString PackageNamespace = TextNamespaceUtil::ExtractPackageNamespace(Namespace);
				const FString TextNamespace = TextNamespaceUtil::StripPackageNamespace(Namespace);

				LocalizedTextToolTip = FText::Format(
					LOCTEXT("LocalizedTextToolTipFmt", "--- Localized Text ---\nPackage: {0}\nNamespace: {1}\nKey: {2}\nSource: {3}"), 
					FText::FromString(PackageNamespace), FText::FromString(TextNamespace), FText::FromString(Key), FText::FromString(*SourceString)
					);
			}
		}
	}
	
	FText BaseToolTipText = EditableTextProperty->GetToolTipText();
	if (FTextLocalizationManager::Get().IsLocalizationLocked())
	{
		const FText LockdownToolTip = FTextLocalizationManager::Get().IsGameLocalizationPreviewEnabled() 
			? LOCTEXT("LockdownToolTip_Preview", "Localization is locked down due to the active game localization preview")
			: LOCTEXT("LockdownToolTip_Other", "Localization is locked down");
		BaseToolTipText = BaseToolTipText.IsEmptyOrWhitespace() ? LockdownToolTip : FText::Format(LOCTEXT("ToolTipLockdownFmt", "!!! {0} !!!\n\n{1}"), LockdownToolTip, BaseToolTipText);
	}

	if (LocalizedTextToolTip.IsEmptyOrWhitespace())
	{
		return BaseToolTipText;
	}
	if (BaseToolTipText.IsEmptyOrWhitespace())
	{
		return LocalizedTextToolTip;
	}

	return FText::Format(LOCTEXT("ToolTipCompleteFmt", "{0}\n\n{1}"), BaseToolTipText, LocalizedTextToolTip);
}

FText STextPropertyEditableTextBox::GetTextValue() const
{
	FText TextValue;

	const int32 NumTexts = EditableTextProperty->GetNumTexts();
	if (NumTexts == 1)
	{
		TextValue = EditableTextProperty->GetText(0);
	}
	else if (NumTexts > 1)
	{
		TextValue = MultipleValuesText;
	}

	return TextValue;
}

void STextPropertyEditableTextBox::OnTextChanged(const FText& NewText)
{
	const int32 NumTexts = EditableTextProperty->GetNumTexts();

	FText TextErrorMsg;

	// Don't validate the Multiple Values text if there are multiple properties being set
	if (NumTexts > 0 && (NumTexts == 1 || NewText.ToString().Equals(MultipleValuesText.ToString(), ESearchCase::CaseSensitive)))
	{
		EditableTextProperty->IsValidText(NewText, TextErrorMsg);
	}

	// Update or clear the error message
	SetTextError(TextErrorMsg);
}

void STextPropertyEditableTextBox::OnTextCommitted(const FText& NewText, ETextCommit::Type CommitInfo)
{
	const int32 NumTexts = EditableTextProperty->GetNumTexts();

	// Don't commit the Multiple Values text if there are multiple properties being set
	if (NumTexts > 0 && (NumTexts == 1 || !NewText.ToString().Equals(MultipleValuesText.ToString(), ESearchCase::CaseSensitive)))
	{
		FText TextErrorMsg;
		if (EditableTextProperty->IsValidText(NewText, TextErrorMsg))
		{
			// Valid text; clear any error
			SetTextError(FText::GetEmpty());
		}
		else
		{
			// Invalid text; set the error and prevent the new text from being set
			SetTextError(TextErrorMsg);
			return;
		}

		const FString& SourceString = NewText.ToString();
		for (int32 TextIndex = 0; TextIndex < NumTexts; ++TextIndex)
		{
			const FText PropertyValue = EditableTextProperty->GetText(TextIndex);

			// Only apply the change if the new text is different
			if (PropertyValue.ToString().Equals(NewText.ToString(), ESearchCase::CaseSensitive))
			{
				continue;
			}

			// Is the new text is empty, just use the empty instance
			if (NewText.IsEmpty())
			{
				EditableTextProperty->SetText(TextIndex, FText::GetEmpty());
				continue;
			}

			// Maintain culture invariance when editing the text
			if (PropertyValue.IsCultureInvariant())
			{
				EditableTextProperty->SetText(TextIndex, FText::AsCultureInvariant(NewText.ToString()));
				continue;
			}

			FString NewNamespace;
			FString NewKey;
#if USE_STABLE_LOCALIZATION_KEYS
			{
				// Get the stable namespace and key that we should use for this property
				const FString* TextSource = FTextInspector::GetSourceString(PropertyValue);
				EditableTextProperty->GetStableTextId(
					TextIndex, 
					IEditableTextProperty::ETextPropertyEditAction::EditedSource, 
					TextSource ? *TextSource : FString(), 
					FTextInspector::GetNamespace(PropertyValue).Get(FString()), 
					FTextInspector::GetKey(PropertyValue).Get(FString()), 
					NewNamespace, 
					NewKey
					);
			}
#else	// USE_STABLE_LOCALIZATION_KEYS
			{
				// We want to preserve the namespace set on this property if it's *not* the default value
				if (!EditableTextProperty->IsDefaultValue())
				{
					// Some properties report that they're not the default, but still haven't been set from a property, so we also check the property key to see if it's a valid GUID before allowing the namespace to persist
					FGuid TmpGuid;
					if (FGuid::Parse(FTextInspector::GetKey(PropertyValue).Get(FString()), TmpGuid))
					{
						NewNamespace = FTextInspector::GetNamespace(PropertyValue).Get(FString());
					}
				}

				NewKey = FGuid::NewGuid().ToString();
			}
#endif	// USE_STABLE_LOCALIZATION_KEYS

			EditableTextProperty->SetText(TextIndex, FText::ChangeKey(NewNamespace, NewKey, NewText));
		}
	}
}

void STextPropertyEditableTextBox::SetTextError(const FText& InErrorMsg)
{
	if (MultiLineWidget.IsValid())
	{
		MultiLineWidget->SetError(InErrorMsg);
	}

	if (SingleLineWidget.IsValid())
	{
		SingleLineWidget->SetError(InErrorMsg);
	}
}

FText STextPropertyEditableTextBox::GetNamespaceValue() const
{
	FText NamespaceValue;

	const int32 NumTexts = EditableTextProperty->GetNumTexts();
	if (NumTexts == 1)
	{
		const FText PropertyValue = EditableTextProperty->GetText(0);
		TOptional<FString> FoundNamespace = FTextInspector::GetNamespace(PropertyValue);
		if (FoundNamespace.IsSet())
		{
			NamespaceValue = FText::FromString(TextNamespaceUtil::StripPackageNamespace(FoundNamespace.GetValue()));
		}
	}
	else if (NumTexts > 1)
	{
		NamespaceValue = MultipleValuesText;
	}

	return NamespaceValue;
}

void STextPropertyEditableTextBox::OnNamespaceChanged(const FText& NewText)
{
	FText ErrorMessage;
	const FText ErrorCtx = LOCTEXT("TextNamespaceErrorCtx", "Namespace");
	IsValidIdentity(NewText, &ErrorMessage, &ErrorCtx);

	NamespaceEditableTextBox->SetError(ErrorMessage);
}

void STextPropertyEditableTextBox::OnNamespaceCommitted(const FText& NewText, ETextCommit::Type CommitInfo)
{
	if (!IsValidIdentity(NewText))
	{
		return;
	}

	const int32 NumTexts = EditableTextProperty->GetNumTexts();

	// Don't commit the Multiple Values text if there are multiple properties being set
	if (NumTexts > 0 && (NumTexts == 1 || NewText.ToString() != MultipleValuesText.ToString()))
	{
		const FString& TextNamespace = NewText.ToString();
		for (int32 TextIndex = 0; TextIndex < NumTexts; ++TextIndex)
		{
			const FText PropertyValue = EditableTextProperty->GetText(TextIndex);

			// Only apply the change if the new namespace is different - we want to keep the keys stable where possible
			const FString CurrentTextNamespace = TextNamespaceUtil::StripPackageNamespace(FTextInspector::GetNamespace(PropertyValue).Get(FString()));
			if (CurrentTextNamespace.Equals(TextNamespace, ESearchCase::CaseSensitive))
			{
				continue;
			}

			// Get the stable namespace and key that we should use for this property
			FString NewNamespace;
			FString NewKey;
#if USE_STABLE_LOCALIZATION_KEYS
			{
				const FString* TextSource = FTextInspector::GetSourceString(PropertyValue);
				EditableTextProperty->GetStableTextId(
					TextIndex, 
					IEditableTextProperty::ETextPropertyEditAction::EditedNamespace, 
					TextSource ? *TextSource : FString(), 
					TextNamespace, 
					FTextInspector::GetKey(PropertyValue).Get(FString()), 
					NewNamespace, 
					NewKey
					);
			}
#else	// USE_STABLE_LOCALIZATION_KEYS
			{
				NewNamespace = TextNamespace;

				// If the current key is a GUID, then we can preserve that when setting the new namespace
				NewKey = FTextInspector::GetKey(PropertyValue).Get(FString());
				{
					FGuid TmpGuid;
					if (!FGuid::Parse(NewKey, TmpGuid))
					{
						NewKey = FGuid::NewGuid().ToString();
					}
				}
			}
#endif	// USE_STABLE_LOCALIZATION_KEYS

			EditableTextProperty->SetText(TextIndex, FText::ChangeKey(NewNamespace, NewKey, PropertyValue));
		}
	}
}

FText STextPropertyEditableTextBox::GetKeyValue() const
{
	FText KeyValue;

	const int32 NumTexts = EditableTextProperty->GetNumTexts();
	if (NumTexts == 1)
	{
		const FText PropertyValue = EditableTextProperty->GetText(0);
		TOptional<FString> FoundKey = FTextInspector::GetKey(PropertyValue);
		if (FoundKey.IsSet())
		{
			KeyValue = FText::FromString(FoundKey.GetValue());
		}
	}
	else if (NumTexts > 1)
	{
		KeyValue = MultipleValuesText;
	}

	return KeyValue;
}

#if USE_STABLE_LOCALIZATION_KEYS

void STextPropertyEditableTextBox::OnKeyChanged(const FText& NewText)
{
	FText ErrorMessage;
	const FText ErrorCtx = LOCTEXT("TextKeyErrorCtx", "Key");
	const bool bIsValidName = IsValidIdentity(NewText, &ErrorMessage, &ErrorCtx);

	if (NewText.IsEmptyOrWhitespace())
	{
		ErrorMessage = LOCTEXT("TextKeyEmptyErrorMsg", "Key cannot be empty so a new key will be assigned");
	}
	else if (bIsValidName)
	{
		// Valid name, so check it won't cause an identity conflict (only test if we have a single text selected to avoid confusion)
		const int32 NumTexts = EditableTextProperty->GetNumTexts();
		if (NumTexts == 1)
		{
			const FText PropertyValue = EditableTextProperty->GetText(0);

			const FString TextNamespace = FTextInspector::GetNamespace(PropertyValue).Get(FString());
			const FString TextKey = NewText.ToString();

			// Get the stable namespace and key that we should use for this property
			// If it comes back with the same namespace but a different key then it means there was an identity conflict
			FString NewNamespace;
			FString NewKey;
			const FString* TextSource = FTextInspector::GetSourceString(PropertyValue);
			EditableTextProperty->GetStableTextId(
				0,
				IEditableTextProperty::ETextPropertyEditAction::EditedKey,
				TextSource ? *TextSource : FString(),
				TextNamespace,
				TextKey,
				NewNamespace,
				NewKey
				);

			if (TextNamespace.Equals(NewNamespace, ESearchCase::CaseSensitive) && !TextKey.Equals(NewKey, ESearchCase::CaseSensitive))
			{
				ErrorMessage = LOCTEXT("TextKeyConflictErrorMsg", "Identity (namespace & key) is being used by a different text within this package so a new key will be assigned");
			}
		}
	}

	KeyEditableTextBox->SetError(ErrorMessage);
}

void STextPropertyEditableTextBox::OnKeyCommitted(const FText& NewText, ETextCommit::Type CommitInfo)
{
	if (!IsValidIdentity(NewText))
	{
		return;
	}

	const int32 NumTexts = EditableTextProperty->GetNumTexts();

	// Don't commit the Multiple Values text if there are multiple properties being set
	if (NumTexts > 0 && (NumTexts == 1 || NewText.ToString() != MultipleValuesText.ToString()))
	{
		const FString& TextKey = NewText.ToString();
		for (int32 TextIndex = 0; TextIndex < NumTexts; ++TextIndex)
		{
			const FText PropertyValue = EditableTextProperty->GetText(TextIndex);

			// Only apply the change if the new key is different - we want to keep the keys stable where possible
			const FString CurrentTextKey = FTextInspector::GetKey(PropertyValue).Get(FString());
			if (CurrentTextKey.Equals(TextKey, ESearchCase::CaseSensitive))
			{
				continue;
			}

			// Get the stable namespace and key that we should use for this property
			FString NewNamespace;
			FString NewKey;
			const FString* TextSource = FTextInspector::GetSourceString(PropertyValue);
			EditableTextProperty->GetStableTextId(
				TextIndex, 
				IEditableTextProperty::ETextPropertyEditAction::EditedKey, 
				TextSource ? *TextSource : FString(), 
				FTextInspector::GetNamespace(PropertyValue).Get(FString()), 
				TextKey, 
				NewNamespace, 
				NewKey
				);

			EditableTextProperty->SetText(TextIndex, FText::ChangeKey(NewNamespace, NewKey, PropertyValue));
		}
	}
}

FText STextPropertyEditableTextBox::GetPackageValue() const
{
	FText PackageValue;

	const int32 NumTexts = EditableTextProperty->GetNumTexts();
	if (NumTexts == 1)
	{
		const FText PropertyValue = EditableTextProperty->GetText(0);
		TOptional<FString> FoundNamespace = FTextInspector::GetNamespace(PropertyValue);
		if (FoundNamespace.IsSet())
		{
			PackageValue = FText::FromString(TextNamespaceUtil::ExtractPackageNamespace(FoundNamespace.GetValue()));
		}
	}
	else if (NumTexts > 1)
	{
		PackageValue = MultipleValuesText;
	}

	return PackageValue;
}

#endif // USE_STABLE_LOCALIZATION_KEYS

ECheckBoxState STextPropertyEditableTextBox::GetLocalizableCheckState() const
{
	const int32 NumTexts = EditableTextProperty->GetNumTexts();
	if (NumTexts == 1)
	{
		const FText PropertyValue = EditableTextProperty->GetText(0);

		const bool bIsLocalized = !PropertyValue.IsCultureInvariant();
		return bIsLocalized ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
	}

	return ECheckBoxState::Unchecked;
}

void STextPropertyEditableTextBox::HandleLocalizableCheckStateChanged(ECheckBoxState InCheckboxState)
{
	const int32 NumTexts = EditableTextProperty->GetNumTexts();

	if (InCheckboxState == ECheckBoxState::Checked)
	{
		for (int32 TextIndex = 0; TextIndex < NumTexts; ++TextIndex)
		{
			const FText PropertyValue = EditableTextProperty->GetText(TextIndex);

			// Assign a key to any currently culture invariant texts
			if (PropertyValue.IsCultureInvariant())
			{
				// Get the stable namespace and key that we should use for this property
				FString NewNamespace;
				FString NewKey;
				EditableTextProperty->GetStableTextId(
					TextIndex,
					IEditableTextProperty::ETextPropertyEditAction::EditedKey,
					PropertyValue.ToString(),
					FString(),
					FString(),
					NewNamespace,
					NewKey
					);

				EditableTextProperty->SetText(TextIndex, FInternationalization::Get().ForUseOnlyByLocMacroAndGraphNodeTextLiterals_CreateText(*PropertyValue.ToString(), *NewNamespace, *NewKey));
			}
		}
	}
	else
	{
		for (int32 TextIndex = 0; TextIndex < NumTexts; ++TextIndex)
		{
			const FText PropertyValue = EditableTextProperty->GetText(TextIndex);

			// Clear the identity from any non-culture invariant texts
			if (!PropertyValue.IsCultureInvariant())
			{
				const FString* TextSource = FTextInspector::GetSourceString(PropertyValue);
				EditableTextProperty->SetText(TextIndex, FText::AsCultureInvariant(PropertyValue.ToString()));
			}
		}
	}
}

FText STextPropertyEditableTextBox::GetAdvancedTextSettingsComboToolTip() const
{
	if (IsTextLocalizable())
	{
		return LOCTEXT("AdvancedTextSettingsComboToolTip", "Edit advanced text settings.");
	}
	else
	{
		return LOCTEXT("TextNotLocalizedWarningToolTip", "This text is marked as 'culture invariant' and won't be gathered for localization.\nYou can change this by editing the advanced text settings.");
	}
}

const FSlateBrush* STextPropertyEditableTextBox::GetAdvancedTextSettingsComboImage() const
{
	if (IsTextLocalizable())
	{
		return FAppStyle::Get().GetBrush("LocalizationDashboard.MenuIcon");
	}
	else
	{
		return FCoreStyle::Get().GetBrush("Icons.Warning");
	}
}

bool STextPropertyEditableTextBox::IsValidIdentity(const FText& InIdentity, FText* OutReason, const FText* InErrorCtx) const
{
	const FString InvalidIdentityChars = FString::Printf(TEXT("%s%c%c"), INVALID_NAME_CHARACTERS, TextNamespaceUtil::PackageNamespaceStartMarker, TextNamespaceUtil::PackageNamespaceEndMarker);
	return FName::IsValidXName(InIdentity.ToString(), InvalidIdentityChars, OutReason, InErrorCtx);
}

#undef LOCTEXT_NAMESPACE
