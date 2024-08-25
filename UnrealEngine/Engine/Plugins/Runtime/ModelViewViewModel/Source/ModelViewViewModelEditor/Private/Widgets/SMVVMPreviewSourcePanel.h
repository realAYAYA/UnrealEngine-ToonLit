// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Debugging/MVVMDebugging.h"
#include "Widgets/SCompoundWidget.h"

class STableViewBase;
class ITableRow;
class FWidgetBlueprintEditor;
class UMVVMView;
class UUserWidget;

#ifndef UE_WITH_MVVM_DEBUGGING
  #error "MVVM Debugging needs to be enabled."
#endif

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
#if UE_WITH_MVVM_DEBUGGING
	void HandleViewChanged(const FDebugging::FView&, const FDebugging::FViewSourceValueArgs&);
#endif
	TSharedRef<ITableRow> GenerateWidget(TSharedPtr<Private::SPreviewSourceEntry> Entry, const TSharedRef<STableViewBase>& OwnerTable) const;

	TWeakObjectPtr<UMVVMView> WeakView;
	TWeakPtr<FWidgetBlueprintEditor> WeakEditor;
	TSharedPtr<Private::SPreviewSourceView> SourceListView;
	TArray<TSharedPtr<Private::SPreviewSourceEntry>> SourceList;
	mutable bool bInternalSelection = false;
};

} // namespace
