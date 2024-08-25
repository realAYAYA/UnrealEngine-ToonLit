// Copyright Epic Games, Inc. All Rights Reserved.

#include "DMXControlConsoleFaderGroup.h"

#include "Algo/Find.h"
#include "Algo/StableSort.h"
#include "DMXAttribute.h"
#include "DMXControlConsoleFaderBase.h"
#include "DMXControlConsoleFaderGroupRow.h"
#include "DMXControlConsoleFixturePatchCellAttributeFader.h"
#include "DMXControlConsoleFixturePatchFunctionFader.h"
#include "DMXControlConsoleFixturePatchMatrixCell.h"
#include "DMXControlConsoleRawFader.h"
#include "DMXSubsystem.h"
#include "Layouts/Controllers/DMXControlConsoleControllerBase.h"
#include "Library/DMXEntityFixturePatch.h"
#include "Library/DMXEntityFixtureType.h"
#include "Library/DMXLibrary.h"


#define LOCTEXT_NAMESPACE "DMXControlConsoleFaderGroup"

UDMXControlConsoleRawFader* UDMXControlConsoleFaderGroup::AddRawFader()
{
	int32 Universe;
	int32 Address;
	GetNextAvailableUniverseAndAddress(Universe, Address);

	UDMXControlConsoleRawFader* Fader = NewObject<UDMXControlConsoleRawFader>(this, NAME_None, RF_Transactional);
	Fader->SetUniverseID(Universe);
	Fader->SetAddressRange(Address);
	Fader->SetFaderName(FString::FromInt(Elements.Num() + 1));
	Elements.Add(Fader);

	OnElementAdded.Broadcast(Fader);

	return Fader;
}

UDMXControlConsoleFixturePatchFunctionFader* UDMXControlConsoleFaderGroup::AddFixturePatchFunctionFader(const FDMXFixtureFunction& FixtureFunction, const int32 InUniverseID, const int32 StartingChannel)
{
	UDMXControlConsoleFixturePatchFunctionFader* Fader = NewObject<UDMXControlConsoleFixturePatchFunctionFader>(this, NAME_None, RF_Transactional);
	Fader->SetPropertiesFromFixtureFunction(FixtureFunction, InUniverseID, StartingChannel);
	Elements.Add(Fader);

	OnElementAdded.Broadcast(Fader);

	return Fader;
}

UDMXControlConsoleFixturePatchMatrixCell* UDMXControlConsoleFaderGroup::AddFixturePatchMatrixCell(const FDMXCell& Cell, const int32 InUniverseID, const int32 StartingChannel)
{
	UDMXControlConsoleFixturePatchMatrixCell* MatrixCell = NewObject<UDMXControlConsoleFixturePatchMatrixCell>(this, NAME_None, RF_Transactional);
	MatrixCell->SetPropertiesFromCell(Cell, InUniverseID, StartingChannel);
	Elements.Add(MatrixCell);

	OnElementAdded.Broadcast(MatrixCell);

	return MatrixCell;
}

void UDMXControlConsoleFaderGroup::DeleteElement(const TScriptInterface<IDMXControlConsoleFaderGroupElement>& Element)
{
	if (!ensureMsgf(Element, TEXT("Invalid element, cannot delete from '%s'."), *GetName()))
	{
		return;
	}

	if (!ensureMsgf(Elements.Contains(Element), TEXT("'%s' fader group is not owner of '%s'. Cannot delete element correctly."), *GetName(), *Element.GetObject()->GetName()))
	{
		return;
	}

	Elements.Remove(Element);

	OnElementRemoved.Broadcast(Element.GetInterface());
}

void UDMXControlConsoleFaderGroup::ClearElements()
{
	Elements.Reset();
}

