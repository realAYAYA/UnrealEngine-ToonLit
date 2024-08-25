// Copyright Epic Games, Inc. All Rights Reserved.

#include "TypedElementPackagePathWidget.h"

#include "Elements/Columns/TypedElementPackageColumns.h"
#include "Elements/Columns/TypedElementMiscColumns.h"
#include "Elements/Columns/TypedElementSlateWidgetColumns.h"
#include "Elements/Columns/TypedElementValueCacheColumns.h"
#include "Elements/Framework/TypedElementQueryBuilder.h"
#include "TypedElementSubsystems.h"
#include "Widgets/Text/STextBlock.h"

//
// UTypedElementPackagePathWidgetFactory
//

void UTypedElementPackagePathWidgetFactory::RegisterWidgetConstructors(ITypedElementDataStorageInterface& DataStorage,
	ITypedElementDataStorageUiInterface& DataStorageUi) const
{
	using namespace TypedElementDataStorage;

	DataStorageUi.RegisterWidgetFactory<FTypedElementPackagePathWidgetConstructor>(FName(TEXT("General.Cell")), 
		FColumn<FTypedElementPackagePathColumn>());
	DataStorageUi.RegisterWidgetFactory<FTypedElementLoadedPackagePathWidgetConstructor>(FName(TEXT("General.Cell")),
		FColumn<FTypedElementPackageLoadedPathColumn>());
}



//
// FTypedElementPackagePathWidgetConstructor
//

FTypedElementPackagePathWidgetConstructor::FTypedElementPackagePathWidgetConstructor()
	: Super(FTypedElementPackagePathWidgetConstructor::StaticStruct())
{
}

FTypedElementPackagePathWidgetConstructor::FTypedElementPackagePathWidgetConstructor(const UScriptStruct* InTypeInfo)
	: Super(FTypedElementPackagePathWidgetConstructor::StaticStruct())
{
}

TSharedPtr<SWidget> FTypedElementPackagePathWidgetConstructor::CreateWidget(const TypedElementDataStorage::FMetaDataView& Arguments)
{
	return SNew(STextBlock)
		.OverflowPolicy(ETextOverflowPolicy::Ellipsis)
		.Justification(ETextJustify::Right);
}

bool FTypedElementPackagePathWidgetConstructor::FinalizeWidget(
	ITypedElementDataStorageInterface* DataStorage,
	ITypedElementDataStorageUiInterface* DataStorageUi,
	TypedElementRowHandle Row,
	const TSharedPtr<SWidget>& Widget)
{
	TypedElementRowHandle TargetRow = DataStorage->GetColumn<FTypedElementRowReferenceColumn>(Row)->Row;
	if (const FTypedElementPackagePathColumn* Path = DataStorage->GetColumn<FTypedElementPackagePathColumn>(TargetRow))
	{
		STextBlock* TextWidget = static_cast<STextBlock*>(Widget.Get());
		FText Text = FText::FromString(Path->Path);
		TextWidget->SetToolTipText(Text);
		TextWidget->SetText(MoveTemp(Text));
		return true;
	}
	else
	{
		return false;
	}
}



//
// FTypedElementPackagePathWidgetConstructor
//

FTypedElementLoadedPackagePathWidgetConstructor::FTypedElementLoadedPackagePathWidgetConstructor()
	: Super(FTypedElementLoadedPackagePathWidgetConstructor::StaticStruct())
{
}

bool FTypedElementLoadedPackagePathWidgetConstructor::FinalizeWidget(
	ITypedElementDataStorageInterface* DataStorage,
	ITypedElementDataStorageUiInterface* DataStorageUi,
	TypedElementRowHandle Row,
	const TSharedPtr<SWidget>& Widget)
{
	TypedElementRowHandle TargetRow = DataStorage->GetColumn<FTypedElementRowReferenceColumn>(Row)->Row;
	if (const FTypedElementPackageLoadedPathColumn* Path = DataStorage->GetColumn<FTypedElementPackageLoadedPathColumn>(TargetRow))
	{
		STextBlock* TextWidget = static_cast<STextBlock*>(Widget.Get());
		FText Text = FText::FromString(Path->LoadedPath.GetLocalFullPath());
		TextWidget->SetToolTipText(Text);
		TextWidget->SetText(MoveTemp(Text));
		return true;
	}
	else
	{
		return false;
	}
}
