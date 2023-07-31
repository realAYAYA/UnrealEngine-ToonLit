// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Templates/SharedPointer.h"
#include "UObject/ObjectKey.h"
#include "NiagaraCurveSelectionViewModel.generated.h"

class FCompileConstantResolver;
class UNiagaraDataInterfaceCurveBase;
struct FNiagaraEmitterHandle;
class UNiagaraNodeFunctionCall;
class FNiagaraPlaceholderDataInterfaceHandle;
class UNiagaraScript;
class UNiagaraStackEditorData;
class UNiagaraSystem;
class FNiagaraSystemViewModel;
struct FRichCurve;

struct NIAGARAEDITOR_API FNiagaraCurveSelectionTreeNodeDataId
{
	FNiagaraCurveSelectionTreeNodeDataId()
		: Object(nullptr)
	{
	}

	bool operator==(const FNiagaraCurveSelectionTreeNodeDataId& Other) const;

	FName UniqueName;
	FGuid Guid;
	UObject* Object;

	static FNiagaraCurveSelectionTreeNodeDataId FromUniqueName(FName UniqueName);
	static FNiagaraCurveSelectionTreeNodeDataId FromGuid(FGuid Guid);
	static FNiagaraCurveSelectionTreeNodeDataId FromObject(UObject* Object);
};

enum class ENiagaraCurveSelectionNodeStyleMode
{
	TopLevelObject,
	Script,
	Module,
	DynamicInput,
	DataInterface,
	CurveComponent
};

struct NIAGARAEDITOR_API FNiagaraCurveSelectionTreeNode : TSharedFromThis<FNiagaraCurveSelectionTreeNode>
{
public:
	FNiagaraCurveSelectionTreeNode();

	const FNiagaraCurveSelectionTreeNodeDataId& GetDataId() const;

	void SetDataId(const FNiagaraCurveSelectionTreeNodeDataId& InDataId);

	FGuid GetNodeUniqueId() const;

	FText GetDisplayName() const;

	void SetDisplayName(FText InDisplayName);

	FText GetSecondDisplayName() const;

	void SetSecondDisplayName(FText InSecondDisplayName);

	ENiagaraCurveSelectionNodeStyleMode GetStyleMode() const;

	FName GetExecutionCategory() const;

	FName GetExecutionSubcategory() const;

	bool GetIsParameter() const;

	void SetStyle(ENiagaraCurveSelectionNodeStyleMode InStyleMode, FName InExecutionCategory, FName InExecutionSubcategory, bool bInIsParameter);

	TSharedPtr<FNiagaraCurveSelectionTreeNode> GetParent() const;

protected:
	void SetParent(TSharedPtr<FNiagaraCurveSelectionTreeNode> InParent);

public:
	const TArray<TSharedRef<FNiagaraCurveSelectionTreeNode>>& GetChildNodes() const;

	void SetChildNodes(const TArray<TSharedRef<FNiagaraCurveSelectionTreeNode>> InChildNodes);

	static TSharedPtr<FNiagaraCurveSelectionTreeNode> FindNodeWithDataId(const TArray<TSharedRef<FNiagaraCurveSelectionTreeNode>>& Nodes, FNiagaraCurveSelectionTreeNodeDataId DataId);

	TWeakObjectPtr<UNiagaraDataInterfaceCurveBase> GetCurveDataInterface() const;

	FRichCurve* GetCurve() const;

	FName GetCurveName() const;

	FLinearColor GetCurveColor() const;

	bool GetCurveIsReadOnly() const;

	void SetCurveDataInterface(UNiagaraDataInterfaceCurveBase* InCurveDataInterface);

	void SetPlaceholderDataInterfaceHandle(TSharedPtr<FNiagaraPlaceholderDataInterfaceHandle> InPlaceholderDataInterfaceHandle);

	void SetCurveData(UNiagaraDataInterfaceCurveBase* InCurveDataInterface, FRichCurve* InCurve, FName InCurveName, FLinearColor InCurveColor);

	const TOptional<FObjectKey>& GetDisplayedObjectKey() const;

	void SetDisplayedObjectKey(FObjectKey InDisplayedObjectKey);

	bool GetShowInTree() const;

	void SetShowInTree(bool bInShouldShowInTree);

	bool GetIsExpanded() const;

	void SetIsExpanded(bool bInIsExpanded);

	bool GetIsEnabled() const;

	void SetIsEnabled(bool bInIsEnabled);

	bool GetIsEnabledAndParentIsEnabled() const;

	const TArray<int32>& GetSortIndices() const;

	void UpdateSortIndices(int32 Index);

	void ResetCachedEnabledState();

	FSimpleMulticastDelegate& GetOnCurveChanged();

