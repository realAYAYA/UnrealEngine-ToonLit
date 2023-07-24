// Copyright Epic Games, Inc. All Rights Reserved.

#include "DMXControlConsoleData.h"

#include "DMXControlConsoleFaderGroup.h"
#include "DMXControlConsoleFaderGroupRow.h"
#include "IO/DMXOutputPort.h"
#include "Library/DMXEntityFixturePatch.h"
#include "Library/DMXEntityFixtureType.h"
#include "Library/DMXLibrary.h"

#include "Algo/Sort.h"


#define LOCTEXT_NAMESPACE "DMXControlConsole"

UDMXControlConsoleFaderGroupRow* UDMXControlConsoleData::AddFaderGroupRow(const int32 RowIndex = 0)
{
	if (!ensureMsgf(RowIndex >= 0, TEXT("Invalid index. Cannot add new fader group row to '%s' correctly."), *GetName()))
	{
		return nullptr;
	}

	UDMXControlConsoleFaderGroupRow* FaderGroupRow = NewObject<UDMXControlConsoleFaderGroupRow>(this, NAME_None, RF_Transactional);
	FaderGroupRows.Insert(FaderGroupRow, RowIndex);
	FaderGroupRow->AddFaderGroup(0);

	return FaderGroupRow;
}

void UDMXControlConsoleData::DeleteFaderGroupRow(const TObjectPtr<UDMXControlConsoleFaderGroupRow>& FaderGroupRow)
{
	if (!ensureMsgf(FaderGroupRow, TEXT("Invalid fader group row, cannot delete from '%s'."), *GetName()))
	{
		return;
	}
	
	if (!ensureMsgf(FaderGroupRows.Contains(FaderGroupRow), TEXT("'%s' is not owner of '%s'. Cannot delete fader group row correctly."), *GetName(), *FaderGroupRow->GetName()))
	{
		return;
	}

	FaderGroupRows.Remove(FaderGroupRow);
}

void UDMXControlConsoleData::GenerateFromDMXLibrary()
{
	if (!DMXLibrary.IsValid())
	{
		return;
	}

	TArray<UDMXEntityFixturePatch*> FixturePatchesInLibrary = DMXLibrary->GetEntitiesTypeCast<UDMXEntityFixturePatch>();

	// Get All Fixture Patches in use in a Fader Group
	auto FaderGroupsHasFixturePatchInUseLambda = [](const UDMXControlConsoleFaderGroup* FaderGroup)
	{
		return FaderGroup && FaderGroup->HasFixturePatch();
	};
	auto GetFixturePatchFromFaderGroupLambda = [](UDMXControlConsoleFaderGroup* FaderGroup)
	{
		return FaderGroup->GetFixturePatch();
	};
	const TArray<UDMXControlConsoleFaderGroup*> AllFaderGroups = GetAllFaderGroups();
	TArray<UDMXEntityFixturePatch*> AllFixturePatchesInUse;
	Algo::TransformIf(AllFaderGroups, AllFixturePatchesInUse, FaderGroupsHasFixturePatchInUseLambda, GetFixturePatchFromFaderGroupLambda);

	FixturePatchesInLibrary.RemoveAll([AllFixturePatchesInUse](UDMXEntityFixturePatch* FixturePatch)
		{
			return AllFixturePatchesInUse.Contains(FixturePatch);
		});

	auto SortFixturePatchesLambda = [](const UDMXEntityFixturePatch* ItemA, const UDMXEntityFixturePatch* ItemB)
		{
			const int32 UniverseIDA = ItemA->GetUniverseID();
			const int32 UniverseIDB = ItemB->GetUniverseID();

			const int32 StartingChannelA = ItemA->GetStartingChannel();
			const int32 StartingChannelB = ItemB->GetStartingChannel();

			const int64 AbsoluteChannelA = (UniverseIDA - 1) * DMX_MAX_ADDRESS + StartingChannelA;
			const int64 AbsoluteChannelB = (UniverseIDB - 1) * DMX_MAX_ADDRESS + StartingChannelB;

			return AbsoluteChannelA < AbsoluteChannelB;
		};

	Algo::Sort(FixturePatchesInLibrary, SortFixturePatchesLambda);

	int32 CurrentUniverseID = 0;
	for (int32 FixturePatchIndex = 0; FixturePatchIndex < FixturePatchesInLibrary.Num(); ++FixturePatchIndex)
	{
		UDMXEntityFixturePatch* FixturePatch = FixturePatchesInLibrary[FixturePatchIndex];
		if (!FixturePatch)
		{
			continue;
		}

		const int32 UniverseID = FixturePatch->GetUniverseID();
		UDMXControlConsoleFaderGroupRow* FaderGroupRow;
		UDMXControlConsoleFaderGroup* RowFirstFaderGroup = nullptr;
		if (UniverseID > CurrentUniverseID)
		{
			CurrentUniverseID = UniverseID;
			FaderGroupRow = AddFaderGroupRow(FaderGroupRows.Num());
			RowFirstFaderGroup = FaderGroupRow->GetFaderGroups()[0];
		}
		else
		{
			FaderGroupRow = FaderGroupRows.Last();
		}

		if (!FaderGroupRow)
		{
			continue;
		}

		const int32 NextFaderGroupIndex = FaderGroupRow->GetFaderGroups().Num();

		UDMXControlConsoleFaderGroup* FaderGroup = FaderGroupRow->AddFaderGroup(NextFaderGroupIndex);
		if (!FaderGroup)
		{
			continue;
		}
		FaderGroup->GenerateFromFixturePatch(FixturePatch);

		if (RowFirstFaderGroup)
		{
			FaderGroupRow->DeleteFaderGroup(RowFirstFaderGroup);
		}
	}
}

