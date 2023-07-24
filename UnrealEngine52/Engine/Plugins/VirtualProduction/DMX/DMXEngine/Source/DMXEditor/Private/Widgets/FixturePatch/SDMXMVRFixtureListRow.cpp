// Copyright Epic Games, Inc. All Rights Reserved.

#include "SDMXMVRFixtureListRow.h"

#include "DMXEditor.h"
#include "DMXEditorStyle.h"
#include "Library/DMXEntityFixturePatch.h"
#include "Library/DMXEntityFixtureType.h"
#include "Library/DMXLibrary.h"
#include "MVR/DMXMVRGeneralSceneDescription.h"
#include "Widgets/FixturePatch/SDMXMVRFixtureList.h"
#include "Widgets/FixturePatch/DMXMVRFixtureListItem.h"

#include "SSearchableComboBox.h"
#include "Engine/EngineTypes.h"
#include "Widgets/Text/SInlineEditableTextBlock.h"
#include "Widgets/Input/STextComboBox.h"
#include "Styling/AppStyle.h"

#define LOCTEXT_NAMESPACE "SDMXMVRFixtureListRow"

/////////////////////////////////////////////////////
// SDMXMVRFixtureFixtureTypePicker

/** Widget to pick a fixture type for an MVR Fixture */

class SDMXMVRFixtureFixtureTypePicker
	: public SCompoundWidget
{
	DECLARE_DELEGATE_OneParam(FDMXMVRFixtureListRowOnFixtureTypeSelectedDelegate, UDMXEntityFixtureType* /** Selected Fixture Type */);

public:
	SLATE_BEGIN_ARGS(SDMXMVRFixtureFixtureTypePicker)
	{}
		/** Called when the combo box selection changed */
		SLATE_EVENT(FDMXMVRFixtureListRowOnFixtureTypeSelectedDelegate, OnFixtureTypeSelected)

	SLATE_END_ARGS()

	/** Constructs this widget */
	void Construct(const FArguments& InArgs, UDMXLibrary* InDMXLibrary)
	{
		if (!InDMXLibrary)
		{
			return;
		}

		WeakDMXLibrary = InDMXLibrary;
		OnFixtureTypeSelectedDelegate = InArgs._OnFixtureTypeSelected;


		InDMXLibrary->GetOnEntitiesAdded().AddSP(this, &SDMXMVRFixtureFixtureTypePicker::OnEntitiesAddedOrRemoved);
		InDMXLibrary->GetOnEntitiesRemoved().AddSP(this, &SDMXMVRFixtureFixtureTypePicker::OnEntitiesAddedOrRemoved);
		UDMXEntityFixtureType::GetOnFixtureTypeChanged().AddSP(this, &SDMXMVRFixtureFixtureTypePicker::OnFixtureTypeChanged);

		ChildSlot
			[
				SAssignNew(ComboBox, SSearchableComboBox)
				.OptionsSource(&FixtureTypeNames)
				.OnGenerateWidget(this, &SDMXMVRFixtureFixtureTypePicker::OnGenerateWidget)
				.OnSelectionChanged(this, &SDMXMVRFixtureFixtureTypePicker::OnSelectionChanged)
				.Content()						
				[
					SNew(STextBlock)
					.Text(this, &SDMXMVRFixtureFixtureTypePicker::GetSelectedItemText)
				]
			];

		RefreshInternal();
	}

	/** Sets the currently selected Fixture Type */
	void SetSelection(UDMXEntityFixtureType* FixtureType)
	{
		if (FixtureType)
		{
			TSharedPtr<FString> const* FixtureTypeNameToSelectPtr = FixtureTypeNames.FindByPredicate([FixtureType](const TSharedPtr<FString>& FixtureTypeName)
				{
					return *FixtureTypeName == FixtureType->Name;
				});
			if (ensureAlwaysMsgf(FixtureTypeNameToSelectPtr, TEXT("Trying to select a fixture type but Fixture Type %s is not present."), *FixtureType->Name))
			{
				ComboBox->SetSelectedItem(*FixtureTypeNameToSelectPtr);
			}
		}
		else
		{
			checkf(FixtureTypeNames.Num() > 0, TEXT("Expected at least a 'None' entry in FitureTypesName, but the array is empty."));
			ComboBox->SetSelectedItem(FixtureTypeNames[0]);
		}
	}

	/** Requests to refresh the widget on the next tick */
	void RequestRefresh()
	{
		if (!RefreshTimerHandle.IsValid())
		{
			RefreshTimerHandle = GEditor->GetTimerManager()->SetTimerForNextTick(FTimerDelegate::CreateSP(this, &SDMXMVRFixtureFixtureTypePicker::RefreshInternal));
		}
	}

private:
	/** Called when the combo box generates a widget */
	TSharedRef<SWidget> OnGenerateWidget(TSharedPtr<FString> InItem)
	{
		return
			SNew(STextBlock)
			.Text(FText::FromString(*InItem));
	}

	/** Called when the combo box selection changed */
	void OnSelectionChanged(TSharedPtr<FString> NewSelection, ESelectInfo::Type SelectInfo)
	{
		if (SelectInfo == ESelectInfo::Direct)
		{
			return;
		}

		if (UDMXEntityFixtureType* SelectedFixtureType = GetSelectedFixtureType())
		{
			OnFixtureTypeSelectedDelegate.ExecuteIfBound(SelectedFixtureType);
		}
	}

	UDMXEntityFixtureType* GetSelectedFixtureType() const
	{
		UDMXEntityFixtureType* SelectedFixtureType = nullptr;
		const TSharedPtr<FString> SelectedItem = ComboBox->GetSelectedItem();
		if (TWeakObjectPtr<UDMXEntityFixtureType> const* FixtureTypePtr = FixtureTypeNameToFixtureTypeMap.Find(SelectedItem))
		{
			SelectedFixtureType = FixtureTypePtr->Get();
		}

		return SelectedFixtureType;
	}

	/** Returns the text of the selected item */
	FText GetSelectedItemText() const
	{
		if (ComboBox.IsValid())
		{
			if (const TSharedPtr<FString> SelectedItem = ComboBox->GetSelectedItem())
			{
				return FText::FromString(*SelectedItem);
			}
		}

		return FText::GetEmpty();
	}

	/** Refreshes the Combo Box */
	void RefreshInternal()
	{
		RefreshTimerHandle.Invalidate();

		FixtureTypeNames.Reset();
		const UDMXLibrary* DMXLibrary = WeakDMXLibrary.Get();
		if (!DMXLibrary)
		{
			return;
		}

		const TArray<UDMXEntityFixtureType*> FixtureTypes = DMXLibrary->GetEntitiesTypeCast<UDMXEntityFixtureType>();

		if (FixtureTypes.IsEmpty())
		{
			const TSharedRef<FString> NoneItem = MakeShared<FString>(TEXT("None"));
			FixtureTypeNames.Add(NoneItem);
			ComboBox->SetSelectedItem(NoneItem);
		}
		else
		{
			for (UDMXEntityFixtureType* FixtureType : FixtureTypes)
			{
				const TSharedPtr<FString> FixtureTypeName = MakeShared<FString>(FixtureType->Name);

				FixtureTypeNames.Add(FixtureTypeName);
				FixtureTypeNameToFixtureTypeMap.Add(FixtureTypeName, FixtureType);
			}
		}

		if (!ensureMsgf(!FixtureTypeNames.IsEmpty(), TEXT("No combo box option for Fixture Type.")))
		{
			return;
		}

		const FString SelectedFixtureTypeName = ComboBox->GetSelectedItem() ? *ComboBox->GetSelectedItem() : TEXT("");
		const TSharedPtr<FString>* SelectionPtr = Algo::FindByPredicate(FixtureTypeNames, [SelectedFixtureTypeName](const TSharedPtr<FString>& FixtureTypeName)
			{
				return *FixtureTypeName == SelectedFixtureTypeName;
			});
		if (!SelectionPtr)
		{
			ComboBox->SetSelectedItem(FixtureTypeNames[0], ESelectInfo::Direct);
		}

		ComboBox->RefreshOptions();
	}

	/** Called when the Entities in the DMX Library were added or removed */
	void OnEntitiesAddedOrRemoved(UDMXLibrary* DMXLibrary, TArray<UDMXEntity*> AddedOrRemovedEntities)
	{
		if (DMXLibrary == WeakDMXLibrary)
		{
			RequestRefresh();
		}
	}

	/** Called when a Fixture Type changed */
	void OnFixtureTypeChanged(const UDMXEntityFixtureType* ChangedFixtureType)
	{
		if (ChangedFixtureType && ChangedFixtureType->GetParentLibrary() == WeakDMXLibrary)
		{
			RequestRefresh();
		}
	}

	/** Names of Fixture Types in the Combo Box */
	TArray<TSharedPtr<FString>> FixtureTypeNames;

	/** Map from the Fixture Type Name to the actual Fixture Type object */
	TMap<TSharedPtr<FString>, TWeakObjectPtr<UDMXEntityFixtureType>> FixtureTypeNameToFixtureTypeMap;

	/** The combo box widget in use */
	TSharedPtr<SSearchableComboBox> ComboBox;

	/** The DMX Library from which to select fixture types */
	TWeakObjectPtr<UDMXLibrary> WeakDMXLibrary;

	/** Timer handle used for the RequestRefresh method */
	FTimerHandle RefreshTimerHandle;

	// Slate args
	FDMXMVRFixtureListRowOnFixtureTypeSelectedDelegate OnFixtureTypeSelectedDelegate;
};


