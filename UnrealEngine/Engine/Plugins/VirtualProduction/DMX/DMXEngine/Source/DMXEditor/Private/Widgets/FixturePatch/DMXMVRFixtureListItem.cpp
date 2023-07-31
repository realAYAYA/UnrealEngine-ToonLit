// Copyright Epic Games, Inc. All Rights Reserved.

#include "DMXMVRFixtureListItem.h"

#include "DMXEditor.h"
#include "DMXFixturePatchSharedData.h"
#include "Library/DMXEntityFixturePatch.h"
#include "Library/DMXEntityReference.h"
#include "Library/DMXLibrary.h"
#include "MVR/Types/DMXMVRFixtureNode.h"

#include "ScopedTransaction.h"
#include "Algo/MinElement.h"


#define LOCTEXT_NAMESPACE "DMXMVRFixtureListItem"

FDMXMVRFixtureListItem::FDMXMVRFixtureListItem(TWeakPtr<FDMXEditor> InDMXEditor, UDMXMVRFixtureNode& InMVRFixtureNode)
	: MVRFixtureNode(&InMVRFixtureNode)
	, WeakDMXEditor(InDMXEditor)
{
	const UDMXLibrary* const DMXLibrary = GetDMXLibrary();
	if (!DMXLibrary)
	{
		return;
	}

	const TSharedPtr<FDMXEditor> DMXEditor = WeakDMXEditor.Pin();
	if (!DMXEditor)
	{
		return;
	}
	FixturePatchSharedData = DMXEditor->GetFixturePatchSharedData();

	const TArray<UDMXEntityFixturePatch*> FixturePatches = DMXLibrary->GetEntitiesTypeCast<UDMXEntityFixturePatch>();
	UDMXEntityFixturePatch* const* FixturePatchPtr = FixturePatches.FindByPredicate([this](const UDMXEntityFixturePatch* FixturePatch)
		{
			return FixturePatch->GetMVRFixtureUUID() == MVRFixtureNode->UUID;
		});

	if (!ensureAlwaysMsgf(FixturePatchPtr, TEXT("Trying to create an MVR Fixture List Item, but there's no corresponding Fixture Patch for the MVR Fixture UUID.")))
	{
		return;
	}
	WeakFixturePatch = *FixturePatchPtr;

	// Keep the MVR Fixture Name sync with the Fixture Patch name
	MVRFixtureNode->Name = WeakFixturePatch->Name;
}

const FGuid& FDMXMVRFixtureListItem::GetMVRUUID() const
{
	check(MVRFixtureNode);
	return MVRFixtureNode->UUID;
}

FLinearColor FDMXMVRFixtureListItem::GetBackgroundColor() const
{
	if (!ErrorStatusText.IsEmpty())
	{
		return FLinearColor::Red;
	}

	if (UDMXEntityFixturePatch* FixturePatch = WeakFixturePatch.Get())
	{
		return FixturePatch->EditorColor;
	}

	return FLinearColor::Red;
}

FString FDMXMVRFixtureListItem::GetFixturePatchName() const
{
	if (const UDMXEntityFixturePatch* FixturePatch = GetFixturePatch())
	{
		return FixturePatch->Name;
	}

	return FString();
}

void FDMXMVRFixtureListItem::SetFixturePatchName(const FString& InDesiredName, FString& OutNewName)
{
	check(MVRFixtureNode);
	if (UDMXEntityFixturePatch* FixturePatch = WeakFixturePatch.Get())
	{
		if (FixturePatch->Name == InDesiredName)
		{
			OutNewName = InDesiredName;
			return;
		}

		const FScopedTransaction SetFixturePatchNameTransaction(LOCTEXT("SetFixturePatchNameTransaction", "Set Fixture Patch Name"));
		FixturePatch->PreEditChange(UDMXEntityFixturePatch::StaticClass()->FindPropertyByName(GET_MEMBER_NAME_CHECKED(UDMXEntity, Name)));

		FixturePatch->SetName(InDesiredName);
		OutNewName = FixturePatch->Name;

		MVRFixtureNode->Name = FixturePatch->Name;

		FixturePatch->PostEditChange();
	}
}

FString FDMXMVRFixtureListItem::GetFixtureID() const
{
	check(MVRFixtureNode);
	return MVRFixtureNode->FixtureID;
}

void FDMXMVRFixtureListItem::SetFixtureID(int32 InFixtureID)
{
	check(MVRFixtureNode);

	UDMXLibrary* DMXLibrary = GetDMXLibrary();
	if (DMXLibrary)
	{
		const FScopedTransaction SetMVRFixtureFixtureIDTransaction(LOCTEXT("SetMVRFixtureFixtureIDTransaction", "Set MVR Fixture ID"));
		DMXLibrary->PreEditChange(UDMXLibrary::StaticClass()->FindPropertyByName(UDMXLibrary::GetGeneralSceneDescriptionPropertyName()));

		MVRFixtureNode->FixtureID = FString::FromInt(InFixtureID);

		DMXLibrary->PostEditChange();
	}
}

UDMXEntityFixtureType* FDMXMVRFixtureListItem::GetFixtureType() const
{
	if (UDMXEntityFixturePatch* FixturePatch = WeakFixturePatch.Get())
	{
		return FixturePatch->GetFixtureType();
	}

	return nullptr;
}

