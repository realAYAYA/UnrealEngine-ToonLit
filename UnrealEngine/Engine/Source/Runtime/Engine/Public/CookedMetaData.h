// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UObject/Object.h"
#include "UObject/UObjectGlobals.h"
#include "CookedMetaData.generated.h"

/**
 * Cooked meta-data for a UObject.
 */
USTRUCT()
struct ENGINE_API FObjectCookedMetaDataStore
{
public:
	GENERATED_BODY()

	bool HasMetaData() const;
	void CacheMetaData(const UObject* SourceObject);
	void ApplyMetaData(UObject* TargetObject) const;

private:
	UPROPERTY()
	TMap<FName, FString> ObjectMetaData;
};

/**
 * Cooked meta-data for a FField.
 */
USTRUCT()
struct ENGINE_API FFieldCookedMetaDataStore
{
public:
	GENERATED_BODY()

	bool HasMetaData() const;
	void CacheMetaData(const FField* SourceField);
	void ApplyMetaData(FField* TargetField) const;

private:
	UPROPERTY()
	TMap<FName, FString> FieldMetaData;
};

/**
 * Cooked meta-data for a UStruct, including its nested FProperty data.
 */
USTRUCT()
struct ENGINE_API FStructCookedMetaDataStore
{
public:
	GENERATED_BODY()

	bool HasMetaData() const;
	void CacheMetaData(const UStruct* SourceStruct);
	void ApplyMetaData(UStruct* TargetStruct) const;

private:
	UPROPERTY()
	FObjectCookedMetaDataStore ObjectMetaData;

	UPROPERTY()
	TMap<FName, FFieldCookedMetaDataStore> PropertiesMetaData;
};

/**
 * Cooked meta-data for a UEnum.
 */
UCLASS(Optional)
class ENGINE_API UEnumCookedMetaData : public UObject
{
public:
	GENERATED_BODY()

	virtual bool HasMetaData() const;
	virtual void CacheMetaData(const UEnum* SourceEnum);
	virtual void ApplyMetaData(UEnum* TargetEnum) const;

protected:
	UPROPERTY()
	FObjectCookedMetaDataStore EnumMetaData;
};

/**
 * Cooked meta-data for a UScriptStruct, including its nested FProperty data.
 */
UCLASS(Optional)
class ENGINE_API UStructCookedMetaData : public UObject
{
public:
	GENERATED_BODY()

	virtual bool HasMetaData() const;
	virtual void CacheMetaData(const UScriptStruct* SourceStruct);
	virtual void ApplyMetaData(UScriptStruct* TargetStruct) const;

protected:
	UPROPERTY()
	FStructCookedMetaDataStore StructMetaData;
};

/**
 * Cooked meta-data for a UClass, including its nested FProperty and UFunction data.
 */
UCLASS(Optional)
class ENGINE_API UClassCookedMetaData : public UObject
{
public:
	GENERATED_BODY()

	virtual bool HasMetaData() const;
	virtual void CacheMetaData(const UClass* SourceClass);
	virtual void ApplyMetaData(UClass* TargetClass) const;

protected:
	UPROPERTY()
	FStructCookedMetaDataStore ClassMetaData;

	UPROPERTY()
	TMap<FName, FStructCookedMetaDataStore> FunctionsMetaData;
};

namespace CookedMetaDataUtil
{

template <typename CookedMetaDataType>
CookedMetaDataType* NewCookedMetaData(UObject* Outer, FName Name, TSubclassOf<CookedMetaDataType> Class = CookedMetaDataType::StaticClass())
{
	return NewObject<CookedMetaDataType>(Outer, Class, Name, RF_Standalone | RF_Public);
}

template <typename CookedMetaDataType>
CookedMetaDataType* FindCookedMetaData(UObject* Outer, const TCHAR* Name)
{
	return FindObject<CookedMetaDataType>(Outer, Name);
}

template <typename CookedMetaDataType, typename CookedMetaDataPtrType>
void PurgeCookedMetaData(CookedMetaDataPtrType& CookedMetaDataPtr)
{
	FNameBuilder BaseMetaDataName(CookedMetaDataPtr->GetFName());
	BaseMetaDataName << TEXT("_PURGED");

	CookedMetaDataPtr->ClearFlags(RF_Standalone | RF_Public);
	CookedMetaDataPtr->Rename(FNameBuilder(MakeUniqueObjectName(CookedMetaDataPtr->GetOuter(), CookedMetaDataPtr->GetClass(), FName(BaseMetaDataName))).ToString(), nullptr, REN_DoNotDirty | REN_DontCreateRedirectors | REN_ForceNoResetLoaders | REN_NonTransactional);
	CookedMetaDataPtr = nullptr;
}

}
