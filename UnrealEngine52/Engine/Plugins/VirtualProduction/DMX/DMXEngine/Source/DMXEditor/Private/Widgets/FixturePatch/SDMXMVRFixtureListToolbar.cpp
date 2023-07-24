// Copyright Epic Games, Inc. All Rights Reserved.

#include "SDMXMVRFixtureListToolbar.h"

#include "DMXEditor.h"
#include "DMXEditorUtils.h"
#include "DMXFixturePatchSharedData.h"
#include "DMXMVRFixtureListItem.h"
#include "Library/DMXEntityFixturePatch.h"
#include "Library/DMXEntityFixtureType.h"
#include "Commands/DMXEditorCommands.h"
#include "Widgets/SDMXEntityDropdownMenu.h"

#include "EditorStyleSet.h"
#include "ScopedTransaction.h"
#include "Algo/MaxElement.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Input/SSearchBox.h"
#include "Widgets/Layout/SSeparator.h"
#include "Widgets/Layout/SWrapBox.h"


#define LOCTEXT_NAMESPACE "SDMXMVRFixtureListToolbar"

void SDMXMVRFixtureListToolbar::Construct(const FArguments& InArgs, TWeakPtr<FDMXEditor> InDMXEditor)
{
	WeakDMXEditor = InDMXEditor;
	OnSearchChanged = InArgs._OnSearchChanged;

	ChildSlot
		.HAlign(HAlign_Fill)
		.VAlign(VAlign_Fill)
		[
			SNew(SBorder)
			.Padding(FMargin(8.f, 8.f))
			.BorderBackgroundColor(FLinearColor(0.6f, 0.6f, 0.6f))
			.BorderImage(FAppStyle::GetBrush("DetailsView.CategoryTop"))
			[
				SNew(SWrapBox)
				.InnerSlotPadding(FVector2D(14.f, 8.f))
				.UseAllottedWidth(true)

				// Add Fixture Button
				+ SWrapBox::Slot()
				.HAlign(HAlign_Left)
				.VAlign(VAlign_Center)
				[
					GenerateFixtureTypeDropdownMenu()
				]
										
				+ SWrapBox::Slot()
				.HAlign(HAlign_Left)
				.VAlign(VAlign_Center)
				[
					SNew(SSeparator)
					.Orientation(EOrientation::Orient_Vertical)
				]

				// Search
				+ SWrapBox::Slot()
				.HAlign(HAlign_Left)
				.VAlign(VAlign_Center)
				[
					SNew(SSearchBox)
					.MinDesiredWidth(400.f)
					.OnTextChanged(this, &SDMXMVRFixtureListToolbar::OnSearchTextChanged)
					.ToolTipText(LOCTEXT("SearchBarTooltip", "Examples:\n\n* PatchName\n* FixtureTypeName\n* SomeMode\n* 1.\n* 1.1\n* Universe 1\n* Uni 1-3\n* Uni 1, 3\n* Uni 1, 4-5'."))
				]
									
				+ SWrapBox::Slot()
				.HAlign(HAlign_Left)
				.VAlign(VAlign_Center)
				[
					SNew(SSeparator)
					.Orientation(EOrientation::Orient_Vertical)
				]

				// Show Conflicts Only option
				+ SWrapBox::Slot()
				.HAlign(HAlign_Left)
				.VAlign(VAlign_Center)
				[
					SNew(STextBlock)
					.Text(LOCTEXT("ShowConflictsOnlyCheckBoxLabel", "Show Conflicts only"))
					.Font(FAppStyle::GetFontStyle(TEXT("PropertyWindow.NormalFont")))
				]

				+ SWrapBox::Slot()
				.HAlign(HAlign_Left)
				.VAlign(VAlign_Center)
				[
					SNew(SCheckBox)
					.IsChecked(false)
					.OnCheckStateChanged(this, &SDMXMVRFixtureListToolbar::OnShowConflictsOnlyCheckStateChanged)
				]
			]
		];
}

