// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ControlRigBlueprint.h"
#include "EdGraph/EdGraph.h"
#include "Graph/ControlRigGraphNode.h"
#include "Rigs/RigHierarchy.h"
#include "Rigs/RigHierarchyController.h"
#include "RigVMModel/RigVMGraph.h"
#include "RigVMCore/RigVM.h"
#include "Drawing/ControlRigDrawContainer.h"
#include "ControlRigGraph.generated.h"

class UControlRigBlueprint;
class UControlRigGraphSchema;
class UControlRig;
class URigVMController;
struct FRigCurveContainer;

DECLARE_MULTICAST_DELEGATE_OneParam(FControlRigGraphNodeClicked, UControlRigGraphNode*);

UCLASS()
class CONTROLRIGDEVELOPER_API UControlRigGraph : public UEdGraph, public IRigVMEditorSideObject
{
	GENERATED_BODY()

public:
	UControlRigGraph();

	/** IRigVMEditorSideObject interface */
	virtual FRigVMClient* GetRigVMClient() const override;
	virtual FString GetRigVMNodePath() const override;
	virtual void HandleRigVMGraphRenamed(const FString& InOldNodePath, const FString& InNewNodePath) override;

	/** Set up this graph */
	void Initialize(UControlRigBlueprint* InBlueprint);

	/** Get the skeleton graph schema */
	const UControlRigGraphSchema* GetControlRigGraphSchema();

#if WITH_EDITORONLY_DATA
	/** Customize blueprint changes based on backwards compatibility */
	virtual void Serialize(FArchive& Ar) override;
#endif
#if WITH_EDITOR

	FORCEINLINE const TArray<TSharedPtr<FString>>* GetBoneNameList(URigVMPin* InPin = nullptr) const
	{
		return GetElementNameList(ERigElementType::Bone);
	}
	FORCEINLINE const TArray<TSharedPtr<FString>>* GetControlNameList(URigVMPin* InPin = nullptr) const
	{
		return GetElementNameList(ERigElementType::Control);
	}
	FORCEINLINE const TArray<TSharedPtr<FString>>* GetControlNameListWithoutAnimationChannels(URigVMPin* InPin = nullptr) const
	{
		return &ControlNameListWithoutAnimationChannels;
	}
	FORCEINLINE const TArray<TSharedPtr<FString>>* GetNullNameList(URigVMPin* InPin = nullptr) const
	{
		return GetElementNameList(ERigElementType::Null);
	}
	FORCEINLINE const TArray<TSharedPtr<FString>>* GetCurveNameList(URigVMPin* InPin = nullptr) const
	{
		return GetElementNameList(ERigElementType::Curve);
	}

	void CacheNameLists(URigHierarchy* InHierarchy, const FControlRigDrawContainer* DrawContainer, TArray<TSoftObjectPtr<UControlRigShapeLibrary>> ShapeLibraries);
	const TArray<TSharedPtr<FString>>* GetElementNameList(ERigElementType InElementType = ERigElementType::Bone) const;
	const TArray<TSharedPtr<FString>>* GetElementNameList(URigVMPin* InPin = nullptr) const;
	const TArray<TSharedPtr<FString>> GetSelectedElementsNameList() const;
	const TArray<TSharedPtr<FString>>* GetDrawingNameList(URigVMPin* InPin = nullptr) const;
	const TArray<TSharedPtr<FString>>* GetEntryNameList(URigVMPin* InPin = nullptr) const;
	const TArray<TSharedPtr<FString>>* GetShapeNameList(URigVMPin* InPin = nullptr) const;

	bool bSuspendModelNotifications;
	bool bIsTemporaryGraphForCopyPaste;

	UEdGraphNode* FindNodeForModelNodeName(const FName& InModelNodeName, const bool bCacheIfRequired = true);

	UControlRigBlueprint* GetBlueprint() const;
	URigVMGraph* GetModel() const;
	URigVMController* GetController() const;
	bool IsRootGraph() const { return GetRootGraph() == this; }
	const UControlRigGraph* GetRootGraph() const;