TArray<TScriptInterface<IDMXControlConsoleFaderGroupElement>> UDMXControlConsoleFaderGroup::GetElements(bool bSortByUniverseAndAddress) const
{
	TArray<TScriptInterface<IDMXControlConsoleFaderGroupElement>> ElementsArray = Elements;
	if (bSortByUniverseAndAddress)
	{
		const auto SortElementsLambda = [](const TScriptInterface<IDMXControlConsoleFaderGroupElement>& Item)
			{
				if (Item)
				{
					const int64 UniverseID = Item->GetUniverseID();
					const int64 StartingChannel = Item->GetStartingAddress();
					const int64 AbsoluteChannel = (UniverseID - 1) * DMX_MAX_ADDRESS + StartingChannel;
					return AbsoluteChannel;
				}

				return TNumericLimits<int64>::Max();
			};

		Algo::StableSortBy(ElementsArray, SortElementsLambda);
	}

	return ElementsArray;
}

TArray<UDMXControlConsoleFaderBase*> UDMXControlConsoleFaderGroup::GetAllFaders() const
{
	TArray<UDMXControlConsoleFaderBase*> AllFaders;

	for (const TScriptInterface<IDMXControlConsoleFaderGroupElement>& Element : Elements)
	{
		if (!Element)
		{
			continue;
		}

		AllFaders.Append(Element->GetFaders());
	}

	return AllFaders;
}

void UDMXControlConsoleFaderGroup::SortElementsByStartingAddress() const
{
	// Sort elements
	const auto SortElementsByStartingAddressLambda = [](const TScriptInterface<IDMXControlConsoleFaderGroupElement>& Item)
		{
			if (Item)
			{
				const int32 StartingAddress = Item->GetStartingAddress();
				return StartingAddress;
			}

			return TNumericLimits<int32>::Max();
		};

	Algo::StableSortBy(Elements, SortElementsByStartingAddressLambda);
}

int32 UDMXControlConsoleFaderGroup::GetIndex() const
{
	const UDMXControlConsoleFaderGroupRow& FaderGroupRow = GetOwnerFaderGroupRowChecked();

	const TArray<UDMXControlConsoleFaderGroup*> FaderGroups = FaderGroupRow.GetFaderGroups();
	return FaderGroups.IndexOfByKey(this);
}

UDMXControlConsoleFaderGroupRow& UDMXControlConsoleFaderGroup::GetOwnerFaderGroupRowChecked() const
{
	UDMXControlConsoleFaderGroupRow* Outer = Cast<UDMXControlConsoleFaderGroupRow>(GetOuter());
	checkf(Outer, TEXT("Invalid outer for '%s', cannot get fader group owner correctly."), *GetName());

	return *Outer;
}

UDMXControlConsoleControllerBase* UDMXControlConsoleFaderGroup::GetFaderGroupController() const
{
	return CachedWeakFaderGroupController.Get();
}

void UDMXControlConsoleFaderGroup::SetFaderGroupController(UDMXControlConsoleControllerBase* NewController)
{
	SoftControllerPtr = NewController;
	CachedWeakFaderGroupController = NewController;
}

void UDMXControlConsoleFaderGroup::SetFaderGroupName(const FString& NewName)
{
	FaderGroupName = NewName;
}

void UDMXControlConsoleFaderGroup::GenerateFromFixturePatch(UDMXEntityFixturePatch* InFixturePatch)
{
	if (!InFixturePatch)
	{
		return;
	}

	SubscribeToFixturePatchDelegates();

	Modify();
	SoftFixturePatchPtr = InFixturePatch;
	CachedWeakFixturePatch = InFixturePatch;

	FaderGroupName = InFixturePatch->GetDisplayName();
	
	ClearElements();

	const int32 UniverseID = InFixturePatch->GetUniverseID();
	const int32 StartingChannel = InFixturePatch->GetStartingChannel();

	// Generate Faders from Fixture Functions
	const TMap<FDMXAttributeName, FDMXFixtureFunction> FunctionsMap = InFixturePatch->GetAttributeFunctionsMap();
	for (const TTuple<FDMXAttributeName, FDMXFixtureFunction>& FunctionTuple : FunctionsMap)
	{
		const FDMXFixtureFunction FixtureFunction = FunctionTuple.Value;
		UDMXControlConsoleFaderBase* FunctionFader = AddFixturePatchFunctionFader(FixtureFunction, UniverseID, StartingChannel);
	}

	// Generate Faders from Fixture Matrices
	FDMXFixtureMatrix FixtureMatrix;
	if (InFixturePatch->GetMatrixProperties(FixtureMatrix))
	{
		TArray<FDMXCell> Cells;
		InFixturePatch->GetAllMatrixCells(Cells);
		for (const FDMXCell& Cell : Cells)
		{
			UDMXControlConsoleFixturePatchMatrixCell* MatrixCell = AddFixturePatchMatrixCell(Cell, UniverseID, StartingChannel);
		}
	}

	SortElementsByStartingAddress();
	OnFixturePatchChangedDelegate.Broadcast(this, InFixturePatch);
}

