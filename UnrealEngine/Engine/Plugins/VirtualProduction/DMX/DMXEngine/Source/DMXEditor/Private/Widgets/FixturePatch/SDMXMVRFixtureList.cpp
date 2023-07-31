// Copyright Epic Games, Inc. All Rights Reserved.

#include "SDMXMVRFixtureList.h"

#include "DMXEditor.h"
#include "DMXEditorSettings.h"
#include "DMXEditorUtils.h"
#include "DMXFixturePatchSharedData.h"
#include "DMXRuntimeUtils.h"
#include "Commands/DMXEditorCommands.h"
#include "Library/DMXEntityFixturePatch.h"
#include "Library/DMXEntityFixtureType.h"
#include "Library/DMXLibrary.h"
#include "Widgets/FixturePatch/DMXMVRFixtureListItem.h"
#include "Widgets/FixturePatch/SDMXMVRFixtureListRow.h"
#include "Widgets/FixturePatch/SDMXMVRFixtureListToolbar.h"

#include "Factories.h"
#include "ScopedTransaction.h"
#include "TimerManager.h"
#include "UnrealExporter.h"
#include "Algo/MinElement.h"
#include "Algo/Sort.h"
#include "Exporters/Exporter.h"
#include "Framework/Application/SlateApplication.h"
#include "Framework/Commands/GenericCommands.h"
#include "Framework/Commands/UICommandList.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "HAL/PlatformApplicationMisc.h"
#include "Styling/AppStyle.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Views/SHeaderRow.h"
#include "Widgets/Views/SListView.h"


#define LOCTEXT_NAMESPACE "SDMXMVRFixtureList"

namespace UE::DMX::SDMXMVRFixtureList::Private
{
	/** Copies a fixture patch as text to the clipboard */
	static void ClipboardCopyFixturePatches(const TArray<UDMXEntityFixturePatch*>& FixturePatches)
	{
		// Clear the mark state for saving.
		UnMarkAllObjects(EObjectMark(OBJECTMARK_TagExp | OBJECTMARK_TagImp));

		const FExportObjectInnerContext Context;
		FStringOutputDevice Archive;

		// Export the component object(s) to text for copying
		for (UDMXEntityFixturePatch* FixturePatch : FixturePatches)
		{
			if (!FixturePatch)
			{
				continue;
			}

			// Export the entity object to the given string
			UExporter::ExportToOutputDevice(&Context, FixturePatch, nullptr, Archive, TEXT("copy"), 0, PPF_ExportsNotFullyQualified | PPF_Copy | PPF_Delimited, false, GetTransientPackage());
		}

		// Copy text to clipboard
		FString ExportedText = Archive;

		// Avoid exporting the OnFixturePatchReceived Binding
		TArray<FString> ExportedTextLines;
		constexpr bool bCullEmpty = false;
		ExportedText.ParseIntoArrayLines(ExportedTextLines, bCullEmpty);
		FString ExportedTextWithoutOnFixturePatchReceivedBinding;
		for (const FString& String : ExportedTextLines)
		{
			if (String.Contains(TEXT("OnFixturePatchReceivedDMX")))
			{
				continue;
			}
			ExportedTextWithoutOnFixturePatchReceivedBinding.Append(String + LINE_TERMINATOR);
		}

		FPlatformApplicationMisc::ClipboardCopy(*ExportedTextWithoutOnFixturePatchReceivedBinding);
	}

	/** Duplicates an existing patch */
	UDMXEntityFixturePatch* DuplicatePatchByMVRFixtureUUID(UDMXLibrary* DMXLibrary, const FGuid& MVRFixtureUUID)
	{
		if (!DMXLibrary)
		{
			return nullptr;
		}

		// Find the first free Addresses
		TArray<UDMXEntityFixturePatch*> FixturePatches = DMXLibrary->GetEntitiesTypeCast<UDMXEntityFixturePatch>();
		if (FixturePatches.Num() == 0)
		{
			return nullptr;
		}

		FixturePatches.Sort([](const UDMXEntityFixturePatch& FixturePatchA, const UDMXEntityFixturePatch& FixturePatchB)
			{
				const bool bUniverseIsSmaller = FixturePatchA.GetUniverseID() < FixturePatchB.GetUniverseID();
				const bool bUniverseIsEqual = FixturePatchA.GetUniverseID() == FixturePatchB.GetUniverseID();
				const bool bAddressIsSmallerOrEqual = FixturePatchA.GetStartingChannel() <= FixturePatchB.GetStartingChannel();

				return bUniverseIsSmaller || (bUniverseIsEqual && bAddressIsSmallerOrEqual);
			});

		int32 Address = FixturePatches.Last()->GetStartingChannel() + FixturePatches.Last()->GetChannelSpan();
		int32 Universe = -1;
		if (Address > DMX_MAX_ADDRESS)
		{
			Address = 1;
			Universe = FixturePatches.Last()->GetUniverseID() + 1;
		}
		else
		{
			Universe = FixturePatches.Last()->GetUniverseID();
		}

		UDMXEntityFixturePatch** FixturePatchToDuplicatePtr = Algo::FindByPredicate(FixturePatches, [MVRFixtureUUID](const UDMXEntityFixturePatch* FixturePatch)
			{
				return FixturePatch->GetMVRFixtureUUID() == MVRFixtureUUID;
			});

		if (!ensureMsgf(FixturePatchToDuplicatePtr, TEXT("Trying to duplicate fixture patch, but source fixture patch cannot be found")))
		{
			return nullptr;
		}
		UDMXEntityFixturePatch* FixturePatchToDuplicate = *FixturePatchToDuplicatePtr;
		check(FixturePatchToDuplicate);

		// Duplicate
		DMXLibrary->PreEditChange(UDMXLibrary::StaticClass()->FindPropertyByName(UDMXLibrary::GetEntitiesPropertyName()));
		UDMXEntityFixtureType* FixtureTypeOfPatchToDuplicate = FixturePatchToDuplicate->GetFixtureType();
		if (FixtureTypeOfPatchToDuplicate && FixtureTypeOfPatchToDuplicate->GetParentLibrary() != DMXLibrary)
		{
			FDMXEntityFixtureTypeConstructionParams FixtureTypeConstructionParams;
			FixtureTypeConstructionParams.DMXCategory = FixtureTypeOfPatchToDuplicate->DMXCategory;
			FixtureTypeConstructionParams.Modes = FixtureTypeOfPatchToDuplicate->Modes;
			FixtureTypeConstructionParams.ParentDMXLibrary = DMXLibrary;

			constexpr bool bMarkLibraryDirty = false;
			UDMXEntityFixtureType::CreateFixtureTypeInLibrary(FixtureTypeConstructionParams, FixtureTypeOfPatchToDuplicate->Name, bMarkLibraryDirty);
		}

		// Duplicate the Fixture Patch
		const int32 ChannelSpan = FixturePatchToDuplicate->GetChannelSpan();
		if (Address + ChannelSpan - 1 > DMX_MAX_ADDRESS)
		{
			Address = 1;
			Universe++;
		}

		FDMXEntityFixturePatchConstructionParams ConstructionParams;
		ConstructionParams.FixtureTypeRef = FixturePatchToDuplicate->GetFixtureType();
		ConstructionParams.ActiveMode = FixturePatchToDuplicate->GetActiveModeIndex();
		ConstructionParams.UniverseID = Universe;
		ConstructionParams.StartingAddress = Address;

		constexpr bool bMarkLibraryDirty = false;
		UDMXEntityFixturePatch* NewFixturePatch = UDMXEntityFixturePatch::CreateFixturePatchInLibrary(ConstructionParams, FixturePatchToDuplicate->Name, bMarkLibraryDirty);

		Address += ChannelSpan;

		DMXLibrary->PostEditChange();

		return NewFixturePatch;
	}

