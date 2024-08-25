// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "NiagaraNodeFunctionCall.h"
#include "NiagaraNodeAssignment.h"
#include "NiagaraSimulationStageBase.h"
#include "ViewModels/HierarchyEditor/NiagaraHierarchyViewModelBase.h"
#include "NiagaraSummaryViewViewModel.generated.h"

class UNiagaraStackEventHandlerPropertiesItem;
class FNiagaraEmitterViewModel;

UCLASS()
class UNiagaraHierarchySummaryDataRefreshContext : public UNiagaraHierarchyDataRefreshContext
{
	GENERATED_BODY()

public:
	UPROPERTY(Transient)
	TArray<TObjectPtr<UNiagaraRendererProperties>> Renderers;

	TSharedPtr<FNiagaraEmitterViewModel> EmitterViewModel;
};

UCLASS(MinimalAPI)
class UNiagaraHierarchyModule : public UNiagaraHierarchyItem
{
	GENERATED_BODY()
public:
	NIAGARAEDITOR_API void Initialize(const UNiagaraNodeFunctionCall& ModuleNode);
};

UCLASS(MinimalAPI)
class UNiagaraHierarchyModuleInput : public UNiagaraHierarchyItem
{
	GENERATED_BODY()

public:
	UNiagaraHierarchyModuleInput() {}
	virtual ~UNiagaraHierarchyModuleInput() override {}

	NIAGARAEDITOR_API void Initialize(const UNiagaraNodeFunctionCall& FunctionCall, FGuid InputGuid);

	void SetDisplayNameOverride(const FText& InText) { DisplayNameOverride = InText; }
	FText GetDisplayNameOverride() const { return DisplayNameOverride; }

	FText GetTooltipOverride() const { return TooltipOverride; }
private:
	/** If specified, will override how this input is presented in the stack. */
	UPROPERTY(EditAnywhere, Category="Niagara")
	FText DisplayNameOverride;

	/** If specified, will override how the tooltip of this input in the stack. */
	UPROPERTY(EditAnywhere, Category="Niagara", meta = (MultiLine = "true"))
	FText TooltipOverride;
};

UCLASS(MinimalAPI)
class UNiagaraHierarchyAssignmentInput : public UNiagaraHierarchyItem
{
	GENERATED_BODY()

public:
	UNiagaraHierarchyAssignmentInput() {}
	virtual ~UNiagaraHierarchyAssignmentInput() override {}

	NIAGARAEDITOR_API void Initialize(const UNiagaraNodeAssignment& AssignmentNode, FName AssignmentTarget);

	FText GetTooltipOverride() const { return TooltipOverride; }
private:
	/** If specified, will override how the tooltip of this input in the stack. */
	UPROPERTY(EditAnywhere, Category="Niagara", meta = (MultiLine = "true"))
	FText TooltipOverride;
};

UCLASS(MinimalAPI)
class UNiagaraHierarchyEmitterProperties : public UNiagaraHierarchyItem
{
	GENERATED_BODY()
public:
	void Initialize(const FVersionedNiagaraEmitter& Emitter);
};

UCLASS(MinimalAPI)
class UNiagaraHierarchyRenderer : public UNiagaraHierarchyItem
{
	GENERATED_BODY()
public:
	NIAGARAEDITOR_API void Initialize(const UNiagaraRendererProperties& Renderer);
};

UCLASS(MinimalAPI)
class UNiagaraHierarchyEventHandler : public UNiagaraHierarchyItem
{
	GENERATED_BODY()
public:
	NIAGARAEDITOR_API void Initialize(const FNiagaraEventScriptProperties& EventHandler);
};

UCLASS(MinimalAPI)
class UNiagaraHierarchyEventHandlerProperties : public UNiagaraHierarchyItem
{
	GENERATED_BODY()
public:
	NIAGARAEDITOR_API void Initialize(const FNiagaraEventScriptProperties& EventHandler);
	
	static NIAGARAEDITOR_API FNiagaraHierarchyIdentity MakeIdentity(const FNiagaraEventScriptProperties& EventHandler);
};

UCLASS(MinimalAPI)
class UNiagaraHierarchySimStage : public UNiagaraHierarchyItem
{
	GENERATED_BODY()
public:
	NIAGARAEDITOR_API void Initialize(const UNiagaraSimulationStageBase& SimStage);
};

