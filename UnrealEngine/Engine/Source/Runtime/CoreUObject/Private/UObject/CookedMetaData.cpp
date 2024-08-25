// Copyright Epic Games, Inc. All Rights Reserved.

#include "UObject/CookedMetaData.h"
#include "UObject/Package.h"
#include "UObject/MetaData.h"
#include "UObject/UnrealType.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(CookedMetaData)

namespace CookedMetaDataUtil::Internal
{

void PrepareCookedMetaDataForPurge(UObject* CookedMetaDataPtr)
{
	// Skip the rename for cooked packages, as IO store cannot currently handle renames
	if (!CookedMetaDataPtr->GetPackage()->HasAnyPackageFlags(PKG_Cooked))
	{
		FNameBuilder BaseMetaDataName(CookedMetaDataPtr->GetFName());
		BaseMetaDataName << TEXT("_PURGED");
		CookedMetaDataPtr->Rename(FNameBuilder(MakeUniqueObjectName(CookedMetaDataPtr->GetOuter(), CookedMetaDataPtr->GetClass(), FName(BaseMetaDataName))).ToString(), nullptr, REN_DoNotDirty | REN_DontCreateRedirectors | REN_ForceNoResetLoaders | REN_NonTransactional);
	}

	CookedMetaDataPtr->ClearFlags(RF_Standalone | RF_Public);
}

template <typename CookedMetaDataOuterType, typename CookedMetaDataType>
void PostLoadCookedMetaData(CookedMetaDataType* CookedMetaDataPtr)
{
#if WITH_EDITORONLY_DATA
	checkf(CookedMetaDataPtr->GetPackage()->HasAnyPackageFlags(PKG_Cooked), TEXT("Cooked meta-data should only be loaded for a cooked package!"));

	if (CookedMetaDataOuterType* Owner = CastChecked<CookedMetaDataOuterType>(CookedMetaDataPtr->GetOuter()))
	{
		Owner->ConditionalPostLoad();
		CookedMetaDataPtr->ApplyMetaData(Owner);
		PrepareCookedMetaDataForPurge(CookedMetaDataPtr);
	}
#endif
}

} // namespace CookedMetaDataUtil::Internal


bool FObjectCookedMetaDataStore::HasMetaData() const
{
	return ObjectMetaData.Num() > 0;
}

void FObjectCookedMetaDataStore::CacheMetaData(const UObject* SourceObject)
{
	ObjectMetaData.Reset();

#if WITH_EDITORONLY_DATA
	if (UPackage* SourcePackage = SourceObject->GetPackage())
	{
		if (const UMetaData* SourceMetaData = SourcePackage->GetMetaData())
		{
			if (const TMap<FName, FString>* SourceObjectMetaData = SourceMetaData->ObjectMetaDataMap.Find(SourceObject))
			{
				ObjectMetaData = *SourceObjectMetaData;
			}
		}
	}
#endif
}

void FObjectCookedMetaDataStore::ApplyMetaData(UObject* TargetObject) const
{
#if WITH_EDITORONLY_DATA
	if (UPackage* TargetPackage = TargetObject->GetPackage())
	{
		if (UMetaData* TargetMetaData = TargetPackage->GetMetaData())
		{
			TargetMetaData->ObjectMetaDataMap.FindOrAdd(TargetObject).Append(ObjectMetaData);
		}
	}
#endif
}


bool FFieldCookedMetaDataStore::HasMetaData() const
{
	return FieldMetaData.Num() > 0;
}

void FFieldCookedMetaDataStore::CacheMetaData(const FField* SourceField)
{
	FieldMetaData.Reset();

#if WITH_EDITORONLY_DATA
	if (const TMap<FName, FString>* SourceFieldMetaData = SourceField->GetMetaDataMap())
	{
		FieldMetaData = *SourceFieldMetaData;
	}
#endif
}

void FFieldCookedMetaDataStore::ApplyMetaData(FField* TargetField) const
{
#if WITH_EDITORONLY_DATA
	TargetField->AppendMetaData(FieldMetaData);
#endif
}


bool FStructCookedMetaDataStore::HasMetaData() const
{
	return ObjectMetaData.HasMetaData()
		|| PropertiesMetaData.Num() > 0;
}