	/** Text object factory for pasting DMX Fixture Patches */
	struct FDMXFixturePatchObjectTextFactory final
		: public FCustomizableTextObjectFactory
	{
		/** Constructor */
		FDMXFixturePatchObjectTextFactory(UDMXLibrary* InDMXLibrary)
			: FCustomizableTextObjectFactory(GWarn)
			, WeakDMXLibrary(InDMXLibrary)
		{}

		/** Returns true if Fixture Patches can be constructed from the Text Buffer */
		static bool CanCreate(const FString& InTextBuffer, UDMXLibrary* InDMXLibrary)
		{
			TSharedRef<FDMXFixturePatchObjectTextFactory> Factory = MakeShared<FDMXFixturePatchObjectTextFactory>(InDMXLibrary);

			// Create new objects if we're allowed to
			return Factory->CanCreateObjectsFromText(InTextBuffer);
		}

		/**
		 * Constructs a new object factory from the given text buffer. Returns the factor or nullptr if no factory can be created.
		 * An updated General Scene Description of the library needs be passed explicitly to avoid recurring update calls.
		 */
		static bool Create(const FString& InTextBuffer, UDMXLibrary* InDMXLibrary, TArray<UDMXEntityFixturePatch*>& OutNewFixturePatches)
		{
			if (!InDMXLibrary)
			{
				return false;
			}

			OutNewFixturePatches.Reset();

			const TSharedRef<FDMXFixturePatchObjectTextFactory> Factory = MakeShared < FDMXFixturePatchObjectTextFactory>(InDMXLibrary);

			// Create new objects if we're allowed to
			if (Factory->CanCreateObjectsFromText(InTextBuffer))
			{
				Factory->WeakDMXLibrary = InDMXLibrary;

				EObjectFlags ObjectFlags = RF_Transactional;
				Factory->ProcessBuffer(InDMXLibrary, ObjectFlags, InTextBuffer);

				OutNewFixturePatches = Factory->NewFixturePatches;
			}

			return true;
		}

	protected:
		//~ Begin FCustomizableTextObjectFactory implementation
		virtual bool CanCreateClass(UClass* ObjectClass, bool& bOmitSubObjs) const override
		{
			return ObjectClass->IsChildOf(UDMXEntityFixturePatch::StaticClass());
		}

		virtual void ProcessConstructedObject(UObject* NewObject) override
		{
			UDMXLibrary* DMXLibrary = WeakDMXLibrary.Get();
			UDMXMVRGeneralSceneDescription* GeneralSceneDescription = DMXLibrary ? DMXLibrary->GetLazyGeneralSceneDescription() : nullptr;
			UDMXEntityFixturePatch* NewFixturePatch = Cast<UDMXEntityFixturePatch>(NewObject);
			if (DMXLibrary && GeneralSceneDescription && NewFixturePatch)
			{
				const FScopedTransaction Transaction(TransactionText);
				const FGuid MVRFixtureUUID = NewFixturePatch->GetMVRFixtureUUID();

				const bool bIsDuplicating = GeneralSceneDescription->FindFixtureNode(MVRFixtureUUID) != nullptr;
				if (bIsDuplicating)
				{
					// In cases where duplicates are created, create new instances to follow the common construction path for new Fixture Patches that ensures unqiue MVR UUIDs.
					// Otherwise just add the new instances to the library, but keep the orig MVR UUID (e.g. when cut and paste or copy from one library to another).

					using namespace UE::DMX::SDMXMVRFixtureList::Private;
					UDMXEntityFixturePatch* DuplicatedFixturePatch = DuplicatePatchByMVRFixtureUUID(DMXLibrary, MVRFixtureUUID);
					NewFixturePatches.Add(DuplicatedFixturePatch);

					// Remove the fixtue patch added by the factory
					UDMXEntityFixturePatch::RemoveFixturePatchFromLibrary(NewFixturePatch);
				}
				else
				{
					// Simply assign the new patch to the library
					NewFixturePatch->Rename(*MakeUniqueObjectName(DMXLibrary, NewFixturePatch->GetClass()).ToString(), DMXLibrary, REN_DoNotDirty | REN_DontCreateRedirectors);
					NewFixturePatch->SetName(FDMXRuntimeUtils::FindUniqueEntityName(DMXLibrary, NewFixturePatch->GetClass(), NewFixturePatch->GetDisplayName()));
					NewFixturePatch->SetParentLibrary(DMXLibrary);
					NewFixturePatch->RefreshID();

					NewFixturePatches.Add(NewFixturePatch);
				}
			}
		}
		//~ End FCustomizableTextObjectFactory implementation

	private:
		/** Instantiated Fixture Patches */
		TArray<UDMXEntityFixturePatch*> NewFixturePatches;

		/** Transaction text displayed when pasting */
		FText TransactionText;

		/** Weak DMX Editor in which the operation should occur */
		TWeakObjectPtr<UDMXLibrary> WeakDMXLibrary;
	};
}


/** Helper to generate Status Text for MVR Fixture List Items */
class FDMXMVRFixtureListStatusTextGenerator
{
public:
	FDMXMVRFixtureListStatusTextGenerator(const TArray<TSharedPtr<FDMXMVRFixtureListItem>>& InItems)
		: Items(InItems)
	{}

	/** Generates warning texts. Returns a map of those Items that need a warning set along with the warning Text */
	TMap<TSharedPtr<FDMXMVRFixtureListItem>, FText> GenerateWarningTexts() const
	{
		TMap<TSharedPtr<FDMXMVRFixtureListItem>, FText> AccumulatedConflicts;

		TMap<TSharedPtr<FDMXMVRFixtureListItem>, FText> FixtureTypeIssues = GetFixtureTypeIssues();
		AppendConflictTexts(FixtureTypeIssues, AccumulatedConflicts);

		TMap<TSharedPtr<FDMXMVRFixtureListItem>, FText> FixtureIDIssues = GetFixtureIDIssues();
		AppendConflictTexts(FixtureIDIssues, AccumulatedConflicts);

		TMap<TSharedPtr<FDMXMVRFixtureListItem>, FText> FixtureIDConflicts = GetFixtureIDConflicts();
		AppendConflictTexts(FixtureIDConflicts, AccumulatedConflicts);

		TMap<TSharedPtr<FDMXMVRFixtureListItem>, FText> ChannelExcessConflicts = GetChannelExcessConflicts();
		AppendConflictTexts(ChannelExcessConflicts, AccumulatedConflicts);

		TMap<TSharedPtr<FDMXMVRFixtureListItem>, FText> ChannelOverlapConflicts = GetChannelOverlapConflicts();
		AppendConflictTexts(ChannelOverlapConflicts, AccumulatedConflicts);

		return AccumulatedConflicts;
	}

private:
	void AppendConflictTexts(const TMap<TSharedPtr<FDMXMVRFixtureListItem>, FText>& InItemToConflictTextMap, TMap<TSharedPtr<FDMXMVRFixtureListItem>, FText>& InOutConflictTexts) const
	{
		for (const TTuple<TSharedPtr<FDMXMVRFixtureListItem>, FText>& ItemToConflictTextPair : InItemToConflictTextMap)
		{
			if (InOutConflictTexts.Contains(ItemToConflictTextPair.Key))
			{
				const FText LineTerminator = FText::FromString(LINE_TERMINATOR);
				const FText AccumulatedErrorText = FText::Format(FText::FromString(TEXT("{0}{1}{2}{3}")), InOutConflictTexts[ItemToConflictTextPair.Key], LineTerminator, LineTerminator, ItemToConflictTextPair.Value);
				InOutConflictTexts[ItemToConflictTextPair.Key] = AccumulatedErrorText;
			}
			else
			{
				InOutConflictTexts.Add(ItemToConflictTextPair);
			}
		}
	}

	/** The patch of an item. Useful to Get Conflicts with Other */
	struct FItemPatch
	{
		FItemPatch(const TSharedPtr<FDMXMVRFixtureListItem>& InItem)
			: Item(InItem)
		{
			Universe = Item->GetUniverse();
			AddressRange = TRange<int32>(Item->GetAddress(), Item->GetAddress() + Item->GetNumChannels());
		}

