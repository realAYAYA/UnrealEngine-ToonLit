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

static void UpdateExportedTextWidget(const void* Data, FTypedElementSlateWidgetReferenceColumn& Widget, 
	const FTypedElementScriptStructTypeInfoColumn& TypeInfo)
{
	TSharedPtr<SWidget> WidgetPointer = Widget.Widget.Pin();
	checkf(WidgetPointer, TEXT("Referenced widget is not valid. A constructed widget may not have been cleaned up. This can "
		"also happen if this processor is running in the same phase as the processors responsible for cleaning up old "
		"references."));
	checkf(WidgetPointer->GetType() == STextBlock::StaticWidgetClass().GetWidgetType(),
		TEXT("Stored widget with FTypedElementExportedTextWidgetTag doesn't match type %s, but was a %s."),
		*(STextBlock::StaticWidgetClass().GetWidgetType().ToString()),
		*(WidgetPointer->GetTypeAsString()));

	FString Label;
	TypeInfo.TypeInfo->ExportText(Label, Data, nullptr, nullptr, PPF_None, nullptr);
	STextBlock* TextWidget = static_cast<STextBlock*>(WidgetPointer.Get());
	FText Text = FText::FromString(MoveTemp(Label));
	TextWidget->SetToolTipText(Text);
	TextWidget->SetText(MoveTemp(Text));
}

static void UpdateExportedTextWidget(ITypedElementDataStorageInterface& DataStorage, FTypedElementSlateWidgetReferenceColumn& Widget,
	const FTypedElementScriptStructTypeInfoColumn& TypeInfo, const FTypedElementRowReferenceColumn& ReferencedRow)
{
	if (void* Data = DataStorage.GetColumnData(ReferencedRow.Row, TypeInfo.TypeInfo.Get()))
	{
		UpdateExportedTextWidget(Data, Widget, TypeInfo);
	}
}

static TypedElementQueryHandle RegisterUpdateCallback(ITypedElementDataStorageInterface& DataStorage, const UScriptStruct* Target)
{
	using namespace TypedElementQueryBuilder;
	using DSI = ITypedElementDataStorageInterface;
	namespace DS = TypedElementDataStorage;
	
	TypedElementQueryHandle TypeDataQuery = DataStorage.RegisterQuery(
		Select()
			.ReadOnly(Target)
		.Where()
			.Any<FTypedElementSyncFromWorldTag, FTypedElementSyncBackToWorldTag>()
		.Compile());

	FString Name = TEXT("Sync exported text widgets (");
	Target->AppendName(Name);
	Name += ')';

	return DataStorage.RegisterQuery(
		Select(FName(Name),
			FProcessor(DSI::EQueryTickPhase::FrameEnd, DataStorage.GetQueryTickGroupName(DSI::EQueryTickGroups::SyncWidgets))
				.ForceToGameThread(true),
			[](
				DS::IQueryContext& Context, 
				FTypedElementSlateWidgetReferenceColumn& Widget,
				const FTypedElementScriptStructTypeInfoColumn& TypeInfo,
				const FTypedElementRowReferenceColumn& ReferencedRow)
			{
				Context.RunSubquery(0, ReferencedRow.Row, 
					[&Widget, &TypeInfo](const DS::FQueryDescription&, DS::ISubqueryContext& SubqueryContext)
					{
						UpdateExportedTextWidget(SubqueryContext.GetColumn(TypeInfo.TypeInfo.Get()), Widget, TypeInfo);
					});
			})
		.Where()
			.All<FTypedElementExportedTextWidgetTag>()
		.DependsOn()
			.SubQuery(TypeDataQuery)
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

TSharedPtr<SWidget> FTypedElementExportedTextWidgetConstructor::CreateWidget(const TypedElementDataStorage::FMetaDataView& Arguments)
{
	return SNew(STextBlock);
}

bool FTypedElementExportedTextWidgetConstructor::FinalizeWidget(
	ITypedElementDataStorageInterface* DataStorage,
	ITypedElementDataStorageUiInterface* DataStorageUi,
	TypedElementRowHandle Row,
	const TSharedPtr<SWidget>& Widget)
{
	FTypedElementScriptStructTypeInfoColumn& TypeInfoColumn = *DataStorage->GetColumn<FTypedElementScriptStructTypeInfoColumn>(Row);

	UpdateExportedTextWidget(
		*DataStorage,
		*DataStorage->GetColumn<FTypedElementSlateWidgetReferenceColumn>(Row),
		TypeInfoColumn,
		*DataStorage->GetColumn<FTypedElementRowReferenceColumn>(Row));
	
	UTypedElementExportedTextWidgetFactory* Factory = 
		UTypedElementExportedTextWidgetFactory::StaticClass()->GetDefaultObject<UTypedElementExportedTextWidgetFactory>();
	if (Factory && !Factory->RegisteredTypes.Contains(TypeInfoColumn.TypeInfo))
	{
		RegisterUpdateCallback(*DataStorage, TypeInfoColumn.TypeInfo.Get());
		Factory->RegisteredTypes.Add(TypeInfoColumn.TypeInfo);
	}
	
	return true;
}

#undef LOCTEXT_NAMESPACE
