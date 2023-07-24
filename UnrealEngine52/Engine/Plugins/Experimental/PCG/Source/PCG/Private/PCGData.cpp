// Copyright Epic Games, Inc. All Rights Reserved.

#include "PCGData.h"

#include "PCGNode.h"
#include "PCGParamData.h"
#include "PCGPin.h"
#include "PCGSettings.h"
#include "Data/PCGPointData.h"
#include "Data/PCGSpatialData.h"
#include "Data/PCGUnionData.h"

#include "HAL/IConsoleManager.h"
#include "Serialization/ArchiveCrc32.h"
#include "UObject/Package.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(PCGData)

static TAutoConsoleVariable<bool> CVarCachePropagateCrcThroughBooleanData(
	TEXT("pcg.Cache.PropagateCrcThroughBooleanData"),
	false,
	TEXT("Whether intersection, union, difference combine Crc values from operands. If false they fall back to using data UID."));

void FPCGRootSet::Clear()
{
	for (TPair<UObject*, int32>& Entry : RootSet)
	{
		Entry.Key->RemoveFromRoot();
	}

	RootSet.Reset();
}

void FPCGRootSet::Add(UObject* InObject)
{
	check(InObject);
	AddInternal(InObject);
}

void FPCGRootSet::AddInternal(UObject* InObject)
{
	check(InObject && !InObject->IsA<UPackage>());

	if (int32* Found = RootSet.Find(InObject))
	{
		(*Found)++;
	}
	else if (!InObject->IsRooted() && InObject->GetPackage() == GetTransientPackage())
	{
		InObject->AddToRoot();
		RootSet.Emplace(InObject, 1);
	}
}

void FPCGRootSet::Remove(UObject* InObject)
{
	if (!InObject)
	{
		UE_LOG(LogPCG, Warning, TEXT("Trying to remove a null object from the rootset"));
		return;
	}

	RemoveInternal(InObject);
}

void FPCGRootSet::RemoveInternal(UObject* InObject)
{
	check(InObject && !InObject->IsA<UPackage>());

	if (int32* Found = RootSet.Find(InObject))
	{
		check(InObject->IsRooted());
		(*Found)--;

		if (*Found == 0)
		{
			InObject->RemoveFromRoot();
			RootSet.Remove(InObject);
		}
	}
}

UPCGData::UPCGData(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	if (!HasAnyFlags(RF_ClassDefaultObject))
	{
		UID = ++UIDCounter;
	}
}

FPCGCrc UPCGData::GetOrComputeCrc(bool bFullDataCrc) const
{
	if (Crc.IsValid())
	{
		return Crc;
	}

	Crc = ComputeCrc(bFullDataCrc);

	return Crc;
}

FPCGCrc UPCGData::ComputeCrc(bool bFullDataCrc) const
{
	FArchiveCrc32 Ar;
	AddToCrc(Ar, bFullDataCrc);

	return FPCGCrc(Ar.GetCrc());
}

void UPCGData::AddToCrc(FArchiveCrc32& Ar, bool bFullDataCrc) const
{
	// Fallback implementation uses UID to ensure every object returns a different Crc.
	uint64 UIDValue = UID;
	Ar << UIDValue;
}

bool UPCGData::PropagateCrcThroughBooleanData() const
{
	return CVarCachePropagateCrcThroughBooleanData.GetValueOnAnyThread();
}

void UPCGData::VisitDataNetwork(TFunctionRef<void(const UPCGData*)> Action) const
{
	Action(this);
}

bool FPCGTaggedData::operator==(const FPCGTaggedData& Other) const
{
	return Data == Other.Data &&
		Tags.Num() == Other.Tags.Num() &&
		Tags.Includes(Other.Tags) &&
		Pin == Other.Pin;
}

bool FPCGTaggedData::operator!=(const FPCGTaggedData& Other) const
{
	return !operator==(Other);
}

TArray<FPCGTaggedData> FPCGDataCollection::GetInputs() const
{
	return TaggedData.FilterByPredicate([](const FPCGTaggedData& Data) {
		return Cast<UPCGSpatialData>(Data.Data) != nullptr;
		});
}

TArray<FPCGTaggedData> FPCGDataCollection::GetInputsByPin(const FName& InPinLabel) const
{
	return TaggedData.FilterByPredicate([&InPinLabel](const FPCGTaggedData& Data) {
		return Data.Pin == InPinLabel;
		});
}

const UPCGSpatialData* FPCGDataCollection::GetSpatialUnionOfInputsByPin(const FName& InPinLabel, bool& bOutUnionDataCreated) const
{
	TArray<FPCGTaggedData> SpatialDataOnPin = TaggedData.FilterByPredicate([&InPinLabel](const FPCGTaggedData& Data) {
		return Data.Pin == InPinLabel && Data.Data && Data.Data->IsA<UPCGSpatialData>();
	});

	const UPCGSpatialData* Result = nullptr;
	UPCGUnionData* Union = nullptr;
	for (const FPCGTaggedData& Data : SpatialDataOnPin)
	{
		const UPCGSpatialData* SpatialInput = Data.Data ? Cast<const UPCGSpatialData>(Data.Data) : nullptr;
		if (!ensure(SpatialInput))
		{
			continue;
		}

		if (!Result)
		{
			// First valid data
			Result = SpatialInput;
		}
		else
		{
			if (!Union)
			{
				// Second valid data - set up union
				Union = NewObject<UPCGUnionData>();
				Union->Initialize(Result, SpatialInput);

				// Make result union
				Result = Union;
			}
			else
			{
				// Nth valid data
				Union->AddData(SpatialInput);
			}
		}
	}

	// Indicate whether we created a union data
	bOutUnionDataCreated = !!Union;

	return Result;
}

