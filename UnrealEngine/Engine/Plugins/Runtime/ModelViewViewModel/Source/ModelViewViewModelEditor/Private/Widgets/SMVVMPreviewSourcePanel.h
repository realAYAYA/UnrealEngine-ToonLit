// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Widgets/SCompoundWidget.h"

class STableViewBase;
class ITableRow;
class FWidgetBlueprintEditor;
class UUserWidget;

namespace UE::MVVM::Private { class SPreviewSourceView; }
namespace UE::MVVM::Private { class SPreviewSourceEntry; }

namespace UE::MVVM
{

/**
 *
 */
class SPreviewSourcePanel : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SPreviewSourcePanel) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, TSharedPtr<FWidgetBlueprintEditor> Editor);

private:
	void HandlePreviewWidgetChanged();
	void HandleSelectedObjectChanged();
	void HandleSourceSelectionChanged(TSharedPtr<Private::SPreviewSourceEntry> Entry, ESelectInfo::Type SelectionType) const;
	TSharedRef<ITableRow> GenerateWidget(TSharedPtr<Private::SPreviewSourceEntry> Entry, const TSharedRef<STableViewBase>& OwnerTable) const;

	TWeakPtr<FWidgetBlueprintEditor> WeakEditor;
	TSharedPtr<Private::SPreviewSourceView> SourceListView;
	TArray<TSharedPtr<Private::SPreviewSourceEntry>> SourceList;
	mutable bool bInternalSelection = false;
};

} // namespace