/////////////////////////////////////////////////////
// SDMXMVRFixtureModePicker

/** Widget to pick a fixture type for an MVR Fixture */
class SDMXMVRFixtureModePicker
	: public SCompoundWidget
{
	DECLARE_DELEGATE_OneParam(FDMXMVRFixtureListRowOnModeSelectedDelegate, int32 /** Selected Mode Index */);

public:
	SLATE_BEGIN_ARGS(SDMXMVRFixtureModePicker)
	{}
		/** Called when the combo box selection changed */
		SLATE_EVENT(FDMXMVRFixtureListRowOnModeSelectedDelegate, OnModeSelected)

	SLATE_END_ARGS()

	/** Constructs this widget */
	void Construct(const FArguments& InArgs)
	{
		OnModeSelectedDelegate = InArgs._OnModeSelected;

		UDMXEntityFixtureType::GetOnFixtureTypeChanged().AddSP(this, &SDMXMVRFixtureModePicker::OnFixtureTypeChanged);

		ChildSlot
			[
				SNew(SVerticalBox)
				.IsEnabled_Lambda([this]
					{
						return bEnabled;
					})
				
				+ SVerticalBox::Slot()
				[
					SAssignNew(ComboBox, STextComboBox)
					.Visibility_Lambda([this]()
						{
							return WeakFixtureType.IsValid() ? EVisibility::Visible : EVisibility::Hidden;
						})
					.OptionsSource(&ModeNames)
					.OnSelectionChanged(this, &SDMXMVRFixtureModePicker::OnSelectionChanged)
				]

				+ SVerticalBox::Slot()
				[
					SNew(STextBlock)
					.Visibility_Lambda([this]()
						{
							return WeakFixtureType.IsValid() ? EVisibility::Collapsed : EVisibility::Visible;
						})
					.Text(LOCTEXT("NoModeBecauseNoFixtureTypeSelectedInfo", "No Fixture Type Selected"))
					.Font(FAppStyle::GetFontStyle(TEXT("PropertyWindow.NormalFont")))
				]
			];

		RefreshOptions();
	}

	/** Sets the selection */
	void SetSelection(UDMXEntityFixtureType* FixtureType, int32 ModeIndex)
	{
		WeakFixtureType = FixtureType;		
		RefreshOptions();

		if (FixtureType && FixtureType->Modes.IsValidIndex(ModeIndex))
		{
			if (ensureAlwaysMsgf(ModeNames.IsValidIndex(ModeIndex), TEXT("Trying to select a Mode but its index is not valid.")))
			{
				ComboBox->SetSelectedItem(ModeNames[ModeIndex]);
			}
		}
	}

private:
	/** Called when the combo box selection changed */
	void OnSelectionChanged(TSharedPtr<FString> NewSelection, ESelectInfo::Type SelectInfo)
	{
		UDMXEntityFixtureType* FixtureType = WeakFixtureType.Get();
		if (ensureAlwaysMsgf(FixtureType, TEXT("Tried to select Mode, but Fixture Type is no longer valid.")))
		{
			const TSharedPtr<FString> SelectedItem = ComboBox->GetSelectedItem();
			int32 const* ModeIndexPtr = ModeNameToModeIndexMap.Find(SelectedItem);
			if (ensureAlwaysMsgf(ModeIndexPtr, TEXT("Tried to select Mode, but the Mode Index is no longer valid.")))
			{
				OnModeSelectedDelegate.ExecuteIfBound(*ModeIndexPtr);
			}
		}
	}

	/** Returns the text of the selected item */
	FText GetSelectedItemText() const
	{
		const TSharedPtr<FString> SelectedItem = ComboBox->GetSelectedItem();
		return FText::FromString(*SelectedItem);
	}

	/** Refreshes the Combo Box */
	void RefreshOptions()
	{
		// Remember selection
		const FString PreviousSelectedModeName = ComboBox->GetSelectedItem().IsValid() ? *ComboBox->GetSelectedItem() : FString();

		ModeNames.Reset();

		if (UDMXEntityFixtureType* FixtureType = WeakFixtureType.Get())
		{
			for (int32 ModeIndex = 0; ModeIndex < FixtureType->Modes.Num(); ModeIndex++)
			{
				const TSharedPtr<FString> ModeName = MakeShared<FString>(FixtureType->Modes[ModeIndex].ModeName);

				ModeNames.Add(ModeName);
				ModeNameToModeIndexMap.Add(ModeName, ModeIndex);
			}
		}

		if (ModeNames.Num() == 0)
		{
			const FText NoModeAvailableText = LOCTEXT("NoModeAvailableInModeComboBoxText", "No Mode available");
			ModeNames.Add(MakeShared<FString>(NoModeAvailableText.ToString()));

			bEnabled = false;
		}
		else
		{
			bEnabled = true;
		}

		ComboBox->RefreshOptions();

		// Restore selection
		TSharedPtr<FString>* PreviousSelectionPtr = Algo::FindByPredicate(ModeNames, [PreviousSelectedModeName](const TSharedPtr<FString>& ModeName)
			{
				return ModeName.IsValid() && *ModeName == PreviousSelectedModeName;
			});
		if (PreviousSelectionPtr)
		{
			ComboBox->SetSelectedItem(*PreviousSelectionPtr);
		}
	}

	/** Called when the Entities in the DMX Library were added or removed */
	void OnFixtureTypeChanged(const UDMXEntityFixtureType* ChangedFixtureType)
	{
		if (ChangedFixtureType == WeakFixtureType)
		{
			RefreshOptions();
		}
	}

	/** True if the Combo Box should be enabled */
	bool bEnabled = true;

	/** Names of Modes in the Combo Box */
	TArray<TSharedPtr<FString>> ModeNames;

	/** Map from the Fixture Type Name to the actual Mode Index */
	TMap<TSharedPtr<FString>, int32> ModeNameToModeIndexMap;

	/** The combo box widget in use */
	TSharedPtr<STextComboBox> ComboBox;

	/** The Fixture Type for which the Modes are currently displayed */
	TWeakObjectPtr<UDMXEntityFixtureType> WeakFixtureType;

	// Slate args
	FDMXMVRFixtureListRowOnModeSelectedDelegate OnModeSelectedDelegate;
};


