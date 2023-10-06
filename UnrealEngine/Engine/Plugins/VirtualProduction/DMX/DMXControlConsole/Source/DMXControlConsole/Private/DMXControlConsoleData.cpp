// Copyright Epic Games, Inc. All Rights Reserved.

#include "DMXControlConsoleData.h"

#include "Algo/Find.h"
#include "Algo/Sort.h"
#include "DMXControlConsoleFaderGroup.h"
#include "DMXControlConsoleFaderGroupRow.h"
#include "IO/DMXOutputPort.h"
#include "Library/DMXEntityFixturePatch.h"
#include "Library/DMXEntityFixtureType.h"
#include "Library/DMXLibrary.h"


#define LOCTEXT_NAMESPACE "DMXControlConsole"

namespace UE::DMXControlConsole::DMXControlConsoleData::Private
{
	/** Returns the absolute channel of a fixture patch */
	int64 GetFixturePatchChannelAbsolute(const UDMXEntityFixturePatch* FixturePatch)
	{
		if (FixturePatch)
		{
			return (int64)FixturePatch->GetUniverseID() * DMX_UNIVERSE_SIZE + FixturePatch->GetStartingChannel();
		}
		else
		{
			return TNumericLimits<int32>::Max();
		}
	};
}


#if WITH_EDITOR
FSimpleMulticastDelegate UDMXControlConsoleData::OnDMXLibraryChanged;
#endif // WITH_EDITOR

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

#if WITH_EDITOR
TArray<UDMXControlConsoleFaderGroup*> UDMXControlConsoleData::GetAllActiveFaderGroups() const
{
	TArray<UDMXControlConsoleFaderGroup*> AllActiveFaderGroups = GetAllFaderGroups();
	AllActiveFaderGroups.RemoveAll([](const UDMXControlConsoleFaderGroup* FaderGroup)
			{
				return FaderGroup && !FaderGroup->IsActive();
			});
	
	return AllActiveFaderGroups;
}
#endif // WITH_EDITOR

void UDMXControlConsoleData::GenerateFromDMXLibrary()
{
	if (!CachedWeakDMXLibrary.IsValid())
	{
		return;
	}

	TArray<UDMXEntityFixturePatch*> FixturePatchesInLibrary = CachedWeakDMXLibrary->GetEntitiesTypeCast<UDMXEntityFixturePatch>();

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

	using namespace UE::DMXControlConsole::DMXControlConsoleData::Private;
	Algo::StableSortBy(FixturePatchesInLibrary, TFunction<int64(UDMXEntityFixturePatch*)>(&GetFixturePatchChannelAbsolute));

	int32 CurrentUniverseID = 0;
	for (int32 FixturePatchIndex = 0; FixturePatchIndex < FixturePatchesInLibrary.Num(); ++FixturePatchIndex)
	{
		UDMXEntityFixturePatch* FixturePatch = FixturePatchesInLibrary[FixturePatchIndex];
		if (!FixturePatch)
		{
			continue;
		}

		const int32 UniverseID = FixturePatch->GetUniverseID();
		UDMXControlConsoleFaderGroupRow* FaderGroupRow = nullptr;
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

UDMXControlConsoleFaderGroup* UDMXControlConsoleData::FindFaderGroupByFixturePatch(const UDMXEntityFixturePatch* InFixturePatch) const
{
	if (InFixturePatch)
	{
		const TArray<UDMXControlConsoleFaderGroup*> AllFaderGroups = GetAllFaderGroups();
		UDMXControlConsoleFaderGroup* const* FaderGroupPtr = Algo::FindByPredicate(AllFaderGroups, [InFixturePatch](const UDMXControlConsoleFaderGroup* FaderGroup)
			{
				return IsValid(FaderGroup) && FaderGroup->GetFixturePatch() == InFixturePatch;
			});

		return FaderGroupPtr ? *FaderGroupPtr : nullptr;
	}

	return nullptr;
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

void UDMXControlConsoleData::Clear()
{
	FaderGroupRows.Reset();
}

void UDMXControlConsoleData::ClearPatchedFaderGroups()
{
	const TArray<UDMXControlConsoleFaderGroup*> AllFaderGroups = GetAllFaderGroups();
	for (UDMXControlConsoleFaderGroup* FaderGroup : AllFaderGroups)
	{
		if (FaderGroup && FaderGroup->HasFixturePatch())
		{
			FaderGroup->Destroy();
		}
	}
}

void UDMXControlConsoleData::ClearAll(bool bOnlyPatchedFaderGroups)
{
	if (bOnlyPatchedFaderGroups)
	{
		ClearPatchedFaderGroups();
	}
	else
	{
		ClearAll();
	}
	CachedWeakDMXLibrary.Reset();
	SoftDMXLibraryPtr.Reset();
}

void UDMXControlConsoleData::OnFixturePatchAddedToLibrary(UDMXLibrary* Library, TArray<UDMXEntity*> Entities)
{
	if (Library != CachedWeakDMXLibrary)
	{
		return;
	}

	TArray<UDMXEntityFixturePatch*> FixturePatches;
	Algo::TransformIf(Entities, FixturePatches,
		[](const UDMXEntity* Entity)
		{
			return Entity && Entity->GetClass() == UDMXEntityFixturePatch::StaticClass();
		},
		[](UDMXEntity* Entity)
		{
			return CastChecked<UDMXEntityFixturePatch>(Entity);
		});

	using namespace UE::DMXControlConsole::DMXControlConsoleData::Private;
	Algo::StableSortBy(FixturePatches, TFunction<int64(UDMXEntityFixturePatch*)>(&GetFixturePatchChannelAbsolute));

	// Generate Fader Group for each new Entity in DMX Library
	int32 CurrentUniverseID = 0;
	for (UDMXEntity* Entity : Entities)
	{
		UDMXEntityFixturePatch* FixturePatch = Cast<UDMXEntityFixturePatch>(Entity);
		if (!FixturePatch)
		{
			continue;
		}

		const int32 UniverseID = FixturePatch->GetUniverseID();
		UDMXControlConsoleFaderGroupRow* FaderGroupRow = nullptr;
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

		OnFaderGroupAdded.Broadcast(FaderGroup);
	}
}

void UDMXControlConsoleData::PostLoad()
{
	Super::PostLoad();

	Modify();
	CachedWeakDMXLibrary = Cast<UDMXLibrary>(SoftDMXLibraryPtr.ToSoftObjectPath().TryLoad());

	UDMXLibrary::GetOnEntitiesAdded().AddUObject(this, &UDMXControlConsoleData::OnFixturePatchAddedToLibrary);
}

#if WITH_EDITOR
void UDMXControlConsoleData::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	const FName PropertyName = PropertyChangedEvent.GetPropertyName();
	if (PropertyName == GET_MEMBER_NAME_CHECKED(UDMXControlConsoleData, SoftDMXLibraryPtr))
	{
		CachedWeakDMXLibrary = Cast<UDMXLibrary>(SoftDMXLibraryPtr.ToSoftObjectPath().TryLoad());

		OnDMXLibraryChanged.Broadcast();
	}
}
#endif // WITH_EDITOR

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
#endif // WITH_EDITOR
	
	const TArray<UDMXControlConsoleFaderGroup*> FaderGroups = GetAllFaderGroups();
	for (const UDMXControlConsoleFaderGroup* FaderGroup : FaderGroups)
	{
		if (!FaderGroup || FaderGroup->IsMuted())
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

#undef LOCTEXT_NAMESPACE
