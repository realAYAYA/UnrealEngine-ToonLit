// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "SMassDebuggerViewBase.h"


struct FMassDebuggerProcessorData;
struct FMassDebuggerArchetypeData;
class SRichTextBlock;
struct FMassDebuggerModel;
class SMassProcessor;
template<typename T> class SListView;
class SVerticalBox;

class SMassProcessorsView : public SMassDebuggerViewBase
{
public:
	SLATE_BEGIN_ARGS(SMassProcessorsView)
	{
	}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, TSharedRef<FMassDebuggerModel> InDebuggerModel);

	void PopulateProcessorList();

protected:
	virtual void OnRefresh() override;
	virtual void OnProcessorsSelected(TConstArrayView<TSharedPtr<FMassDebuggerProcessorData>> SelectedProcessors, ESelectInfo::Type SelectInfo) override;
	virtual void OnArchetypesSelected(TConstArrayView<TSharedPtr<FMassDebuggerArchetypeData>> SelectedArchetypes, ESelectInfo::Type SelectInfo) override;

	void ProcessorListSelectionChanged(TSharedPtr<FMassDebuggerProcessorData> SelectedItem, ESelectInfo::Type SelectInfo);

	TSharedPtr<SListView<TSharedPtr<FMassDebuggerProcessorData>>> ProcessorsListWidget;	
	TSharedPtr<SListView<TSharedPtr<FMassDebuggerProcessorData>>> SelectedProcessorsListWidget;
	TSharedPtr<SMassProcessor> ProcessorWidget;
	TSharedPtr<SVerticalBox> ProcessorsBox;
};