		/** Returns a conflict text if this item conflicts with Other */
		FText GetConfictsWithOther(const FItemPatch& Other) const
		{
			// No conflict with self
			if (Other.Item == Item)
			{
				return FText::GetEmpty();
			}

			// No conflict with the same patch
			if (Item->GetFixturePatch() == Other.Item->GetFixturePatch())
			{
				return FText::GetEmpty();
			}

			// No conflict if not in the same universe
			if (Other.Universe != Universe)
			{
				return FText::GetEmpty();
			}

			// No conflict if channels don't overlap
			if (!AddressRange.Overlaps(Other.AddressRange))
			{
				return FText::GetEmpty();
			}

			// No conflict if patches are functionally equal
			if (AddressRange.GetLowerBound() == Other.AddressRange.GetLowerBound() &&
				Item->GetFixtureType() == Other.Item->GetFixtureType() &&
				Item->GetModeIndex() == Other.Item->GetModeIndex())
			{
				return FText::GetEmpty();
			}

			const FText FixtureIDText = MakeBeautifulItemText(Item);
			const FText OtherFixtureIDText = MakeBeautifulItemText(Other.Item);
			if (AddressRange.GetLowerBound() == Other.AddressRange.GetLowerBound() &&
				Item->GetFixtureType() == Other.Item->GetFixtureType())
			{
				// Modes confict
				check(Item->GetModeIndex() != Other.Item->GetModeIndex());
				return FText::Format(LOCTEXT("ModeConflict", "Uses same Address and Fixture Type as Fixture {1}, but Modes differ."), FixtureIDText, OtherFixtureIDText);
			}
			else if (AddressRange.GetLowerBound() == Other.AddressRange.GetLowerBound())
			{
				// Fixture Types conflict
				check(Item->GetFixtureType() != Other.Item->GetFixtureType());
				return FText::Format(LOCTEXT("FixtureTypeConflict", "Uses same Address as Fixture {1}, but Fixture Types differ."), FixtureIDText, OtherFixtureIDText);
			}
			else
			{
				// Addresses conflict
				return FText::Format(LOCTEXT("AddressConflict", "Overlaps Addresses with Fixture {1}"), FixtureIDText, OtherFixtureIDText);
			}
		}

		FORCEINLINE const TSharedPtr<FDMXMVRFixtureListItem>& GetItem() const { return Item; };

	private:
		int32 Universe = -1;
		TRange<int32> AddressRange;

		TSharedPtr<FDMXMVRFixtureListItem> Item;
	};

	/** Returns a Map of Items to Channels that have Fixture Types with issues set */
	TMap<TSharedPtr<FDMXMVRFixtureListItem>, FText> GetFixtureTypeIssues() const
	{
		TMap<TSharedPtr<FDMXMVRFixtureListItem>, FText> ItemToIssueMap;
		for (const TSharedPtr<FDMXMVRFixtureListItem>& Item : Items)
		{
			if (!Item->GetFixtureType())
			{
				const FText IssueText = LOCTEXT("NoFixtureTypeIssue", "No Fixture Type selected.");
				ItemToIssueMap.Add(Item, IssueText);
			}
			else if (Item->GetFixtureType()->Modes.IsEmpty())
			{
				const FText IssueText = LOCTEXT("NoModesIssue", "Fixture Type has no Modes defined.");
				ItemToIssueMap.Add(Item, IssueText);
			}
			else if (Item->GetFixturePatch()->GetActiveMode() && 
				!Item->GetFixturePatch()->GetActiveMode()->bFixtureMatrixEnabled &&
				Item->GetFixturePatch()->GetActiveMode()->Functions.IsEmpty())
			{
				const FText IssueText = LOCTEXT("ActiveModeHasNoFunctionsIssue", "Mode does not define any Functions.");
				ItemToIssueMap.Add(Item, IssueText);
			}
		}

		return ItemToIssueMap;
	}

	/** Returns a Map of Items to Channels exceeding the DMX address range Texts */
	TMap<TSharedPtr<FDMXMVRFixtureListItem>, FText> GetChannelExcessConflicts() const
	{
		TMap<TSharedPtr<FDMXMVRFixtureListItem>, FText> ItemToConflictMap;
		for (const TSharedPtr<FDMXMVRFixtureListItem>& Item : Items)
		{
			const int32 EndingAddress = Item->GetAddress() + Item->GetNumChannels() - 1;
			if (Item->GetAddress() < 1 &&
				EndingAddress > DMX_MAX_ADDRESS)
			{
				const FText ConflictText = FText::Format(LOCTEXT("ChannelExceedsMinAndMaxChannelConflict", "Exceeds available DMX Address range. Staring Address is {0} but min Address is 1. Ending Address is {1} but max Address is 512."), Item->GetAddress(), EndingAddress);
				ItemToConflictMap.Add(Item, ConflictText);
			}
			else if (Item->GetAddress() < 1)
			{
				const FText ConflictText = FText::Format(LOCTEXT("ChannelExceedsMinChannelNumberConflict", "Exceeds available DMX Address range. Staring Address is {0} but min Address is 1."), Item->GetAddress());
				ItemToConflictMap.Add(Item, ConflictText);
			}
			else if (EndingAddress > DMX_MAX_ADDRESS)
			{
				const FText ConflictText = FText::Format(LOCTEXT("ChannelExeedsMaxChannelNumberConflict", "Exceeds available DMX Address range. Ending Address is {0} but max Address is 512."), EndingAddress);
				ItemToConflictMap.Add(Item, ConflictText);
			}			
		}

		return ItemToConflictMap;
	}

	/** Returns a Map of Items to overlapping Channel conflict Texts */
	TMap<TSharedPtr<FDMXMVRFixtureListItem>, FText> GetChannelOverlapConflicts() const
	{
		TArray<FItemPatch> ItemPatches;
		ItemPatches.Reserve(Items.Num());
		for (const TSharedPtr<FDMXMVRFixtureListItem>& Item : Items)
		{
			FItemPatch ItemPatch(Item);
			ItemPatches.Add(ItemPatch);
		}

		TMap<TSharedPtr<FDMXMVRFixtureListItem>, FText> ItemToConflictMap;
		for (const FItemPatch& ItemPatch : ItemPatches)
		{
			for (const FItemPatch& Other : ItemPatches)
			{
				const FText ConflictWithOtherText = ItemPatch.GetConfictsWithOther(Other);
				if (!ConflictWithOtherText.IsEmpty())
				{
					if (ItemToConflictMap.Contains(ItemPatch.GetItem()))
					{
						FText AppendConflictText = FText::Format(FText::FromString(TEXT("{0}{1}{2}")), ItemToConflictMap[ItemPatch.GetItem()], FText::FromString(FString(LINE_TERMINATOR)), ConflictWithOtherText);
						ItemToConflictMap[ItemPatch.GetItem()] = AppendConflictText;
					}
					else
					{
						ItemToConflictMap.Add(ItemPatch.GetItem(), ConflictWithOtherText);
					}
				}
			}
		}

		return ItemToConflictMap;
	}

	/** Returns an Map of Items to Fixture IDs issues Texts */
	TMap<TSharedPtr<FDMXMVRFixtureListItem>, FText> GetFixtureIDIssues() const
	{
		TMap<TSharedPtr<FDMXMVRFixtureListItem>, FText> Result;
		for (const TSharedPtr<FDMXMVRFixtureListItem>& Item : Items)
		{
			int32 FixtureIDNumerical;
			if (!LexTryParseString(FixtureIDNumerical, *Item->GetFixtureID()))
			{
				Result.Add(Item, LOCTEXT("FixtureIDNotNumericalIssueText", "FID has to be a number."));
			}
		}
		return Result;
	}