/////////////////////////////////////////////////////
// SDMXMVRFixtureListRow

void SDMXMVRFixtureListRow::Construct(const FArguments& InArgs, const TSharedRef<STableViewBase>& InOwnerTable, const TSharedRef<FDMXMVRFixtureListItem>& InItem)
{
	Item = InItem;
	OnRowRequestsStatusRefresh = InArgs._OnRowRequestsStatusRefresh;
	OnRowRequestsListRefresh = InArgs._OnRowRequestsListRefresh;
	IsSelected = InArgs._IsSelected;
	
	SetBorderBackgroundColor(TAttribute<FSlateColor>::CreateLambda([this]()
		{
			if (Item.IsValid())
			{
				return Item->GetBackgroundColor();
			}
			return FLinearColor::Black;
		}));


	SMultiColumnTableRow<TSharedPtr<FDMXMVRFixtureListItem>>::Construct(
		FSuperRowType::FArguments()
		.Style(&FDMXEditorStyle::Get().GetWidgetStyle<FTableRowStyle>("MVRFixtureList.Row")), 
		InOwnerTable);
}

void SDMXMVRFixtureListRow::EnterFixturePatchNameEditingMode()
{
	if (FixturePatchNameTextBlock.IsValid())
	{
		FixturePatchNameTextBlock->EnterEditingMode();
	}
}

