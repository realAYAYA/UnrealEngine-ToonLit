// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ClassViewerFilter.h"
#include "Widgets/SCompoundWidget.h"

class SClassViewer;
class UWidgetBlueprint;

namespace UE::MVVM
{
class SSourceBindingList;

class FViewModelClassFilter : public IClassViewerFilter
{
public:
	FViewModelClassFilter() = default;

	virtual bool IsClassAllowed(const FClassViewerInitializationOptions& InInitOptions, const UClass* InClass, TSharedRef<FClassViewerFilterFuncs> InFilterFuncs) override;
	virtual bool IsUnloadedClassAllowed(const FClassViewerInitializationOptions& InInitOptions, const TSharedRef<const IUnloadedBlueprintData> InUnloadedClassData, TSharedRef<FClassViewerFilterFuncs> InFilterFuncs) override;
};

class SMVVMSelectViewModel : public SCompoundWidget
{
public:
	DECLARE_DELEGATE_OneParam(FOnValueChanged, const UClass*);

	SLATE_BEGIN_ARGS(SMVVMSelectViewModel) {}
		SLATE_EVENT(FSimpleDelegate, OnCancel)
		SLATE_EVENT(FOnValueChanged, OnViewModelCommitted)
	SLATE_END_ARGS()

public:
	void Construct(const FArguments& InArgs, const UWidgetBlueprint* WidgetBlueprint);

private:
	void HandleClassPicked(UClass* ClassPicked);
	FReply HandleSelected();
	FReply HandleCancel();
	bool HandleIsSelectionEnabled() const;

private:
	TSharedPtr<SWidget> ClassViewer;
	TSharedPtr<SSourceBindingList> BindingListWidget;
	TWeakObjectPtr<const UClass> SelectedClass;

	FSimpleDelegate OnCancel;
	FOnValueChanged OnViewModelCommitted;
};

} //namespace