void UDMXControlConsoleFaderGroup::ReloadFixturePatch()
{
	CachedWeakFixturePatch = SoftFixturePatchPtr.LoadSynchronous();
	UpdateFaderGroupFromFixturePatch(CachedWeakFixturePatch.Get());
}

bool UDMXControlConsoleFaderGroup::HasFixturePatch() const
{
	return GetFixturePatch() != nullptr;
}

bool UDMXControlConsoleFaderGroup::HasMatrixProperties() const
{
	if (!HasFixturePatch())
	{
		return false;
	}

	FDMXFixtureMatrix FixtureMatrix;
	const UDMXEntityFixturePatch* FixturePatch = GetFixturePatch();
	return FixturePatch->GetMatrixProperties(FixtureMatrix);
}

TMap<int32, TMap<int32, uint8>> UDMXControlConsoleFaderGroup::GetUniverseToFragmentMap() const
{
	TMap<int32, TMap<int32, uint8>> UniverseToFragmentMap;

	if (HasFixturePatch())
	{
		return UniverseToFragmentMap;
	}

	UDMXSubsystem* DMXSubsystem = UDMXSubsystem::GetDMXSubsystem_Pure();
	check(DMXSubsystem);

	const TArray<UDMXControlConsoleFaderBase*> AllFaders = GetAllFaders();
	for (UDMXControlConsoleFaderBase* Fader : AllFaders)
	{
		if (!Fader || !Fader->IsEnabled())
		{
			continue;
		}

		const UDMXControlConsoleRawFader* RawFader = Cast<UDMXControlConsoleRawFader>(Fader);
		if (!RawFader)
		{
			continue;
		}
		
		TMap<int32, uint8>& FragmentMapRef = UniverseToFragmentMap.FindOrAdd(RawFader->GetUniverseID());

		TArray<uint8> ByteArray;
		DMXSubsystem->IntValueToBytes(RawFader->GetValue(), RawFader->GetDataType(), ByteArray, RawFader->GetUseLSBMode());

		for (int32 ByteIndex = 0; ByteIndex < ByteArray.Num(); ByteIndex++)
		{
			const int32 CurrentAddress = RawFader->GetStartingAddress() + ByteIndex;
			FragmentMapRef.FindOrAdd(CurrentAddress) = ByteArray[ByteIndex];
		}
	}

	return UniverseToFragmentMap;
}

TMap<FDMXAttributeName, int32> UDMXControlConsoleFaderGroup::GetAttributeMap() const
{
	TMap<FDMXAttributeName, int32> AttributeMap;

	if (!HasFixturePatch())
	{
		return AttributeMap;
	}

	for (UDMXControlConsoleFaderBase* Fader : GetAllFaders())
	{
		if (!Fader || !Fader->IsEnabled())
		{
			continue;
		}

		const UDMXControlConsoleFixturePatchFunctionFader* FixturePatchFunctionFader = Cast<UDMXControlConsoleFixturePatchFunctionFader>(Fader);
		if (!FixturePatchFunctionFader)
		{
			continue;
		}
		
		const FDMXAttributeName& AttributeName = FixturePatchFunctionFader->GetAttributeName();
		const uint32 Value = FixturePatchFunctionFader->GetValue();
		AttributeMap.Add(AttributeName, Value);
	}

	return AttributeMap;
}