UCLASS(MinimalAPI)
class UNiagaraHierarchySimStageProperties : public UNiagaraHierarchyItem
{
	GENERATED_BODY()
public:
	NIAGARAEDITOR_API void Initialize(const UNiagaraSimulationStageBase& SimStage);
	
	static NIAGARAEDITOR_API FNiagaraHierarchyIdentity MakeIdentity(const UNiagaraSimulationStageBase& SimStage);
};

UCLASS(MinimalAPI)
class UNiagaraSummaryViewViewModel : public UNiagaraHierarchyViewModelBase
{
	GENERATED_BODY()
public:
	UNiagaraSummaryViewViewModel() {}
	virtual ~UNiagaraSummaryViewViewModel() override
	{
	}

	NIAGARAEDITOR_API void Initialize(TSharedRef<FNiagaraEmitterViewModel> EmitterViewModel);
	NIAGARAEDITOR_API virtual void FinalizeInternal() override;
	
	NIAGARAEDITOR_API TSharedRef<FNiagaraEmitterViewModel> GetEmitterViewModel() const;
	
	NIAGARAEDITOR_API virtual UNiagaraHierarchyRoot* GetHierarchyRoot() const override;
	NIAGARAEDITOR_API virtual TSharedPtr<FNiagaraHierarchyItemViewModelBase> CreateViewModelForData(UNiagaraHierarchyItemBase* ItemBase, TSharedPtr<FNiagaraHierarchyItemViewModelBase> Parent) override;
	
	NIAGARAEDITOR_API virtual void PrepareSourceItems(UNiagaraHierarchyRoot* SourceRoot, TSharedPtr<FNiagaraHierarchyRootViewModel>) override;
	NIAGARAEDITOR_API virtual void SetupCommands() override;
	
	NIAGARAEDITOR_API virtual TSharedRef<FNiagaraHierarchyDragDropOp> CreateDragDropOp(TSharedRef<FNiagaraHierarchyItemViewModelBase> Item) override;
	
	virtual bool SupportsDetailsPanel() override { return true; }
	NIAGARAEDITOR_API virtual TArray<TTuple<UClass*, FOnGetDetailCustomizationInstance>> GetInstanceCustomizations() override;

	NIAGARAEDITOR_API TMap<FGuid, UObject*> GetObjectsForProperties();

	NIAGARAEDITOR_API UNiagaraNodeFunctionCall* GetFunctionCallNode(const FGuid& NodeIdentity);
	NIAGARAEDITOR_API void ClearFunctionCallNodeCache(const FGuid& NodeIdentity);
	NIAGARAEDITOR_API TOptional<struct FInputData> GetInputData(const UNiagaraHierarchyModuleInput& Input);

private:
	NIAGARAEDITOR_API void OnScriptGraphChanged(const FEdGraphEditAction& Action, const UNiagaraScript& Script);
	NIAGARAEDITOR_API void OnRenderersChanged();
	NIAGARAEDITOR_API void OnSimStagesChanged();
	NIAGARAEDITOR_API void OnEventHandlersChanged();
protected:
	// The cache is used to speed up access across different inputs, as the view models for both regular inputs & modules, dynamic inputs & assignment nodes need to 'find' these nodes which is expensive
	TMap<FGuid, TWeakObjectPtr<UNiagaraNodeFunctionCall>> FunctionCallCache;
	TWeakPtr<FNiagaraEmitterViewModel> EmitterViewModelWeak;
};

/** The view model for both module nodes & dynamic input nodes */
struct FNiagaraFunctionViewModel : public FNiagaraHierarchyItemViewModel
{
	FNiagaraFunctionViewModel(UNiagaraHierarchyModule* HierarchyModule, TSharedPtr<FNiagaraHierarchyItemViewModelBase> InParent, TWeakObjectPtr<UNiagaraSummaryViewViewModel> ViewModel, bool bInIsForHierarchy)
	: FNiagaraHierarchyItemViewModel(HierarchyModule, InParent, ViewModel, bInIsForHierarchy)
	{ }

	virtual ~FNiagaraFunctionViewModel() override;
	
	TWeakObjectPtr<UNiagaraNodeFunctionCall> GetFunctionCallNode() const;	
	
	bool IsFromBaseEmitter() const;

	void SetSection(UNiagaraHierarchySection& InSection) { Section = &InSection; }

