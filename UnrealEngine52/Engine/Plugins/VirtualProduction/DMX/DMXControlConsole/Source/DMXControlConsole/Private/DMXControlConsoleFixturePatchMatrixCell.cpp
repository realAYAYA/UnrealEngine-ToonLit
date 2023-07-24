// Copyright Epic Games, Inc. All Rights Reserved.

#include "DMXControlConsoleFixturePatchMatrixCell.h"

#include "DMXAttribute.h"
#include "DMXProtocolTypes.h"
#include "DMXControlConsoleFaderGroup.h"
#include "DMXControlConsoleFixturePatchCellAttributeFader.h"
#include "Library/DMXEntityFixturePatch.h"
#include "Library/DMXEntityFixtureType.h"

#include "Algo/Find.h"
#include "Algo/Sort.h"


#define LOCTEXT_NAMESPACE "DMXControlConsoleFixturePatchMatrixCell"

UDMXControlConsoleFaderGroup& UDMXControlConsoleFixturePatchMatrixCell::GetOwnerFaderGroupChecked() const
{
	UDMXControlConsoleFaderGroup* Outer = Cast<UDMXControlConsoleFaderGroup>(GetOuter());
	checkf(Outer, TEXT("Invalid outer for '%s', cannot get fader owner correctly."), *GetName());

	return *Outer;
}

int32 UDMXControlConsoleFixturePatchMatrixCell::GetIndex() const
{
	int32 Index = -1;

	const UDMXControlConsoleFaderGroup* Outer = Cast<UDMXControlConsoleFaderGroup>(GetOuter());
	if (!ensureMsgf(Outer, TEXT("Invalid outer for '%s', cannot get fader index correctly."), *GetName()))
	{
		return Index;
	}

	const TArray<TScriptInterface<IDMXControlConsoleFaderGroupElement>>& Elements = Outer->GetElements();
	Index = Elements.IndexOfByKey(this);

	return Index;
}

int32 UDMXControlConsoleFixturePatchMatrixCell::GetUniverseID() const
{
	if (!CellAttributeFaders.IsEmpty())
	{
		return CellAttributeFaders[0]->GetUniverseID();
	}

	return 1;
}

int32 UDMXControlConsoleFixturePatchMatrixCell::GetStartingAddress() const
{
	if (!CellAttributeFaders.IsEmpty())
	{
		return CellAttributeFaders[0]->GetStartingAddress();
	}

	return 1;
}

int32 UDMXControlConsoleFixturePatchMatrixCell::GetEndingAddress() const
{
	if (!CellAttributeFaders.IsEmpty())
	{
		return CellAttributeFaders.Last()->GetStartingAddress();
	}

	return 1;
}

void UDMXControlConsoleFixturePatchMatrixCell::Destroy() 
{
	UDMXControlConsoleFaderGroup* Outer = Cast<UDMXControlConsoleFaderGroup>(GetOuter());
	if (!ensureMsgf(Outer, TEXT("Invalid outer for '%s', cannot destroy fader correctly."), *GetName()))
	{
		return;
	}

#if WITH_EDITOR
	Outer->PreEditChange(UDMXControlConsoleFaderGroup::StaticClass()->FindPropertyByName(UDMXControlConsoleFaderGroup::GetElementsPropertyName()));
#endif // WITH_EDITOR

	Outer->DeleteElement(this);

#if WITH_EDITOR
	Outer->PostEditChange();
#endif // WITH_EDITOR
}

UDMXControlConsoleFixturePatchCellAttributeFader* UDMXControlConsoleFixturePatchMatrixCell::AddFixturePatchCellAttributeFader(const FDMXFixtureCellAttribute& CellAttribute, const int32 InUniverseID, const int32 StartingChannel)
{
	UDMXControlConsoleFixturePatchCellAttributeFader* CellAttributeFader = NewObject<UDMXControlConsoleFixturePatchCellAttributeFader>(this, NAME_None, RF_Transactional);
	CellAttributeFader->SetPropertiesFromFixtureCellAttribute(CellAttribute, InUniverseID, StartingChannel);
	CellAttributeFaders.Add(CellAttributeFader);

	return CellAttributeFader;
}

