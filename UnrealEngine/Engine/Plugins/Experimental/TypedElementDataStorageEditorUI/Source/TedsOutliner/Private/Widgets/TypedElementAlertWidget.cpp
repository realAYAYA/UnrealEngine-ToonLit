// Copyright Epic Games, Inc. All Rights Reserved.

#include "Widgets/TypedElementAlertWidget.h"

#include "Columns/UIPropertiesColumns.h"
#include "Elements/Columns/TypedElementAlertColumns.h"
#include "Elements/Columns/TypedElementMiscColumns.h"
#include "Elements/Columns/TypedElementSlateWidgetColumns.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "TEDS"

namespace AlertWidgetInternal
{
	void UpdateWidget(const TSharedPtr<SWidget>& Widget, const FText& Alert, bool bIsWarning, uint16 ErrorCount, uint16 WarningCount)
	{
		if (Widget)
		{
			uint32 ChildCount = ErrorCount + WarningCount;
			if (FChildren* Children = Widget->GetChildren())
			{
				SImage& Icon = static_cast<SImage&>(*Children->GetSlotAt(0).GetWidget());
				if (!Alert.IsEmpty() && ChildCount > 0)
				{
					Icon.SetToolTipText(FText::Format(LOCTEXT("ChildAlertCountWithMessage", "Errors: {0}\nWarnings: {1}\n\n{2}"),
						FText::AsNumber(ErrorCount + (bIsWarning ? 0 : 1)),
						FText::AsNumber(WarningCount + (bIsWarning ? 1 : 0)), Alert));
					Icon.SetImage(bIsWarning
						? FAppStyle::GetBrush("Icons.WarningWithColor")
						: FAppStyle::GetBrush("Icons.ErrorWithColor"));
					ChildCount++; // The row has an alert and a child alert so the count should be the total of both.
				}
				else if (!Alert.IsEmpty())
				{
					Icon.SetToolTipText(Alert);
					Icon.SetImage(bIsWarning
						? FAppStyle::GetBrush("Icons.WarningWithColor")
						: FAppStyle::GetBrush("Icons.ErrorWithColor"));
				}
				else if (ChildCount > 0)
				{
					Icon.SetToolTipText(FText::Format(LOCTEXT("ChildAlertCount", "Errors: {0}\nWarnings: {1}"),
						FText::AsNumber(ErrorCount), FText::AsNumber(WarningCount)));
					Icon.SetImage(FAppStyle::GetBrush("Icons.Warning"));
				}

				STextBlock& CounterText = static_cast<STextBlock&>(*Children->GetSlotAt(1).GetWidget());
				if (ChildCount == 0)
				{
					CounterText.SetText(FText::GetEmpty());
				}
				else if (ChildCount <= 9)
				{
					CounterText.SetText(FText::AsNumber(ChildCount));
				}
				else
				{
					CounterText.SetText(FText::FromString(TEXT("*")));
				}
			}
		}
	}
}

//
// UTypedElementAlertWidgetFactory
//

void UTypedElementAlertWidgetFactory::RegisterWidgetConstructors(
	ITypedElementDataStorageInterface& DataStorage,
	ITypedElementDataStorageUiInterface& DataStorageUi) const
{
	using namespace TypedElementDataStorage;

	DataStorageUi.RegisterWidgetFactory<FTypedElementAlertWidgetConstructor>(FName(TEXT("General.Cell")),
		FColumn<FTypedElementAlertColumn>() || FColumn<FTypedElementChildAlertColumn>());
	
	DataStorageUi.RegisterWidgetFactory<FTypedElementAlertHeaderWidgetConstructor>(FName(TEXT("General.Header")),
		FColumn<FTypedElementAlertColumn>() || FColumn<FTypedElementChildAlertColumn>());
}

void UTypedElementAlertWidgetFactory::RegisterQueries(ITypedElementDataStorageInterface& DataStorage)
{
	RegisterAlertQueries(DataStorage);
	RegisterAlertHeaderQueries(DataStorage);
}

