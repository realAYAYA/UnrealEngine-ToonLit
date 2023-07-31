// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Widgets/SCompoundWidget.h"


struct FMassDebuggerModel;
struct FMassDebuggerProcessingGraph;
class SMassProcessingGraphView;
template<typename T> class SListView;

class SMassProcessingView : public SCompoundWidget
{
public:

	SLATE_BEGIN_ARGS(SMassProcessingView){}
	SLATE_END_ARGS()

	~SMassProcessingView();

	void Construct(const FArguments& InArgs, TSharedRef<FMassDebuggerModel> InDebuggerModel);

protected:
	void OnRefresh();

	TSharedPtr<FMassDebuggerModel> DebuggerModel;

	TSharedPtr<SListView<TSharedPtr<FMassDebuggerProcessingGraph>>> GraphsListWidget;

	TSharedPtr<SMassProcessingGraphView> ProcessingGraphWidget;

	FDelegateHandle OnRefreshHandle;
};
