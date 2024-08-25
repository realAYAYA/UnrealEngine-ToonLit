// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Components/DMMaterialStage.h"
#include "DMEDefs.h"
#include "DMObjectMaterialProperty.h"
#include "EditorUndoClient.h"
#include "Widgets/SCompoundWidget.h"

class FUICommandList;
class IDetailTreeNode;
class IPropertyHandle;
class IPropertyRowGenerator;
class IStructureDetailsView;
class SBox;
class SDMMaterialParameters;
class SDMPropertyEdit;
class SDMSlot;
class SDMToolBar;
class SDockTab;
class SExpandableArea;
class SScrollBox;
class SWidgetSwitcher;
class UDMMaterialSlot;
class UDMMaterialStage;
class UDMMaterialStageExpression;
class UDMMaterialValueFloat1;
class UDynamicMaterialModel;
enum class ECheckBoxState : uint8;
enum class EDMExpressionMenu : uint8;
enum EMaterialDomain : int;
struct FDMActorMaterialSlot;

class SDMEditor : public SCompoundWidget, public FSelfRegisteringEditorUndoClient
{
	SLATE_DECLARE_WIDGET(SDMEditor, SCompoundWidget);

	SLATE_BEGIN_ARGS(SDMEditor) {}
	SLATE_END_ARGS()

public:
	static TSharedRef<FAssetThumbnailPool> GetThumbnailPool();

	static TSharedRef<SWidget> GetEmptyContent();

	static FDMPropertyHandle GetPropertyHandle(const SWidget* InOwner, UDMMaterialComponent* InComponent, const FName& InPropertyName);
	static void ClearPropertyHandles(const SWidget* InOwner);

	void Construct(const FArguments& InArgs, TWeakObjectPtr<UDynamicMaterialModel> InModelWeak);
	virtual ~SDMEditor() override;

	UDynamicMaterialModel* GetMaterialModel() const { return MaterialModelWeak.Get(); }
	void SetMaterialModel(UDynamicMaterialModel* InMaterialModel);

	const FDMObjectMaterialProperty& GetMaterialObjectProperty() const { return ObjectProperty; }
	void SetMaterialObjectProperty(const FDMObjectMaterialProperty& InObjectProperty);

	void SetMaterialActor(AActor* InActor);

	const TArray<TSharedPtr<SDMSlot>>& GetSlotWidgets() const { return SlotWidgets; }
	TSharedPtr<SDMSlot> GetSlotWidget(UDMMaterialSlot* Slot) const;

	void RefreshGlobalOpacitySlider();
	void RefreshParametersList();
	void RefreshSlotPickerList();
	void RefreshSlotsList();

	//~ Begin SWidget
	virtual bool SupportsKeyboardFocus() const override { return true; }
	virtual void Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime) override;
	//~ End SWidget

	void ClearEditor();

	int32 GetActiveSlotIndex() const { return ActiveSlotIndex; }
	void SetActiveSlotIndex(int InSlotIndex);

	const TSharedPtr<FUICommandList>& GetCommandList() const { return CommandList; }

	bool CanAddNewLayer() const;
	void AddNewLayer();

	bool CanInsertNewLayer() const;
	void InsertNewLayer();

	bool CanCopySelectedLayer() const;
	void CopySelectedLayer();

	bool CanCutSelectedLayer() const;
	void CutSelectedLayer();

	bool CanPasteLayer() const;
	void PasteLayer();

	bool CanDuplicateSelectedLayer() const;
	void DuplicateSelectedLayer();

	bool CanDeleteSelectedLayer() const;
	void DeleteSelectedLayer();

	//~ Begin SWidget
	virtual FReply OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent) override;
	//~ End SWidget

	//~ Begin FUndoClient
	virtual void PostUndo(bool bSuccess) override;
	virtual void PostRedo(bool bSuccess) override;
	//~ End FUndoClient

