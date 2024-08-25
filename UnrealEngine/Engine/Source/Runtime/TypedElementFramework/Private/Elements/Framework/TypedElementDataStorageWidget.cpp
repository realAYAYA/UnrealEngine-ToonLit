// Copyright Epic Games, Inc. All Rights Reserved.

#include "Elements/Framework/TypedElementDataStorageWidget.h"
#include "Elements/Interfaces/TypedElementDataStorageUiInterface.h"

STedsWidget::STedsWidget()
	: UiRowHandle(TypedElementDataStorage::InvalidRowHandle)
{

}

void STedsWidget::Construct(const FArguments& InArgs)
{
	UiRowHandle = InArgs._UiRowHandle;
	ConstructorTypeInfo = InArgs._ConstructorTypeInfo;

	ChildSlot
	[
		InArgs._Content.Widget
	];
}

const UScriptStruct* STedsWidget::GetWidgetConstructorTypeInfo() const
{
	return ConstructorTypeInfo;
}

void STedsWidget::SetContent(const TSharedRef< SWidget >& InContent)
{
	ChildSlot
	[
		InContent
	];
}