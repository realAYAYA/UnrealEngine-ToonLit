// Copyright Epic Games, Inc. All Rights Reserved.

#include "TypedElementLabelWidget.h"

#include "Elements/Columns/TypedElementLabelColumns.h"
#include "Elements/Columns/TypedElementMiscColumns.h"
#include "Elements/Columns/TypedElementSlateWidgetColumns.h"
#include "Elements/Columns/TypedElementValueCacheColumns.h"
#include "Elements/Framework/TypedElementQueryBuilder.h"
#include "TypedElementSubsystems.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "TypedElementUI_LabelWidget"

//
// UTypedElementLabelWidgetFactory
//

static void UpdateTextWidget(const TWeakPtr<SWidget>& Widget, const FTypedElementLabelColumn& Label, const uint64* HashValue)
{
	TSharedPtr<SWidget> WidgetPointer = Widget.Pin();
	checkf(WidgetPointer, TEXT("Referenced widget is not valid. A constructed widget may not have been cleaned up. This can "
		"also happen if this processor is running in the same phase as the processors responsible for cleaning up old "
		"references."));
	checkf(WidgetPointer->GetType() == STextBlock::StaticWidgetClass().GetWidgetType(),
		TEXT("Stored widget with FTypedElementLabelWidgetConstructor doesn't match type %s, but was a %s."),
		*(STextBlock::StaticWidgetClass().GetWidgetType().ToString()),
		*(WidgetPointer->GetTypeAsString()));
	STextBlock* WidgetInstance = static_cast<STextBlock*>(WidgetPointer.Get());
	WidgetInstance->SetText(FText::FromString(Label.Label));
	if (!HashValue)
	{
		WidgetInstance->SetToolTipText(FText::FromString(Label.Label));
	}
	else
	{
		WidgetInstance->SetToolTipText(FText::FromString(FString::Format(TEXT("{0}\nHash: {1}"), { Label.Label, *HashValue })));
	}
}

static void SyncColumnsToWidget(
	ITypedElementDataStorageInterface* DataStorage, 
	TypedElementRowHandle TargetRow, 
	FTypedElementU64IntValueCacheColumn& TextHash,
	const TWeakPtr<SWidget>& Widget,
	bool bShowHashInToolTip)
{
	if (const FTypedElementLabelColumn* LabelColumn = DataStorage->GetColumn<FTypedElementLabelColumn>(TargetRow))
	{
		if (const FTypedElementLabelHashColumn* LabelHashColumn = DataStorage->GetColumn<FTypedElementLabelHashColumn>(TargetRow))
		{
			if (LabelHashColumn->LabelHash != TextHash.Value)
			{
				UpdateTextWidget(Widget, *LabelColumn, bShowHashInToolTip ? &LabelHashColumn->LabelHash : nullptr);
				TextHash.Value = LabelHashColumn->LabelHash;
			}
		}
		else
		{
			UpdateTextWidget(Widget, *LabelColumn, nullptr);
		}
	}
}

void UTypedElementLabelWidgetFactory::RegisterQueries(ITypedElementDataStorageInterface& DataStorage) const
{
	using namespace TypedElementQueryBuilder;
	using DSI = ITypedElementDataStorageInterface;

	DataStorage.RegisterQuery(
		Select(
			TEXT("Sync label to widget"),
			FProcessor(DSI::EQueryTickPhase::FrameEnd, DataStorage.GetQueryTickGroupName(DSI::EQueryTickGroups::SyncWidgets))
				.ForceToGameThread(true),
			[](	FCachedQueryContext<UTypedElementDataStorageSubsystem>& Context, 
				FTypedElementSlateWidgetReferenceColumn& Widget,
				FTypedElementU64IntValueCacheColumn& TextHash,
				const FTypedElementLabelWidgetColumn& Config,
				const FTypedElementRowReferenceColumn& Target)
			{
				DSI* DataStorage = Context.GetCachedMutableDependency<UTypedElementDataStorageSubsystem>().Get();
				checkf(DataStorage, TEXT("FTypedElementsDataStorageUiModule tried to process widgets before the "
					"Typed Elements Data Storage interface is available."));
				
				if (DataStorage->HasColumns<FTypedElementSyncFromWorldTag>(Target.Row) ||
					DataStorage->HasColumns<FTypedElementSyncBackToWorldTag>(Target.Row))
				{
					SyncColumnsToWidget(DataStorage, Target.Row, TextHash, Widget.Widget, Config.bShowHashInTooltip);
				}
			})
		.Compile()
	);
}