	bool IsDynamicInput() const { return bIsDynamicInput; }
private:	
	virtual void Initialize() override;
	virtual void RefreshChildrenDataInternal() override;
	void RefreshChildrenInputs(bool bClearCache = false) const;
	
	virtual FString ToString() const override;

	virtual FCanPerformActionResults IsEditableByUser() override;
	virtual bool CanHaveChildren() const override { return bIsForHierarchy == false; }
	virtual bool CanRenameInternal() override { return false; }
	virtual FCanPerformActionResults CanDropOnInternal(TSharedPtr<FNiagaraHierarchyItemViewModelBase>, EItemDropZone ItemDropZone) override;
	virtual const UNiagaraHierarchySection* GetSectionInternal() const override;
	
	virtual bool RepresentsExternalData() const override { return true; }
	virtual bool DoesExternalDataStillExist(const UNiagaraHierarchyDataRefreshContext* Context) const override { ClearCache(); return GetFunctionCallNode().IsValid(); }
	
	void OnScriptApplied(UNiagaraScript* NiagaraScript, FGuid Guid);

	void ClearCache() const;
private:
	FDelegateHandle OnScriptAppliedHandle;
	mutable TOptional<bool> IsFromBaseEmitterCache;
	TWeakObjectPtr<UNiagaraHierarchySection> Section;
	bool bIsDynamicInput = false;
};

struct FInputData
{
	FName InputName;
	FNiagaraTypeDefinition Type;
	FNiagaraVariableMetaData MetaData;
	bool bIsStatic = false;
	TArray<FGuid> ChildrenInputGuids;
	UNiagaraNodeFunctionCall* FunctionCallNode;
};

struct FNiagaraModuleInputViewModel : public FNiagaraHierarchyItemViewModel
{	
	FNiagaraModuleInputViewModel(UNiagaraHierarchyModuleInput* ModuleInput, TSharedPtr<FNiagaraHierarchyItemViewModelBase> InParent, TWeakObjectPtr<UNiagaraSummaryViewViewModel> ViewModel, bool bInIsForHierarchy)
	: FNiagaraHierarchyItemViewModel(ModuleInput, InParent, ViewModel, bInIsForHierarchy) {}

public:
	TOptional<FInputData> GetInputData() const;
	FText GetSummaryInputNameOverride() const;
	
protected:
	TWeakObjectPtr<UNiagaraNodeFunctionCall> GetModuleNode() const;

	virtual FString ToString() const override;
	virtual TArray<FString> GetSearchTerms() const override;
	
	bool IsFromBaseEmitter() const;

	void ClearCache() const;

	void RefreshChildDynamicInputs(bool bClearCache = false);
	
	virtual bool CanHaveChildren() const override;
	virtual FCanPerformActionResults IsEditableByUser() override;
	virtual bool RepresentsExternalData() const override { return true; }
	virtual bool DoesExternalDataStillExist(const UNiagaraHierarchyDataRefreshContext* Context) const override { ClearCache(); return GetInputData().IsSet(); }
	virtual void RefreshChildrenDataInternal() override;

	virtual FCanPerformActionResults CanDropOnInternal(TSharedPtr<FNiagaraHierarchyItemViewModelBase>, EItemDropZone ItemDropZone) override;
	virtual void OnDroppedOnInternal(TSharedPtr<FNiagaraHierarchyItemViewModelBase> DroppedItem, EItemDropZone ItemDropZone) override;

	virtual void PopulateDynamicContextMenuSection(FToolMenuSection& DynamicSection) override;
private:
	TOptional<FInputData> FindInputDataInternal() const;
	void AddNativeChildrenInputs();
	bool CanAddNativeChildrenInputs() const;
	TArray<FNiagaraHierarchyIdentity> GetNativeChildInputIdentities() const;
private:
	mutable TOptional<FInputData> InputDataCache;
	mutable TOptional<bool> IsFromBaseEmitterCache;
};

struct FNiagaraAssignmentInputViewModel : public FNiagaraHierarchyItemViewModel
{	
	FNiagaraAssignmentInputViewModel(UNiagaraHierarchyAssignmentInput* ModuleInput, TSharedPtr<FNiagaraHierarchyItemViewModelBase> InParent, TWeakObjectPtr<UNiagaraSummaryViewViewModel> ViewModel, bool bInIsForHierarchy)
	: FNiagaraHierarchyItemViewModel(ModuleInput, InParent, ViewModel, bInIsForHierarchy) {}