TArray<FPCGTaggedData> FPCGDataCollection::GetTaggedInputs(const FString& InTag) const
{
	return GetTaggedTypedInputs<UPCGSpatialData>(InTag);
}

TArray<FPCGTaggedData> FPCGDataCollection::GetAllSettings() const
{
	return TaggedData.FilterByPredicate([](const FPCGTaggedData& Data) {
		return Cast<UPCGSettings>(Data.Data) != nullptr;
		});
}

TArray<FPCGTaggedData> FPCGDataCollection::GetAllParams() const
{
	return TaggedData.FilterByPredicate([](const FPCGTaggedData& Data) {
		return Cast<UPCGParamData>(Data.Data) != nullptr;
	});
}

TArray<FPCGTaggedData> FPCGDataCollection::GetParamsByPin(const FName& InPinLabel) const
{
	return TaggedData.FilterByPredicate([&InPinLabel](const FPCGTaggedData& Data) {
		return Data.Pin == InPinLabel && Cast<UPCGParamData>(Data.Data);
		});
}

TArray<FPCGTaggedData> FPCGDataCollection::GetTaggedParams(const FString& InTag) const
{
	return TaggedData.FilterByPredicate([&InTag](const FPCGTaggedData& Data) {
		return Data.Tags.Contains(InTag) && Cast<UPCGParamData>(Data.Data) != nullptr;
		});
}

UPCGParamData* FPCGDataCollection::GetParamsWithDeprecation(const UPCGNode* Node) const
{
	// First try with the param pin
	if (UPCGParamData* ParamData = GetFirstParamsOnParamsPin())
	{
		return ParamData;
	}

	// If there is nothing on the params pin, temporarily supports reading param from the input pin, to avoid breaking graphs with nodes that doesn't have a param pin
	// Log a warning too
	if (Node)
	{
		for (const UPCGPin* InputPin : Node->GetInputPins())
		{
			// To avoid matching the individual parameters pins added by the override system, we'll ignore
			// any input pin that only accepts params.
			if (InputPin->Properties.AllowedTypes == EPCGDataType::Param)
			{
				continue;
			}

			for (const FPCGTaggedData& TaggedDatum : GetParamsByPin(InputPin->Properties.Label))
			{
				if (UPCGParamData* Params = Cast<UPCGParamData>(TaggedDatum.Data))
				{
					UE_LOG(LogPCG, Warning, TEXT("[%s] Found an Attribute Set data on an input pin that should not accept attributes. Make sure to re-wire it to the Overrides pin if it is used for overrides."), *Node->GetNodeTitle().ToString());
					return Params;
				}
			}
		}
	}

	return nullptr;
}

UPCGParamData* FPCGDataCollection::GetParams() const
{
	for (const FPCGTaggedData& TaggedDatum : TaggedData)
	{
		if (UPCGParamData* Params = Cast<UPCGParamData>(TaggedDatum.Data))
		{
			return Params; 
		}
	}

	return nullptr;
}

UPCGParamData* FPCGDataCollection::GetFirstParamsOnParamsPin() const
{
	TArray<FPCGTaggedData> ParamsOnDefaultPin = GetParamsByPin(PCGPinConstants::DefaultParamsLabel);
	return (ParamsOnDefaultPin.IsEmpty() ? nullptr : Cast<UPCGParamData>(ParamsOnDefaultPin[0].Data));
}

void FPCGDataCollection::GetResourceSizeEx(FResourceSizeEx& CumulativeResourceSize) const
{
	for (const FPCGTaggedData& Data : TaggedData)
	{
		if (Data.Data)
		{
			Data.Data->VisitDataNetwork([&CumulativeResourceSize](const UPCGData* Data) {
				// Cast away const-ness. The extended mode of GetResourceSizeEx accounts memory for all objects outer'd
				// to this object, and that calls GetObjectsWithOuter which is non-const. We don't use this extended mode
				// and we need to be able to operate on const objects.
				const_cast<UPCGData*>(Data)->GetResourceSizeEx(CumulativeResourceSize);
			});
		}
	}
}

const UPCGSettings* FPCGDataCollection::GetSettings(const UPCGSettings* InDefaultSettings) const
{
	if (!InDefaultSettings)
	{
		return GetSettings<UPCGSettings>();
	}
	else
	{
		const FPCGTaggedData* MatchingData = TaggedData.FindByPredicate([InDefaultSettings](const FPCGTaggedData& Data) {
			return Data.Data &&
				(Data.Data->GetClass() == InDefaultSettings->GetClass() ||
					Data.Data->GetClass()->IsChildOf(InDefaultSettings->GetClass()));
			});

		return MatchingData ? Cast<const UPCGSettings>(MatchingData->Data) : InDefaultSettings;
	}
}

