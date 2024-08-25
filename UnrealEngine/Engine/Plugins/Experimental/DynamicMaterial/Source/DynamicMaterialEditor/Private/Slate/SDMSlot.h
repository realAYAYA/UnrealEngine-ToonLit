// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "DMDefs.h"
#include "Slate/Layers/SDMSlotLayerView.h"
#include "Slate/SDMEditor.h"
#include "Widgets/SCompoundWidget.h"

class SBox;
class SDMEditor;
class SDMSlotLayerView;
class SImage;
class SScrollBox;
class SSplitter;
class SStackBox;
class UDMMaterialLayerObject;
class UDMMaterialSlot;
class UDMMaterialStage;
class UDMMaterialStageBlend;
class UDMMaterialStageExpression;
class UDMMaterialStageGradient;
class UDMMaterialStageInputValue;
class UDMMaterialValue;
class UDMMaterialValueFloat1;
enum class EDMMaterialLayerStage : uint8;
struct FPropertyChangedEvent;

class SDMSlot : public SCompoundWidget
{
	SLATE_BEGIN_ARGS(SDMSlot)
		: _SlotPreviewSize(64)
		, _LayerPreviewSize(48)
	{}
		SLATE_ATTRIBUTE(int32, SlotPreviewSize)
		SLATE_ATTRIBUTE(int32, LayerPreviewSize)
	SLATE_END_ARGS()

	friend class UDMMaterialStageExpression;

public:
	virtual ~SDMSlot() override;

	void Construct(const FArguments& InArgs, const TSharedRef<SDMEditor>& InEditor, UDMMaterialSlot* InSlot);

	TSharedPtr<SDMEditor> GetEditorWidget() const { return EditorWidgetWeak.Pin(); }
	UDMMaterialSlot* GetSlot() const { return SlotWeak.Get(); }

	TSharedPtr<SDMStage> FindStageWidget(UDMMaterialStage* const InStage) const;
	
	void RemoveLayerByStage(UDMMaterialStage* const InStage, const bool bInSelectNextMaskIfMask = false);

	int32 GetLayerCount() const;

	TArray<int32> GetSelectedLayerIndices() const;
	void ClearSelection();

	void InvalidateMainWidget();
	void InvalidateHeaderPropertyListWidget();
	void InvalidateSlotSettingsRowWidget();
	void InvalidateComponentEditWidget();

	void AddNewLayer_NewLocalValue(EDMValueType InType);
	void AddNewLayer_GlobalValue(UDMMaterialValue* InValue);
	void AddNewLayer_NewGlobalValue(EDMValueType InType);
	void AddNewLayer_Slot(UDMMaterialSlot* InSlot, EDMMaterialPropertyType InMaterialProperty);
	void AddNewLayer_Expression(TSubclassOf<UDMMaterialStageExpression> InExpressionClass, EDMMaterialLayerStage LayerEnabledMask);
	void AddNewLayer_Blend(TSubclassOf<UDMMaterialStageBlend> InBlendClass);
	void AddNewLayer_Gradient(TSubclassOf<UDMMaterialStageGradient> InGradientClass);
	void AddNewLayer_UV();
	void AddNewLayer_MaterialFunction();
	void AddNewLayer_SceneTexture();

	void RemoveSlot();

	bool CheckValidity();

	bool GetLayerRowsButtonsCanRemove() const;
	FReply OnLayerRowButtonsRemoveClicked();

	UDMMaterialLayerObject* GetSelectedLayer() const;

	void SetSelectedLayer(UDMMaterialLayerObject* InLayer) const;

	UDMMaterialComponent* GetEditedComponent() const;

	void SetEditedComponent(UDMMaterialComponent* InComponent);

protected:
	TWeakPtr<SDMEditor> EditorWidgetWeak;

	TAttribute<int32> SlotPreviewSize;
	TAttribute<int32> LayerPreviewSize;

