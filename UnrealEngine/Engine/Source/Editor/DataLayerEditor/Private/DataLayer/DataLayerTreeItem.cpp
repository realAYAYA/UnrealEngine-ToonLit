// Copyright Epic Games, Inc. All Rights Reserved.

#include "DataLayerTreeItem.h"

#include "DataLayer/DataLayerEditorSubsystem.h"
#include "ISceneOutlinerTreeItem.h"
#include "Internationalization/Internationalization.h"
#include "Internationalization/Text.h"
#include "SDataLayerTreeLabel.h"
#include "SceneOutlinerStandaloneTypes.h"
#include "Widgets/DeclarativeSyntaxSupport.h"

class ISceneOutliner;
class SWidget;
template <typename ItemType> class STableRow;

#define LOCTEXT_NAMESPACE "DataLayer"

const FSceneOutlinerTreeItemType FDataLayerTreeItem::Type(&ISceneOutlinerTreeItem::Type);

FDataLayerTreeItem::FDataLayerTreeItem(UDataLayerInstance* InDataLayerInstance)
	: ISceneOutlinerTreeItem(Type)
	, DataLayerInstance(InDataLayerInstance)
	, ID(InDataLayerInstance)
	, bIsHighlighedtIfSelected(false)
{
	Flags.bIsExpanded = false;
}

FString FDataLayerTreeItem::GetDisplayString() const
{
	const UDataLayerInstance* DataLayerInstancePtr = DataLayerInstance.Get();
	return DataLayerInstancePtr ? DataLayerInstancePtr->GetDataLayerShortName() : LOCTEXT("DataLayerForMissingDataLayer", "(Deleted Data Layer)").ToString();
}

bool FDataLayerTreeItem::GetVisibility() const
{
	const UDataLayerInstance* DataLayerInstancePtr = DataLayerInstance.Get();
	return DataLayerInstancePtr && DataLayerInstancePtr->IsVisible();
}

bool FDataLayerTreeItem::ShouldShowVisibilityState() const
{
	const UDataLayerInstance* DataLayerInstancePtr = DataLayerInstance.Get();
	return DataLayerInstancePtr && !DataLayerInstancePtr->IsReadOnly();
}

bool FDataLayerTreeItem::CanInteract() const 
{
	return true;
}

TSharedRef<SWidget> FDataLayerTreeItem::GenerateLabelWidget(ISceneOutliner& Outliner, const STableRow<FSceneOutlinerTreeItemPtr>& InRow)
{
	return SNew(SDataLayerTreeLabel, *this, Outliner, InRow);
}

void FDataLayerTreeItem::OnVisibilityChanged(const bool bNewVisibility)
{
	if (UDataLayerInstance* DataLayerInstancePtr = DataLayerInstance.Get())
	{
		UDataLayerEditorSubsystem::Get()->SetDataLayerVisibility(DataLayerInstancePtr, bNewVisibility);
	}
}

bool FDataLayerTreeItem::ShouldBeHighlighted() const
{
	if (bIsHighlighedtIfSelected)
	{
		if (UDataLayerInstance* DataLayerInstancePtr = DataLayerInstance.Get())
		{
			return UDataLayerEditorSubsystem::Get()->DoesDataLayerContainSelectedActors(DataLayerInstancePtr);
		}
	}
	return false;
}

#undef LOCTEXT_NAMESPACE