// Copyright Epic Games, Inc. All Rights Reserved.

#include "DMXControlConsoleFixturePatchMatrixCell.h"

#include "Algo/Find.h"
#include "Algo/Sort.h"
#include "DMXAttribute.h"
#include "DMXProtocolTypes.h"
#include "DMXControlConsoleFaderGroup.h"
#include "DMXControlConsoleFixturePatchCellAttributeFader.h"
#include "Layouts/Controllers/DMXControlConsoleControllerBase.h"
#include "Library/DMXEntityFixturePatch.h"
#include "Library/DMXEntityFixtureType.h"


#define LOCTEXT_NAMESPACE "DMXControlConsoleFixturePatchMatrixCell"

UDMXControlConsoleFaderGroup& UDMXControlConsoleFixturePatchMatrixCell::GetOwnerFaderGroupChecked() const
{
	UDMXControlConsoleFaderGroup* Outer = Cast<UDMXControlConsoleFaderGroup>(GetOuter());
	checkf(Outer, TEXT("Invalid outer for '%s', cannot get fader owner correctly."), *GetName());

	return *Outer;
}

UDMXControlConsoleControllerBase* UDMXControlConsoleFixturePatchMatrixCell::GetElementController() const
{
	return CachedWeakElementController.Get();
}

void UDMXControlConsoleFixturePatchMatrixCell::SetElementController(UDMXControlConsoleControllerBase* NewController)
{
	SoftControllerPtr = NewController;
	CachedWeakElementController = NewController;
}

int32 UDMXControlConsoleFixturePatchMatrixCell::GetIndex() const
{
	const UDMXControlConsoleFaderGroup& OwnerFaderGroup = GetOwnerFaderGroupChecked();
	const TArray<TScriptInterface<IDMXControlConsoleFaderGroupElement>>& Elements = OwnerFaderGroup.GetElements();
	const int32 Index = Elements.IndexOfByKey(this);
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

#if WITH_EDITOR
void UDMXControlConsoleFixturePatchMatrixCell::SetIsMatchingFilter(bool bMatches)
{
	bIsMatchingFilter = HasVisibleInEditorCellAttributeFaders();
}
#endif // WITH_EDITOR

void UDMXControlConsoleFixturePatchMatrixCell::Destroy() 
{
	UDMXControlConsoleFaderGroup& OwnerFaderGroup = GetOwnerFaderGroupChecked();

#if WITH_EDITOR
	OwnerFaderGroup.PreEditChange(UDMXControlConsoleFaderGroup::StaticClass()->FindPropertyByName(UDMXControlConsoleFaderGroup::GetElementsPropertyName()));
#endif // WITH_EDITOR

	OwnerFaderGroup.DeleteElement(this);

#if WITH_EDITOR
	OwnerFaderGroup.PostEditChange();
#endif // WITH_EDITOR
}

UDMXControlConsoleFixturePatchCellAttributeFader* UDMXControlConsoleFixturePatchMatrixCell::AddFixturePatchCellAttributeFader(const FDMXFixtureCellAttribute& CellAttribute, const int32 InUniverseID, const int32 StartingChannel)
{
	UDMXControlConsoleFixturePatchCellAttributeFader* CellAttributeFader = NewObject<UDMXControlConsoleFixturePatchCellAttributeFader>(this, NAME_None, RF_Transactional);
	CellAttributeFader->SetPropertiesFromFixtureCellAttribute(CellAttribute, InUniverseID, StartingChannel);
	CellAttributeFaders.Add(CellAttributeFader);

	UDMXControlConsoleFaderGroup& OwnerFaderGroup = GetOwnerFaderGroupChecked();
	OwnerFaderGroup.OnElementAdded.Broadcast(CellAttributeFader);

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

	UDMXControlConsoleFaderGroup& OwnerFaderGroup = GetOwnerFaderGroupChecked();
	OwnerFaderGroup.OnElementRemoved.Broadcast(CellAttributeFader);
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

		UDMXControlConsoleFixturePatchCellAttributeFader* CellAttributeFader = AddFixturePatchCellAttributeFader(CellAttribute, InUniverseID, AbsoluteChannel);
	}

	SortElementsByStartingAddress();
}

void UDMXControlConsoleFixturePatchMatrixCell::SortElementsByStartingAddress() const
{
	const auto SortElementsByStartingAddressLambda = [](const TScriptInterface<IDMXControlConsoleFaderGroupElement>& ItemA, const TScriptInterface<IDMXControlConsoleFaderGroupElement>& ItemB)
		{
			const int32 StartingAddressA = ItemA->GetStartingAddress();
			const int32 StartingAddressB = ItemB->GetStartingAddress();

			return StartingAddressA < StartingAddressB;
		};

	Algo::Sort(CellAttributeFaders, SortElementsByStartingAddressLambda);
}

#if WITH_EDITOR
bool UDMXControlConsoleFixturePatchMatrixCell::HasVisibleInEditorCellAttributeFaders() const
{
	for (const UDMXControlConsoleFaderBase* CellAttributeFader : CellAttributeFaders)
	{
		if (CellAttributeFader && CellAttributeFader->IsMatchingFilter())
		{
			return true;
		}
	}

	return false;
}
#endif // WITH_EDITOR

#if WITH_EDITOR
void UDMXControlConsoleFixturePatchMatrixCell::ShowAllFadersInEditor()
{
	for (UDMXControlConsoleFaderBase* CellAttributeFader : CellAttributeFaders)
	{
		if (!CellAttributeFader)
		{
			continue;
		}

		CellAttributeFader->SetIsMatchingFilter(true);
	}
}
#endif // WITH_EDITOR

void UDMXControlConsoleFixturePatchMatrixCell::PostLoad()
{
	Super::PostLoad();

	CachedWeakElementController = Cast<UDMXControlConsoleControllerBase>(SoftControllerPtr.ToSoftObjectPath().TryLoad());

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
	const auto IsAttributNoLongerInUseLambda = [AttributeToChannelMap](UDMXControlConsoleFaderBase* Fader)
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

		const auto IsAttributeAlreadyInUseLambda = [AttributeName](UDMXControlConsoleFaderBase* Fader)
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
			UDMXControlConsoleFixturePatchCellAttributeFader* NewCellAttributeFader = AddFixturePatchCellAttributeFader(CellAttribute, UniverseID, AbsoluteChannel);
		}
	}

	const auto SortFadersByStartingAddressLambda = [](const UDMXControlConsoleFaderBase* ItemA, const UDMXControlConsoleFaderBase* ItemB)
	{
		const int32 StartingAddressA = ItemA->GetStartingAddress();
		const int32 StartingAddressB = ItemB->GetStartingAddress();

		return StartingAddressA < StartingAddressB;
	};

	Algo::Sort(CellAttributeFaders, SortFadersByStartingAddressLambda);
}

#undef LOCTEXT_NAMESPACE