	void HandleModifiedEvent(ERigVMGraphNotifType InNotifType, URigVMGraph* InGraph, UObject* InSubject);

	int32 GetInstructionIndex(const UControlRigGraphNode* InNode, bool bAsInput);

	UPROPERTY()
	FString ModelNodePath;

	UPROPERTY()
	bool bIsFunctionDefinition;

	FControlRigPublicFunctionData GetPublicFunctionData() const;

private:

	template<class T>
	static bool IncludeElementInNameList(const T* InElement)
	{
		return true;
	}

	template<class T>
	void CacheNameListForHierarchy(URigHierarchy* InHierarchy, TArray<TSharedPtr<FString>>& OutNameList, bool bFilter = true)
	{
        TArray<FString> Names;
		for (auto Element : *InHierarchy)
		{
			if(Element->IsA<T>())
			{
				if(!bFilter || IncludeElementInNameList<T>(Cast<T>(Element)))
				{
					Names.Add(Element->GetName().ToString());
				}
			}
		}
		Names.Sort();

		OutNameList.Reset();
		OutNameList.Add(MakeShared<FString>(FName(NAME_None).ToString()));
		for (const FString& Name : Names)
		{
			OutNameList.Add(MakeShared<FString>(Name));
		}
	}

	template<class T>
	void CacheNameList(const T& ElementList, TArray<TSharedPtr<FString>>& OutNameList)
	{
		TArray<FString> Names;
		for (auto Element : ElementList)
		{
			Names.Add(Element.Name.ToString());
		}
		Names.Sort();

		OutNameList.Reset();
		OutNameList.Add(MakeShared<FString>(FName(NAME_None).ToString()));
		for (const FString& Name : Names)
		{
			OutNameList.Add(MakeShared<FString>(Name));
		}
	}

	TMap<ERigElementType, TArray<TSharedPtr<FString>>> ElementNameLists;
	TArray<TSharedPtr<FString>>	ControlNameListWithoutAnimationChannels;
	TArray<TSharedPtr<FString>> DrawingNameList;
	TArray<TSharedPtr<FString>> EntryNameList;
	TArray<TSharedPtr<FString>> ShapeNameList;
	int32 LastHierarchyTopologyVersion;

	bool bIsSelecting;

	FControlRigGraphNodeClicked OnGraphNodeClicked;

	TMap<URigVMNode*, TPair<int32, int32>> CachedInstructionIndices;

	void RemoveNode(UEdGraphNode* InNode);

#endif
#if WITH_EDITORONLY_DATA

	UPROPERTY(transient)
	TObjectPtr<URigVMController> TemplateController;

#endif
#if WITH_EDITOR

	URigVMController* GetTemplateController();

protected:
	void HandleVMCompiledEvent(UObject* InCompiledObject, URigVM* InVM);

private:
	static TArray<TSharedPtr<FString>> EmptyElementNameList;
	TMap<FName, UEdGraphNode*> ModelNodePathToEdNode;
	mutable TWeakObjectPtr<URigVMGraph> CachedModelGraph;

	friend class UControlRigUnitNodeSpawner;
	friend class UControlRigVariableNodeSpawner;
	friend class UControlRigParameterNodeSpawner;
	friend class UControlRigRerouteNodeSpawner;
	friend class UControlRigBranchNodeSpawner;
	friend class UControlRigIfNodeSpawner;
	friend class UControlRigSelectNodeSpawner;
	friend class UControlRigTemplateNodeSpawner;
	friend class UControlRigEnumNodeSpawner;
	friend class UControlRigFunctionRefNodeSpawner;
	friend class UControlRigArrayNodeSpawner;
	friend class UControlRigInvokeEntryNodeSpawner;

#endif
	friend class UControlRigGraphNode;
	friend class FControlRigEditor;
	friend class SControlRigGraphNode;
	friend class UControlRigBlueprint;
};

template<>
inline bool UControlRigGraph::IncludeElementInNameList<FRigControlElement>(const FRigControlElement* InElement)
{
	return !InElement->IsAnimationChannel();
}