	/** Returns an Map of Items to Fixture IDs conflict Texts */
	TMap<TSharedPtr<FDMXMVRFixtureListItem>, FText> GetFixtureIDConflicts() const
	{
		TMap<FString, TArray<TSharedPtr<FDMXMVRFixtureListItem>>> FixtureIDMap;
		FixtureIDMap.Reserve(Items.Num());
		for (const TSharedPtr<FDMXMVRFixtureListItem>& Item : Items)
		{
			FixtureIDMap.FindOrAdd(Item->GetFixtureID()).Add(Item);
		}
		TArray<TArray<TSharedPtr<FDMXMVRFixtureListItem>>> FixtureIDConflicts;
		FixtureIDMap.GenerateValueArray(FixtureIDConflicts);
		FixtureIDConflicts.RemoveAll([](const TArray<TSharedPtr<FDMXMVRFixtureListItem>>& ConflictingItems)
			{
				return ConflictingItems.Num() < 2;
			});
		
		TMap<TSharedPtr<FDMXMVRFixtureListItem>, FText> ItemToConflictMap;
		for (TArray<TSharedPtr<FDMXMVRFixtureListItem>>& ConflictingItems : FixtureIDConflicts)
		{
			ConflictingItems.Sort([](const TSharedPtr<FDMXMVRFixtureListItem>& ItemA, const TSharedPtr<FDMXMVRFixtureListItem>& ItemB)
				{
					return ItemA->GetFixtureID() < ItemB->GetFixtureID();
				});

			check(ConflictingItems.Num() > 0);
			FText ConflictText = FText::Format(LOCTEXT("BaseFixtureIDConflictText", "Ambiguous FIDs in {0}"), MakeBeautifulItemText(ConflictingItems[0]));
			for (int32 ConflictingItemIndex = 1; ConflictingItemIndex < ConflictingItems.Num(); ConflictingItemIndex++)
			{
				ConflictText = FText::Format(LOCTEXT("AppendFixtureIDConflictText", "{0}, {1}"), ConflictText, MakeBeautifulItemText(ConflictingItems[ConflictingItemIndex]));
			}
			
			for (const TSharedPtr<FDMXMVRFixtureListItem>& Item : ConflictingItems)
			{
				ItemToConflictMap.Add(Item, ConflictText);
			}
		}

		return ItemToConflictMap;
	}

	static FText MakeBeautifulItemText(const TSharedPtr<FDMXMVRFixtureListItem>& Item)
	{
		const FString AddressesString = FString::FromInt(Item->GetUniverse()) + TEXT(".") + FString::FromInt(Item->GetAddress());
		const FString ItemNameString = TEXT("'") + Item->GetFixturePatchName() + TEXT("'");
		const FString BeautifulItemString = ItemNameString + TEXT(" (") + AddressesString + TEXT(")");;
		return FText::FromString(BeautifulItemString);
	}

	/** The items the class handles */
	TArray<TSharedPtr<FDMXMVRFixtureListItem>> Items;
};


const FName FDMXMVRFixtureListCollumnIDs::Status = "Status";
const FName FDMXMVRFixtureListCollumnIDs::FixturePatchName = "FixturePatchName";
const FName FDMXMVRFixtureListCollumnIDs::FixtureID = "FixtureID";
const FName FDMXMVRFixtureListCollumnIDs::FixtureType = "FixtureType";
const FName FDMXMVRFixtureListCollumnIDs::Mode = "Mode";
const FName FDMXMVRFixtureListCollumnIDs::Patch = "Patch";

SDMXMVRFixtureList::SDMXMVRFixtureList()
	: SortMode(EColumnSortMode::Ascending)
	, SortedByColumnID(FDMXMVRFixtureListCollumnIDs::Patch)
{}

SDMXMVRFixtureList::~SDMXMVRFixtureList()
{
	SaveHeaderRowSettings();
}

void SDMXMVRFixtureList::PostUndo(bool bSuccess)
{
	RequestListRefresh();
}

void SDMXMVRFixtureList::PostRedo(bool bSuccess)
{
	RequestListRefresh();
}

void SDMXMVRFixtureList::Construct(const FArguments& InArgs, TWeakPtr<FDMXEditor> InDMXEditor)
{
	if (!InDMXEditor.IsValid())
	{
		return;
	}
	
	WeakDMXEditor = InDMXEditor;
	FixturePatchSharedData = InDMXEditor.Pin()->GetFixturePatchSharedData();

	// Handle Entity changes
	UDMXLibrary::GetOnEntitiesAdded().AddSP(this, &SDMXMVRFixtureList::OnEntityAddedOrRemoved);
	UDMXLibrary::GetOnEntitiesRemoved().AddSP(this, &SDMXMVRFixtureList::OnEntityAddedOrRemoved);
	UDMXEntityFixturePatch::GetOnFixturePatchChanged().AddSP(this, &SDMXMVRFixtureList::OnFixturePatchChanged);
	UDMXEntityFixtureType::GetOnFixtureTypeChanged().AddSP(this, &SDMXMVRFixtureList::OnFixtureTypeChanged);

	// Handle Shared Data selection changes
	FixturePatchSharedData->OnFixturePatchSelectionChanged.AddSP(this, &SDMXMVRFixtureList::OnFixturePatchSharedDataSelectedFixturePatches);

	static const FTableViewStyle TableViewStyle = FAppStyle::Get().GetWidgetStyle<FTableViewStyle>("TreeView");

	ChildSlot
	[
		SNew(SVerticalBox)

		+ SVerticalBox::Slot()
		.HAlign(HAlign_Fill)
		.AutoHeight()
		[
			SAssignNew(Toolbar, SDMXMVRFixtureListToolbar, WeakDMXEditor)
			.OnSearchChanged(this, &SDMXMVRFixtureList::OnSearchChanged)
		]

		+ SVerticalBox::Slot()
		.HAlign(HAlign_Fill)
		.FillHeight(1.f)
		[
			SAssignNew(ListView, FDMXMVRFixtureListType)
			.ListViewStyle(&TableViewStyle)
			.HeaderRow(GenerateHeaderRow())
			.ItemHeight(40.0f)
			.ListItemsSource(&ListSource)
			.OnGenerateRow(this, &SDMXMVRFixtureList::OnGenerateRow)
			.OnSelectionChanged(this, &SDMXMVRFixtureList::OnSelectionChanged)
			.OnContextMenuOpening(this, &SDMXMVRFixtureList::OnContextMenuOpening)
		]
	];

	RegisterCommands();
	RefreshList();

	AdoptSelectionFromFixturePatchSharedData();

	// Make an initial selection if nothing was selected from Fixture Patch Shared Data, as if the user clicked it
	if (ListView->GetSelectedItems().IsEmpty() && !ListSource.IsEmpty())
	{
		ListView->SetSelection(ListSource[0], ESelectInfo::OnMouseClick);
	}
}

void SDMXMVRFixtureList::RequestListRefresh()
{
	if (RequestListRefreshTimerHandle.IsValid())
	{
		return;
	}

	RequestListRefreshTimerHandle = GEditor->GetTimerManager()->SetTimerForNextTick(FTimerDelegate::CreateSP(this, &SDMXMVRFixtureList::RefreshList));
}

void SDMXMVRFixtureList::EnterFixturePatchNameEditingMode()
{
	if (ListView.IsValid())
	{
		const TArray<TSharedPtr<FDMXMVRFixtureListItem>> SelectedItems = ListView->GetSelectedItems();
		if (SelectedItems.Num() == 0)
		{
			const TSharedPtr<SDMXMVRFixtureListRow>* SelectedRowPtr = Rows.FindByPredicate([&SelectedItems](const TSharedPtr<SDMXMVRFixtureListRow>& Row)
				{
					return Row->GetItem() == SelectedItems[0];
				});
			if (SelectedRowPtr)
			{
				(*SelectedRowPtr)->EnterFixturePatchNameEditingMode();
			}
		}
	}
}	

FReply SDMXMVRFixtureList::ProcessCommandBindings(const FKeyEvent& InKeyEvent)
{
	if (CommandList->ProcessCommandBindings(InKeyEvent))
	{
		return FReply::Handled();
	}

	return FReply::Unhandled();
}

FReply SDMXMVRFixtureList::OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent)
{
	return ProcessCommandBindings(InKeyEvent);
}

void SDMXMVRFixtureList::OnSearchChanged()
{
	RequestListRefresh();
}

