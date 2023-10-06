// Copyright Epic Games, Inc. All Rights Reserved.

#include "TypedElementExportedTextWidget.h"

#include "Elements/Columns/TypedElementMiscColumns.h"
#include "Elements/Columns/TypedElementSlateWidgetColumns.h"
#include "Elements/Columns/TypedElementTypeInfoColumns.h"
#include "Elements/Framework/TypedElementQueryBuilder.h"
#include "TypedElementSubsystems.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "TypedElementUI_ExportedTextWidget"

//
// UTypedElementExportedTextWidgetFactory
//

static void UpdateExportedTextWidget(ITypedElementDataStorageInterface& DataStorage, FTypedElementSlateWidgetReferenceColumn& Widget,
	const FTypedElementScriptStructTypeInfoColumn& TypeInfo, const FTypedElementRowReferenceColumn& ReferencedRow)
{
	TSharedPtr<SWidget> WidgetPointer = Widget.Widget.Pin();
	checkf(WidgetPointer, TEXT("Referenced widget is not valid. A constructed widget may not have been cleaned up. This can "
		"also happen if this processor is running in the same phase as the processors responsible for cleaning up old "
		"references."));
	checkf(WidgetPointer->GetType() == STextBlock::StaticWidgetClass().GetWidgetType(),
		TEXT("Stored widget with FTypedElementExportedTextWidgetTag doesn't match type %s, but was a %s."),
		*(STextBlock::StaticWidgetClass().GetWidgetType().ToString()),
		*(WidgetPointer->GetTypeAsString()));

	if (void* Data = DataStorage.GetColumnData(ReferencedRow.Row, TypeInfo.TypeInfo.Get()))
	{
		FString Label;
		TypeInfo.TypeInfo->ExportText(Label, Data, nullptr, nullptr, PPF_None, nullptr);
		STextBlock* TextWidget = static_cast<STextBlock*>(WidgetPointer.Get());
		FText Text = FText::FromString(MoveTemp(Label));
		TextWidget->SetToolTipText(Text);
		TextWidget->SetText(MoveTemp(Text));
	}
}

void UTypedElementExportedTextWidgetFactory::RegisterQueries(ITypedElementDataStorageInterface& DataStorage) const
{
	using namespace TypedElementQueryBuilder;
	using DSI = ITypedElementDataStorageInterface;

	DataStorage.RegisterQuery(
		Select(TEXT("Sync exported text widgets"),
		FProcessor(DSI::EQueryTickPhase::FrameEnd, DataStorage.GetQueryTickGroupName(DSI::EQueryTickGroups::SyncWidgets))
			.ForceToGameThread(true),
			[](FCachedQueryContext<UTypedElementDataStorageSubsystem>& Context, FTypedElementSlateWidgetReferenceColumn& Widget,
				const FTypedElementScriptStructTypeInfoColumn& TypeInfo, const FTypedElementRowReferenceColumn& ReferencedRow)
			{
				UTypedElementDataStorageSubsystem& Subsystem = Context.GetCachedMutableDependency<UTypedElementDataStorageSubsystem>();
				DSI* DataStorage = Subsystem.Get();
				checkf(DataStorage, TEXT("FTypedElementsDataStorageUiModule tried to process widgets before the "
					"Typed Elements Data Storage interface is available."));
				
				if (DataStorage->HasColumns<FTypedElementSyncFromWorldTag>(ReferencedRow.Row) ||
					DataStorage->HasColumns<FTypedElementSyncBackToWorldTag>(ReferencedRow.Row))
				{
					UpdateExportedTextWidget(*DataStorage, Widget, TypeInfo, ReferencedRow);
				}
			}
		)
	.Where()
		.All<FTypedElementExportedTextWidgetTag>()
	.Compile());

}

void UTypedElementExportedTextWidgetFactory::RegisterWidgetConstructors(ITypedElementDataStorageInterface& DataStorage,
	ITypedElementDataStorageUiInterface& DataStorageUi) const
{
	DataStorageUi.RegisterWidgetFactory(FName(TEXT("General.Cell.Default")), FTypedElementExportedTextWidgetConstructor::StaticStruct());
}



//
// FTypedElementExportedTextWidgetConstructor
//

FTypedElementExportedTextWidgetConstructor::FTypedElementExportedTextWidgetConstructor()
	: Super(FTypedElementExportedTextWidgetConstructor::StaticStruct())
{
}

TConstArrayView<const UScriptStruct*> FTypedElementExportedTextWidgetConstructor::GetAdditionalColumnsList() const
{
	static TTypedElementColumnTypeList<
		FTypedElementRowReferenceColumn,
		FTypedElementScriptStructTypeInfoColumn,
		FTypedElementExportedTextWidgetTag> Columns;
	return Columns;
}

bool FTypedElementExportedTextWidgetConstructor::CanBeReused() const
{
	return true;
}

TSharedPtr<SWidget> FTypedElementExportedTextWidgetConstructor::CreateWidget()
{
	return SNew(STextBlock);
}

bool FTypedElementExportedTextWidgetConstructor::FinalizeWidget(
	ITypedElementDataStorageInterface* DataStorage,
	ITypedElementDataStorageUiInterface* DataStorageUi,
	TypedElementRowHandle Row,
	const TSharedPtr<SWidget>& Widget)
{
	UpdateExportedTextWidget(
		*DataStorage,
		*DataStorage->GetColumn<FTypedElementSlateWidgetReferenceColumn>(Row),
		*DataStorage->GetColumn<FTypedElementScriptStructTypeInfoColumn>(Row),
		*DataStorage->GetColumn<FTypedElementRowReferenceColumn>(Row));
	return true;
}

#undef LOCTEXT_NAMESPACE