TSharedRef<SWidget> SDMXMVRFixtureListRow::GenerateWidgetForColumn(const FName& ColumnName)
{
	if (ColumnName == FDMXMVRFixtureListCollumnIDs::FixturePatchName)
	{
		return GenerateFixturePatchNameRow();
	}
	else if (ColumnName == FDMXMVRFixtureListCollumnIDs::Status)
	{
		return GenerateStatusRow();
	}
	else if (ColumnName == FDMXMVRFixtureListCollumnIDs::FixtureID)
	{
		return GenerateFixtureIDRow();
	}
	else if (ColumnName == FDMXMVRFixtureListCollumnIDs::FixtureType)
	{
		return GenerateFixtureTypeRow();
	}
	else if (ColumnName == FDMXMVRFixtureListCollumnIDs::Mode)
	{
		return GenerateModeRow();
	}
	else if (ColumnName == FDMXMVRFixtureListCollumnIDs::Patch)
	{
		return GeneratePatchRow();
	}

	return SNullWidget::NullWidget;
}

TSharedRef<SWidget> SDMXMVRFixtureListRow::GenerateFixturePatchNameRow()
{
	return
		SAssignNew(FixturePatchNameBorder, SBorder)
		.HAlign(HAlign_Fill)
		.VAlign(VAlign_Center)
		.Padding(4.f)
		.BorderImage(FAppStyle::GetBrush("NoBorder"))
		.OnMouseDoubleClick(this, &SDMXMVRFixtureListRow::OnFixturePatchNameBorderDoubleClicked)
		[
			SNew(SBorder)
			.HAlign(HAlign_Fill)
			.VAlign(VAlign_Center)
			.BorderImage(FDMXEditorStyle::Get().GetBrush("DMXEditor.RoundedPropertyBorder"))
			[
				SAssignNew(FixturePatchNameTextBlock, SInlineEditableTextBlock)
				.Text_Lambda([this]()
				{
					const FString FixturePatchName = Item->GetFixturePatchName();
					return FText::FromString(FixturePatchName);
				})
				.Font(FCoreStyle::GetDefaultFontStyle("Regular", 10))
				.OnTextCommitted(this, &SDMXMVRFixtureListRow::OnFixturePatchNameCommitted)
				.IsSelected(IsSelected)
			]
		];
}

