// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Framework/SlateDelegates.h"
#include "Widgets/SCompoundWidget.h"

class AActor;
class SWidget;
class UDMMaterialStageExpression;
class UDynamicMaterialModel;
enum class EDMExpressionMenu : uint8;
struct FDMObjectMaterialProperty;

DECLARE_DELEGATE_OneParam(FDMOnActorMaterailSlotChanged, TSharedPtr<FDMObjectMaterialProperty> /** NewSelectedSlot */)

/**
 * Material Designer ToolBar
 * 
 * Displays the selected actor that the Material Designer is editing and allows for switching between slots for that actor.
 */
class SDMToolBar : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SDMToolBar) {}
		SLATE_ARGUMENT(TWeakObjectPtr<AActor>, MaterialActor)
		SLATE_ARGUMENT(TWeakObjectPtr<UDynamicMaterialModel>, MaterialModel)
		SLATE_EVENT(FDMOnActorMaterailSlotChanged, OnSlotChanged)
		SLATE_EVENT(FOnGetContent, OnGetSettingsMenu)
	SLATE_END_ARGS()

	virtual ~SDMToolBar() {}

	void Construct(const FArguments& InArgs);

	const TArray<TSharedPtr<FDMObjectMaterialProperty>>& GetMaterialProperties() const { return ActorMaterialProperties; }
	void SetMaterialProperties(const TArray<TSharedPtr<FDMObjectMaterialProperty>>& InActorMaterialProperties);

	const AActor* GetMaterialActor() const { return MaterialActorWeak.Get(); }
	void SetMaterialActor(AActor* InActor, const int32 InActiveSlotIndex = 0);

	UDynamicMaterialModel* GetMaterialModel() const { return MaterialModelWeak.Get(); }
	void SetMaterialModel(UDynamicMaterialModel* InModel);

	FText GetSlotActorDisplayName() const;

protected:
	TWeakObjectPtr<AActor> MaterialActorWeak;
	TWeakObjectPtr<UDynamicMaterialModel> MaterialModelWeak;
	FDMOnActorMaterailSlotChanged OnSlotChanged;
	FOnGetContent OnGetSettingsMenu;

	TArray<TSharedPtr<FDMObjectMaterialProperty>> ActorMaterialProperties;
	int32 SelectedMaterialSlotIndex;

	TSharedRef<SWidget> CreateToolBarEntries();

	TSharedRef<SWidget> CreateToolBarButton(TAttribute<const FSlateBrush*> InImageBrush, const TAttribute<FText>& InTooltipText, FOnClicked InOnClicked);
	TSharedRef<SWidget> CreateSlotsComboBoxWidget();

	TSharedRef<SWidget> GenerateSelectedMaterialSlotRow(TSharedPtr<FDMObjectMaterialProperty> InSelectedSlot) const;
	FText GetSlotDisplayName(TSharedPtr<FDMObjectMaterialProperty> InSlot) const;
	FText GetSelectedMaterialSlotName() const;
	void OnMaterialSlotChanged(TSharedPtr<FDMObjectMaterialProperty> InSelectedSlot, ESelectInfo::Type InSelectInfoType);

	const FMargin GetDefaultToolBarButtonContentPadding() const { return FMargin(2.0f); }
	const FVector2D GetDefaultToolBarButtonSize() const { return FVector2D(20.0f); }

	EVisibility GetSlotsComboBoxWidgetVisibiltiy() const;

	const FSlateBrush* GetFollowSelectionBrush() const;
	FSlateColor GetFollowSelectionColor() const;
	FReply OnFollowSelectionButtonClicked();
};