void SDMXMVRFixtureList::RefreshList()
{	
	RequestListRefreshTimerHandle.Invalidate();

	SaveHeaderRowSettings();

	// Clear cached data
	Rows.Reset();

	TSharedPtr<FDMXEditor> DMXEditor = WeakDMXEditor.Pin();
	if (!DMXEditor.IsValid())
	{
		return;
	}

	UDMXLibrary* DMXLibrary = DMXEditor->GetDMXLibrary();
	if (!IsValid(DMXLibrary))
	{
		return;
	}

	UDMXMVRGeneralSceneDescription* GeneralSceneDescription = DMXLibrary->GetLazyGeneralSceneDescription();
	if (!GeneralSceneDescription)
	{
		return;
	}

	// Remove all that are no longer a patch
	const TArray<UDMXEntityFixturePatch*> FixturePatches = DMXLibrary->GetEntitiesTypeCast<UDMXEntityFixturePatch>();
	ListSource.RemoveAll([&FixturePatches](const TSharedPtr<FDMXMVRFixtureListItem>& Item)
		{
			return !FixturePatches.Contains(Item->GetFixturePatch());
		});
	
	bool bUpdatedGeneralSceneDescription = false;
	for (UDMXEntityFixturePatch* FixturePatch : FixturePatches)
	{
		bool bExisting = ListSource.ContainsByPredicate([FixturePatch](const TSharedPtr<FDMXMVRFixtureListItem>& Item)
			{
				return FixturePatch == Item->GetFixturePatch();
			});

		if (!bExisting)
		{
			if (!bUpdatedGeneralSceneDescription)
			{	
				// Ony update the General Scene Description when Nodes were never acquired, or new patches were added.
				DMXLibrary->UpdateGeneralSceneDescription();
				bUpdatedGeneralSceneDescription = true;
			}

			UDMXMVRFixtureNode* MVRFixtureNode = FindMVRFixtureNode(GeneralSceneDescription, FixturePatch);
			if (!MVRFixtureNode)
			{
				continue;
			}
			const TSharedRef<FDMXMVRFixtureListItem> NewItem = MakeShared<FDMXMVRFixtureListItem>(DMXEditor.ToSharedRef(), *MVRFixtureNode);

			ListSource.Add(NewItem);
		}
	}

	// Generate status texts
	GenereateStatusText();

	// Apply search filters. Relies on up-to-date status to find conflicts.
	if (Toolbar.IsValid())
	{
		ListSource = Toolbar->FilterItems(ListSource);
	}
	
	// Update and sort the list and its widgets
	ListView->RebuildList();

	SortByColumnID(EColumnSortPriority::Max, FDMXMVRFixtureListCollumnIDs::Patch, EColumnSortMode::Ascending);

	AdoptSelectionFromFixturePatchSharedData();
}

void SDMXMVRFixtureList::GenereateStatusText()
{
	for (const TSharedPtr<FDMXMVRFixtureListItem>& Item : ListSource)
	{
		Item->WarningStatusText = FText::GetEmpty();
		Item->ErrorStatusText = FText::GetEmpty();
	}

	FDMXMVRFixtureListStatusTextGenerator StatusTextGenerator(ListSource);

	const TMap<TSharedPtr<FDMXMVRFixtureListItem>, FText> WarningTextMap = StatusTextGenerator.GenerateWarningTexts();
	for (const TTuple<TSharedPtr<FDMXMVRFixtureListItem>, FText>& ItemToWarningTextPair : WarningTextMap)
	{
		ItemToWarningTextPair.Key->WarningStatusText = ItemToWarningTextPair.Value;
	}
}

TSharedRef<ITableRow> SDMXMVRFixtureList::OnGenerateRow(TSharedPtr<FDMXMVRFixtureListItem> InItem, const TSharedRef<STableViewBase>& OwnerTable)
{
	const TSharedRef<SDMXMVRFixtureListRow> NewRow =
		SNew(SDMXMVRFixtureListRow, OwnerTable, InItem.ToSharedRef())
		.OnRowRequestsListRefresh(this, &SDMXMVRFixtureList::RequestListRefresh)
		.OnRowRequestsStatusRefresh(this, &SDMXMVRFixtureList::GenereateStatusText)
		.IsSelected_Lambda([this, InItem]()
			{
				return ListView->IsItemSelected(InItem);
			});

	Rows.Add(NewRow);
	return NewRow;
}

void SDMXMVRFixtureList::OnSelectionChanged(TSharedPtr<FDMXMVRFixtureListItem> InItem, ESelectInfo::Type SelectInfo)
{
	if (SelectInfo == ESelectInfo::Direct)
	{
		return;
	}
 
	const TArray<TSharedPtr<FDMXMVRFixtureListItem>> SelectedItems = ListView->GetSelectedItems();
	TArray<TWeakObjectPtr<UDMXEntityFixturePatch>> FixturePatchesToSelect;
	for (const TSharedPtr<FDMXMVRFixtureListItem>& Item : SelectedItems)
	{
		if (UDMXEntityFixturePatch* FixturePatch = Item->GetFixturePatch())
		{
			FixturePatchesToSelect.AddUnique(FixturePatch);
		}
	}

	FixturePatchSharedData->SelectFixturePatches(FixturePatchesToSelect);

	if (FixturePatchesToSelect.Num() > 0)
	{
		const int32 SelectedUniverse = FixturePatchSharedData->GetSelectedUniverse();
		const int32 UniverseOfFirstItem = FixturePatchesToSelect[0]->GetUniverseID();
		if (SelectedUniverse != UniverseOfFirstItem)
		{
			FixturePatchSharedData->SelectUniverse(UniverseOfFirstItem);
		}
	}
}

void SDMXMVRFixtureList::OnEntityAddedOrRemoved(UDMXLibrary* DMXLibrary, TArray<UDMXEntity*> Entities)
{
	if (bChangingDMXLibrary)
	{
		return;
	}

	RequestListRefresh();
}

void SDMXMVRFixtureList::OnFixturePatchChanged(const UDMXEntityFixturePatch* FixturePatch)
{
	if (bChangingDMXLibrary)
	{
		return;
	}

	// Refresh only if the fixture patch is in the library this editor handles
	const UDMXLibrary* DMXLibrary = WeakDMXEditor.IsValid() ? WeakDMXEditor.Pin()->GetDMXLibrary() : nullptr;
	if (FixturePatch && FixturePatch->GetParentLibrary() == DMXLibrary)
	{
		RequestListRefresh();
	}
}

void SDMXMVRFixtureList::OnFixtureTypeChanged(const UDMXEntityFixtureType* FixtureType)
{
	if (bChangingDMXLibrary)
	{
		return;
	}

	// Refresh only if the fixture type is in the library this editor handles
	const UDMXLibrary* DMXLibrary = WeakDMXEditor.IsValid() ? WeakDMXEditor.Pin()->GetDMXLibrary() : nullptr;
	if (FixtureType && FixtureType->GetParentLibrary() == DMXLibrary)
	{
		RequestListRefresh();
	}
}

void SDMXMVRFixtureList::OnFixturePatchSharedDataSelectedFixturePatches()
{
	if (!bChangingDMXLibrary)
	{
		TArray<TWeakObjectPtr<UDMXEntityFixturePatch>> SelectedFixturePatches = FixturePatchSharedData->GetSelectedFixturePatches();
		SelectedFixturePatches.RemoveAll([](TWeakObjectPtr<UDMXEntityFixturePatch> FixturePatch)
			{
				return !FixturePatch.IsValid();
			});

		TArray<TSharedPtr<FDMXMVRFixtureListItem>> NewSelection;
		for (const TSharedPtr<FDMXMVRFixtureListItem>& Item : ListSource)
		{
			if (SelectedFixturePatches.Contains(Item->GetFixturePatch()))
			{
				NewSelection.Add(Item);
			}
		}

		if (NewSelection.Num() > 0)
		{
			ListView->ClearSelection();
			ListView->SetItemSelection(NewSelection, true, ESelectInfo::OnMouseClick);
		}
		else
		{
			ListView->ClearSelection();
		}
	}
}

void SDMXMVRFixtureList::AdoptSelectionFromFixturePatchSharedData()
{
	if (!ListView.IsValid())
	{
		return;
	}

	const TArray<TWeakObjectPtr<UDMXEntityFixturePatch>> SelectedFixturePatches = FixturePatchSharedData->GetSelectedFixturePatches();

	TArray<TSharedPtr<FDMXMVRFixtureListItem>> NewSelection;
	for (const TWeakObjectPtr<UDMXEntityFixturePatch> SelectedFixturePatch : SelectedFixturePatches)
	{
		const TSharedPtr<FDMXMVRFixtureListItem>* SelectedItemPtr = ListSource.FindByPredicate([SelectedFixturePatch](const TSharedPtr<FDMXMVRFixtureListItem>& Item)
			{
				return SelectedFixturePatch.IsValid() && Item->GetFixturePatch() == SelectedFixturePatch;
			});

		if (SelectedItemPtr)
		{
			NewSelection.Add(*SelectedItemPtr);
		}
	}

	if (NewSelection.Num() > 0)
	{
		ListView->ClearSelection();

		constexpr bool bSelected = true;
		ListView->SetItemSelection(NewSelection, bSelected, ESelectInfo::OnMouseClick);
		ListView->RequestScrollIntoView(NewSelection[0]);
	}
}

