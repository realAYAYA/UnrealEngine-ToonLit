// Copyright Epic Games, Inc. All Rights Reserved.

#include "Elements/Interfaces/TypedElementDataStorageUiInterface.h"
#include "Elements/Framework/TypedElementColumnUtils.h"
#include "Elements/Columns/TypedElementSlateWidgetColumns.h"

TSharedPtr<SWidget> FTypedElementWidgetConstructor::Construct(
	TypedElementRowHandle Row,
	ITypedElementDataStorageInterface* DataStorage,
	ITypedElementDataStorageUiInterface* DataStorageUi,
	TConstArrayView<TypedElement::ColumnUtils::Argument> Arguments)
{
	ApplyArguments(Arguments);
	TSharedPtr<SWidget> Widget = CreateWidget();
	if (Widget)
	{
		AddColumns(DataStorage, Row, Widget);
	}
	return Widget;
}

void FTypedElementWidgetConstructor::ApplyArguments(TConstArrayView<TypedElement::ColumnUtils::Argument> Arguments)
{
	SetColumnValues(*this, Arguments);
}

void FTypedElementWidgetConstructor::AddColumns(
	ITypedElementDataStorageInterface* DataStorage, TypedElementRowHandle Row, const TSharedPtr<SWidget>& Widget)
{
	DataStorage->AddTag<FTypedElementSlateWidgetReferenceDeletesRowTag>(Row);
	FTypedElementSlateWidgetReferenceColumn* StorageColumn = DataStorage->AddOrGetColumn<FTypedElementSlateWidgetReferenceColumn>(Row);
	checkf(StorageColumn, TEXT("Expected Typed Element Data Storage to contain a FTypedElementSlateWidgetReferenceFragment for row %llu."), Row);
	StorageColumn->Widget = Widget;
}