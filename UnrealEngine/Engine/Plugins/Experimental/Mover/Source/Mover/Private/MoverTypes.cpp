// Copyright Epic Games, Inc. All Rights Reserved.


#include "MoverTypes.h"
#include "Blueprint/BlueprintExceptionInfo.h"
#include "MoverLog.h"

#define LOCTEXT_NAMESPACE "MoverData"

FMoverOnImpactParams::FMoverOnImpactParams() 
	: AttemptedMoveDelta(0) 
{
}

FMoverOnImpactParams::FMoverOnImpactParams(const FName& ModeName, const FHitResult& Hit, const FVector& Delta)
	: MovementModeName(ModeName)
	, HitResult(Hit)
	, AttemptedMoveDelta(Delta)
{
}

FMoverDataStructBase::FMoverDataStructBase()
{
}

FMoverDataStructBase* FMoverDataStructBase::Clone() const
{
	// If child classes don't override this, collections will not work
	checkf(false, TEXT("FMoverDataStructBase::Clone() being called erroneously. This should always be overridden in derived types!"));
	return nullptr;
}

UScriptStruct* FMoverDataStructBase::GetScriptStruct() const
{
	checkf(false, TEXT("FMoverDataStructBase::GetScriptStruct() being called erroneously. This should always be overridden in derived types!"));
	return FMoverDataStructBase::StaticStruct();
}


bool FMoverDataStructBase::ShouldReconcile(const FMoverDataStructBase& AuthorityState) const
{
	checkf(false, TEXT("FMoverDataStructBase::ShouldReconcile being called erroneously. This should always be overridden in derived types that comprise STATE data (sync/aux)!"));
	return false;
}

void FMoverDataStructBase::Interpolate(const FMoverDataStructBase& From, const FMoverDataStructBase& To, float Pct)
{
	checkf(false, TEXT("FMoverDataStructBase::Interpolate being called erroneously. This should always be overridden in derived types that comprise STATE data (sync/aux)!"));
}




FMoverDataCollection::FMoverDataCollection()
{
}

bool FMoverDataCollection::NetSerialize(FArchive& Ar, UPackageMap* Map, bool& bOutSuccess)
{
	NetSerializeDataArray(Ar, DataArray);

	if (Ar.IsError())
	{
		bOutSuccess = false;
		return false;
	}

	bOutSuccess = true;
	return true;
}



FMoverDataCollection& FMoverDataCollection::operator=(const FMoverDataCollection& Other)
{
	// Perform deep copy of this Group
	if (this != &Other)
	{
		// Deep copy active data blocks
		DataArray.Empty(Other.DataArray.Num());
		for (int i = 0; i < Other.DataArray.Num(); ++i)
		{
			if (Other.DataArray[i].IsValid())
			{
				FMoverDataStructBase* CopyOfSourcePtr = Other.DataArray[i]->Clone();
				DataArray.Add(TSharedPtr<FMoverDataStructBase>(CopyOfSourcePtr));
			}
			else
			{
				UE_LOG(LogMover, Warning, TEXT("FMoverDataCollection::operator= trying to copy invalid Other DataArray element"));
			}
		}
	}

	return *this;
}

bool FMoverDataCollection::operator==(const FMoverDataCollection& Other) const
{
	// Deep move-by-move comparison
	if (DataArray.Num() != Other.DataArray.Num())
	{
		return false;
	}

	for (int32 i = 0; i < DataArray.Num(); ++i)
	{
		if (DataArray[i].IsValid() == Other.DataArray[i].IsValid())
		{
			if (DataArray[i].IsValid())
			{
				// TODO: Implement deep equality checks
				// 				if (!DataArray[i]->MatchesAndHasSameState(Other.DataArray[i].Get()))
				// 				{
				// 					return false; // They're valid and don't match/have same state
				// 				}
			}
		}
		else
		{
			return false; // Mismatch in validity
		}
	}

	return true;
}

bool FMoverDataCollection::operator!=(const FMoverDataCollection& Other) const
{
	return !(FMoverDataCollection::operator==(Other));
}


