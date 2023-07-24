// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/Map.h"
#include "Elements/Interfaces/TypedElementDataStorageInterface.h"
#include "Elements/Interfaces/TypedElementDataStorageUiInterface.h"
#include "UObject/ObjectMacros.h"

#include "TypedElementDatabaseUI.generated.h"

UCLASS()
class TYPEDELEMENTSDATASTORAGE_API UTypedElementDatabaseUi final
	: public UObject
	, public ITypedElementDataStorageUiInterface
{
	GENERATED_BODY()

public:
	~UTypedElementDatabaseUi() override = default;

	void Initialize(ITypedElementDataStorageInterface* StorageInterface);
	void Deinitialize();

	void RegisterWidgetFactory(FName Purpose, const UScriptStruct* Constructor) override;
	
	void ConstructWidgets(FName Purpose, TConstArrayView<TypedElement::ColumnUtils::Argument> Arguments,
		const WidgetCreatedCallback& ConstructionCallback) override;
	TSharedPtr<SWidget> ConstructWidget(TypedElementRowHandle Row, FTypedElementWidgetConstructor& Constructor,
		TConstArrayView<TypedElement::ColumnUtils::Argument> Arguments) override;

protected:
	void RegisterWidgetFactory(FName Purpose, const UScriptStruct* ConstructorType,
		TUniquePtr<FTypedElementWidgetConstructor>&& Constructor) override;

private:
	struct FInstanceConstructor
	{
		TUniquePtr<FTypedElementWidgetConstructor> Constructor;
		const UScriptStruct* ConstructorType;
	};

	void CreateStandardArchetypes();

	void CreateWidgetInstanceFromDescription(
		const UScriptStruct* Target,
		TConstArrayView<TypedElement::ColumnUtils::Argument> Arguments,
		const WidgetCreatedCallback& ConstructionCallback);

	void CreateWidgetInstanceFromInstance(
		FTypedElementWidgetConstructor* SourceConstructor,
		const UScriptStruct* Target,
		TConstArrayView<TypedElement::ColumnUtils::Argument> Arguments,
		const WidgetCreatedCallback& ConstructionCallback);

	void CreateWidgetInstance(
		FTypedElementWidgetConstructor& Constructor,
		TConstArrayView<TypedElement::ColumnUtils::Argument> Arguments,
		const WidgetCreatedCallback& ConstructionCallback,
		const UScriptStruct* Target);

	ITypedElementDataStorageInterface* Storage{ nullptr };
	TypedElementTableHandle WidgetTable{ TypedElementInvalidTableHandle };
	TMultiMap<FName, const UScriptStruct*> WidgetFactoryStructs;
	TMultiMap<FName, FInstanceConstructor> WidgetFactoryInstances;
};