void SDMXMVRFixtureList::AutoAssignFixturePatches()
{
	if (FixturePatchSharedData.IsValid())
	{
		TArray<UDMXEntityFixturePatch*> FixturePatchesToAutoAssign;
		const TArray<TWeakObjectPtr<UDMXEntityFixturePatch>> SelectedFixturePatches = FixturePatchSharedData->GetSelectedFixturePatches();
		for (TWeakObjectPtr<UDMXEntityFixturePatch> FixturePatch : SelectedFixturePatches)
		{
			if (FixturePatch.IsValid())
			{
				FixturePatchesToAutoAssign.Add(FixturePatch.Get());
			}
		}

		if (FixturePatchesToAutoAssign.IsEmpty())
		{
			return;
		}

		constexpr bool bAllowDecrementUniverse = false;
		constexpr bool bAllowDecrementChannels = true;
		FDMXEditorUtils::AutoAssignedChannels(bAllowDecrementUniverse, bAllowDecrementChannels, FixturePatchesToAutoAssign);
		FixturePatchSharedData->SelectUniverse(FixturePatchesToAutoAssign[0]->GetUniverseID());

		RequestListRefresh();
	}
}

TSharedRef<SHeaderRow> SDMXMVRFixtureList::GenerateHeaderRow()
{
	const float StatusColumnWidth = FMath::Max(FAppStyle::GetBrush("Icons.Warning")->GetImageSize().X + 6.f, FAppStyle::GetBrush("Icons.Error")->GetImageSize().X + 6.f);

	HeaderRow = SNew(SHeaderRow);
	SHeaderRow::FColumn::FArguments ColumnArgs;

	HeaderRow->AddColumn(
		SHeaderRow::FColumn::FArguments()
		.ColumnId(FDMXMVRFixtureListCollumnIDs::FixturePatchName)
		.SortMode(this, &SDMXMVRFixtureList::GetColumnSortMode, FDMXMVRFixtureListCollumnIDs::FixturePatchName)
		.OnSort(this, &SDMXMVRFixtureList::SortByColumnID)
		.DefaultLabel(LOCTEXT("FixturePatchNameColumnLabel", "Fixture Patch"))
		.FillWidth(0.2f)
	);

	HeaderRow->AddColumn(
		SHeaderRow::FColumn::FArguments()
		.ColumnId(FDMXMVRFixtureListCollumnIDs::Status)
		.DefaultLabel(LOCTEXT("StatusColumnLabel", ""))
		.FixedWidth(StatusColumnWidth)
	);

	HeaderRow->AddColumn(
		SHeaderRow::FColumn::FArguments()
		.ColumnId(FDMXMVRFixtureListCollumnIDs::FixtureID)
		.SortMode(this, &SDMXMVRFixtureList::GetColumnSortMode, FDMXMVRFixtureListCollumnIDs::FixtureID)
		.OnSort(this, &SDMXMVRFixtureList::SortByColumnID)
		.DefaultLabel(LOCTEXT("FixtureIDColumnLabel", "FID"))
		.FillWidth(0.1f)
	);

	HeaderRow->AddColumn(
		SHeaderRow::FColumn::FArguments()
		.ColumnId(FDMXMVRFixtureListCollumnIDs::FixtureType)
		.SortMode(this, &SDMXMVRFixtureList::GetColumnSortMode, FDMXMVRFixtureListCollumnIDs::FixtureType)
		.OnSort(this, &SDMXMVRFixtureList::SortByColumnID)
		.DefaultLabel(LOCTEXT("FixtureTypeColumnLabel", "FixtureType"))
		.FillWidth(0.2f)
	);

	HeaderRow->AddColumn(
		SHeaderRow::FColumn::FArguments()
		.ColumnId(FDMXMVRFixtureListCollumnIDs::Mode)
		.SortMode(this, &SDMXMVRFixtureList::GetColumnSortMode, FDMXMVRFixtureListCollumnIDs::Mode)
		.OnSort(this, &SDMXMVRFixtureList::SortByColumnID)
		.DefaultLabel(LOCTEXT("ModeColumnLabel", "Mode"))
		.FillWidth(0.2f)
	);

	HeaderRow->AddColumn(
		SHeaderRow::FColumn::FArguments()
		.ColumnId(FDMXMVRFixtureListCollumnIDs::Patch)
		.SortMode(this, &SDMXMVRFixtureList::GetColumnSortMode, FDMXMVRFixtureListCollumnIDs::Patch)
		.OnSort(this, &SDMXMVRFixtureList::SortByColumnID)
		.DefaultLabel(LOCTEXT("PatchColumnLabel", "Patch"))
		.FillWidth(0.1f)
	);

	// Restore user settings
	RestoresHeaderRowSettings();

	return HeaderRow.ToSharedRef();
}


void SDMXMVRFixtureList::SetKeyboardFocus()
{
	FSlateApplication::Get().SetKeyboardFocus(AsShared());
}

void SDMXMVRFixtureList::SaveHeaderRowSettings()
{
	UDMXEditorSettings* EditorSettings = GetMutableDefault<UDMXEditorSettings>();
	if (HeaderRow.IsValid() && EditorSettings)
	{
		for (const SHeaderRow::FColumn& Column : HeaderRow->GetColumns())
		{
			if (Column.ColumnId == FDMXMVRFixtureListCollumnIDs::FixtureID)
			{
				EditorSettings->MVRFixtureListSettings.FixtureIDColumnWidth = Column.Width.Get();
			}
			if (Column.ColumnId == FDMXMVRFixtureListCollumnIDs::FixtureType)
			{
				EditorSettings->MVRFixtureListSettings.FixtureTypeColumnWidth = Column.Width.Get();
			}
			else if (Column.ColumnId == FDMXMVRFixtureListCollumnIDs::Mode)
			{
				EditorSettings->MVRFixtureListSettings.ModeColumnWidth = Column.Width.Get();
			}
			else if (Column.ColumnId == FDMXMVRFixtureListCollumnIDs::Patch)
			{
				EditorSettings->MVRFixtureListSettings.PatchColumnWidth = Column.Width.Get();
			}
		}

		EditorSettings->SaveConfig();
	}
}

void SDMXMVRFixtureList::RestoresHeaderRowSettings()
{
	if (const UDMXEditorSettings* EditorSettings = GetDefault<UDMXEditorSettings>())
	{
		const float FixtureIDColumnWidth = EditorSettings->MVRFixtureListSettings.FixtureIDColumnWidth;
		if (FixtureIDColumnWidth > 20.f)
		{
			HeaderRow->SetColumnWidth(FDMXMVRFixtureListCollumnIDs::FixtureID, FixtureIDColumnWidth);
		}

		const float FixtureTypeColumnWidth = EditorSettings->MVRFixtureListSettings.FixtureTypeColumnWidth;
		if (FixtureTypeColumnWidth > 20.f)
		{
			HeaderRow->SetColumnWidth(FDMXMVRFixtureListCollumnIDs::FixtureType, FixtureTypeColumnWidth);
		}

		const float ModeColumnWidth = EditorSettings->MVRFixtureListSettings.ModeColumnWidth;
		if (ModeColumnWidth > 20.f)
		{
			HeaderRow->SetColumnWidth(FDMXMVRFixtureListCollumnIDs::Mode, ModeColumnWidth);
		}

		const float PatchColumnWidth = EditorSettings->MVRFixtureListSettings.PatchColumnWidth;
		if (PatchColumnWidth > 20.f)
		{
			HeaderRow->SetColumnWidth(FDMXMVRFixtureListCollumnIDs::Patch, PatchColumnWidth);
		}
	}
}