TMap<FIntPoint, TMap<FDMXAttributeName, float>> UDMXControlConsoleFaderGroup::GetMatrixCoordinateToAttributeMap() const
{
	TMap<FIntPoint, TMap<FDMXAttributeName, float>> CoordinateToMatrixAttributeMap;

	if (!HasFixturePatch() || !HasMatrixProperties())
	{
		return CoordinateToMatrixAttributeMap;
	}

	for (const TScriptInterface<IDMXControlConsoleFaderGroupElement>& Element : Elements)
	{
		const UDMXControlConsoleFixturePatchMatrixCell* MatrixCell = Cast<UDMXControlConsoleFixturePatchMatrixCell>(Element.GetObject());
		if (!MatrixCell)
		{
			continue;
		}

		// Get cell coordinates
		const int32 CellX = MatrixCell->GetCellX();
		const int32 CellY = MatrixCell->GetCellY();
		const FIntPoint CellCoordinates(CellX, CellY);
		TMap<FDMXAttributeName, float>& AttributeValueMapRef = CoordinateToMatrixAttributeMap.FindOrAdd(CellCoordinates);

		//Get attribute to value map
		const TArray<UDMXControlConsoleFaderBase*>& MatrixCellFaders = MatrixCell->GetFaders();
		for (UDMXControlConsoleFaderBase* Fader : MatrixCellFaders)
		{
			if (!Fader || !Fader->IsEnabled())
			{
				continue;
			}

			const UDMXControlConsoleFixturePatchCellAttributeFader* CellAttributeFader = CastChecked<UDMXControlConsoleFixturePatchCellAttributeFader>(Fader);
			const FDMXAttributeName& AttributeName = CellAttributeFader->GetAttributeName();

			const EDMXFixtureSignalFormat DataType = CellAttributeFader->GetDataType();
			const uint8 NumChannels = static_cast<uint8>(DataType) + 1;
			const float ValueRange = FMath::Pow(2.f, 8.f * NumChannels) - 1;

			const uint32 Value = CellAttributeFader->GetValue();
			const float RelativeValue = Value / ValueRange;

			AttributeValueMapRef.FindOrAdd(AttributeName) = RelativeValue;
		}
	}

	return CoordinateToMatrixAttributeMap;
}

UDMXControlConsoleFaderGroup* UDMXControlConsoleFaderGroup::Duplicate() const
{
	if (HasFixturePatch())
	{
		return nullptr;
	}

	UDMXControlConsoleFaderGroupRow& FaderGroupRow = GetOwnerFaderGroupRowChecked();
	const int32 Index = GetIndex();

	FaderGroupRow.Modify();
	return FaderGroupRow.DuplicateFaderGroup(Index);
}

void UDMXControlConsoleFaderGroup::Clear()
{
	FaderGroupName = GetName();

	SoftFixturePatchPtr.Reset();
	CachedWeakFixturePatch.Reset();
	ClearElements();

	OnFixturePatchChangedDelegate.Broadcast(this, nullptr);
}

void UDMXControlConsoleFaderGroup::Destroy()
{
	UDMXControlConsoleFaderGroupRow& FaderGroupRow = GetOwnerFaderGroupRowChecked();

#if WITH_EDITOR
	FaderGroupRow.PreEditChange(UDMXControlConsoleFaderGroupRow::StaticClass()->FindPropertyByName(UDMXControlConsoleFaderGroupRow::GetFaderGroupsPropertyName()));
#endif // WITH_EDITOR

	FaderGroupRow.DeleteFaderGroup(this);

#if WITH_EDITOR
	FaderGroupRow.PostEditChange();
#endif // WITH_EDITOR
}

void UDMXControlConsoleFaderGroup::SetEnabled(bool bEnable)
{
	bIsEnabled = bEnable;
}