TArray<TSharedPtr<FDMXMVRFixtureListItem>> SDMXMVRFixtureListToolbar::FilterItems(const TArray<TSharedPtr<FDMXMVRFixtureListItem>>& Items)
{
	// Apply 'conflicts only' if enabled
	TArray<TSharedPtr<FDMXMVRFixtureListItem>> Result = Items;
	if (bShowConfictsOnly)
	{
		Result.RemoveAll([](const TSharedPtr<FDMXMVRFixtureListItem>& Item)
			{
				return
					Item->ErrorStatusText.IsEmpty() &&
					Item->WarningStatusText.IsEmpty();
			});
	}

	// Filter and return in order of precendence
	if (SearchString.IsEmpty())
	{
		return Result;
	}

	const TArray<int32> Universes = FDMXEditorUtils::ParseUniverses(SearchString);
	if(!Universes.IsEmpty())
	{
		Result.RemoveAll([Universes](const TSharedPtr<FDMXMVRFixtureListItem>& Item)
			{
				return !Universes.Contains(Item->GetUniverse());
			});

		return Result;
	}

	int32 Address;
	if (FDMXEditorUtils::ParseAddress(SearchString, Address))
	{
		Result.RemoveAll([Address](const TSharedPtr<FDMXMVRFixtureListItem>& Item)
			{
				return Item->GetAddress() != Address;
			});

		return Result;
	}
	
	const TArray<int32> FixtureIDs = FDMXEditorUtils::ParseFixtureIDs(SearchString);
	for (int32 FixtureID : FixtureIDs)
	{
		TArray<TSharedPtr<FDMXMVRFixtureListItem>> FixtureIDsOnlyResult = Result;
		FixtureIDsOnlyResult.RemoveAll([FixtureID](const TSharedPtr<FDMXMVRFixtureListItem>& Item)
			{
				int32 OtherFixtureIDNumerical;
				if (FDMXEditorUtils::ParseFixtureID(Item->GetFixtureID(), OtherFixtureIDNumerical))
				{
					return OtherFixtureIDNumerical != FixtureID;
				}
				return true;
			});

		if (FixtureIDsOnlyResult.Num() > 0)
		{
			return FixtureIDsOnlyResult;
		}
	}

	Result.RemoveAll([this](const TSharedPtr<FDMXMVRFixtureListItem>& Item)
		{
			return !Item->GetFixturePatchName().Contains(SearchString);
		});

	return Result;
}

TSharedRef<SWidget> SDMXMVRFixtureListToolbar::GenerateFixtureTypeDropdownMenu()
{
	FText AddButtonLabel = FDMXEditorCommands::Get().AddNewEntityFixturePatch->GetLabel();
	FText AddButtonToolTip = FDMXEditorCommands::Get().AddNewEntityFixturePatch->GetDescription();

	TSharedRef<SComboButton> AddComboButton = 
		SNew(SComboButton)
			.ButtonContent()
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(FMargin(0, 1))
				[
					SNew(SImage)
					.Image(FAppStyle::GetBrush("Plus"))
				]
				+ SHorizontalBox::Slot()
				.VAlign(VAlign_Center)
				.AutoWidth()
				.Padding(FMargin(2, 0, 2, 0))
				[
					SNew(STextBlock)
					.Text(AddButtonLabel)
				]
			]
			.MenuContent()
			[

				SNew(SVerticalBox)
				
				// Bulk Add Fixture Patches 
				+ SVerticalBox::Slot()
				.AutoHeight()
				[
					SNew(SHorizontalBox)
					
					+SHorizontalBox::Slot()
					.HAlign(HAlign_Left)
					.VAlign(VAlign_Center)
					.Padding(8.f, 2.f, 4.f, 2.f)
					[
						SNew(STextBlock)
						.Text(LOCTEXT("BulkAddPatchesLabel", "Quantity"))
					]

					+ SHorizontalBox::Slot()
					.HAlign(HAlign_Fill)
					.VAlign(VAlign_Center)
					.Padding(4.f, 2.f, 4.f, 2.f)
					[
						SNew(SEditableTextBox)
						.SelectAllTextWhenFocused(true)
						.ClearKeyboardFocusOnCommit(false)
						.SelectAllTextOnCommit(true)
						.Text_Lambda([this]()
							{
								return FText::FromString(FString::FromInt(NumFixturePatchesToAdd));
							})
						.OnVerifyTextChanged_Lambda([](const FText& InNewText, FText& OutErrorMessage)
							{
								int32 Value;
								if (!LexTryParseString<int32>(Value, *InNewText.ToString()) ||
									Value < 1)
								{
									OutErrorMessage = LOCTEXT("BulkAddPatchesBadString", "Needs a numeric value > 0");
									return false;
								}
								return true;
							})
						.OnTextCommitted_Lambda([this](const FText& Text, ETextCommit::Type CommitType)
							{
								int32 Value;
								if (LexTryParseString<int32>(Value, *Text.ToString()) &&
									Value > 0)
								{
									constexpr int32 MaxNumPatchesToBulkAdd = 512;
									NumFixturePatchesToAdd = FMath::Min(Value, MaxNumPatchesToBulkAdd);
								}
							})
					]
				]

				// Fixture Type Selection
				+ SVerticalBox::Slot()
				[
					SAssignNew(FixtureTypeDropdownMenu, SDMXEntityDropdownMenu<UDMXEntityFixtureType>)
					.DMXEditor(WeakDMXEditor)
					.OnEntitySelected(this, &SDMXMVRFixtureListToolbar::OnAddNewMVRFixtureClicked)
				]
			]
			.IsFocusable(true)
			.ContentPadding(FMargin(5.0f, 1.0f))
			.ComboButtonStyle(FAppStyle::Get(), "ToolbarComboButton")
			.ButtonStyle(FAppStyle::Get(), "FlatButton.Success")
			.ForegroundColor(FLinearColor::White)
			.ToolTipText(AddButtonToolTip)
			.OnComboBoxOpened(FOnComboBoxOpened::CreateLambda([this]() 
				{ 
					FixtureTypeDropdownMenu->RefreshEntitiesList();
				}));

	FixtureTypeDropdownMenu->SetComboButton(AddComboButton);

	return AddComboButton;
}