bool FMoverDataCollection::ShouldReconcile(const FMoverDataCollection& Other) const
{
	// Collections must have matching elements, and those elements are piece-wise tested for needing reconciliation
	if (DataArray.Num() != Other.DataArray.Num())
	{
		return true;
	}

	for (int32 i = 0; i < DataArray.Num(); ++i)
	{
		const FMoverDataStructBase* DataElement = DataArray[i].Get();
		const FMoverDataStructBase* OtherDataElement = Other.FindDataByType(DataElement->GetScriptStruct());
		
		if (OtherDataElement == nullptr || 
		    DataElement->ShouldReconcile(*OtherDataElement))
		{
			return true;
		}
	}

	return false;
}

void FMoverDataCollection::Interpolate(const FMoverDataCollection& From, const FMoverDataCollection& To, float Pct)
{
	// Piece-wise interpolation of matching data blocks
	for (int32 i = 0; i < From.DataArray.Num(); ++i)
	{
		const FMoverDataStructBase* FromElement = From.DataArray[i].Get();
		
		if (const FMoverDataStructBase* ToElement = To.FindDataByType(FromElement->GetScriptStruct()))
		{
			FMoverDataStructBase* InterpElement = FindOrAddDataByType(FromElement->GetScriptStruct());
			InterpElement->Interpolate(*FromElement, *ToElement, Pct);
		}
	}

	// TODO: What if From or To have any block types that are unique to them?


}


void FMoverDataCollection::AddStructReferencedObjects(FReferenceCollector& Collector) const
{
	for (const TSharedPtr<FMoverDataStructBase>& Data : DataArray)
	{
		if (Data.IsValid())
		{
			Data->AddReferencedObjects(Collector);
		}
	}
}

void FMoverDataCollection::ToString(FAnsiStringBuilderBase& Out) const
{
	for (const TSharedPtr<FMoverDataStructBase>& Data : DataArray)
	{
		if (Data.IsValid())
		{
			UScriptStruct* Struct = Data->GetScriptStruct();
			Out.Appendf("\n[%s]\n", TCHAR_TO_ANSI(*Struct->GetName()));
			Data->ToString(Out);
		}
	}
}

struct FMoverDataDeleter
{
	FORCEINLINE void operator()(FMoverDataStructBase* Object) const
	{
		check(Object);
		UScriptStruct* ScriptStruct = Object->GetScriptStruct();
		check(ScriptStruct);
		ScriptStruct->DestroyStruct(Object);
		FMemory::Free(Object);
	}
};

//static 
TSharedPtr<FMoverDataStructBase> FMoverDataCollection::CreateDataByType(const UScriptStruct* DataStructType)
{
	FMoverDataStructBase* NewDataBlock = (FMoverDataStructBase*)FMemory::Malloc(DataStructType->GetCppStructOps()->GetSize());
	DataStructType->InitializeStruct(NewDataBlock);

	return TSharedPtr<FMoverDataStructBase>(NewDataBlock, FMoverDataDeleter());
}


FMoverDataStructBase* FMoverDataCollection::AddDataByType(const UScriptStruct* DataStructType)
{
	if (ensure(!FindDataByType(DataStructType)))
	{
		TSharedPtr<FMoverDataStructBase> NewDataInstance = CreateDataByType(DataStructType);
		AddOrOverwriteData(NewDataInstance);
		return NewDataInstance.Get();
	}
	
	return nullptr;
}


void FMoverDataCollection::AddOrOverwriteData(const TSharedPtr<FMoverDataStructBase> DataInstance)
{
	RemoveDataByType(DataInstance->GetScriptStruct());
	DataArray.Add(DataInstance);
}


FMoverDataStructBase* FMoverDataCollection::FindDataByType(const UScriptStruct* DataStructType) const
{
	for (const TSharedPtr<FMoverDataStructBase>& Data : DataArray)
	{
		UStruct* CandidateStruct = Data->GetScriptStruct();
		while (CandidateStruct)
		{
			if (DataStructType == CandidateStruct)
			{
				return Data.Get();
			}

			CandidateStruct = CandidateStruct->GetSuperStruct();
		}
	}

	return nullptr;
}


FMoverDataStructBase* FMoverDataCollection::FindOrAddDataByType(const UScriptStruct* DataStructType)
{
	if (FMoverDataStructBase* ExistingData = FindDataByType(DataStructType))
	{
		return ExistingData;
	}

	return AddDataByType(DataStructType);
}