#if WITH_EDITOR
void UDMXControlConsoleFaderGroup::ShowAllElementsInEditor()
{
	for (const TScriptInterface<IDMXControlConsoleFaderGroupElement>& Element : Elements)
	{
		if (!Element.GetInterface())
		{
			continue;
		}

		Element.GetInterface()->SetIsMatchingFilter(true);

		if (UDMXControlConsoleFixturePatchMatrixCell* MatrixCell = Cast<UDMXControlConsoleFixturePatchMatrixCell>(Element.GetObject()))
		{
			MatrixCell->ShowAllFadersInEditor();
		}
	}
}
#endif

void UDMXControlConsoleFaderGroup::PostInitProperties()
{
	Super::PostInitProperties();

	FaderGroupName = GetName();
}

void UDMXControlConsoleFaderGroup::PostLoad()
{
	Super::PostLoad();

	CachedWeakFaderGroupController = Cast<UDMXControlConsoleControllerBase>(SoftControllerPtr.ToSoftObjectPath().TryLoad());
	if (!CachedWeakFaderGroupController.IsValid())
	{
		Destroy();
		return;
	}

	if (SoftFixturePatchPtr.IsNull())
	{
		return;
	}

	CachedWeakFixturePatch = Cast<UDMXEntityFixturePatch>(SoftFixturePatchPtr.ToSoftObjectPath().TryLoad());
	if (CachedWeakFixturePatch.IsValid())
	{
		SubscribeToFixturePatchDelegates();	
		UpdateFaderGroupFromFixturePatch(CachedWeakFixturePatch.Get());
	}
	else
	{
		Modify();
		Clear();
	}
}

#if WITH_EDITOR
void UDMXControlConsoleFaderGroup::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	const FName PropertyName = PropertyChangedEvent.GetPropertyName();
	if (PropertyName == GET_MEMBER_NAME_CHECKED(UDMXControlConsoleFaderGroup, SoftFixturePatchPtr))
	{
		CachedWeakFixturePatch = Cast<UDMXEntityFixturePatch>(SoftFixturePatchPtr.ToSoftObjectPath().TryLoad());
	}
}
#endif // WITH_EDITOR

void UDMXControlConsoleFaderGroup::OnFixturePatchRemovedFromLibrary(UDMXLibrary* Library, TArray<UDMXEntity*> Entities)
{
	const UDMXEntityFixturePatch* FixturePatch = GetFixturePatch();
	if (!FixturePatch)
	{
		return;
	}

	if (Library != FixturePatch->GetParentLibrary())
	{
		return;
	}

	if (!Entities.Contains(FixturePatch))
	{
		return;
	}

	Modify();
	Clear();
}

void UDMXControlConsoleFaderGroup::OnFixturePatchChanged(const UDMXEntityFixturePatch* InFixturePatch)
{
	UDMXEntityFixturePatch* MyFixturePatch = GetFixturePatch();
	if (!MyFixturePatch ||
		MyFixturePatch != InFixturePatch)
	{
		return;
	}

	UpdateFaderGroupFromFixturePatch(MyFixturePatch);
}

void UDMXControlConsoleFaderGroup::UpdateFaderGroupFromFixturePatch(UDMXEntityFixturePatch* InFixturePatch)
{
	if (!InFixturePatch)
	{
		return;
	}

	UpdateFixturePatchFunctionFaders(InFixturePatch);
	UpdateFixturePatchMatrixCells(InFixturePatch);
	SortElementsByStartingAddress();

	OnFixturePatchChangedDelegate.Broadcast(this, InFixturePatch);
}

