// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Bindings/MVVMCompiledBindingLibrary.h"
#include "MVVMViewClassExtension.h"
#include "MVVMViewListViewBaseExtension.generated.h"

class UListViewBase;
class UMVVMView;
class UUserWidget;

UCLASS()
class MODELVIEWVIEWMODEL_API UMVVMViewListViewBaseExtension : public UMVVMViewClassExtension
{
	GENERATED_BODY()

public:
	//~ Begin UMVVMViewClassExtension overrides
	virtual void OnSourcesInitialized(UUserWidget* UserWidget, UMVVMView* View) override;
	virtual void OnSourcesUninitialized(UUserWidget* UserWidget, UMVVMView* View) override;
	//~ End UMVVMViewClassExtension overrides

#if WITH_EDITOR
	struct FInitListViewBaseExtensionArgs
	{
		FInitListViewBaseExtensionArgs(FName InWidgetName, FName InEntryViewModelName, const FMVVMVCompiledFieldPath& InWidgetPath)
			: WidgetName(InWidgetName),
			EntryViewModelName(InEntryViewModelName),
			WidgetPath(InWidgetPath)
		{}

		FName WidgetName;
		FName EntryViewModelName;
		FMVVMVCompiledFieldPath WidgetPath;
	};

	void Initialize(FInitListViewBaseExtensionArgs InArgs);
#endif

	FName GetEntryViewModelName() const
	{ 
		return EntryViewModelName; 
	}

private:
	void HandleSetViewModelOnEntryWidget(UUserWidget& EntryWidget, UUserWidget* OwningUserWidget, UListViewBase* ListWidget, FName EntryViewmodelName);

private:
	UPROPERTY(VisibleAnywhere, Category = "MVVM Extension")
	FName WidgetName;

	UPROPERTY(VisibleAnywhere, Category = "MVVM Extension")
	FName EntryViewModelName;

	UPROPERTY()
	FMVVMVCompiledFieldPath WidgetPath;

	UPROPERTY(Transient)
	TWeakObjectPtr<UListViewBase> CachedListViewWidget;
};