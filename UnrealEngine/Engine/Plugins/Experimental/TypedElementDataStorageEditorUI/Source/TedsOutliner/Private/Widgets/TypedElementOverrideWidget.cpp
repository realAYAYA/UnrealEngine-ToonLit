// Copyright Epic Games, Inc. All Rights Reserved.

#include "TypedElementOverrideWidget.h"

#include "Editor.h"
#include "Elements/Columns/TypedElementMiscColumns.h"
#include "Elements/Columns/TypedElementOverrideColumns.h"
#include "Elements/Columns/TypedElementSlateWidgetColumns.h"
#include "Elements/Columns/TypedElementTypeInfoColumns.h"
#include "Elements/Framework/TypedElementQueryBuilder.h"
#include "Elements/Framework/TypedElementRegistry.h"
#include "TypedElementOutlinerColumnIntegration.h"
#include "Widgets/Images/SLayeredImage.h"

#define LOCTEXT_NAMESPACE "TypedElementOverrideWidget"

//
// UTypedElementOverrideWidgetFactory
//

void UTypedElementOverrideWidgetFactory::RegisterWidgetConstructors(
	ITypedElementDataStorageInterface& DataStorage, ITypedElementDataStorageUiInterface& DataStorageUi) const
{
	using namespace TypedElementDataStorage;

	// The  widget is a specific widget for the Scene Outliner's item label column
	DataStorageUi.RegisterWidgetFactory<FTypedElementOverrideWidgetConstructor>(FTypedElementSceneOutlinerQueryBinder::ItemLabelCellWidgetPurpose,
		FColumn<FTypedElementClassTypeInfoColumn>() || FColumn<FObjectOverrideColumn>() );
}

void UTypedElementOverrideWidgetFactory::RegisterQueries(ITypedElementDataStorageInterface& DataStorage)
{
	using namespace TypedElementQueryBuilder;
	using DSI = ITypedElementDataStorageInterface;

	const TypedElementQueryHandle UpdateWidget = DataStorage.RegisterQuery(
		Select()
		.Where()
			.Any<FTypedElementSyncFromWorldTag, FTypedElementSyncBackToWorldTag>()
			.All<FObjectOverrideColumn>()
		.Compile());
	
	DataStorage.RegisterQuery(
		Select(
			TEXT("Sync override status to widget"),
			FProcessor(DSI::EQueryTickPhase::FrameEnd, DataStorage.GetQueryTickGroupName(DSI::EQueryTickGroups::SyncWidgets))
				.ForceToGameThread(true),
			[](
				DSI::IQueryContext& Context, 
				const FTypedElementSlateWidgetReferenceColumn& Widget,
				const FTypedElementRowReferenceColumn& Target)
				{
					FTypedElementOverrideWidgetConstructor::UpdateOverrideWidget(Widget.Widget, Target.Row);
				})
		.Where()
			.All<FTypedElementOverrideWidgetTag>()
		.DependsOn()
			.SubQuery(UpdateWidget)
		.Compile()
		);
}

//
// FTypedElementOverrideWidgetConstructor
//

FTypedElementOverrideWidgetConstructor::FTypedElementOverrideWidgetConstructor()
: Super(FTypedElementOverrideWidgetConstructor::StaticStruct())
{
}

TSharedPtr<SWidget> FTypedElementOverrideWidgetConstructor::CreateWidget(const TypedElementDataStorage::FMetaDataView& Arguments)
{
	return SNew(SLayeredImage)
			.DesiredSizeOverride(FVector2D(16.f, 16.f))
			.ColorAndOpacity(FSlateColor::UseForeground());
}

