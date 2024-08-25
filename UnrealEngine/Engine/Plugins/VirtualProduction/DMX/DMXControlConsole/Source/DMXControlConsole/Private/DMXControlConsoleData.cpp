// Copyright Epic Games, Inc. All Rights Reserved.

#include "DMXControlConsoleData.h"

#include "Algo/Find.h"
#include "Algo/Sort.h"
#include "DMXControlConsoleFaderBase.h"
#include "DMXControlConsoleFaderGroup.h"
#include "DMXControlConsoleFaderGroupRow.h"
#include "IO/DMXOutputPort.h"
#include "IO/DMXTrace.h"
#include "Layouts/Controllers/DMXControlConsoleControllerBase.h"
#include "Library/DMXEntityFixturePatch.h"
#include "Library/DMXEntityFixtureType.h"
#include "Library/DMXLibrary.h"
#include "UObject/Package.h"


namespace UE::DMX::Private
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

UDMXControlConsoleFaderGroup* UDMXControlConsoleData::FindFaderGroupByFixturePatch(const UDMXEntityFixturePatch* InFixturePatch) const
{
	if (InFixturePatch)
	{
		const TArray<UDMXControlConsoleFaderGroup*> AllFaderGroups = GetAllFaderGroups();
		UDMXControlConsoleFaderGroup* const* FaderGroupPtr = Algo::FindByPredicate(AllFaderGroups, 
			[InFixturePatch](const UDMXControlConsoleFaderGroup* FaderGroup)
			{
				return IsValid(FaderGroup) && FaderGroup->GetFixturePatch() == InFixturePatch;
			});

		return FaderGroupPtr ? *FaderGroupPtr : nullptr;
	}

	return nullptr;
}

void UDMXControlConsoleData::GenerateFromDMXLibrary()
{
	ClearPatchedFaderGroups();

	// Generate from library only if the library is valid
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

	using namespace UE::DMX::Private;
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

void UDMXControlConsoleData::StartSendingDMX()
{
	bSendDMX = true;
}

void UDMXControlConsoleData::StopSendingDMX()
{
	bSendDMX = false;

	// Handle stop DMX modes
	if (StopDMXMode == EDMXControlConsoleStopDMXMode::DoNotSendValues)
	{
		return;
	}

	const TArray<UDMXControlConsoleFaderGroup*> FaderGroups = GetAllFaderGroups();
	for (UDMXControlConsoleFaderGroup* FaderGroup : FaderGroups)
	{
		UDMXEntityFixturePatch* FixturePatch = FaderGroup->GetFixturePatch();
		if (FixturePatch && StopDMXMode == EDMXControlConsoleStopDMXMode::SendDefaultValues)
		{
			FixturePatch->SendDefaultValues();
		}
		else if (FixturePatch && StopDMXMode == EDMXControlConsoleStopDMXMode::SendZeroValues)
		{
			FixturePatch->SendZeroValues();
		}
		else
		{
			// Send zero to raw faders
			const TMap<int32, TMap<int32, uint8>> UniverseToFragmentMap = FaderGroup->GetUniverseToFragmentMap();
			for (const TTuple<int32, TMap<int32, uint8>>& UniverseToFragementPair : UniverseToFragmentMap)
			{
				TMap<int32, uint8> ChannelToZeroValueMap;
				Algo::Transform(UniverseToFragementPair.Value, ChannelToZeroValueMap,
					[](const TPair<int32, uint8>& ChannelToValuePair)
					{
						return TPair<int32, uint8>(ChannelToValuePair.Key, 0);
					});

				for (const FDMXOutputPortSharedRef& OutputPort : OutputPorts)
				{
					OutputPort->SendDMX(UniverseToFragementPair.Key, ChannelToZeroValueMap);
				}
			}
		}
	}
}

void UDMXControlConsoleData::SetStopDMXMode(EDMXControlConsoleStopDMXMode NewStopDMXMode)
{
	StopDMXMode = NewStopDMXMode;
}

void UDMXControlConsoleData::UpdateOutputPorts(const TArray<FDMXOutputPortSharedRef> InOutputPorts)
{
	if (InOutputPorts.IsEmpty())
	{
		return;
	}

	OutputPorts = InOutputPorts;
}

void UDMXControlConsoleData::Clear(bool bOnlyPatchedFaderGroups)
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

#if WITH_EDITOR
	OnDMXLibraryChangedDelegate.Broadcast();
#endif // WITH_EDITOR
}