bool FMoverDataCollection::RemoveDataByType(const UScriptStruct* DataStructType)
{
	int32 IndexToRemove = -1;

	for (int32 i=0; i < DataArray.Num() && IndexToRemove < 0; ++i)
	{
		UStruct* CandidateStruct = DataArray[i]->GetScriptStruct();
		while (CandidateStruct)
		{
			if (DataStructType == CandidateStruct)
			{
				IndexToRemove = i;
				break;
			}

			CandidateStruct = CandidateStruct->GetSuperStruct();
		}
	}

	if (IndexToRemove >= 0)
	{
		DataArray.RemoveAt(IndexToRemove);
		return true;
	}

	return false;
}

/*static*/
void FMoverDataCollection::NetSerializeDataArray(FArchive& Ar, TArray<TSharedPtr<FMoverDataStructBase>>& DataArray)
{
	uint8 NumDataStructsToSerialize;
	if (Ar.IsSaving())
	{
		NumDataStructsToSerialize = DataArray.Num();
	}

	Ar << NumDataStructsToSerialize;

	if (Ar.IsLoading())
	{
		DataArray.SetNumZeroed(NumDataStructsToSerialize);
	}

	for (int32 i = 0; i < NumDataStructsToSerialize && !Ar.IsError(); ++i)
	{
		TCheckedObjPtr<UScriptStruct> ScriptStruct = DataArray[i].IsValid() ? DataArray[i]->GetScriptStruct() : nullptr;
		UScriptStruct* ScriptStructLocal = ScriptStruct.Get();

		Ar << ScriptStruct;

		if (ScriptStruct.IsValid())
		{
			// Restrict replication to derived classes of FMoverDataStructBase for security reasons:
			// If FMoverDataCollection is replicated through a Server RPC, we need to prevent clients from sending us
			// arbitrary ScriptStructs due to the allocation/reliance on GetCppStructOps below which could trigger a server crash
			// for invalid structs. All provided sources are direct children of FMoverDataStructBase and we never expect to have deep hierarchies
			// so this should not be too costly
			bool bIsDerivedFromBase = false;
			UStruct* CurrentSuperStruct = ScriptStruct->GetSuperStruct();
			while (CurrentSuperStruct)
			{
				if (CurrentSuperStruct == FMoverDataStructBase::StaticStruct())
				{
					bIsDerivedFromBase = true;
					break;
				}
				CurrentSuperStruct = CurrentSuperStruct->GetSuperStruct();
			}

			if (bIsDerivedFromBase)
			{
				if (Ar.IsLoading())
				{
					if (DataArray[i].IsValid() && ScriptStructLocal == ScriptStruct.Get())
					{
						// What we have locally is the same type as we're being serialized into, so we don't need to
						// reallocate - just use existing structure
					}
					else
					{
						// For now, just reset/reallocate the data when loading.
						// Longer term if we want to generalize this and use it for property replication, we should support
						// only reallocating when necessary
						FMoverDataStructBase* NewDataBlock = (FMoverDataStructBase*)FMemory::Malloc(ScriptStruct->GetCppStructOps()->GetSize());
						ScriptStruct->InitializeStruct(NewDataBlock);

						DataArray[i] = TSharedPtr<FMoverDataStructBase>(NewDataBlock, FMoverDataDeleter());
					}
				}

				bool bIgnoredSuccess = false;
				DataArray[i]->NetSerialize(Ar, nullptr, bIgnoredSuccess);
			}
			else
			{
				UE_LOG(LogMover, Error, TEXT("FMoverDataCollection::NetSerialize: ScriptStruct not derived from FMoverDataStructBase attempted to serialize."));
				Ar.SetError();
				break;
			}
		}
		else if (ScriptStruct.IsError())
		{
			UE_LOG(LogMover, Error, TEXT("FMoverDataCollection::NetSerialize: Invalid ScriptStruct serialized."));
			Ar.SetError();
			break;
		}
	}

}



void UMoverDataCollectionLibrary::K2_AddDataToCollection(FMoverDataCollection& Collection, const int32& SourceAsRawBytes)
{
	// This will never be called, the exec version below will be hit instead
	checkNoEntry();
}