void UTypedElementAlertWidgetFactory::RegisterAlertQueries(ITypedElementDataStorageInterface& DataStorage)
{
	using namespace TypedElementQueryBuilder;
	using namespace TypedElementDataStorage;
	using DSI = ITypedElementDataStorageInterface;
	
	TypedElementQueryHandle UpdateWidget_OnlyAlert = DataStorage.RegisterQuery(
		Select()
			.ReadOnly<FTypedElementAlertColumn>()
		.Where()
			.Any<FTypedElementSyncFromWorldTag, FTypedElementSyncBackToWorldTag>()
			.None<FTypedElementChildAlertColumn>()
		.Compile());

	TypedElementQueryHandle UpdateWidget_OnlyChildAlert = DataStorage.RegisterQuery(
		Select()
			.ReadOnly<FTypedElementChildAlertColumn>()
		.Where()
			.Any<FTypedElementSyncFromWorldTag, FTypedElementSyncBackToWorldTag>()
			.None<FTypedElementAlertColumn>()
		.Compile());

	TypedElementQueryHandle UpdateWidget_Both = DataStorage.RegisterQuery(
		Select()
			.ReadOnly<FTypedElementAlertColumn, FTypedElementChildAlertColumn>()
		.Where()
			.Any<FTypedElementSyncFromWorldTag, FTypedElementSyncBackToWorldTag>()
		.Compile());
	
	DataStorage.RegisterQuery(
		Select(TEXT("Sync Transform column to heads up display"),
			FProcessor(DSI::EQueryTickPhase::FrameEnd, DataStorage.GetQueryTickGroupName(DSI::EQueryTickGroups::SyncWidgets))
			.ForceToGameThread(true),
			[](IQueryContext& Context,
				FTypedElementSlateWidgetReferenceColumn& Widget,
				const FTypedElementRowReferenceColumn& ReferenceColumn)
			{
				Context.RunSubquery(0, ReferenceColumn.Row, CreateSubqueryCallbackBinding(
					[&Widget](const FTypedElementAlertColumn& Alert)
					{
						checkf(
							Alert.AlertType == FTypedElementAlertColumnType::Warning ||
							Alert.AlertType == FTypedElementAlertColumnType::Error,
							TEXT("Alert column has unsupported type %i"), static_cast<int>(Alert.AlertType));
						AlertWidgetInternal::UpdateWidget(Widget.Widget.Pin(), Alert.Message, 
							Alert.AlertType == FTypedElementAlertColumnType::Warning, 0, 0);
					}));
				Context.RunSubquery(1, ReferenceColumn.Row, CreateSubqueryCallbackBinding(
					[&Widget](const FTypedElementChildAlertColumn& ChildAlert)
					{
						AlertWidgetInternal::UpdateWidget(Widget.Widget.Pin(), FText::GetEmpty(), false,
							ChildAlert.Counts[static_cast<size_t>(FTypedElementAlertColumnType::Error)],
							ChildAlert.Counts[static_cast<size_t>(FTypedElementAlertColumnType::Warning)]);
					}));
				Context.RunSubquery(2, ReferenceColumn.Row, CreateSubqueryCallbackBinding(
					[&Widget](const FTypedElementAlertColumn& Alert, const FTypedElementChildAlertColumn& ChildAlert)
					{
						checkf(
							Alert.AlertType == FTypedElementAlertColumnType::Warning ||
							Alert.AlertType == FTypedElementAlertColumnType::Error,
							TEXT("Alert column has unsupported type %i"), static_cast<int>(Alert.AlertType));
						AlertWidgetInternal::UpdateWidget(Widget.Widget.Pin(), Alert.Message,
							Alert.AlertType == FTypedElementAlertColumnType::Warning,
							ChildAlert.Counts[static_cast<size_t>(FTypedElementAlertColumnType::Error)],
							ChildAlert.Counts[static_cast<size_t>(FTypedElementAlertColumnType::Warning)]);
					}));
			}
		)
		.Where()
			.All<FTypedElementAlertWidgetTag>()
			.DependsOn()
				.SubQuery(UpdateWidget_OnlyAlert)
				.SubQuery(UpdateWidget_OnlyChildAlert)
				.SubQuery(UpdateWidget_Both)
		.Compile());
}

void UTypedElementAlertWidgetFactory::RegisterAlertHeaderQueries(ITypedElementDataStorageInterface& DataStorage)
{
	using namespace TypedElementQueryBuilder;
	using namespace TypedElementDataStorage;
	using DSI = ITypedElementDataStorageInterface;
	
	TypedElementQueryHandle AlertCount = DataStorage.RegisterQuery(
		Count()
		.Where()
			.Any<FTypedElementAlertColumn>()
		.Compile());
	
	DataStorage.RegisterQuery(
		Select(TEXT("Update alert header"),
			FProcessor(DSI::EQueryTickPhase::FrameEnd, DataStorage.GetQueryTickGroupName(DSI::EQueryTickGroups::SyncWidgets))
			.ForceToGameThread(true),
			[](IQueryContext& Context, RowHandle Row, FTypedElementSlateWidgetReferenceColumn& Widget)
			{
				FQueryResult Result = Context.RunSubquery(0);
				if (Result.Count > 0)
				{
					if (TSharedPtr<SWidget> WidgetPtr = Widget.Widget.Pin())
					{
						static_cast<SImage*>(WidgetPtr.Get())->SetImage(FAppStyle::GetBrush("Icons.WarningWithColor"));
						Context.AddColumns<FTypedElementAlertHeaderActiveWidgetTag>(Row);
					}
				}
			}
		)
		.Where()
			.All<FTypedElementAlertHeaderWidgetTag>()
			.None<FTypedElementAlertHeaderActiveWidgetTag>()
		.DependsOn()
			.SubQuery(AlertCount)
		.Compile());

	DataStorage.RegisterQuery(
		Select(TEXT("Update active alert header"),
			FProcessor(DSI::EQueryTickPhase::FrameEnd, DataStorage.GetQueryTickGroupName(DSI::EQueryTickGroups::SyncWidgets))
			.ForceToGameThread(true),
			[](IQueryContext& Context, RowHandle Row, FTypedElementSlateWidgetReferenceColumn& Widget)
			{
				FQueryResult Result = Context.RunSubquery(0);
				if (Result.Count == 0)
				{
					if (TSharedPtr<SWidget> WidgetPtr = Widget.Widget.Pin())
					{
						static_cast<SImage*>(WidgetPtr.Get())->SetImage(FAppStyle::GetBrush("Icons.Warning"));
						Context.RemoveColumns<FTypedElementAlertHeaderActiveWidgetTag>(Row);
					}
				}
			}
		)
		.Where()
			.All<FTypedElementAlertHeaderWidgetTag, FTypedElementAlertHeaderActiveWidgetTag>()
		.DependsOn()
			.SubQuery(AlertCount)
		.Compile());
}



