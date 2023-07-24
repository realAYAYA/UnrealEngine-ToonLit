// Copyright Epic Games, Inc. All Rights Reserved.

#include "TypedElementDatabaseUI.h"

#include "Elements/Columns/TypedElementSlateWidgetColumns.h"
#include "GenericPlatform/GenericPlatformMemory.h"

void UTypedElementDatabaseUi::Initialize(ITypedElementDataStorageInterface* StorageInterface)
{
	checkf(StorageInterface, TEXT("Typed Element's Database compatibility manager is being initialized with an invalid storage target."));

	Storage = StorageInterface;
	CreateStandardArchetypes();
}

void UTypedElementDatabaseUi::Deinitialize()
{
}

void UTypedElementDatabaseUi::RegisterWidgetFactory(FName Purpose, const UScriptStruct* Constructor)
{
	checkf(Constructor->IsChildOf(FTypedElementWidgetConstructor::StaticStruct()),
		TEXT("Attempting to register a Typed Elements widget constructor '%s' that isn't deriving from FTypedElementWidgetConstructor."),
		*Constructor->GetFullName());
	WidgetFactoryStructs.Add(Purpose, Constructor);
}

void UTypedElementDatabaseUi::RegisterWidgetFactory(
	FName Purpose, const UScriptStruct* ConstructorType, TUniquePtr<FTypedElementWidgetConstructor>&& Constructor)
{
	WidgetFactoryInstances.Add(Purpose, { MoveTemp(Constructor), ConstructorType });
}

void UTypedElementDatabaseUi::ConstructWidgets(FName Purpose, TConstArrayView<TypedElement::ColumnUtils::Argument> Arguments,
	const WidgetCreatedCallback& ConstructionCallback)
{
	// Construct with structs
	for (TMultiMap<FName, const UScriptStruct*>::TKeyIterator KeyValue = WidgetFactoryStructs.CreateKeyIterator(Purpose); 
		KeyValue; ++KeyValue)
	{
		const UScriptStruct* Target = KeyValue.Value();
		CreateWidgetInstanceFromDescription(Target, Arguments, ConstructionCallback);
	}

	// Construct with instances
	for (TMultiMap<FName, FInstanceConstructor>::TKeyIterator KeyValue = WidgetFactoryInstances.CreateKeyIterator(Purpose); 
		KeyValue; ++KeyValue)
	{
		FTypedElementWidgetConstructor* Source = KeyValue.Value().Constructor.Get();
		const UScriptStruct* Target = KeyValue.Value().ConstructorType;
		CreateWidgetInstanceFromInstance(Source, Target, Arguments, ConstructionCallback);
	}
}

void UTypedElementDatabaseUi::CreateWidgetInstanceFromDescription(
	const UScriptStruct* Target,
	TConstArrayView<TypedElement::ColumnUtils::Argument> Arguments,
	const WidgetCreatedCallback& ConstructionCallback)
{
	FTypedElementWidgetConstructor* Constructor = reinterpret_cast<FTypedElementWidgetConstructor*>(
		FMemory_Alloca_Aligned(Target->GetStructureSize(), Target->GetMinAlignment()));
	if (Constructor)
	{
		Target->InitializeStruct(Constructor);
		CreateWidgetInstance(*Constructor, Arguments, ConstructionCallback, Target);
	}
	else
	{
		checkf(false, TEXT("Remaining stack space is too small to create a Typed Elements widget constructor from a description."));
	}
}

void UTypedElementDatabaseUi::CreateWidgetInstanceFromInstance(
	FTypedElementWidgetConstructor* SourceConstructor,
	const UScriptStruct* Target,
	TConstArrayView<TypedElement::ColumnUtils::Argument> Arguments,
	const WidgetCreatedCallback& ConstructionCallback)
{
	FTypedElementWidgetConstructor* Constructor = reinterpret_cast<FTypedElementWidgetConstructor*>(
		FMemory_Alloca_Aligned(Target->GetStructureSize(), Target->GetMinAlignment()));
	if (Constructor)
	{
		Target->InitializeStruct(Constructor);
		Target->CopyScriptStruct(Constructor, SourceConstructor);
		CreateWidgetInstance(*Constructor, Arguments, ConstructionCallback, Target);
	}
	else
	{
		checkf(false, TEXT("Remaining stack space is too small to create a Typed Elements widget constructor from an instance."));
	}
}

void UTypedElementDatabaseUi::CreateWidgetInstance(
	FTypedElementWidgetConstructor& Constructor, 
	TConstArrayView<TypedElement::ColumnUtils::Argument> Arguments, 
	const WidgetCreatedCallback& ConstructionCallback, 
	const UScriptStruct* Target)
{
	TypedElementRowHandle Row = Storage->AddRow(WidgetTable);
	TSharedPtr<SWidget> Widget = Constructor.Construct(Row, Storage, this, Arguments);
	if (Widget)
	{
		ConstructionCallback(Widget.ToSharedRef(), Row);
	}
	else
	{
		Storage->RemoveRow(Row);
	}

	Target->DestroyStruct(&Constructor);
}

TSharedPtr<SWidget> UTypedElementDatabaseUi::ConstructWidget(TypedElementRowHandle Row, FTypedElementWidgetConstructor& Constructor,
	TConstArrayView<TypedElement::ColumnUtils::Argument> Arguments)
{
	return Constructor.Construct(Row, Storage, this, Arguments);
}

void UTypedElementDatabaseUi::CreateStandardArchetypes()
{
	WidgetTable = Storage->RegisterTable(MakeArrayView(
		{
			FTypedElementSlateWidgetReferenceColumn::StaticStruct(),
			FTypedElementSlateWidgetReferenceDeletesRowTag::StaticStruct()
		}), FName("Editor_WidgetTable"));
}