bool FTypedElementOverrideWidgetConstructor::FinalizeWidget(ITypedElementDataStorageInterface* DataStorage,
	ITypedElementDataStorageUiInterface* DataStorageUi, TypedElementRowHandle Row, const TSharedPtr<SWidget>& Widget)
{
	const TypedElementRowHandle TargetRow = DataStorage->GetColumn<FTypedElementRowReferenceColumn>(Row)->Row;

	if (const FTypedElementClassTypeInfoColumn* TypeInfoColumn = DataStorage->GetColumn<FTypedElementClassTypeInfoColumn>(TargetRow))
	{
		// Add the icon showing the type of the widget as the base image
		SLayeredImage* WidgetInstance = static_cast<SLayeredImage*>(Widget.Get());
		WidgetInstance->SetImage(GetIconForRow(DataStorage, TargetRow, TypeInfoColumn));

		// Add override info if applicable
		if(const FObjectOverrideColumn* OverrideColumn = DataStorage->GetColumn<FObjectOverrideColumn>(TargetRow))
		{
			AddOverrideBadge(Widget, OverrideColumn->OverriddenState);
		}

		return true;
	}

	return false;

}

TConstArrayView<const UScriptStruct*> FTypedElementOverrideWidgetConstructor::GetAdditionalColumnsList() const
{
	static TTypedElementColumnTypeList<FTypedElementOverrideWidgetTag> Columns;
	return Columns;
}

void FTypedElementOverrideWidgetConstructor::AddOverrideBadge(const TWeakPtr<SWidget>& Widget, EOverriddenState OverriddenState)
{
	if(const TSharedPtr<SWidget> WidgetPtr = Widget.Pin())
	{
		const TSharedPtr<SLayeredImage> WidgetInstance = StaticCastSharedPtr<SLayeredImage>(WidgetPtr);

		WidgetInstance->RemoveAllLayers();

		FText Tooltip;

		switch(OverriddenState)
		{
		case EOverriddenState::Added:
			WidgetInstance->AddLayer(FAppStyle::GetBrush("SceneOutliner.OverrideAddedBase"));
			WidgetInstance->AddLayer(FAppStyle::GetBrush("SceneOutliner.OverrideAdded"));

			Tooltip = LOCTEXT("OverrideAddedTooltip", "This entity has been added.");
			break;
			
		case EOverriddenState::AllOverridden:
			// Not implemented yet
			break;
			
		case EOverriddenState::HasOverrides:
			WidgetInstance->AddLayer(FAppStyle::GetBrush("SceneOutliner.OverrideInsideBase"));
			WidgetInstance->AddLayer(FAppStyle::GetBrush("SceneOutliner.OverrideInside"));
			
			Tooltip = LOCTEXT("OverrideInsideTooltip", "At least one property or child has an override.");
			break;
			
		case EOverriddenState::NoOverrides:
			// No icon for no overrides
			break;
			
		case EOverriddenState::SubObjectsHasOverrides:
			// Not implemented yet
			break;
		}
		
		// We only add the tooltip if there already isn't one, because entities are currently dirtied almost every frame, which ends up calling this function
		// and invalidating the tooltip every frame - leading to it never get drawn
		if(!WidgetInstance->GetToolTip())
		{
			WidgetInstance->SetToolTipText(Tooltip);
		}
		
	}
}

void FTypedElementOverrideWidgetConstructor::RemoveOverrideBadge(const TWeakPtr<SWidget>& Widget)
{
	if(const TSharedPtr<SWidget> WidgetPtr = Widget.Pin())
	{
		const TSharedPtr<SLayeredImage> WidgetInstance = StaticCastSharedPtr<SLayeredImage>(WidgetPtr);
		WidgetInstance->RemoveAllLayers();
		WidgetInstance->SetToolTipText(FText::GetEmpty());
	}
}

void FTypedElementOverrideWidgetConstructor::UpdateOverrideWidget(const TWeakPtr<SWidget>& Widget, const TypedElementRowHandle TargetRow)
{
	const ITypedElementDataStorageInterface* DataStorageInterface = UTypedElementRegistry::GetInstance()->GetDataStorage();

	if(const FObjectOverrideColumn* OverrideColumn = DataStorageInterface->GetColumn<FObjectOverrideColumn>(TargetRow))
	{
		AddOverrideBadge(Widget, OverrideColumn->OverriddenState);
	}
	else
	{
		RemoveOverrideBadge(Widget);
	}
}

#undef LOCTEXT_NAMESPACE