void UDMXControlConsoleFaderGroup::UpdateFixturePatchFunctionFaders(UDMXEntityFixturePatch* InFixturePatch)
{
	if (!InFixturePatch || !InFixturePatch->GetActiveMode())
	{
		return;
	}

	const int32 UniverseID = InFixturePatch->GetUniverseID();
	const int32 StartingChannel = InFixturePatch->GetStartingChannel();

	const TMap<FDMXAttributeName, FDMXFixtureFunction> FunctionsMap = InFixturePatch->GetAttributeFunctionsMap();
	// Remove all FixturePatchFunctionFaders which Attribute is no longer in use
	const auto IsAttributeNoLongerInUseLambda = [FunctionsMap](const TScriptInterface<IDMXControlConsoleFaderGroupElement>& Element)
	{
		const UObject* ElementObject = Element.GetObject();
		if (!ElementObject)
		{
			return true;
		}

		const UDMXControlConsoleFixturePatchFunctionFader* FixturePatchFunctionFader = Cast<UDMXControlConsoleFixturePatchFunctionFader>(ElementObject);
		if (FixturePatchFunctionFader)
		{
			const FDMXAttributeName& AttributeName = FixturePatchFunctionFader->GetAttributeName();
			if (!FunctionsMap.Contains(AttributeName))
			{
				return true;
			}
		}

		return false;
	};

	Elements.RemoveAll(IsAttributeNoLongerInUseLambda);

	// Update FixturePatchFunctionFaders which Attributes are already in use and create Faders for new Attributes
	for (const TTuple<FDMXAttributeName, FDMXFixtureFunction>& FunctionTuple : FunctionsMap)
	{
		const FDMXAttributeName& AttributeName = FunctionTuple.Key;
		const FDMXFixtureFunction FixtureFunction = FunctionTuple.Value;

		const auto IsAttributeAlreadyInUseLambda = [AttributeName](const TScriptInterface<IDMXControlConsoleFaderGroupElement>& Element)
		{
			const UObject* ElementObject = Element.GetObject();
			if (!ElementObject)
			{
				return false;
			}

			const UDMXControlConsoleFixturePatchFunctionFader* FixturePatchFunctionFader = Cast<UDMXControlConsoleFixturePatchFunctionFader>(ElementObject);
			if (!FixturePatchFunctionFader)
			{
				return false;
			}

			if (FixturePatchFunctionFader->GetAttributeName() != AttributeName)
			{
				return false;
			}

			return true;
		};

		TScriptInterface<IDMXControlConsoleFaderGroupElement>* MyElement = Algo::FindByPredicate(Elements, IsAttributeAlreadyInUseLambda);
		if (MyElement)
		{
			UObject* MyElementObject = MyElement->GetObject();
			if (MyElementObject)
			{
				UDMXControlConsoleFixturePatchFunctionFader* MyFixturePatchFunctionFader = Cast<UDMXControlConsoleFixturePatchFunctionFader>(MyElementObject);
				if (MyFixturePatchFunctionFader)
				{
					// SetPropertiesFromFixtureFunction gets the the default value from the patch and sets it. 
					// Hence here remember the current value and set it back after setting properties.
					const uint32 Value = MyFixturePatchFunctionFader->GetValue();
					MyFixturePatchFunctionFader->SetPropertiesFromFixtureFunction(FixtureFunction, UniverseID, StartingChannel);
					MyFixturePatchFunctionFader->SetValue(Value);
				}
			}
		}
		else
		{
			UDMXControlConsoleFaderBase* FunctionFader = AddFixturePatchFunctionFader(FixtureFunction, UniverseID, StartingChannel);
		}
	}
}