EColumnSortMode::Type SDMXMVRFixtureList::GetColumnSortMode(const FName ColumnId) const
{
	if (SortedByColumnID != ColumnId)
	{
		return EColumnSortMode::None;
	}

	return SortMode;
}

void SDMXMVRFixtureList::SortByColumnID(const EColumnSortPriority::Type SortPriority, const FName& ColumnId, const EColumnSortMode::Type InSortMode)
{
	SortMode = InSortMode;
	SortedByColumnID = ColumnId;

	const bool bAscending = InSortMode == EColumnSortMode::Ascending ? true : false;

	if (ColumnId == FDMXMVRFixtureListCollumnIDs::FixturePatchName)
	{
		Algo::Sort(ListSource, [bAscending](const TSharedPtr<FDMXMVRFixtureListItem>& ItemA, const TSharedPtr<FDMXMVRFixtureListItem>& ItemB)
			{
				const FString FixturePatchNameA = ItemA->GetFixturePatchName();
				const FString FixturePatchNameB = ItemB->GetFixturePatchName();

				const bool bIsGreater = FixturePatchNameA >= FixturePatchNameB;
				return bAscending ? !bIsGreater : bIsGreater;
			});
	}
	else if (ColumnId == FDMXMVRFixtureListCollumnIDs::FixtureID)
	{
		Algo::Sort(ListSource, [bAscending](const TSharedPtr<FDMXMVRFixtureListItem>& ItemA, const TSharedPtr<FDMXMVRFixtureListItem>& ItemB)
			{
				bool bIsGreater = [ItemA, ItemB]()
				{
					const FString FixtureIDStringA = ItemA->GetFixtureID();
					const FString FixtureIDStringB = ItemB->GetFixtureID();

					int32 FixtureIDA = 0;
					int32 FixtureIDB = 0;

					const bool bCanParseA = LexTryParseString(FixtureIDA, *FixtureIDStringA);
					const bool bCanParseB = LexTryParseString(FixtureIDB, *FixtureIDStringB);
					
					const bool bIsNumeric = bCanParseA && bCanParseB;
					if (bIsNumeric)
					{
						return FixtureIDA >= FixtureIDB;
					}
					else
					{
						return FixtureIDStringA >= FixtureIDStringB;
					}
				}();

				return bAscending ? !bIsGreater : bIsGreater;
			});
	}
	else if (ColumnId == FDMXMVRFixtureListCollumnIDs::FixtureType)
	{
		Algo::Sort(ListSource, [bAscending](const TSharedPtr<FDMXMVRFixtureListItem>& ItemA, const TSharedPtr<FDMXMVRFixtureListItem>& ItemB)
			{
				const FString FixtureTypeA = ItemA->GetFixtureType()->Name;
				const FString FixtureTypeB = ItemB->GetFixtureType()->Name;

				const bool bIsGreater = FixtureTypeA >= FixtureTypeB;
				return bAscending ? !bIsGreater : bIsGreater;
			});
	}
	else if (ColumnId == FDMXMVRFixtureListCollumnIDs::Mode)
	{
		Algo::Sort(ListSource, [bAscending](const TSharedPtr<FDMXMVRFixtureListItem>& ItemA, const TSharedPtr<FDMXMVRFixtureListItem>& ItemB)
			{
				const bool bIsGreater = ItemA->GetModeIndex() >= ItemB->GetModeIndex();
				return bAscending ? !bIsGreater : bIsGreater;
			});
	}
	else if (ColumnId == FDMXMVRFixtureListCollumnIDs::Patch)
	{
		Algo::Sort(ListSource, [bAscending](const TSharedPtr<FDMXMVRFixtureListItem>& ItemA, const TSharedPtr<FDMXMVRFixtureListItem>& ItemB)
			{
				const UDMXEntityFixturePatch* FixturePatchA = ItemA->GetFixturePatch();
				const UDMXEntityFixturePatch* FixturePatchB = ItemB->GetFixturePatch();

				const bool bIsUniverseIDGreater = ItemA->GetUniverse() > ItemB->GetUniverse();
				const bool bIsSameUniverse = ItemA->GetUniverse() == ItemB->GetUniverse();
				const bool bAreAddressesGreater = ItemA->GetAddress() > ItemB->GetAddress();

				const bool bIsGreater = bIsUniverseIDGreater || (bIsSameUniverse && bAreAddressesGreater);
				return bAscending ? !bIsGreater : bIsGreater;
			});
	}

	ListView->RequestListRefresh();
}

UDMXMVRFixtureNode* SDMXMVRFixtureList::FindMVRFixtureNode(UDMXMVRGeneralSceneDescription* GeneralSceneDescription, UDMXEntityFixturePatch* FixturePatch) const
{
	if (!GeneralSceneDescription || !FixturePatch)
	{
		return nullptr;
	}

	const FGuid& MVRFixtureUUID  = FixturePatch->GetMVRFixtureUUID();
	return GeneralSceneDescription->FindFixtureNode(MVRFixtureUUID);
}

TSharedPtr<SWidget> SDMXMVRFixtureList::OnContextMenuOpening()
{
	if (!ListView.IsValid())
	{
		return SNullWidget::NullWidget;
	}

	const bool bCloseWindowAfterMenuSelection = true;
	FMenuBuilder MenuBuilder(bCloseWindowAfterMenuSelection, CommandList);

	// Auto Assign Section
	MenuBuilder.BeginSection("AutoAssignSection", LOCTEXT("AutoAssignSection", "Auto-Assign"));
	{
		// Auto Assign Entry
		const FUIAction Action(FExecuteAction::CreateSP(this, &SDMXMVRFixtureList::AutoAssignFixturePatches));

		const FText AutoAssignText = LOCTEXT("AutoAssignContextMenuEntry", "Auto-Assign Selection");
		const TSharedRef<SWidget> Widget =
			SNew(STextBlock)
			.Text(AutoAssignText);

		MenuBuilder.AddMenuEntry(Action, Widget);
		MenuBuilder.EndSection();
	}

	// Basic Operations Section
	MenuBuilder.BeginSection("BasicOperationsSection", LOCTEXT("BasicOperationsSection", "Basic Operations"));
	{
		MenuBuilder.AddMenuEntry(FGenericCommands::Get().Cut);
		MenuBuilder.AddMenuEntry(FGenericCommands::Get().Copy);
		MenuBuilder.AddMenuEntry(FGenericCommands::Get().Paste);
		MenuBuilder.AddMenuEntry(FGenericCommands::Get().Duplicate);
		MenuBuilder.AddMenuEntry(FGenericCommands::Get().Delete);
	}
	MenuBuilder.EndSection();

	return MenuBuilder.MakeWidget();
}

void SDMXMVRFixtureList::RegisterCommands()
{
	if (CommandList.IsValid())
	{
		return;
	}

	// listen to common editor shortcuts for copy/paste etc
	CommandList = MakeShared<FUICommandList>();
	CommandList->MapAction(FGenericCommands::Get().Cut, 
		FUIAction(
			FExecuteAction::CreateSP(this, &SDMXMVRFixtureList::OnCutSelectedItems),
			FCanExecuteAction::CreateSP(this, &SDMXMVRFixtureList::CanCutItems)
		)
	);
	CommandList->MapAction(FGenericCommands::Get().Copy,
		FUIAction(
			FExecuteAction::CreateSP(this, &SDMXMVRFixtureList::OnCopySelectedItems),
			FCanExecuteAction::CreateSP(this, &SDMXMVRFixtureList::CanCopyItems)
		)
	);
	CommandList->MapAction(FGenericCommands::Get().Paste,
		FUIAction(
			FExecuteAction::CreateSP(this, &SDMXMVRFixtureList::OnPasteItems),
			FCanExecuteAction::CreateSP(this, &SDMXMVRFixtureList::CanPasteItems)
		)
	);
	CommandList->MapAction(FGenericCommands::Get().Duplicate,
		FUIAction(
			FExecuteAction::CreateSP(this, &SDMXMVRFixtureList::OnDuplicateItems),
			FCanExecuteAction::CreateSP(this, &SDMXMVRFixtureList::CanDuplicateItems)
		)
	);
	CommandList->MapAction(FGenericCommands::Get().Delete,
		FUIAction(
			FExecuteAction::CreateSP(this, &SDMXMVRFixtureList::OnDeleteItems),
			FCanExecuteAction::CreateSP(this, &SDMXMVRFixtureList::CanDeleteItems),
			EUIActionRepeatMode::RepeatEnabled
		)
	);
}