void FStructCookedMetaDataStore::CacheMetaData(const UStruct* SourceStruct)
{
	ObjectMetaData.CacheMetaData(SourceStruct);

	for (TFieldIterator<const FProperty> SourcePropertyIt(SourceStruct, EFieldIterationFlags::None); SourcePropertyIt; ++SourcePropertyIt)
	{
		FFieldCookedMetaDataStore SourcePropertyMetaData;
		SourcePropertyMetaData.CacheMetaData(*SourcePropertyIt);

		if (SourcePropertyMetaData.HasMetaData())
		{
			PropertiesMetaData.Add(SourcePropertyIt->GetFName(), MoveTemp(SourcePropertyMetaData));
		}
	}
}

void FStructCookedMetaDataStore::ApplyMetaData(UStruct* TargetStruct) const
{
	ObjectMetaData.ApplyMetaData(TargetStruct);

	for (TFieldIterator<FProperty> TargetPropertyIt(TargetStruct, EFieldIterationFlags::None); TargetPropertyIt; ++TargetPropertyIt)
	{
		if (const FFieldCookedMetaDataStore* TargetPropertyMetaData = PropertiesMetaData.Find(TargetPropertyIt->GetFName()))
		{
			TargetPropertyMetaData->ApplyMetaData(*TargetPropertyIt);
		}
	}
}


void UEnumCookedMetaData::PostLoad()
{
	Super::PostLoad();
	CookedMetaDataUtil::Internal::PostLoadCookedMetaData<UEnum, UEnumCookedMetaData>(this);
}

bool UEnumCookedMetaData::HasMetaData() const
{
	return EnumMetaData.HasMetaData();
}

void UEnumCookedMetaData::CacheMetaData(const UEnum* SourceEnum)
{
	EnumMetaData.CacheMetaData(SourceEnum);
}

void UEnumCookedMetaData::ApplyMetaData(UEnum* TargetEnum) const
{
	EnumMetaData.ApplyMetaData(TargetEnum);
}



void UStructCookedMetaData::PostLoad()
{
	Super::PostLoad();
	CookedMetaDataUtil::Internal::PostLoadCookedMetaData<UScriptStruct, UStructCookedMetaData>(this);
}

bool UStructCookedMetaData::HasMetaData() const
{
	return StructMetaData.HasMetaData();
}

void UStructCookedMetaData::CacheMetaData(const UScriptStruct* SourceStruct)
{
	StructMetaData.CacheMetaData(SourceStruct);
}

void UStructCookedMetaData::ApplyMetaData(UScriptStruct* TargetStruct) const
{
	StructMetaData.ApplyMetaData(TargetStruct);
}


void UClassCookedMetaData::PostLoad()
{
	Super::PostLoad();
	CookedMetaDataUtil::Internal::PostLoadCookedMetaData<UClass, UClassCookedMetaData>(this);
}

bool UClassCookedMetaData::HasMetaData() const
{
	return ClassMetaData.HasMetaData()
		|| FunctionsMetaData.Num() > 0;
}

void UClassCookedMetaData::CacheMetaData(const UClass* SourceClass)
{
	ClassMetaData.CacheMetaData(SourceClass);

	for (TFieldIterator<const UFunction> SourceFunctionIt(SourceClass, EFieldIterationFlags::None); SourceFunctionIt; ++SourceFunctionIt)
	{
		FStructCookedMetaDataStore SourceFunctionMetaData;
		SourceFunctionMetaData.CacheMetaData(*SourceFunctionIt);

		if (SourceFunctionMetaData.HasMetaData())
		{
			FunctionsMetaData.Add(SourceFunctionIt->GetFName(), MoveTemp(SourceFunctionMetaData));
		}
	}
}

void UClassCookedMetaData::ApplyMetaData(UClass* TargetClass) const
{
	ClassMetaData.ApplyMetaData(TargetClass);

	for (TFieldIterator<UFunction> TargetFunctionIt(TargetClass, EFieldIterationFlags::None); TargetFunctionIt; ++TargetFunctionIt)
	{
		if (const FStructCookedMetaDataStore* TargetFunctionMetaData = FunctionsMetaData.Find(TargetFunctionIt->GetFName()))
		{
			TargetFunctionMetaData->ApplyMetaData(*TargetFunctionIt);
		}
	}
}