void UDMXControlConsoleFaderGroup::UpdateFixturePatchMatrixCells(UDMXEntityFixturePatch* InFixturePatch)
{
	if (!InFixturePatch || !InFixturePatch->GetActiveMode())
	{
		return;
	}

	const int32 UniverseID = InFixturePatch->GetUniverseID();
	const int32 StartingChannel = InFixturePatch->GetStartingChannel();

	// Check changes on Fixture Matrices
	FDMXFixtureMatrix FixtureMatrix;
	if (InFixturePatch->GetMatrixProperties(FixtureMatrix))
	{
		TArray<FDMXCell> Cells;
		InFixturePatch->GetAllMatrixCells(Cells);

		// Remove all FixturePatchMatrixCell no longer in use
		const auto IsCellNoLongerInUseLambda = [Cells](const TScriptInterface<IDMXControlConsoleFaderGroupElement>& Element)
		{
			UObject* ElementObject = Element.GetObject();
			if (!ElementObject)
			{
				return true;
			}

			UDMXControlConsoleFixturePatchMatrixCell* FixturePatchMatrixCell = Cast<UDMXControlConsoleFixturePatchMatrixCell>(ElementObject);
			if (FixturePatchMatrixCell)
			{
				const int32 CellID = FixturePatchMatrixCell->GetCellID();

				if (!Cells.ContainsByPredicate([CellID](const FDMXCell& Cell)
					{
						return CellID == Cell.CellID;
					}))
				{
					return true;
				}
			}

			return false;
		};

		Elements.RemoveAll(IsCellNoLongerInUseLambda);

		// Create Elements for new Matrix Cells
		for (const FDMXCell& Cell : Cells)
		{
			const auto IsCellAlreadyInUseLambda = [Cell](const TScriptInterface<IDMXControlConsoleFaderGroupElement>& Element)
			{
				const UObject* ElementObject = Element.GetObject();
				if (!ElementObject)
				{
					return false;
				}

				const UDMXControlConsoleFixturePatchMatrixCell* FixturePatchMatrixCell = Cast<UDMXControlConsoleFixturePatchMatrixCell>(ElementObject);
				if (!FixturePatchMatrixCell)
				{
					return false;
				}

				if (FixturePatchMatrixCell->GetCellID() != Cell.CellID)
				{
					return false;
				}

				return true;
			};

			const TScriptInterface<IDMXControlConsoleFaderGroupElement>* MyElement = Algo::FindByPredicate(Elements, IsCellAlreadyInUseLambda);
			if (MyElement)
			{
				continue;
			}

			UDMXControlConsoleFixturePatchMatrixCell* MatrixCell = AddFixturePatchMatrixCell(Cell, UniverseID, StartingChannel);
		}
	}
	else
	{
		const auto IsMatrixCellLambda = [](const TScriptInterface<IDMXControlConsoleFaderGroupElement>& Element)
			{
				UObject* ElementObject = Element.GetObject();
				if (!ElementObject)
				{
					return false;
				}

				UDMXControlConsoleFixturePatchMatrixCell* FixturePatchMatrixCell = Cast<UDMXControlConsoleFixturePatchMatrixCell>(ElementObject);
				if (!FixturePatchMatrixCell)
				{
					return false;
				}

				return true;
			};

		Elements.RemoveAll(IsMatrixCellLambda);
	}
}

void UDMXControlConsoleFaderGroup::SubscribeToFixturePatchDelegates()
{
	if (!UDMXLibrary::GetOnEntitiesRemoved().IsBoundToObject(this))
	{
		UDMXLibrary::GetOnEntitiesRemoved().AddUObject(this, &UDMXControlConsoleFaderGroup::OnFixturePatchRemovedFromLibrary);
	}

	if (!UDMXEntityFixturePatch::GetOnFixturePatchChanged().IsBoundToObject(this))
	{
		UDMXEntityFixturePatch::GetOnFixturePatchChanged().AddUObject(this, &UDMXControlConsoleFaderGroup::OnFixturePatchChanged);
	}
}

void UDMXControlConsoleFaderGroup::GetNextAvailableUniverseAndAddress(int32& OutUniverse, int32& OutAddress) const
{
	if (Elements.IsEmpty())
	{
		OutUniverse = 1;
		OutAddress = 1;
	}
	else
	{
		const UDMXControlConsoleRawFader* LastFader = Cast<UDMXControlConsoleRawFader>(Elements.Last().GetObject());
		if (LastFader)
		{
			OutAddress = LastFader->GetEndingAddress() + 1;
			OutUniverse = LastFader->GetUniverseID();
			if (OutAddress > DMX_MAX_ADDRESS)
			{
				OutAddress = 1;
				OutUniverse++;
			}
		}
	}
}

#undef LOCTEXT_NAMESPACE