protected:
	static TSharedPtr<FAssetThumbnailPool> ThumbnailPool;

	static TMap<const SWidget*, TArray<FDMPropertyHandle>> PropertyHandleMap;

	static FDMPropertyHandle CreatePropertyHandle(const void* InOwner, UDMMaterialComponent* InComponent, const FName& InPropertyName);

	TSharedPtr<FUICommandList> CommandList;

	TSharedPtr<SBox> Container;
	TSharedPtr<SDMToolBar> Toolbar;
	TSharedPtr<SBox> GlobalOpacityContainer;
	TSharedPtr<SDMMaterialParameters> ParametersWidget;
	TSharedPtr<SBox> SlotsContainer;
	TSharedPtr<SBox> SlotPickerContainer;
	TSharedPtr<SWidgetSwitcher> SlotSwitcher;
	int32 ActiveSlotIndex;

	TWeakObjectPtr<UDynamicMaterialModel> MaterialModelWeak;
	FDMObjectMaterialProperty ObjectProperty;

	TArray<TSharedPtr<SDMSlot>> SlotWidgets;

	void BindCommands();

	TSharedRef<SWidget> CreateMainLayout();
	TSharedRef<SWidget> CreateMaterialSettingsRow();
	TSharedRef<SWidget> CreateGlobalOpacityWidget();
	TSharedRef<SWidget> CreateParametersArea();

	TSharedRef<SWidget> CreateSlotPickerWidget();
	TSharedRef<SWidget> CreateSlotsWidget();

	TSharedRef<SWidget> CreateActorMaterialSlotSelector(const AActor* InActor);

	FReply OnAddSlotButtonClicked();

	void OnMaterialBuilt(UDynamicMaterialModel* InMaterialModel);
	void OnValuesUpdated(UDynamicMaterialModel* InMaterialModel);
	void OnSlotsUpdated(UDynamicMaterialModel* InMaterialModel);

	bool IsGlobalOpacityEnabled() const;

	ECheckBoxState GetRGBSlotCheckState_HasSlot() const;
	void OnRGBSlotCheckStateChanged_HasSlot(ECheckBoxState InCheckState);

	ECheckBoxState GetRGBSlotCheckState_NoSlot() const;
	void OnRGBSlotCheckStateChanged_NoSlot(ECheckBoxState InCheckState);

	bool GetOpacityButtonEnabled_HasSlot() const;
	ECheckBoxState GetOpacitySlotCheckState_HasSlot() const;
	void OnOpacitySlotCheckStateChanged_HasSlot(ECheckBoxState InCheckState);

	bool GetOpacityButtonEnabled_NoSlot() const;
	ECheckBoxState GetOpacitySlotCheckState_NoSlot() const;
	void OnOpacitySlotCheckStateChanged_NoSlot(ECheckBoxState InCheckState);

	FReply OnCreateMaterialButtonClicked(TWeakPtr<FDMObjectMaterialProperty> InMaterialProperty);

	void OnToolBarPropertyChanged(TSharedPtr<FDMObjectMaterialProperty> InNewSelectedProperty);
	TSharedRef<SWidget> MakeToolBarSettingsMenu();

	void OnSettingsChanged(const FPropertyChangedEvent& InPropertyChangedEvent);

	EMaterialDomain GetSelectedDomain() const;
	void OnDomainChanged(const EMaterialDomain InDomain);
	bool CanChangeDomain() const;

	EBlendMode GetSelectedBlendMode() const;
	void OnBlendModeChanged(const EBlendMode InBlendMode);
	bool CanChangeBlendType() const;

	ECheckBoxState IsMaterialUnlit() const;
	void OnMaterialUnlitChanged(const ECheckBoxState InNewCheckState);

	bool CanMaterialBeAnimated() const;
	ECheckBoxState IsMaterialAnimated() const;
	void OnMaterialAnimatedChanged(const ECheckBoxState InNewCheckState);

	bool CanChangeMaterialShadingModel() const;

	void OnUndo();
};