void UDMXControlConsoleFixturePatchMatrixCell::DeleteCellAttributeFader(UDMXControlConsoleFixturePatchCellAttributeFader* CellAttributeFader)
{
	if (!ensureMsgf(CellAttributeFader, TEXT("Invalid fader, cannot delete from '%s'."), *GetName()))
	{
		return;
	}

	if (!ensureMsgf(CellAttributeFaders.Contains(CellAttributeFader), TEXT("'%s' matrix cell is not owner of '%s'. Cannot delete fader correctly."), *GetName(), *CellAttributeFader->GetName()))
	{
		return;
	}

	CellAttributeFaders.Remove(CellAttributeFader);
}

void UDMXControlConsoleFixturePatchMatrixCell::SetPropertiesFromCell(const FDMXCell& Cell, const int32 InUniverseID, const int32 StartingChannel)
{
	// Order of initialization matters
	CellID = Cell.CellID;
	CellX = Cell.Coordinate.X;
	CellY = Cell.Coordinate.Y;

	const FString XToString = FString::FromInt(CellX);
	const FString YToString = FString::FromInt(CellY);

	UDMXControlConsoleFaderGroup& FaderGroup = GetOwnerFaderGroupChecked();
	UDMXEntityFixturePatch* FixturePatch = FaderGroup.GetFixturePatch();
	if (!FixturePatch)
	{
		return;
	}

	FDMXFixtureMatrix FixtureMatrix;
	if (!FixturePatch->GetMatrixProperties(FixtureMatrix))
	{
		return;
	}

	if (!UDMXEntityFixturePatch::GetOnFixturePatchChanged().IsBoundToObject(this))
	{
		UDMXEntityFixturePatch::GetOnFixturePatchChanged().AddUObject(this, &UDMXControlConsoleFixturePatchMatrixCell::OnFixturePatchChanged);
	}

	const TArray<FDMXFixtureCellAttribute> CellAttributes = FixtureMatrix.CellAttributes;
	TMap<FDMXAttributeName, int32> AttributeToChannelMap;
	FixturePatch->GetMatrixCellChannelsRelative(Cell.Coordinate, AttributeToChannelMap);

	for (const FDMXFixtureCellAttribute& CellAttribute : CellAttributes)
	{
		const FDMXAttributeName& AttributeName = CellAttribute.Attribute;
		const int32 RelativeChannel = AttributeToChannelMap.FindRef(AttributeName) - 1;
		const int32 AbsoluteChannel = StartingChannel + RelativeChannel;

		AddFixturePatchCellAttributeFader(CellAttribute, InUniverseID, AbsoluteChannel);
	}

	auto SortFadersByStartingAddressLambda = [](const UDMXControlConsoleFaderBase* ItemA, const UDMXControlConsoleFaderBase* ItemB)
		{
			const int32 StartingAddressA = ItemA->GetStartingAddress();
			const int32 StartingAddressB = ItemB->GetStartingAddress();

			return StartingAddressA < StartingAddressB;
		};

	Algo::Sort(CellAttributeFaders, SortFadersByStartingAddressLambda);
}

void UDMXControlConsoleFixturePatchMatrixCell::PostLoad()
{
	Super::PostLoad();

	UDMXControlConsoleFaderGroup& FaderGroup = GetOwnerFaderGroupChecked();
	UDMXEntityFixturePatch* FixturePatch = FaderGroup.GetFixturePatch();
	if (!FixturePatch)
	{
		return;
	}

	if (!UDMXEntityFixturePatch::GetOnFixturePatchChanged().IsBoundToObject(this))
	{
		UDMXEntityFixturePatch::GetOnFixturePatchChanged().AddUObject(this, &UDMXControlConsoleFixturePatchMatrixCell::OnFixturePatchChanged);
	}

	UpdateFixturePatchCellAttributeFaders(FixturePatch);
}

void UDMXControlConsoleFixturePatchMatrixCell::OnFixturePatchChanged(const UDMXEntityFixturePatch* InFixturePatch)
{
	UDMXControlConsoleFaderGroup& FaderGroup = GetOwnerFaderGroupChecked();
	UDMXEntityFixturePatch* MyFixturePatch = FaderGroup.GetFixturePatch();
	if (!MyFixturePatch ||
		MyFixturePatch != InFixturePatch)
	{
		return;
	}

	UpdateFixturePatchCellAttributeFaders(MyFixturePatch);
}