	virtual FCanPerformActionResults CanDropOnInternal(TSharedPtr<FNiagaraHierarchyItemViewModelBase>, EItemDropZone ItemDropZone) override;
	
	TWeakObjectPtr<UNiagaraNodeAssignment> GetAssignmentNode() const;
	TOptional<FNiagaraStackGraphUtilities::FMatchingFunctionInputData> GetInputData() const;

	virtual bool CanHaveChildren() const override { return false; }

	virtual FString ToString() const override;
	virtual TArray<FString> GetSearchTerms() const override;
	
	bool IsFromBaseEmitter() const;

	void ClearCache() const;
protected:
	virtual FCanPerformActionResults IsEditableByUser() override;
	virtual bool RepresentsExternalData() const override { return true; }
	virtual bool DoesExternalDataStillExist(const UNiagaraHierarchyDataRefreshContext* Context) const override { ClearCache(); return GetInputData().IsSet(); }
private:
	TOptional<FNiagaraStackGraphUtilities::FMatchingFunctionInputData> FindInputDataInternal() const;
private:
	mutable TWeakObjectPtr<UNiagaraNodeAssignment> AssignmentNodeCache;
	mutable TOptional<FNiagaraStackGraphUtilities::FMatchingFunctionInputData> InputDataCache;
	mutable TOptional<bool> IsFromBaseEmitterCache;
};

struct FNiagaraHierarchySummaryCategoryViewModel : public FNiagaraHierarchyCategoryViewModel
{
	FNiagaraHierarchySummaryCategoryViewModel(UNiagaraHierarchyCategory* Category, TSharedPtr<FNiagaraHierarchyItemViewModelBase> InParent, TWeakObjectPtr<UNiagaraSummaryViewViewModel> ViewModel, bool bInIsForHierarchy)
	: FNiagaraHierarchyCategoryViewModel(Category, InParent, ViewModel, bInIsForHierarchy) {}
	
	bool IsFromBaseEmitter() const;
	
protected:
	virtual FCanPerformActionResults IsEditableByUser() override;
	
	mutable TOptional<bool> IsFromBaseEmitterCache;
};

struct FNiagaraHierarchyPropertyViewModel : public FNiagaraHierarchyItemViewModel
{
	FNiagaraHierarchyPropertyViewModel(UNiagaraHierarchyObjectProperty* ObjectProperty, TSharedPtr<FNiagaraHierarchyItemViewModelBase> InParent, TWeakObjectPtr<UNiagaraSummaryViewViewModel> ViewModel, bool bInIsForHierarchy)
		: FNiagaraHierarchyItemViewModel(ObjectProperty, InParent, ViewModel, bInIsForHierarchy) {}

	virtual FString ToString() const override;

	bool IsFromBaseEmitter() const;
protected:
	virtual bool RepresentsExternalData() const override { return true; }
	virtual bool DoesExternalDataStillExist(const UNiagaraHierarchyDataRefreshContext* Context) const override;
	
	virtual FCanPerformActionResults IsEditableByUser() override;
	
	mutable TOptional<bool> IsFromBaseEmitterCache;	
};

struct FNiagaraHierarchyRendererViewModel : public FNiagaraHierarchyItemViewModel
{
	FNiagaraHierarchyRendererViewModel(UNiagaraHierarchyRenderer* Renderer, TSharedPtr<FNiagaraHierarchyItemViewModelBase> InParent, TWeakObjectPtr<UNiagaraSummaryViewViewModel> ViewModel, bool bInIsForHierarchy)
	: FNiagaraHierarchyItemViewModel(Renderer, InParent, ViewModel, bInIsForHierarchy) {}

	virtual FString ToString() const override;
	virtual bool CanRenameInternal() override { return false; }

	UNiagaraRendererProperties* GetRendererProperties() const;

	void SetSection(UNiagaraHierarchySection& InSection) { Section = &InSection; }

	bool IsFromBaseEmitter() const;
protected:
	virtual void RefreshChildrenDataInternal() override;
	
	virtual FCanPerformActionResults IsEditableByUser() override;
	virtual bool CanHaveChildren() const override { return bIsForHierarchy == false; }
	
	virtual bool RepresentsExternalData() const override { return true; }
	virtual bool DoesExternalDataStillExist(const UNiagaraHierarchyDataRefreshContext* Context) const override { return GetRendererProperties() != nullptr;}