	TWeakObjectPtr<UDMMaterialSlot> SlotWeak;
	TSharedPtr<SBox> HeaderPropertyListWidget;
	TSharedPtr<SBox> SlotSettingsRowContainer;
	TSharedPtr<SScrollBox> ComponentEditContainer;

	TSharedPtr<SSplitter> SplitterContainer;
	SSplitter::FSlot* LayerViewSplitterSlot = nullptr;
	SSplitter::FSlot* ExtraSpaceSplitterSlot = nullptr;

	TSharedPtr<SDMSlotLayerView> LayerView;

	TSharedPtr<FScopedTransaction> ScrubbingTransaction;
	float OriginalOpacity;

	TWeakObjectPtr<UDMMaterialStageInputValue> SelectedOpacityStageInputValue;
	TWeakObjectPtr<UDMMaterialValueFloat1> SelectedOpacityInputValue;

	bool bInvalidateMainWidget;
	bool bInvalidateHeaderWidget;
	bool bInvalidateSettingsWidget;
	bool bInvalidateComponentEditWidget;
	FDelegateHandle OnEndFrameDelegateHandle;

	TWeakObjectPtr<UDMMaterialComponent> EditedComponent;

	TSharedRef<SWidget> CreateHeaderPropertyListWidget();
	TSharedRef<SWidget> CreateLayerButtonsRowWidget();
	TSharedRef<SWidget> CreateComponentEditWidget();

	TSharedRef<SWidget> CreateSlotSettingsRow();

	void OnSplitterResized() const;

	UDMMaterialLayerObject* AddNewLayer(UDMMaterialStage* InNewBaseStage, UDMMaterialStage* InNewMaskStage = nullptr);

	void AddPropertyToSlot(EDMMaterialPropertyType Property);

	void OnSlotLayersUpdated(UDMMaterialSlot* InSlot);
	void OnSlotPropertiesUpdated(UDMMaterialSlot* InSlot);
	void OnComponentUpdated(UDMMaterialComponent* InComponent, EDMUpdateType InUpdateType);

	void StartStageOpacityTransaction();
	void EndStageOpacityTransaction(const float InValue) { EndTransaction(); }
	void SetStageOpacity(const float InNewValue);
	void OnStageOpacityChanged(const float InNewValue);
	void OnStageOpacityCommitted(const float InNewValue, const ETextCommit::Type InCommitType);

	void StartTransaction(const FText InDescription);
	void EndTransaction();

	void OnLayerSelected(TSharedPtr<FDMMaterialLayerReference> InLayerItem, const int32 InLayerIndex);
	void OnLayerStageSelected(const bool bInSelected, const TSharedRef<SDMStage>& InStageWidget);

	void RefreshMainWidget();
	void RefreshHeaderPropertyListWidget();
	void RefreshSlotSettingsRowWidget();
	void RefreshComponentEditWidget();

	void HandleEndFrameRefresh();

	FText GetLayerButtonsDescription() const;
	TSharedRef<SWidget> GetLayerButtonsMenuContent();

	bool GetLayerCanAddEffect() const;
	TSharedRef<SWidget> GetLayerEffectsMenuContent();

	bool GetLayerRowsButtonsCanDuplicate() const;
	FReply OnLayerRowButtonsDuplicateClicked();

	int32 GetSourceBlendTypeSwitcherIndex() const;
	TSubclassOf<UDMMaterialStageBlend> GetSelectedSourceBlendType() const;
	void OnSourceBlendTypedSelected(const TSubclassOf<UDMMaterialStageBlend> InNewItem);

	bool IsOpacityPropertyEnabled() const;

	bool CanRemoveLayerByIndex(const int32 InLayerIndex) const;
	void RemoveLayerByIndex(const int32 InLayerIndex, const bool bInSelectNextMaskIfMask = false);

	bool CanRemoveSlot() const;
	FReply OnRemoveSlotClicked();
};