const UPCGSettingsInterface* FPCGDataCollection::GetSettingsInterface() const
{
	return GetSettings<UPCGSettingsInterface>();
}

const UPCGSettingsInterface* FPCGDataCollection::GetSettingsInterface(const UPCGSettingsInterface* InDefaultSettingsInterface) const
{
	if (!InDefaultSettingsInterface || InDefaultSettingsInterface->GetSettings() == nullptr)
	{
		return GetSettingsInterface();
	}
	else
	{
		const FPCGTaggedData* MatchingData = TaggedData.FindByPredicate([InDefaultSettingsInterface](const FPCGTaggedData& Data) {
			if (UPCGSettingsInterface* DataSettingsInterface = Cast<UPCGSettingsInterface>(Data.Data))
			{
				// Compare settings classes
				return DataSettingsInterface->GetSettings()->GetClass() == InDefaultSettingsInterface->GetSettings()->GetClass() ||
					DataSettingsInterface->GetSettings()->GetClass()->IsChildOf(InDefaultSettingsInterface->GetSettings()->GetClass());
			}

			return false;
		});

		return MatchingData ? Cast<const UPCGSettingsInterface>(MatchingData->Data) : InDefaultSettingsInterface;
	}
}

bool FPCGDataCollection::operator==(const FPCGDataCollection& Other) const
{
	if (bCancelExecution != Other.bCancelExecution || TaggedData.Num() != Other.TaggedData.Num())
	{
		return false;
	}

	// TODO: Once we make the arguments order irrelevant, then this should be updated
	for (int32 DataIndex = 0; DataIndex < TaggedData.Num(); ++DataIndex)
	{
		if (TaggedData[DataIndex] != Other.TaggedData[DataIndex])
		{
			return false;
		}
	}
	
	return true;
}

bool FPCGDataCollection::operator!=(const FPCGDataCollection& Other) const
{
	return !operator==(Other);
}

void FPCGDataCollection::AddToRootSet(FPCGRootSet& RootSet) const
{
	for (const FPCGTaggedData& Data : TaggedData)
	{
		if (Data.Data)
		{
			// This is technically a const_cast
			RootSet.Add(Cast<UObject>(Data.Data));
		}
	}
}

void FPCGDataCollection::RemoveFromRootSet(FPCGRootSet& RootSet) const
{
	for (const FPCGTaggedData& Data : TaggedData)
	{
		if (Data.Data)
		{
			// This is technically a const_cast
			RootSet.Remove(Cast<UObject>(Data.Data));
		}
	}
}

FPCGCrc FPCGDataCollection::ComputeCrc(bool bFullDataCrc)
{
	// If there is no data, will return valid Crc==0 which is fine. No such thing as an invalid FPCGDataCollection
	FArchiveCrc32 Ar;

	for (FPCGTaggedData& Data : TaggedData)
	{
		Ar << Data.Pin;

		if (Data.Data)
		{
			const FPCGCrc DataCrc = Data.Data->GetOrComputeCrc(bFullDataCrc);
			uint32 Result = DataCrc.GetValue();
			Ar << Result;
		}

		Ar << Data.Tags;
	}

	return FPCGCrc(Ar.GetCrc());
}

void FPCGDataCollection::Reset()
{
	// Implementation note: We are assuming that there is no need to remove the data from the rootset here.
	TaggedData.Reset();
	bCancelExecutionOnEmpty = false;
	bCancelExecution = false;
}

TArray<FPCGTaggedData> UPCGDataFunctionLibrary::GetInputs(const FPCGDataCollection& InCollection)
{
	return InCollection.GetInputs();
}

TArray<FPCGTaggedData> UPCGDataFunctionLibrary::GetInputsByPin(const FPCGDataCollection& InCollection, const FName& InPinLabel)
{
	return InCollection.GetInputsByPin(InPinLabel);
}

TArray<FPCGTaggedData> UPCGDataFunctionLibrary::GetTaggedInputs(const FPCGDataCollection& InCollection, const FString& InTag)
{
	return InCollection.GetTaggedInputs(InTag);
}

TArray<FPCGTaggedData> UPCGDataFunctionLibrary::GetParams(const FPCGDataCollection& InCollection)
{
	return InCollection.GetAllParams();
}

TArray<FPCGTaggedData> UPCGDataFunctionLibrary::GetParamsByPin(const FPCGDataCollection& InCollection, const FName& InPinLabel)
{
	return InCollection.GetParamsByPin(InPinLabel);
}

TArray<FPCGTaggedData> UPCGDataFunctionLibrary::GetTaggedParams(const FPCGDataCollection& InCollection, const FString& InTag)
{
	return InCollection.GetTaggedParams(InTag);
}

TArray<FPCGTaggedData> UPCGDataFunctionLibrary::GetAllSettings(const FPCGDataCollection& InCollection)
{
	return InCollection.GetAllSettings();
}
