// Copyright Epic Games, Inc. All Rights Reserved.

#include "SDMXMVRFixtureListToolbar.h"

#include "DMXEditor.h"
#include "DMXMVRFixtureListItem.h"
#include "Library/DMXEntityFixturePatch.h"
#include "Library/DMXEntityFixtureType.h"
#include "Commands/DMXEditorCommands.h"
#include "Widgets/SDMXEntityDropdownMenu.h"

#include "EditorStyleSet.h"
#include "ScopedTransaction.h"
#include "Internationalization/Regex.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Input/SSearchBox.h"
#include "Widgets/Layout/SSeparator.h"
#include "Widgets/Layout/SWrapBox.h"


#define LOCTEXT_NAMESPACE "SDMXMVRFixtureListToolbar"

namespace UE::DMXRuntime::SDMXMVRFixtureListToolbar::Private
{
	UE_NODISCARD bool ParseUniverse(const FString& InputString, int32& OutUniverse)
	{
		// Try to match addresses formating, e.g. '1.', '1:' etc.
		static const TCHAR* ParamDelimiters[] =
		{
			TEXT("."),
			TEXT(","),
			TEXT(":"),
			TEXT(";")
		};

		TArray<FString> ValueStringArray;
		constexpr bool bParseEmpty = true;
		const FString InputStringWithSpace = InputString + TEXT(" "); // So ValueStringArray will be lenght of 2 if Address is empty, e.g. '0.'
		InputStringWithSpace.ParseIntoArray(ValueStringArray, ParamDelimiters, 4, bParseEmpty);
		if (ValueStringArray.Num() == 2)
		{
			if (LexTryParseString<int32>(OutUniverse, *ValueStringArray[0]))
			{
				return true;
			}
		}

		// Try to match strings starting with Uni, e.g. 'Uni 1', 'Universe 1', 'Universe1'
		if (InputString.StartsWith(TEXT("Uni")))
		{
			const FRegexPattern SequenceOfDigitsPattern(TEXT("^[^\\d]*(\\d+)"));
			FRegexMatcher Regex(SequenceOfDigitsPattern, *InputString);
			if (Regex.FindNext())
			{
				const FString UniverseString = Regex.GetCaptureGroup(1);
				if (LexTryParseString<int32>(OutUniverse, *UniverseString))
				{
					return true;
				}
			}
		}

		OutUniverse = -1;
		return false;
	}

	UE_NODISCARD bool ParseAddress(const FString& InputString, int32& OutAddress)
	{
		// Try to match addresses formating, e.g. '1.1', '1:1' etc.
		static const TCHAR* ParamDelimiters[] =
		{
			TEXT("."),
			TEXT(","),
			TEXT(":"),
			TEXT(";")
		};

		TArray<FString> ValueStringArray;
		constexpr bool bParseEmpty = false;
		InputString.ParseIntoArray(ValueStringArray, ParamDelimiters, 4, bParseEmpty);

		if (ValueStringArray.Num() == 2)
		{
			if (LexTryParseString<int32>(OutAddress, *ValueStringArray[1]))
			{
				return true;
			}
		}

		// Try to match strings starting with Uni Ad, e.g. 'Uni 1 Ad 1', 'Universe 1 Address 1', 'Universe1Address1'
		if (InputString.StartsWith(TEXT("Uni")) &&
			InputString.Contains(TEXT("Ad")))
		{
			const FRegexPattern SequenceOfDigitsPattern(TEXT("^[^\\d]*(\\d+)[^\\d]*(\\d+)"));
			FRegexMatcher Regex(SequenceOfDigitsPattern, *InputString);
			if (Regex.FindNext())
			{
				const FString AddressString = Regex.GetCaptureGroup(2);
				if (LexTryParseString<int32>(OutAddress, *AddressString))
				{
					return true;
				}
			}
		}

		OutAddress = -1;
		return false;
	}

