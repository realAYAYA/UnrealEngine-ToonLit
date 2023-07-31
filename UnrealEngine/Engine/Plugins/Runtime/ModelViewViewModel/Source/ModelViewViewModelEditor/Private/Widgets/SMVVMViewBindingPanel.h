// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "Input/Reply.h"
#include "Layout/Visibility.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SCompoundWidget.h"


struct FMVVMBlueprintViewBinding;
class FWidgetBlueprintEditor;
class IDetailsView;
class IStructureDetailsView;
class SBorder;
class UBlueprintExtension;
class UMVVMWidgetBlueprintExtension_View;

enum class ECheckBoxState : uint8;

namespace UE::MVVM
{
class SBindingsList;

class SBindingsPanel : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SBindingsPanel) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, TSharedPtr<FWidgetBlueprintEditor>, bool bInIsDrawerTab);
	virtual ~SBindingsPanel();

	//~ Begin SWidget Interface
	virtual FReply OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent) override;
	virtual bool SupportsKeyboardFocus() const override;
	//~ End SWidget Interface

	void OnBindingListSelectionChanged(TConstArrayView<FMVVMBlueprintViewBinding*> Selection);

	static void RegisterSettingsMenu();

private:
	void HandleBlueprintViewChangedDelegate();

	TSharedRef<SWidget> GenerateEditViewWidget();

	FReply AddDefaultBinding();
	bool CanAddBinding() const;
	FText GetAddBindingToolTip() const;

	TSharedRef<SWidget> GenerateSettingsMenu();

	TSharedRef<SWidget> CreateDrawerDockButton();
	FReply CreateDrawerDockButtonClicked();

	void HandleExtensionAdded(UBlueprintExtension* NewExtension);

	FReply HandleCreateViewModelClicked();

	EVisibility GetVisibility(bool bVisibleWithBindings) const;

	ECheckBoxState GetDetailsVisibleCheckState() const;
	void ToggleDetailsVisibility();

private:
	TWeakPtr<FWidgetBlueprintEditor> WeakBlueprintEditor;
	TSharedPtr<SBindingsList> BindingsList;
	TSharedPtr<SBorder> DetailContainer;
	TSharedPtr<IDetailsView> DetailsView;
	TSharedPtr<IStructureDetailsView> StructDetailsView;
	TWeakObjectPtr<UMVVMWidgetBlueprintExtension_View> MVVMExtension;
	FDelegateHandle BlueprintViewChangedDelegateHandle;
	bool bIsDrawerTab;
};

} // namespace UE::MVVM