void SDMXMVRFixtureListToolbar::OnAddNewMVRFixtureClicked(UDMXEntity* InSelectedFixtureType)
{
	if (!ensureMsgf(InSelectedFixtureType, TEXT("Trying to add Fixture Patches, but selected fixture type is invalid.")))
	{
		return;
	}

	TSharedPtr<FDMXEditor> DMXEditor = WeakDMXEditor.Pin();
	if (!DMXEditor.IsValid())
	{
		return;
	}
	UDMXLibrary* DMXLibrary = DMXEditor->GetDMXLibrary();
	if (!DMXLibrary)
	{
		return;
	}
	UDMXEntityFixtureType* FixtureType = CastChecked<UDMXEntityFixtureType>(InSelectedFixtureType);

	// Find a Universe and Address
	TArray<UDMXEntityFixturePatch*> FixturePatches = DMXLibrary->GetEntitiesTypeCast<UDMXEntityFixturePatch>();
	UDMXEntityFixturePatch* const* LastFixturePatchPtr = Algo::MaxElementBy(FixturePatches, [](const UDMXEntityFixturePatch* FixturePatch)
		{
			return (int64)FixturePatch->GetUniverseID() * DMX_MAX_ADDRESS + FixturePatch->GetStartingChannel();
		});
	int32 Universe = 1;
	int32 Address = 1;
	int32 ChannelSpan = 0;
	if (LastFixturePatchPtr && !FixtureType->Modes.IsEmpty())
	{
		ChannelSpan = FixtureType->Modes[0].ChannelSpan;
		const UDMXEntityFixturePatch& LastFixturePatch = **LastFixturePatchPtr;
		if (LastFixturePatch.GetEndingChannel() + FixtureType->Modes[0].ChannelSpan > DMX_MAX_ADDRESS)
		{
			Universe = LastFixturePatch.GetUniverseID() + 1;
			Address = 1;
		}
		else
		{
			Universe = LastFixturePatch.GetUniverseID();
			Address = LastFixturePatch.GetEndingChannel() + 1;
		}
	}

	// Create a new fixture patches
	const FScopedTransaction CreateFixturePatchTransaction(LOCTEXT("CreateFixturePatchTransaction", "Create Fixture Patch"));
	TArray<UDMXEntityFixturePatch*> NewFixturePatches;
	TArray<TWeakObjectPtr<UDMXEntityFixturePatch>> NewWeakFixturePatches;
	DMXLibrary->PreEditChange(UDMXLibrary::StaticClass()->FindPropertyByName(UDMXLibrary::GetEntitiesPropertyName()));
	for (int32 iNumFixturePatchesAdded = 0; iNumFixturePatchesAdded < NumFixturePatchesToAdd; iNumFixturePatchesAdded++)
	{
		FDMXEntityFixturePatchConstructionParams FixturePatchConstructionParams;
		FixturePatchConstructionParams.FixtureTypeRef = FDMXEntityFixtureTypeRef(FixtureType);
		FixturePatchConstructionParams.ActiveMode = 0;
		FixturePatchConstructionParams.UniverseID = Universe;
		FixturePatchConstructionParams.StartingAddress = Address;

		constexpr bool bMarkLibraryDirty = false;
		UDMXEntityFixturePatch* NewFixturePatch = UDMXEntityFixturePatch::CreateFixturePatchInLibrary(FixturePatchConstructionParams, FixtureType->Name, bMarkLibraryDirty);
		NewFixturePatches.Add(NewFixturePatch);
		NewWeakFixturePatches.Add(NewFixturePatch);	

		// Increment Universe and Address in steps by one. This is enough for auto assign while keeping order of the named patches
		if (Address + ChannelSpan > DMX_MAX_ADDRESS)
		{
			Universe++;
			Address = 1; 
		}
		else
		{
			Address++;
		}
	}

	// Auto assign
	constexpr bool bAllowDecrementUniverse = false;
	constexpr bool bAllowDecrementChannels = false;
	FDMXEditorUtils::AutoAssignedChannels(bAllowDecrementUniverse, bAllowDecrementChannels, NewFixturePatches);

	DMXLibrary->PostEditChange();

	DMXEditor->GetFixturePatchSharedData()->SelectFixturePatches(NewWeakFixturePatches);
}

void SDMXMVRFixtureListToolbar::OnSearchTextChanged(const FText& SearchText)
{
	SearchString = SearchText.ToString();
	OnSearchChanged.ExecuteIfBound();
}

void SDMXMVRFixtureListToolbar::OnShowConflictsOnlyCheckStateChanged(const ECheckBoxState NewCheckState)
{
	bShowConfictsOnly = NewCheckState == ECheckBoxState::Checked;
	OnSearchChanged.ExecuteIfBound();
}

#undef LOCTEXT_NAMESPACE
