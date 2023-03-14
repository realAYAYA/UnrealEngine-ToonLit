// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Fonts/SlateFontInfo.h"
#include "Internationalization/Text.h"
#include "Layout/Visibility.h"
#include "Misc/Attribute.h"
#include "SceneOutlinerFwd.h"
#include "SceneOutlinerStandaloneTypes.h"
#include "Styling/SlateColor.h"
#include "Templates/SharedPointer.h"
#include "Types/SlateEnums.h"
#include "UObject/WeakObjectPtr.h"
#include "UObject/WeakObjectPtrTemplates.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SCompoundWidget.h"

class ISceneOutliner;
class UDataLayerInstance;
struct FDataLayerTreeItem;
struct FSlateBrush;
template <typename ItemType> class STableRow;

struct SDataLayerTreeLabel : FSceneOutlinerCommonLabelData, public SCompoundWidget
{
	SLATE_BEGIN_ARGS(SDataLayerTreeLabel) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, FDataLayerTreeItem& DataLayerItem, ISceneOutliner& SceneOutliner, const STableRow<FSceneOutlinerTreeItemPtr>& InRow);

private:

	FSlateFontInfo GetDisplayNameFont() const;
	FText GetDisplayText() const;
	FText GetTooltipText() const;
	FText GetTypeText() const;
	EVisibility GetTypeTextVisibility() const;
	const FSlateBrush* GetIcon() const;
	FText GetIconTooltip() const;
	FSlateColor GetForegroundColor() const;
	bool OnVerifyItemLabelChanged(const FText& InLabel, FText& OutErrorMessage);
	void OnLabelCommitted(const FText& InLabel, ETextCommit::Type InCommitInfo);
	bool ShouldBeHighlighted() const;
	bool ShouldBeItalic() const;
	bool IsInActorEditorContext() const;
	void OnEnterEditingMode();
	void OnExitEditingMode();

	TWeakPtr<FDataLayerTreeItem> TreeItemPtr;
	TWeakObjectPtr<UDataLayerInstance> DataLayerPtr;
	TAttribute<FText> HighlightText;
	bool bInEditingMode;
};