// static
DEFINE_FUNCTION(UMoverDataCollectionLibrary::execK2_AddDataToCollection)
{
	P_GET_STRUCT_REF(FMoverDataCollection, TargetCollection);

	Stack.MostRecentPropertyAddress = nullptr;
	Stack.MostRecentPropertyContainer = nullptr;
	Stack.StepCompiledIn<FStructProperty>(nullptr);

	void* SourceDataAsRawPtr = Stack.MostRecentPropertyAddress;
	FStructProperty* SourceStructProp = CastField<FStructProperty>(Stack.MostRecentProperty);

	P_FINISH;

	if (!SourceDataAsRawPtr || !SourceStructProp)
	{
		FBlueprintExceptionInfo ExceptionInfo(
			EBlueprintExceptionType::AbortExecution,
			LOCTEXT("MoverDataCollection_AddDataToCollection", "Failed to resolve the SourceAsRawBytes for AddDataToCollection")
		);

		FBlueprintCoreDelegates::ThrowScriptException(P_THIS, Stack, ExceptionInfo);
	}
	else
	{
		P_NATIVE_BEGIN;

		if (ensure(SourceStructProp && SourceStructProp->Struct && SourceStructProp->Struct->IsChildOf(FMoverDataStructBase::StaticStruct()) && SourceDataAsRawPtr))
		{
			FMoverDataStructBase* SourceDataAsBasePtr = reinterpret_cast<FMoverDataStructBase*>(SourceDataAsRawPtr);
			FMoverDataStructBase* ClonedData = SourceDataAsBasePtr->Clone();

			TargetCollection.AddOrOverwriteData( TSharedPtr<FMoverDataStructBase>(ClonedData) );
		}

		P_NATIVE_END;
	}
}


void UMoverDataCollectionLibrary::K2_GetDataFromCollection(bool& DidSucceed, const FMoverDataCollection& Collection, int32& TargetAsRawBytes)
{
	// This will never be called, the exec version below will be hit instead
	checkNoEntry();
}

// static
DEFINE_FUNCTION(UMoverDataCollectionLibrary::execK2_GetDataFromCollection)
{
	P_GET_UBOOL_REF(DidSucceed);
	P_GET_STRUCT_REF(FMoverDataCollection, TargetCollection);

	Stack.MostRecentPropertyAddress = nullptr;
	Stack.MostRecentPropertyContainer = nullptr;
	Stack.StepCompiledIn<FStructProperty>(nullptr);

	void* TargetDataAsRawPtr = Stack.MostRecentPropertyAddress;
	FStructProperty* TargetStructProp = CastField<FStructProperty>(Stack.MostRecentProperty);

	P_FINISH;

	DidSucceed = false;

	if (!TargetDataAsRawPtr || !TargetStructProp)
	{
		FBlueprintExceptionInfo ExceptionInfo(
			EBlueprintExceptionType::AbortExecution,
			LOCTEXT("MoverDataCollection_GetDataFromCollection_UnresolvedTarget", "Failed to resolve the TargetAsRawBytes for GetDataFromCollection")
		);

		FBlueprintCoreDelegates::ThrowScriptException(P_THIS, Stack, ExceptionInfo);
	}
	else if (!TargetStructProp->Struct || !TargetStructProp->Struct->IsChildOf(FMoverDataStructBase::StaticStruct()))
	{
		FBlueprintExceptionInfo ExceptionInfo(
			EBlueprintExceptionType::AbortExecution,
			LOCTEXT("MoverDataCollection_GetDataFromCollection_BadType", "TargetAsRawBytes is not a valid type. Must be a child of FMoverDataStructBase.")
		);

		FBlueprintCoreDelegates::ThrowScriptException(P_THIS, Stack, ExceptionInfo);
	}
	else
	{
		P_NATIVE_BEGIN;

		if (FMoverDataStructBase* FoundDataInstance = TargetCollection.FindDataByType(TargetStructProp->Struct))
		{
			TargetStructProp->Struct->CopyScriptStruct(TargetDataAsRawPtr, FoundDataInstance);
			DidSucceed = true;
		}

		P_NATIVE_END;
	}
}


void UMoverDataCollectionLibrary::ClearDataFromCollection(FMoverDataCollection& Collection)
{
	Collection.Empty();
}

#undef LOCTEXT_NAMESPACE