void UDMXControlConsoleData::PostInitProperties()
{
	Super::PostInitProperties();

	if (!UDMXLibrary::GetOnEntitiesAdded().IsBoundToObject(this))
	{
		UDMXLibrary::GetOnEntitiesAdded().AddUObject(this, &UDMXControlConsoleData::OnFixturePatchAddedToLibrary);
	}
}

void UDMXControlConsoleData::PostLoad()
{
	Super::PostLoad();

	CachedWeakDMXLibrary = Cast<UDMXLibrary>(SoftDMXLibraryPtr.ToSoftObjectPath().TryLoad());
}

#if WITH_EDITOR
void UDMXControlConsoleData::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	const FName PropertyName = PropertyChangedEvent.GetPropertyName();
	if (PropertyName == GET_MEMBER_NAME_CHECKED(UDMXControlConsoleData, SoftDMXLibraryPtr))
	{
		CachedWeakDMXLibrary = Cast<UDMXLibrary>(SoftDMXLibraryPtr.ToSoftObjectPath().TryLoad());

		OnDMXLibraryChangedDelegate.Broadcast();
	}
}
#endif // WITH_EDITOR

void UDMXControlConsoleData::Tick(float InDeltaTime)
{
	// Ensure the cached dmx library and all fader groups are synched e.g. on library reload
	if (CachedWeakDMXLibrary.Get() != SoftDMXLibraryPtr)
	{
		CachedWeakDMXLibrary = SoftDMXLibraryPtr.LoadSynchronous();
		const TArray<UDMXControlConsoleFaderGroup*> AllFaderGroups = GetAllFaderGroups();
		for (UDMXControlConsoleFaderGroup* FaderGroup : AllFaderGroups)
		{
			if (FaderGroup)
			{
				FaderGroup->ReloadFixturePatch();
			}
		}

		OnDMXLibraryReloadedDelegate.Broadcast();
	}
		
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

	UDMXLibrary* DMXLibrary = GetDMXLibrary();
	const FName DMXLibraryName = DMXLibrary ? DMXLibrary->GetFName() : "<Invalid DMX Library>";

	UE_DMX_SCOPED_TRACE_SENDDMX(GetOutermost()->GetFName());
	const TArray<UDMXControlConsoleFaderGroup*> FaderGroups = GetAllFaderGroups();
	for (const UDMXControlConsoleFaderGroup* FaderGroup : FaderGroups)
	{
		if (!FaderGroup || !FaderGroup->IsEnabled())
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
					const TMap<FDMXAttributeName, float> AttributeToRelativeValueMap = CoordinateToAttribute.Value;
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
			UE_DMX_SCOPED_TRACE_SENDDMX("<No Patch>");

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

void UDMXControlConsoleData::ClearPatchedFaderGroups()
{
	const TArray<UDMXControlConsoleFaderGroup*> AllFaderGroups = GetAllFaderGroups();
	for (UDMXControlConsoleFaderGroup* FaderGroup : AllFaderGroups)
	{
		if (!FaderGroup)
		{
			continue;
		}

		if (!FaderGroup->HasFixturePatch())
		{
			continue;
		}

		FaderGroup->Destroy();
	}
}

void UDMXControlConsoleData::ClearAll()
{
	FaderGroupRows.Reset();
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

	using namespace UE::DMX::Private;
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