	virtual const UNiagaraHierarchySection* GetSectionInternal() const override;
	
	TWeakObjectPtr<UNiagaraHierarchySection> Section;
	mutable TOptional<bool> IsFromBaseEmitterCache;
};

/** Emitter Properties currently don't list their individual properties since it's a mix of data of FVersionedNiagaraEmitterData & actual properties on the emitter which requires customization. */
struct FNiagaraHierarchyEmitterPropertiesViewModel : public FNiagaraHierarchyItemViewModel
{
	FNiagaraHierarchyEmitterPropertiesViewModel(UNiagaraHierarchyEmitterProperties* EmitterProperties, TSharedPtr<FNiagaraHierarchyItemViewModelBase> InParent, TWeakObjectPtr<UNiagaraSummaryViewViewModel> ViewModel, bool bInIsForHierarchy)
	: FNiagaraHierarchyItemViewModel(EmitterProperties, InParent, ViewModel, bInIsForHierarchy) {}

	virtual FString ToString() const override;
	virtual bool CanRenameInternal() override { return false; }

	void SetSection(UNiagaraHierarchySection& InSection) { Section = &InSection; }

	bool IsFromBaseEmitter() const;
protected:	
	virtual FCanPerformActionResults IsEditableByUser() override;
	virtual bool CanHaveChildren() const override { return bIsForHierarchy == false; }
	
	virtual bool RepresentsExternalData() const override { return true; }
	virtual bool DoesExternalDataStillExist(const UNiagaraHierarchyDataRefreshContext* Context) const override { return true; }

	virtual const UNiagaraHierarchySection* GetSectionInternal() const override;
	
	TWeakObjectPtr<UNiagaraHierarchySection> Section;
	mutable TOptional<bool> IsFromBaseEmitterCache;
};

struct FNiagaraHierarchyEventHandlerViewModel : public FNiagaraHierarchyItemViewModel
{
	FNiagaraHierarchyEventHandlerViewModel(UNiagaraHierarchyEventHandler* EventHandler, TSharedPtr<FNiagaraHierarchyItemViewModelBase> InParent, TWeakObjectPtr<UNiagaraSummaryViewViewModel> ViewModel, bool bInIsForHierarchy)
	: FNiagaraHierarchyItemViewModel(EventHandler, InParent, ViewModel, bInIsForHierarchy) {}

	virtual FString ToString() const override;
	virtual bool CanRenameInternal() override { return false; }

	FNiagaraEventScriptProperties* GetEventScriptProperties() const;

	void SetSection(UNiagaraHierarchySection& InSection) { Section = &InSection; }

	bool IsFromBaseEmitter() const;
protected:
	virtual void RefreshChildrenDataInternal() override;
	
	virtual FCanPerformActionResults IsEditableByUser() override;
	virtual bool CanHaveChildren() const override { return bIsForHierarchy == false; }
	
	virtual bool RepresentsExternalData() const override { return true; }
	virtual bool DoesExternalDataStillExist(const UNiagaraHierarchyDataRefreshContext* Context) const override { return GetEventScriptProperties() != nullptr;}

	virtual const UNiagaraHierarchySection* GetSectionInternal() const override;
	
	TWeakObjectPtr<UNiagaraHierarchySection> Section;
	mutable TOptional<bool> IsFromBaseEmitterCache;
};

struct FNiagaraHierarchyEventHandlerPropertiesViewModel : public FNiagaraHierarchyItemViewModel
{
	FNiagaraHierarchyEventHandlerPropertiesViewModel(UNiagaraHierarchyEventHandlerProperties* EventHandlerProperties, TSharedPtr<FNiagaraHierarchyItemViewModelBase> InParent, TWeakObjectPtr<UNiagaraSummaryViewViewModel> ViewModel, bool bInIsForHierarchy)
	: FNiagaraHierarchyItemViewModel(EventHandlerProperties, InParent, ViewModel, bInIsForHierarchy) {}

	virtual FString ToString() const override;
	virtual bool CanRenameInternal() override { return false; }

	FNiagaraEventScriptProperties* GetEventScriptProperties() const;

	bool IsFromBaseEmitter() const;
protected:
	virtual void RefreshChildrenDataInternal() override;