void UDMXControlConsoleData::StartSendingDMX()
{
	bSendDMX = true;
}

void UDMXControlConsoleData::StopSendingDMX()
{
	bSendDMX = false;
}

void UDMXControlConsoleData::UpdateOutputPorts(const TArray<FDMXOutputPortSharedRef> InOutputPorts)
{
	if (InOutputPorts.IsEmpty())
	{
		return;
	}

	OutputPorts = InOutputPorts;
}

void UDMXControlConsoleData::Reset()
{
	ClearFaderGroupRows();
}

void UDMXControlConsoleData::Tick(float InDeltaTime)
{
	if (!bSendDMX)
	{
		return;
	}

#if WITH_EDITOR
	if (!bSendDMXInEditor && !GIsPlayInEditorWorld)
	{
		return;
	}
#endif
	
	const TArray<UDMXControlConsoleFaderGroup*> FaderGroups = GetAllFaderGroups();
	for (const UDMXControlConsoleFaderGroup* FaderGroup : FaderGroups)
	{
		if (!FaderGroup)
		{
			continue;
		}

		UDMXEntityFixturePatch* FixturePatch = FaderGroup->GetFixturePatch();
		if (FixturePatch)
		{
			// Send Fixture Patch Function DMX data
			const TMap<FDMXAttributeName, int32> AttributeMap = FaderGroup->GetAttributeMap();
			FixturePatch->SendDMX(AttributeMap);

			// Send Fixture Patch Matrix DMX data
			if (FaderGroup->HasMatrixProperties())
			{
				const TMap<FIntPoint, TMap<FDMXAttributeName, float>> CoordinateToAttributeMap = FaderGroup->GetMatrixCoordinateToAttributeMap();
				for (const TTuple<FIntPoint, TMap<FDMXAttributeName, float>>& CoordinateToAttribute : CoordinateToAttributeMap)
				{
					const FIntPoint CellCoordinate = CoordinateToAttribute.Key;

					TMap<FDMXAttributeName, float> AttributeToRelativeValueMap = CoordinateToAttribute.Value;
					for (const TTuple<FDMXAttributeName, float>& AttributeToRelativeValue : AttributeToRelativeValueMap)
					{
						const FDMXAttributeName& AttributeName = AttributeToRelativeValue.Key;
						const float RelativeValue = AttributeToRelativeValue.Value;
						FixturePatch->SendNormalizedMatrixCellValue(CellCoordinate, AttributeName, RelativeValue);
					}
				}
			}
		}
		else
		{
			// Send Raw DMX data
			const TMap<int32, TMap<int32, uint8>> UniverseToFragmentMap = FaderGroup->GetUniverseToFragmentMap();
			for (const TTuple<int32, TMap<int32, uint8>>& UniverseToFragement : UniverseToFragmentMap)
			{
				for (const FDMXOutputPortSharedRef& OutputPort : OutputPorts)
				{
					OutputPort->SendDMX(UniverseToFragement.Key, UniverseToFragement.Value);
				}
			}
		}
	}
}

TStatId UDMXControlConsoleData::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UDMXControlConsoleData, STATGROUP_Tickables);
}

ETickableTickType UDMXControlConsoleData::GetTickableTickType() const
{
	return ETickableTickType::Always;
}

void UDMXControlConsoleData::ClearFaderGroupRows()
{
	FaderGroupRows.Reset();
}

TArray<UDMXControlConsoleFaderGroup*> UDMXControlConsoleData::GetAllFaderGroups() const
{
	TArray<UDMXControlConsoleFaderGroup*> AllFaderGroups;

	for (const TObjectPtr<UDMXControlConsoleFaderGroupRow>& FaderGroupRow : FaderGroupRows)
	{
		if (!FaderGroupRow)
		{
			continue;
		}

		const TArray<UDMXControlConsoleFaderGroup*> FaderGroups = FaderGroupRow->GetFaderGroups();
		if (FaderGroups.IsEmpty())
		{
			continue;
		}

		AllFaderGroups.Append(FaderGroups);
	}

	return AllFaderGroups;
}

#undef LOCTEXT_NAMESPACE