//
// FTypedElementAlertWidgetConstructor
//

FTypedElementAlertWidgetConstructor::FTypedElementAlertWidgetConstructor()
	: Super(StaticStruct())
{
}

TSharedPtr<SWidget> FTypedElementAlertWidgetConstructor::CreateWidget(
	const TypedElementDataStorage::FMetaDataView& Arguments)
{
	return SNew(SOverlay)
		+ SOverlay::Slot()
			.VAlign(VAlign_Bottom)
			.HAlign(HAlign_Left)
		[
			SNew(SImage)
				.DesiredSizeOverride(FVector2D(15.f, 15.f))
				.ColorAndOpacity(FSlateColor::UseForeground())
				.Image(FAppStyle::GetBrush("Icons.Warning"))
		]
		+ SOverlay::Slot()
			.VAlign(VAlign_Top)
			.HAlign(HAlign_Right)
		[
			SNew(STextBlock)
				.Font(FAppStyle::GetFontStyle("TinyText"))
				.ColorAndOpacity(FSlateColor::UseForeground())
		];
}

TConstArrayView<const UScriptStruct*> FTypedElementAlertWidgetConstructor::GetAdditionalColumnsList() const
{
	static TTypedElementColumnTypeList<FTypedElementRowReferenceColumn, FTypedElementAlertWidgetTag> Columns;
	return Columns;
}

bool FTypedElementAlertWidgetConstructor::FinalizeWidget(ITypedElementDataStorageInterface* DataStorage,
	ITypedElementDataStorageUiInterface* DataStorageUi, TypedElementDataStorage::RowHandle Row, const TSharedPtr<SWidget>& Widget)
{
	using namespace TypedElementDataStorage;

	RowHandle TargetRow = DataStorage->GetColumn<FTypedElementRowReferenceColumn>(Row)->Row;
	const FTypedElementAlertColumn* Alert = DataStorage->GetColumn<FTypedElementAlertColumn>(TargetRow);
	const FTypedElementChildAlertColumn* ChildAlert = DataStorage->GetColumn<FTypedElementChildAlertColumn>(TargetRow);
	
	uint16 ErrorCount = ChildAlert ? ChildAlert->Counts[static_cast<size_t>(FTypedElementAlertColumnType::Error)] : 0;
	uint16 WarningCount = ChildAlert ? ChildAlert->Counts[static_cast<size_t>(FTypedElementAlertColumnType::Warning)] : 0;

	AlertWidgetInternal::UpdateWidget(
		Widget, 
		Alert ? Alert->Message : FText::GetEmpty(), 
		Alert ? (Alert->AlertType == FTypedElementAlertColumnType::Warning) : false, 
		ErrorCount, WarningCount);
	
	return true;
}



//
// FTypedElementAlertHeaderWidgetConstructor
//

FTypedElementAlertHeaderWidgetConstructor::FTypedElementAlertHeaderWidgetConstructor()
	: Super(StaticStruct())
{
}

TSharedPtr<SWidget> FTypedElementAlertHeaderWidgetConstructor::CreateWidget(
	const TypedElementDataStorage::FMetaDataView& Arguments)
{
	return SNew(SImage)
		.DesiredSizeOverride(FVector2D(16.f, 16.f))
		.ColorAndOpacity(FSlateColor::UseForeground())
		.Image(FAppStyle::GetBrush("Icons.Warning"))
		.ToolTipText(FText(LOCTEXT("AlertColumnHeader", "Alerts")));
}

TConstArrayView<const UScriptStruct*> FTypedElementAlertHeaderWidgetConstructor::GetAdditionalColumnsList() const
{
	static TTypedElementColumnTypeList<FTypedElementAlertHeaderWidgetTag> Columns;
	return Columns;
}

bool FTypedElementAlertHeaderWidgetConstructor::FinalizeWidget(ITypedElementDataStorageInterface* DataStorage, 
	ITypedElementDataStorageUiInterface* DataStorageUi, TypedElementDataStorage::RowHandle Row, const TSharedPtr<SWidget>& Widget)
{
	DataStorage->AddOrGetColumn(Row, FUIHeaderPropertiesColumn
		{
			.ColumnSizeMode = EColumnSizeMode::Fixed,
			.Width = 24.0f
		});
	return true;
}

#undef LOCTEXT_NAMESPACE