void FDMXMVRFixtureListItem::SetFixtureType(UDMXEntityFixtureType* FixtureType)
{
	UDMXLibrary* DMXLibrary = GetDMXLibrary();
	UDMXEntityFixturePatch* FixturePatch = WeakFixturePatch.Get();

	if (DMXLibrary && FixturePatch)
	{
		if (FixturePatch->GetFixtureType() == FixtureType)
		{
			return;
		}

		const FScopedTransaction SetFixtureTypeTransaction(LOCTEXT("SetFixtureTypeTransaction", "Set Fixture Type of Patch"));
		FixturePatch->PreEditChange(UDMXEntityFixturePatch::StaticClass()->FindPropertyByName(UDMXEntityFixturePatch::GetParentFixtureTypeTemplatePropertyNameChecked()));

		FixturePatch->SetFixtureType(FixtureType);

		FixturePatch->PostEditChange();
	}
}

int32 FDMXMVRFixtureListItem::GetModeIndex() const
{
	if (UDMXEntityFixturePatch* FixturePatch = WeakFixturePatch.Get())
	{
		return FixturePatch->GetActiveModeIndex();
	}

	return INDEX_NONE;
}

void FDMXMVRFixtureListItem::SetModeIndex(int32 ModeIndex)
{
	UDMXLibrary* DMXLibrary = GetDMXLibrary();
	UDMXEntityFixturePatch* FixturePatch = WeakFixturePatch.Get();

	if (DMXLibrary && FixturePatch)
	{
		if (ModeIndex == FixturePatch->GetActiveModeIndex())
		{
			return;
		}
		
		// If all should be changed, just change the patch
		const FScopedTransaction SetModeTransaction(LOCTEXT("SetModeTransaction", "Set Mode of Patch"));
		FixturePatch->PreEditChange(UDMXEntityFixturePatch::StaticClass()->FindPropertyByName(UDMXEntityFixturePatch::GetParentFixtureTypeTemplatePropertyNameChecked()));

		FixturePatch->SetActiveModeIndex(ModeIndex);
			
		FixturePatch->PostEditChange();
	}
}

int32 FDMXMVRFixtureListItem::GetUniverse() const
{
	if (UDMXEntityFixturePatch* FixturePatch = WeakFixturePatch.Get())
	{
		return FixturePatch->GetUniverseID();
	}

	return -1;
}

int32 FDMXMVRFixtureListItem::GetAddress() const
{
	if (UDMXEntityFixturePatch* FixturePatch = WeakFixturePatch.Get())
	{
		return FixturePatch->GetStartingChannel();
	}

	return -1;
}

void FDMXMVRFixtureListItem::SetAddresses(int32 Universe, int32 Address)
{
	UDMXLibrary* DMXLibrary = GetDMXLibrary();
	UDMXEntityFixturePatch* FixturePatch = WeakFixturePatch.Get();

	if (DMXLibrary && FixturePatch)
	{
		if (FixturePatch->GetUniverseID() == Universe &&
			FixturePatch->GetStartingChannel() == Address)
		{
			return;
		}

		// Only valid values
		const FDMXFixtureMode* ModePtr = FixturePatch->GetActiveMode();
		const int32 MaxAddress = ModePtr ? DMX_MAX_ADDRESS - ModePtr->ChannelSpan + 1 : DMX_MAX_ADDRESS;
		if (Universe < 0 || 
			Universe > DMX_MAX_UNIVERSE || 
			Address < 1 || 
			Address > MaxAddress)
		{
			return;
		}
		
		const FScopedTransaction SetAddressesTransaction(LOCTEXT("SetAddressesTransaction", "Set Addresses of Patch"));
		FixturePatch->PreEditChange(UDMXEntityFixturePatch::StaticClass()->FindPropertyByName(UDMXEntityFixturePatch::GetParentFixtureTypeTemplatePropertyNameChecked()));

		FixturePatch->SetUniverseID(Universe);
		FixturePatch->SetStartingChannel(Address);

		FixturePatch->PostEditChange();

		// Select the universe in Fixture Patch Shared Data
		FixturePatchSharedData->SelectUniverse(Universe);
	}
}

int32 FDMXMVRFixtureListItem::GetNumChannels() const
{
	if (UDMXEntityFixturePatch* FixturePatch = WeakFixturePatch.Get())
	{
		return FixturePatch->GetChannelSpan();
	}

	return -1;
}

UDMXEntityFixturePatch* FDMXMVRFixtureListItem::GetFixturePatch() const
{
	return WeakFixturePatch.Get();
}

UDMXLibrary* FDMXMVRFixtureListItem::GetDMXLibrary() const
{
	if (const TSharedPtr<FDMXEditor> DMXEditor = WeakDMXEditor.Pin())
	{
		UDMXLibrary* DMXLibrary = DMXEditor->GetDMXLibrary();
		if (DMXLibrary)
		{
			return DMXLibrary;
		}
	}

	return nullptr;
}

void FDMXMVRFixtureListItem::AddReferencedObjects(FReferenceCollector& Collector)
{
	Collector.AddReferencedObject(MVRFixtureNode);
}

#undef LOCTEXT_NAMESPACE