	virtual FCanPerformActionResults IsEditableByUser() override;
	virtual bool CanHaveChildren() const override { return bIsForHierarchy == false; }
	
	virtual bool RepresentsExternalData() const override { return true; }
	virtual bool DoesExternalDataStillExist(const UNiagaraHierarchyDataRefreshContext* Context) const override { return GetEventScriptProperties() != nullptr;}

	mutable TOptional<bool> IsFromBaseEmitterCache;
};

struct FNiagaraHierarchySimStageViewModel : public FNiagaraHierarchyItemViewModel
{
	FNiagaraHierarchySimStageViewModel(UNiagaraHierarchySimStage* SimStage, TSharedPtr<FNiagaraHierarchyItemViewModelBase> InParent, TWeakObjectPtr<UNiagaraSummaryViewViewModel> ViewModel, bool bInIsForHierarchy)
	: FNiagaraHierarchyItemViewModel(SimStage, InParent, ViewModel, bInIsForHierarchy) {}

	virtual FString ToString() const override;
	virtual bool CanRenameInternal() override { return false; }

	UNiagaraSimulationStageBase* GetSimStage() const;

	void SetSection(UNiagaraHierarchySection& InSection) { Section = &InSection; }

	bool IsFromBaseEmitter() const;
protected:
	virtual void RefreshChildrenDataInternal() override;
	
	virtual FCanPerformActionResults IsEditableByUser() override;
	virtual bool CanHaveChildren() const override { return bIsForHierarchy == false; }
	
	virtual bool RepresentsExternalData() const override { return true; }
	virtual bool DoesExternalDataStillExist(const UNiagaraHierarchyDataRefreshContext* Context) const override { return GetSimStage() != nullptr;}

	virtual const UNiagaraHierarchySection* GetSectionInternal() const override;
	
	TWeakObjectPtr<UNiagaraHierarchySection> Section;
	mutable TOptional<bool> IsFromBaseEmitterCache;
};

struct FNiagaraHierarchySimStagePropertiesViewModel : public FNiagaraHierarchyItemViewModel
{
	FNiagaraHierarchySimStagePropertiesViewModel(UNiagaraHierarchySimStageProperties* SimStage, TSharedPtr<FNiagaraHierarchyItemViewModelBase> InParent, TWeakObjectPtr<UNiagaraSummaryViewViewModel> ViewModel, bool bInIsForHierarchy)
	: FNiagaraHierarchyItemViewModel(SimStage, InParent, ViewModel, bInIsForHierarchy) {}

	virtual FString ToString() const override;
	virtual bool CanRenameInternal() override { return false; }

	UNiagaraSimulationStageBase* GetSimStage() const;

	bool IsFromBaseEmitter() const;
protected:
	virtual void RefreshChildrenDataInternal() override;

	virtual FCanPerformActionResults IsEditableByUser() override;
	virtual bool CanHaveChildren() const override { return bIsForHierarchy == false; }
	
	virtual bool RepresentsExternalData() const override { return true; }
	virtual bool DoesExternalDataStillExist(const UNiagaraHierarchyDataRefreshContext* Context) const override { return GetSimStage() != nullptr;}

	mutable TOptional<bool> IsFromBaseEmitterCache;
};

class FNiagaraHierarchyInputParameterHierarchyDragDropOp : public FNiagaraHierarchyDragDropOp
{
public:
	DRAG_DROP_OPERATOR_TYPE(FNiagaraInputParameterHierarchyDragDropOp, FNiagaraHierarchyDragDropOp)

	FNiagaraHierarchyInputParameterHierarchyDragDropOp(TSharedPtr<FNiagaraModuleInputViewModel> InputViewModel) : FNiagaraHierarchyDragDropOp(InputViewModel) {}
	
	virtual TSharedRef<SWidget> CreateCustomDecorator() const override;
};

class SNiagaraHierarchyModule : public SCompoundWidget
{
	SLATE_BEGIN_ARGS(SNiagaraHierarchyModule)
		{}
	SLATE_END_ARGS()

public:
	void Construct(const FArguments& InArgs, TSharedPtr<struct FNiagaraFunctionViewModel> InModuleViewModel);
		
	FText GetModuleDisplayName() const;
	
private:
	TWeakPtr<struct FNiagaraFunctionViewModel> ModuleViewModel;
	TSharedPtr<SInlineEditableTextBlock> InlineEditableTextBlock;
};