FReply SDMXMVRFixtureListRow::OnFixturePatchNameBorderDoubleClicked(const FGeometry& InMyGeometry, const FPointerEvent& InMouseEvent)
{
	if (InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
	{
		if (FixturePatchNameTextBlock.IsValid())
		{
			FixturePatchNameTextBlock->EnterEditingMode();
		}
	}

	return FReply::Handled();
}

void SDMXMVRFixtureListRow::OnFixturePatchNameCommitted(const FText& InNewText, ETextCommit::Type InTextCommit)
{
	if (InNewText.IsEmpty())
	{
		return;
	}

	FString ResultingName;
	Item->SetFixturePatchName(InNewText.ToString(), ResultingName);
	FixturePatchNameTextBlock->SetText(FText::FromString(ResultingName));
}

TSharedRef<SWidget> SDMXMVRFixtureListRow::GenerateStatusRow()
{
	return
		SNew(SBox)
		.HAlign(HAlign_Center)
		.VAlign(VAlign_Center)
		[
			SNew(SImage)
			.Image_Lambda([this]()
				{
					if (!Item->ErrorStatusText.IsEmpty())
					{
						return FAppStyle::GetBrush("Icons.Error");
					}

					if (!Item->WarningStatusText.IsEmpty())
					{
						return FAppStyle::GetBrush("Icons.Warning");
					}

					static const FSlateBrush EmptyBrush = FSlateNoResource();
					return &EmptyBrush;
				})
			.ToolTipText_Lambda([this]()
				{
					if (!Item->ErrorStatusText.IsEmpty())
					{
						return Item->ErrorStatusText;
					}
					else if (!Item->WarningStatusText.IsEmpty())
					{
						return Item->WarningStatusText;
					}

					return FText::GetEmpty();
				})
		];
}

TSharedRef<SWidget> SDMXMVRFixtureListRow::GenerateFixtureIDRow()
{
	return
		SNew(SBorder)
		.HAlign(HAlign_Fill)
		.VAlign(VAlign_Center)
		.Padding(4.f)
		.BorderImage(FAppStyle::GetBrush("NoBorder"))
		.OnMouseDoubleClick(this, &SDMXMVRFixtureListRow::OnFixtureIDBorderDoubleClicked)
		[
			SNew(SBorder)
			.HAlign(HAlign_Fill)
			.VAlign(VAlign_Center)
			.BorderImage(FDMXEditorStyle::Get().GetBrush("DMXEditor.RoundedPropertyBorder"))
			[
				SAssignNew(FixtureIDTextBlocK, SInlineEditableTextBlock)
				.Text_Lambda([this]()
				{
					return FText::FromString(Item->GetFixtureID());
				})
				.Font(FCoreStyle::GetDefaultFontStyle("Regular", 10))
				.OnTextCommitted(this, &SDMXMVRFixtureListRow::OnFixtureIDCommitted)
				.IsSelected(IsSelected)
			]
		];
}

FReply SDMXMVRFixtureListRow::OnFixtureIDBorderDoubleClicked(const FGeometry& InMyGeometry, const FPointerEvent& InMouseEvent)
{
	if (InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
	{
		if (FixtureIDTextBlocK.IsValid())
		{
			FixtureIDTextBlocK->EnterEditingMode();
		}
	}

	return FReply::Handled();
}

void SDMXMVRFixtureListRow::OnFixtureIDCommitted(const FText& InNewText, ETextCommit::Type InTextCommit)
{
	const FString StringValue = InNewText.ToString();
	int32 NewFixtureID;
	if (LexTryParseString<int32>(NewFixtureID, *StringValue))
	{
		Item->SetFixtureID(NewFixtureID);

		const FString ParsedFixtureIDString = FString::FromInt(NewFixtureID);
		FixtureIDTextBlocK->SetText(FText::FromString(ParsedFixtureIDString));

		OnRowRequestsStatusRefresh.ExecuteIfBound();
	}
}

TSharedRef<SWidget> SDMXMVRFixtureListRow::GenerateFixtureTypeRow()
{
	UDMXLibrary* DMXLibrary = Item->GetDMXLibrary();
	if (!ensureAlwaysMsgf(DMXLibrary, TEXT("Tried to set fixture type for MVR Fixture, but fixture type is invalid.")))
	{
		return SNullWidget::NullWidget;
	}

	const TSharedRef<SDMXMVRFixtureFixtureTypePicker> FixtureTypePicker =
		SNew(SDMXMVRFixtureFixtureTypePicker, DMXLibrary)
		.OnFixtureTypeSelected(this, &SDMXMVRFixtureListRow::OnFixtureTypeSelected);

	UDMXEntityFixtureType* SelectedFixtureType = Item->GetFixtureType();
	FixtureTypePicker->SetSelection(SelectedFixtureType);
		
	return 
		SNew(SBorder)
		.HAlign(HAlign_Fill)
		.VAlign(VAlign_Center)
		.Padding(4.f)
		.BorderImage(FAppStyle::GetBrush("NoBorder"))
		[
			FixtureTypePicker
		];
}

void SDMXMVRFixtureListRow::OnFixtureTypeSelected(UDMXEntityFixtureType* SelectedFixtureType)
{
	Item->SetFixtureType(SelectedFixtureType);
}

TSharedRef<SWidget> SDMXMVRFixtureListRow::GenerateModeRow()
{
	const TSharedRef<SDMXMVRFixtureModePicker> ModePicker =
		SNew(SDMXMVRFixtureModePicker)
		.OnModeSelected(this, &SDMXMVRFixtureListRow::OnModeSelected);

	UDMXEntityFixtureType* SelectedFixtureType = Item->GetFixtureType();
	const int32 SelectedModeIndex = Item->GetModeIndex();

	ModePicker->SetSelection(SelectedFixtureType, SelectedModeIndex);

	return
		SNew(SBorder)
		.HAlign(HAlign_Fill)
		.VAlign(VAlign_Center)
		.Padding(4.f)
		.BorderImage(FAppStyle::GetBrush("NoBorder"))
		[
			ModePicker
		];
}

void SDMXMVRFixtureListRow::OnModeSelected(int32 SelectedModeIndex)
{
	Item->SetModeIndex(SelectedModeIndex);
}

TSharedRef<SWidget> SDMXMVRFixtureListRow::GeneratePatchRow()
{
	return
		SNew(SBorder)
		.HAlign(HAlign_Fill)
		.VAlign(VAlign_Center)
		.Padding(4.f)
		.BorderImage(FAppStyle::GetBrush("NoBorder"))
		.OnMouseDoubleClick(this, &SDMXMVRFixtureListRow::OnPatchBorderDoubleClicked)
		[
			SNew(SBorder)
			.HAlign(HAlign_Fill)
			.VAlign(VAlign_Center)
			.BorderImage(FDMXEditorStyle::Get().GetBrush("DMXEditor.RoundedPropertyBorder"))
			[
				SAssignNew(PatchTextBlock, SInlineEditableTextBlock)
				.Text_Lambda([this]()
					{
						const int32 UniverseID = Item->GetUniverse();
						const int32 StartingAddress = Item->GetAddress();
						return FText::Format(LOCTEXT("AddressesText", "{0}.{1}"), UniverseID, StartingAddress);
					})
				.OnTextCommitted(this, &SDMXMVRFixtureListRow::OnPatchCommitted)
				.IsSelected(IsSelected)
			]
		];
}

FReply SDMXMVRFixtureListRow::OnPatchBorderDoubleClicked(const FGeometry& InMyGeometry, const FPointerEvent& InMouseEvent)
{
	if (InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
	{
		if (PatchTextBlock.IsValid())
		{
			PatchTextBlock->EnterEditingMode();
		}
	}

	return FReply::Handled();
}

void SDMXMVRFixtureListRow::OnPatchCommitted(const FText& InNewText, ETextCommit::Type InTextCommit)
{
	const FString PatchString = InNewText.ToString();
	static const TCHAR* ParamDelimiters[] =
	{
		TEXT("."),
		TEXT(","),
		TEXT(":"),
		TEXT(";")
	};

	TArray<FString> ValueStringArray;
	PatchString.ParseIntoArray(ValueStringArray, ParamDelimiters, 4);
	if (ValueStringArray.Num() == 2)
	{
		int32 Universe;
		if (!LexTryParseString(Universe, *ValueStringArray[0]))
		{
			return;
		}
		
		int32 Address;
		if (!LexTryParseString(Address, *ValueStringArray[1]))
		{
			return;
		}

		Item->SetAddresses(Universe, Address);

		const FString UniverseString = FString::FromInt(Universe);
		const FString AddressString = FString::FromInt(Address);
		PatchTextBlock->SetText(FText::Format(LOCTEXT("UniverseDotAddressText", "{0}.{1}"), FText::FromString(UniverseString), FText::FromString(AddressString)));

		OnRowRequestsListRefresh.ExecuteIfBound();
	}
}

#undef LOCTEXT_NAMESPACE
