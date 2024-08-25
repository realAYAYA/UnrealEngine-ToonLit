// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include <variant>
#include "Containers/Map.h"
#include "Elements/Common/TypedElementQueryConditions.h"
#include "Elements/Interfaces/TypedElementDataStorageUiInterface.h"
#include "Logging/LogMacros.h"
#include "UObject/ObjectMacros.h"

#include "TypedElementDatabaseUI.generated.h"

class ITypedElementDataStorageInterface;
class ITypedElementDataStorageCompatibilityInterface;

TYPEDELEMENTSDATASTORAGE_API DECLARE_LOG_CATEGORY_EXTERN(LogTypedElementDatabaseUI, Log, All);

UCLASS()
class TYPEDELEMENTSDATASTORAGE_API UTypedElementDatabaseUi final
	: public UObject
	, public ITypedElementDataStorageUiInterface
{
	GENERATED_BODY()

public:
	~UTypedElementDatabaseUi() override = default;

	void Initialize(
		ITypedElementDataStorageInterface* StorageInterface, 
		ITypedElementDataStorageCompatibilityInterface* StorageCompatibilityInterface);
	void Deinitialize();

	void RegisterWidgetPurpose(FName Purpose, EPurposeType Type, FText Description) override;

	bool RegisterWidgetFactory(FName Purpose, const UScriptStruct* Constructor) override;
	bool RegisterWidgetFactory(FName Purpose, const UScriptStruct* Constructor,
		TypedElementDataStorage::FQueryConditions Columns) override;
	bool RegisterWidgetFactory(FName Purpose, TUniquePtr<FTypedElementWidgetConstructor>&& Constructor) override;
	bool RegisterWidgetFactory(FName Purpose, TUniquePtr<FTypedElementWidgetConstructor>&& Constructor,
		TypedElementDataStorage::FQueryConditions Columns) override;

	void CreateWidgetConstructors(FName Purpose,
		const TypedElementDataStorage::FMetaDataView& Arguments, const WidgetConstructorCallback& Callback) override;
	void CreateWidgetConstructors(FName Purpose, EMatchApproach MatchApproach, TArray<TWeakObjectPtr<const UScriptStruct>>& Columns,
		const TypedElementDataStorage::FMetaDataView& Arguments, const WidgetConstructorCallback& Callback) override;

	void ConstructWidgets(FName Purpose, const TypedElementDataStorage::FMetaDataView& Arguments,
		const WidgetCreatedCallback& ConstructionCallback) override;
	TSharedPtr<SWidget> ConstructWidget(TypedElementRowHandle Row, FTypedElementWidgetConstructor& Constructor,
		const TypedElementDataStorage::FMetaDataView& Arguments) override;

	void ListWidgetPurposes(const WidgetPurposeCallback& Callback) const override;

private:
	struct FWidgetFactory
	{
		using ConstructorType = std::variant<const UScriptStruct*, TUniquePtr<FTypedElementWidgetConstructor>>;

		TypedElementDataStorage::FQueryConditions Columns;
		ConstructorType Constructor;

		FWidgetFactory() = default;
		explicit FWidgetFactory(const UScriptStruct* InConstructor);
		explicit FWidgetFactory(TUniquePtr<FTypedElementWidgetConstructor>&& InConstructor);
		FWidgetFactory(const UScriptStruct* InConstructor, TypedElementDataStorage::FQueryConditions&& InColumns);
		FWidgetFactory(TUniquePtr<FTypedElementWidgetConstructor>&& InConstructor, TypedElementDataStorage::FQueryConditions&& InColumns);
	};

	struct FPurposeInfo
	{	
		TArray<FWidgetFactory> Factories;
		FText Description;
		EPurposeType Type;
		bool bIsSorted{ false }; //< Whether or not the array of factories needs to be sorted. The factories themselves are already sorted.
	};

	void CreateStandardArchetypes();

	bool CreateSingleWidgetConstructor(
		const FWidgetFactory::ConstructorType& Constructor,
		const TypedElementDataStorage::FMetaDataView& Arguments,
		TArray<TWeakObjectPtr<const UScriptStruct>> MatchedColumnTypes,
		const TypedElementDataStorage::FQueryConditions& QueryConditions,
		const WidgetConstructorCallback& Callback);

	void CreateWidgetInstance(
		FTypedElementWidgetConstructor& Constructor,
		const TypedElementDataStorage::FMetaDataView& Arguments,
		const WidgetCreatedCallback& ConstructionCallback);

	void CreateWidgetConstructors_LongestMatch(
		const TArray<FWidgetFactory>& WidgetFactories, 
		TArray<TWeakObjectPtr<const UScriptStruct>>& Columns, 
		const TypedElementDataStorage::FMetaDataView& Arguments,
		const WidgetConstructorCallback& Callback);
	void CreateWidgetConstructors_ExactMatch(
		const TArray<FWidgetFactory>& WidgetFactories,
		TArray<TWeakObjectPtr<const UScriptStruct>>& Columns,
		const TypedElementDataStorage::FMetaDataView& Arguments,
		const WidgetConstructorCallback& Callback);
	void CreateWidgetConstructors_SingleMatch(
		const TArray<FWidgetFactory>& WidgetFactories,
		TArray<TWeakObjectPtr<const UScriptStruct>>& Columns,
		const TypedElementDataStorage::FMetaDataView& Arguments,
		const WidgetConstructorCallback& Callback);

	TypedElementTableHandle WidgetTable{ TypedElementInvalidTableHandle };
	
	TMap<FName, FPurposeInfo> WidgetPurposes;
	
	ITypedElementDataStorageInterface* Storage{ nullptr };
	ITypedElementDataStorageCompatibilityInterface* StorageCompatibility{ nullptr };
};