bool SDMXMVRFixtureList::CanCutItems() const
{
	return CanCopyItems() && CanDeleteItems() && !GIsTransacting;
}

void SDMXMVRFixtureList::OnCutSelectedItems()
{
	const TArray<TSharedPtr<FDMXMVRFixtureListItem>> SelectedItems = ListView->GetSelectedItems();

	const FScopedTransaction Transaction(SelectedItems.Num() > 1 ? LOCTEXT("CutMVRFixtures", "Cut Fixtures") : LOCTEXT("CutMVRFixture", "Cut Fixture"));

	OnCopySelectedItems();
	OnDeleteItems();
}

bool SDMXMVRFixtureList::CanCopyItems() const
{
	return FixturePatchSharedData->GetSelectedFixturePatches().Num() > 0 && !GIsTransacting;
}

void SDMXMVRFixtureList::OnCopySelectedItems()
{
	const TArray<TSharedPtr<FDMXMVRFixtureListItem>> SelectedItems = ListView->GetSelectedItems();
	TArray<UDMXEntityFixturePatch*> FixturePatchesToCopy;
	for (const TSharedPtr<FDMXMVRFixtureListItem>& Item : SelectedItems)
	{
		FixturePatchesToCopy.Add(Item->GetFixturePatch());
	}

	using namespace UE::DMX::SDMXMVRFixtureList::Private;
	ClipboardCopyFixturePatches(FixturePatchesToCopy);
}

bool SDMXMVRFixtureList::CanPasteItems() const
{
	using namespace UE::DMX::SDMXMVRFixtureList::Private;

	UDMXLibrary* DMXLibrary = WeakDMXEditor.IsValid() ? WeakDMXEditor.Pin()->GetDMXLibrary() : nullptr;
	if (!DMXLibrary)
	{
		return false;
	}

	// Get the text from the clipboard
	FString TextToImport;
	FPlatformApplicationMisc::ClipboardPaste(TextToImport);

	return FDMXFixturePatchObjectTextFactory::CanCreate(TextToImport, DMXLibrary) && !GIsTransacting;
}

void SDMXMVRFixtureList::OnPasteItems()
{
	using namespace UE::DMX::SDMXMVRFixtureList::Private;

	UDMXLibrary* DMXLibrary = WeakDMXEditor.IsValid() ? WeakDMXEditor.Pin()->GetDMXLibrary() : nullptr;
	if (!DMXLibrary)
	{
		return;
	}

	TGuardValue<bool>(bChangingDMXLibrary, true);
	const FText TransactionText = LOCTEXT("PasteFixturePatchesTransaction", "Paste Fixture Patches");
	const FScopedTransaction PasteTransaction(TransactionText);

	FString TextToImport;
	FPlatformApplicationMisc::ClipboardPaste(TextToImport);

	TArray<UDMXEntityFixturePatch*> PastedFixturePatches;
	if(FDMXFixturePatchObjectTextFactory::Create(TextToImport, DMXLibrary, PastedFixturePatches))
	{
		TArray<TWeakObjectPtr<UDMXEntityFixturePatch>> WeakPastedFixturePatches;
		for (UDMXEntityFixturePatch* FixturePatch : PastedFixturePatches)
		{
			WeakPastedFixturePatches.Add(FixturePatch);
		}

		FixturePatchSharedData->SelectFixturePatches(WeakPastedFixturePatches);

		RequestListRefresh();
		AdoptSelectionFromFixturePatchSharedData();
	}
}

bool SDMXMVRFixtureList::CanDuplicateItems() const
{
	return FixturePatchSharedData->GetSelectedFixturePatches().Num() > 0 && !GIsTransacting;
}

void SDMXMVRFixtureList::OnDuplicateItems()
{
	UDMXLibrary* DMXLibrary = WeakDMXEditor.IsValid() ? WeakDMXEditor.Pin()->GetDMXLibrary() : nullptr;
	if (!DMXLibrary)
	{
		return;
	}

	TGuardValue<bool>(bChangingDMXLibrary, true);
	const FText TransactionText = LOCTEXT("DuplicateFixturePatchesTransaction", "Duplicate Fixture Patches");
	const FScopedTransaction PasteTransaction(TransactionText);

	const TArray<TWeakObjectPtr<UDMXEntityFixturePatch>> SelectedFixturePatches = FixturePatchSharedData->GetSelectedFixturePatches();
	TArray<TWeakObjectPtr<UDMXEntityFixturePatch>> NewFixturePatches;
	for (const TWeakObjectPtr<UDMXEntityFixturePatch> FixturePatch : SelectedFixturePatches)
	{
		if(!FixturePatch.IsValid() || !FixturePatch->GetParentLibrary())
		{
			continue;
		}

		using namespace UE::DMX::SDMXMVRFixtureList::Private;
		UDMXEntityFixturePatch* NewFixturePatch = DuplicatePatchByMVRFixtureUUID(DMXLibrary, FixturePatch->GetMVRFixtureUUID());
		NewFixturePatches.Add(NewFixturePatch);
	}
	FixturePatchSharedData->SelectFixturePatches(NewFixturePatches);

	RequestListRefresh();
	AdoptSelectionFromFixturePatchSharedData();
}

bool SDMXMVRFixtureList::CanDeleteItems() const
{
	return FixturePatchSharedData->GetSelectedFixturePatches().Num() > 0 && !GIsTransacting;
}

void SDMXMVRFixtureList::OnDeleteItems()
{
	TGuardValue<bool>(bChangingDMXLibrary, true);

	const TArray<TSharedPtr<FDMXMVRFixtureListItem>> SelectedItems = ListView->GetSelectedItems();

	if (SelectedItems.Num() == 0)
	{
		return;
	}
	
	// Its safe to assume all patches are in the same Library - A Multi-Library Editor wouldn't make sense.
	UDMXLibrary* DMXLibrary = SelectedItems[0]->GetDMXLibrary();
	if (!DMXLibrary)
	{
		return;
	}
	
	const FText DeleteFixturePatchesTransactionText = FText::Format(LOCTEXT("DeleteFixturePatchesTransaction", "Delete Fixture {0}|plural(one=Patch, other=Patches)"), SelectedItems.Num() > 1);
	const FScopedTransaction DeleteFixturePatchTransaction(DeleteFixturePatchesTransactionText);
	DMXLibrary->PreEditChange(nullptr);

	for (const TSharedPtr<FDMXMVRFixtureListItem>& Item : SelectedItems)
	{
		if (UDMXEntityFixturePatch* FixturePatch = Item->GetFixturePatch())
		{
			FixturePatch->PreEditChange(UDMXEntityFixturePatch::StaticClass()->FindPropertyByName(UDMXEntityFixturePatch::GetMVRFixtureUUIDPropertyNameChecked()));
			UDMXEntityFixturePatch::RemoveFixturePatchFromLibrary(FixturePatch);
			FixturePatch->PostEditChange();
		}
	}

	DMXLibrary->PostEditChange();

	// Make a meaningful selection invariant to ordering of the List
	TSharedPtr<FDMXMVRFixtureListItem> NewSelection;
	for (int32 ItemIndex = 0; ItemIndex < ListSource.Num(); ItemIndex++)
	{
		if (SelectedItems.Contains(ListSource[ItemIndex]))
		{
			if (ListSource.IsValidIndex(ItemIndex + 1) && !SelectedItems.Contains(ListSource[ItemIndex + 1]))
			{
				NewSelection = ListSource[ItemIndex + 1];
				break;
			}
			else if (ListSource.IsValidIndex(ItemIndex - 1) && !SelectedItems.Contains(ListSource[ItemIndex - 1]))
			{
				NewSelection = ListSource[ItemIndex - 1];
				break;
			}
		}
	}
	if (NewSelection.IsValid())
	{
		ListView->SetSelection(NewSelection, ESelectInfo::OnMouseClick);
	}

	RequestListRefresh();
}

#undef LOCTEXT_NAMESPACE