void UDMXControlConsoleFixturePatchMatrixCell::UpdateFixturePatchCellAttributeFaders(UDMXEntityFixturePatch* InFixturePatch)
{
	if (!InFixturePatch || !InFixturePatch->GetActiveMode())
	{
		return;
	}

	FDMXFixtureMatrix FixtureMatrix;
	if (!InFixturePatch->GetMatrixProperties(FixtureMatrix))
	{
		return;
	}

	const int32 UniverseID = InFixturePatch->GetUniverseID();
	const int32 StartingChannel = InFixturePatch->GetStartingChannel();

	const TArray<FDMXFixtureCellAttribute> CellAttributes = FixtureMatrix.CellAttributes;
	const FIntPoint Coordinate = FIntPoint(CellX, CellY);
	TMap<FDMXAttributeName, int32> AttributeToChannelMap;

	InFixturePatch->GetMatrixCellChannelsRelative(Coordinate, AttributeToChannelMap);
	// Destroy all FixturePatchCellAttributeFaders which Attribute is no longer in use
	auto IsAttributNoLongerInUseLambda = [AttributeToChannelMap](UDMXControlConsoleFaderBase* Fader)
		{
			const UDMXControlConsoleFixturePatchCellAttributeFader* CellAttributeFader = Cast<UDMXControlConsoleFixturePatchCellAttributeFader>(Fader);
			if (!CellAttributeFader)
			{
				return true;
			}

			const FDMXAttributeName& AttributeName = CellAttributeFader->GetAttributeName();
			if (!AttributeToChannelMap.Contains(AttributeName))
			{
				return true;
			}

			return false;
		};

	CellAttributeFaders.RemoveAll(IsAttributNoLongerInUseLambda);

	// Update FixturePatchCellAttributeFaders which Attributes are already in use and create CellAttributeFaders for new Attributes
	for (const FDMXFixtureCellAttribute& CellAttribute : CellAttributes)
	{
		const FDMXAttributeName& AttributeName = CellAttribute.Attribute;

		auto IsAttributeAlreadyInUseLambda = [AttributeName](UDMXControlConsoleFaderBase* Fader)
			{
				if (!Fader)
				{
					return false;
				}

				const UDMXControlConsoleFixturePatchCellAttributeFader* CellAttributeFader = Cast<UDMXControlConsoleFixturePatchCellAttributeFader>(Fader);
				if (!CellAttributeFader)
				{
					return false;
				}

				if (CellAttributeFader->GetAttributeName() != AttributeName)
				{
					return false;
				}

				return true;
			};

		const int32 RelativeChannel = AttributeToChannelMap.FindRef(AttributeName) - 1;
		const int32 AbsoluteChannel = StartingChannel + RelativeChannel;

		TObjectPtr<UDMXControlConsoleFaderBase>* MyFader = Algo::FindByPredicate(CellAttributeFaders, IsAttributeAlreadyInUseLambda);
		if (MyFader)
		{
			UDMXControlConsoleFixturePatchCellAttributeFader* MyCellAttributeFader = Cast<UDMXControlConsoleFixturePatchCellAttributeFader>(MyFader->Get());
			if (MyCellAttributeFader)
			{
				// SetPropertiesFromFixtureCellAttribute gets the the default value from the patch and sets it. 
				// Hence here remember the current value and set it back after setting properties.
				const uint32 Value = MyCellAttributeFader->GetValue();
				MyCellAttributeFader->SetPropertiesFromFixtureCellAttribute(CellAttribute, UniverseID, AbsoluteChannel);
				MyCellAttributeFader->SetValue(Value);
			}
		}
		else
		{
			AddFixturePatchCellAttributeFader(CellAttribute, UniverseID, AbsoluteChannel);
		}
	}

	auto SortFadersByStartingAddressLambda = [](const UDMXControlConsoleFaderBase* ItemA, const UDMXControlConsoleFaderBase* ItemB)
	{
		const int32 StartingAddressA = ItemA->GetStartingAddress();
		const int32 StartingAddressB = ItemB->GetStartingAddress();

		return StartingAddressA < StartingAddressB;
	};

	Algo::Sort(CellAttributeFaders, SortFadersByStartingAddressLambda);
}

#undef LOCTEXT_NAMESPACE