	UE_NODISCARD bool ParseFixtureID(const FString& InputString, int32& OutFixtureID)
	{
		if (LexTryParseString<int32>(OutFixtureID, *InputString))
		{
			return true;
		}

		OutFixtureID = -1;
		return false;
	}
}

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
	using namespace UE::DMXRuntime::SDMXMVRFixtureListToolbar::Private;

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

	int32 Universe;
	if (ParseUniverse(SearchString, Universe))
	{
		Result.RemoveAll([Universe](const TSharedPtr<FDMXMVRFixtureListItem>& Item)
			{
				return Item->GetUniverse() != Universe;
			});

		int32 Address;
		if (ParseAddress(SearchString, Address))
		{
			Result.RemoveAll([Address](const TSharedPtr<FDMXMVRFixtureListItem>& Item)
				{
					return Item->GetAddress() != Address;
				});
		}

		return Result;
	}
	
	int32 FixtureIDNumerical;
	if (ParseFixtureID(SearchString, FixtureIDNumerical))
	{
		TArray<TSharedPtr<FDMXMVRFixtureListItem>> FixtureIDsOnlyResult = Result;
		FixtureIDsOnlyResult.RemoveAll([FixtureIDNumerical](const TSharedPtr<FDMXMVRFixtureListItem>& Item)
			{
				int32 OtherFixtureIDNumerical;
				if (ParseFixtureID(Item->GetFixtureID(), OtherFixtureIDNumerical))
				{
					return OtherFixtureIDNumerical != FixtureIDNumerical;
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
				SAssignNew(FixtureTypeDropdownMenu, SDMXEntityDropdownMenu<UDMXEntityFixtureType>)
				.DMXEditor(WeakDMXEditor)
				.OnEntitySelected(this, &SDMXMVRFixtureListToolbar::OnAddNewMVRFixtureClicked)
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

	// Find a universe and address
	TArray<UDMXEntityFixturePatch*> FixturePatches = DMXLibrary->GetEntitiesTypeCast<UDMXEntityFixturePatch>();
	FixturePatches.Sort([](const UDMXEntityFixturePatch& FixturePatchA, const UDMXEntityFixturePatch& FixturePatchB)
		{
			const bool bUniverseIsSmaller = FixturePatchA.GetUniverseID() < FixturePatchB.GetUniverseID();
			const bool bUniverseIsEqual = FixturePatchA.GetUniverseID() == FixturePatchB.GetUniverseID();
			const bool bLastAddressIsSmaller = FixturePatchA.GetEndingChannel() < FixturePatchB.GetEndingChannel();
			
			return bUniverseIsSmaller || (bUniverseIsEqual && bLastAddressIsSmaller);
		});

	int32 Universe = 1; 
	int32 Address = 1;
	if (FixturePatches.Num() > 0)
	{
		const int32 ChannelSpan = FixtureType->Modes.Num() > 0 ? FixtureType->Modes[0].ChannelSpan : 1;
		if (FixturePatches.Last()->GetEndingChannel() + ChannelSpan > DMX_MAX_ADDRESS)
		{
			Universe = FixturePatches.Last()->GetUniverseID() + 1;
			Address = 1;
		}
		else
		{
			Universe = FixturePatches.Last()->GetUniverseID();
			Address = FixturePatches.Last()->GetEndingChannel() + 1;
		}
	}

	// Create a new fixture patch
	FDMXEntityFixturePatchConstructionParams FixturePatchConstructionParams;
	FixturePatchConstructionParams.FixtureTypeRef = FDMXEntityFixtureTypeRef(FixtureType);
	FixturePatchConstructionParams.ActiveMode = 0;
	FixturePatchConstructionParams.UniverseID = Universe;
	FixturePatchConstructionParams.StartingAddress = Address;

	const FScopedTransaction CreateFixturePatchTransaction(LOCTEXT("CreateFixturePatchTransaction", "Create Fixture Patch"));
	constexpr bool bMarkLibraryDirty = true;
	UDMXEntityFixturePatch* NewFixturePatch = UDMXEntityFixturePatch::CreateFixturePatchInLibrary(FixturePatchConstructionParams, FixtureType->Name, bMarkLibraryDirty);
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