void UTypedElementLabelWidgetFactory::RegisterWidgetConstructors(ITypedElementDataStorageInterface& DataStorage,
	ITypedElementDataStorageUiInterface& DataStorageUi) const
{
	 DataStorageUi.RegisterWidgetFactory(FName(TEXT("General.Cell")), FTypedElementLabelWidgetConstructor::StaticStruct(),
		{ FTypedElementLabelColumn::StaticStruct() });
	DataStorageUi.RegisterWidgetFactory(FName(TEXT("General.Cell")), FTypedElementLabelWithHashTooltipWidgetConstructor::StaticStruct(),
		{ FTypedElementLabelColumn::StaticStruct(), FTypedElementLabelHashColumn::StaticStruct() });
}



//
// FTypedElementLabelWidgetConstructor
//

FTypedElementLabelWidgetConstructor::FTypedElementLabelWidgetConstructor()
	: Super(FTypedElementLabelWidgetConstructor::StaticStruct())
{
}

FTypedElementLabelWidgetConstructor::FTypedElementLabelWidgetConstructor(const UScriptStruct* InTypeInfo)
	: Super(InTypeInfo)
{
}

TConstArrayView<const UScriptStruct*> FTypedElementLabelWidgetConstructor::GetAdditionalColumnsList() const
{
	static const TTypedElementColumnTypeList<
		FTypedElementRowReferenceColumn,
		FTypedElementU64IntValueCacheColumn,
		FTypedElementLabelWidgetColumn> Columns;
	return Columns;
}

bool FTypedElementLabelWidgetConstructor::CanBeReused() const
{
	return true;
}

TSharedPtr<SWidget> FTypedElementLabelWidgetConstructor::CreateWidget()
{
	return SNew(STextBlock);
}

bool FTypedElementLabelWidgetConstructor::SetColumns(ITypedElementDataStorageInterface* DataStorage, TypedElementRowHandle Row)
{
	DataStorage->GetColumn<FTypedElementU64IntValueCacheColumn>(Row)->Value = 0;
	DataStorage->GetColumn<FTypedElementLabelWidgetColumn>(Row)->bShowHashInTooltip = false;

	return true;
}

bool FTypedElementLabelWidgetConstructor::FinalizeWidget(
	ITypedElementDataStorageInterface* DataStorage,
	ITypedElementDataStorageUiInterface* DataStorageUi,
	TypedElementRowHandle Row,
	const TSharedPtr<SWidget>& Widget)
{
	SyncColumnsToWidget(
		DataStorage, 
		DataStorage->GetColumn<FTypedElementRowReferenceColumn>(Row)->Row,
		*DataStorage->GetColumn<FTypedElementU64IntValueCacheColumn>(Row),
		Widget,
		false);
	return true;
}



//
// FTypedElementLabelWithHashTooltipWidgetConstructor
//

FTypedElementLabelWithHashTooltipWidgetConstructor::FTypedElementLabelWithHashTooltipWidgetConstructor()
	: Super(FTypedElementLabelWithHashTooltipWidgetConstructor::StaticStruct())
{
}

bool FTypedElementLabelWithHashTooltipWidgetConstructor::SetColumns(ITypedElementDataStorageInterface* DataStorage, TypedElementRowHandle Row)
{
	DataStorage->GetColumn<FTypedElementU64IntValueCacheColumn>(Row)->Value = 0;
	DataStorage->GetColumn<FTypedElementLabelWidgetColumn>(Row)->bShowHashInTooltip = true;

	return true;
}

bool FTypedElementLabelWithHashTooltipWidgetConstructor::FinalizeWidget(
	ITypedElementDataStorageInterface* DataStorage,
	ITypedElementDataStorageUiInterface* DataStorageUi,
	TypedElementRowHandle Row,
	const TSharedPtr<SWidget>& Widget)
{
	SyncColumnsToWidget(
		DataStorage,
		DataStorage->GetColumn<FTypedElementRowReferenceColumn>(Row)->Row,
		*DataStorage->GetColumn<FTypedElementU64IntValueCacheColumn>(Row),
		Widget,
		true);
	return true;
}

#undef LOCTEXT_NAMESPACE
