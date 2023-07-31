// Copyright Epic Games, Inc. All Rights Reserved.

#include "PCGData.h"
#include "PCGSettings.h"
#include "PCGParamData.h"
#include "Data/PCGSpatialData.h"

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
	if (int32* Found = RootSet.Find(InObject))
	{
		(*Found)++;
	}
	else if(!InObject->IsRooted())
	{
		InObject->AddToRoot();
		RootSet.Emplace(InObject, 1);
	}
}
void FPCGRootSet::Remove(UObject* InObject)
{
	check(InObject);
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

TArray<FPCGTaggedData> FPCGDataCollection::GetTaggedInputs(const FString& InTag) const
{
	return TaggedData.FilterByPredicate([&InTag](const FPCGTaggedData& Data) {
		return Data.Tags.Contains(InTag) && Cast<UPCGSpatialData>(Data.Data);
		});
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