	void NotifyCurveChanged();

private:
	FNiagaraCurveSelectionTreeNodeDataId DataId;
	FGuid NodeUniqueId;
	FText DisplayName;
	FText SecondDisplayName;
	TWeakPtr<FNiagaraCurveSelectionTreeNode> ParentWeak;
	TArray<TSharedRef<FNiagaraCurveSelectionTreeNode>> ChildNodes;

	TWeakObjectPtr<UNiagaraDataInterfaceCurveBase> CurveDataInterface;
	TSharedPtr<FNiagaraPlaceholderDataInterfaceHandle> PlaceholderDataInterfaceHandle;
	FRichCurve* Curve;
	FName CurveName;
	FLinearColor CurveColor;

	ENiagaraCurveSelectionNodeStyleMode StyleMode;
	FName ExecutionCategory;
	FName ExecutionSubcategory;
	bool bIsParameter;
	TOptional<FObjectKey> DisplayedObjectKey;
	bool bShowInTree;
	bool bIsExpanded;
	bool bIsEnabled;
	mutable TOptional<bool> bIsEnabledAndParentIsEnabledCache;
	TArray<int32> SortIndices;

	FSimpleMulticastDelegate OnCurveChangedDelegate;
};

UCLASS()
class NIAGARAEDITOR_API UNiagaraCurveSelectionViewModel : public UObject
{
public:
	GENERATED_BODY()

public:
	DECLARE_MULTICAST_DELEGATE_OneParam(FOnRequestSelectNode, FGuid /* NodeIdToSelect */);

public:
	void Initialize(TSharedRef<FNiagaraSystemViewModel> InSystemViewModel);

	void Finalize();

	const TArray<TSharedRef<FNiagaraCurveSelectionTreeNode>>& GetRootNodes();

	void FocusAndSelectCurveDataInterface(UNiagaraDataInterfaceCurveBase& CurveDataInterface);

	void Refresh();

	void RefreshDeferred();

	void Tick();

	FSimpleMulticastDelegate& OnRefreshed();

	FOnRequestSelectNode& OnRequestSelectNode();

private:
	TSharedRef<FNiagaraCurveSelectionTreeNode> CreateNodeForCurveDataInterface(const FNiagaraCurveSelectionTreeNodeDataId& DataId, UNiagaraDataInterfaceCurveBase& CurveDataInterface, bool bIsParameter) const;

	TSharedPtr<FNiagaraCurveSelectionTreeNode> CreateNodeForUserParameters(TArray<TSharedRef<FNiagaraCurveSelectionTreeNode>> OldParentChildNodes, UNiagaraSystem& System) const;

	TSharedPtr<FNiagaraCurveSelectionTreeNode> CreateNodeForFunction(
		const TArray<TSharedRef<FNiagaraCurveSelectionTreeNode>> OldParentChildNodes,
		UNiagaraNodeFunctionCall& FunctionCallNode, UNiagaraStackEditorData& StackEditorData,
		FName ExecutionCategory, FName ExecutionSubCategory,
		FName InputName, bool bIsParameterDynamicInput,
		FCompileConstantResolver& ConstantResolver, FGuid& OwningEmitterHandleId) const;

	TSharedPtr<FNiagaraCurveSelectionTreeNode> CreateNodeForScript(
		TArray<TSharedRef<FNiagaraCurveSelectionTreeNode>> OldParentChildNodes,
		UNiagaraScript& Script, FString ScriptDisplayName, UNiagaraStackEditorData& StackEditorData,
		FName ExecutionCategory, FName ExecutionSubcategory,
		const FNiagaraEmitterHandle* OwningEmitterHandle) const;

	TSharedPtr<FNiagaraCurveSelectionTreeNode> CreateNodeForSystem(TArray<TSharedRef<FNiagaraCurveSelectionTreeNode>> OldParentChildNodes, UNiagaraSystem& System) const;

	TSharedPtr<FNiagaraCurveSelectionTreeNode> CreateNodeForEmitter(TArray<TSharedRef<FNiagaraCurveSelectionTreeNode>> OldParentChildNodes, const FNiagaraEmitterHandle& EmitterHandle) const;

	void DataInterfaceCurveChanged(TWeakObjectPtr<UNiagaraDataInterfaceCurveBase> ChangedCurveDataInterfaceWeak) const;

	void StackEditorDataChanged();

	void UserParametersChanged();

private:
	TWeakPtr<FNiagaraSystemViewModel> SystemViewModelWeak;

	TSharedPtr<FNiagaraCurveSelectionTreeNode> RootCurveSelectionTreeNode;

	FSimpleMulticastDelegate OnRefreshedDelegate;

	FOnRequestSelectNode OnRequestSelectNodeDelegate;

	mutable bool bHandlingInternalCurveChanged;

	bool bRefreshPending;
};