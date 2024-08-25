// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "EdGraph/EdGraph.h"
#include "EdGraph/RigVMEdGraphNode.h"
#include "RigVMModel/RigVMGraph.h"
#include "RigVMCore/RigVM.h"
#include "RigVMModel/RigVMClient.h"
#include "RigVMEdGraph.generated.h"

class URigVMBlueprint;
class URigVMEdGraphSchema;
class URigVMController;

struct FRigVMStringTag
{
public:
	
	FRigVMStringTag()
		: Name(NAME_None)
		, Color(FLinearColor::Red)
	{}

	explicit FRigVMStringTag(const FName& InName, const FLinearColor& InColor)
		: Name(InName)
		, Color(InColor)
	{
	}

	const FName& GetName() const
	{
		return Name;
	}
	
	const FLinearColor& GetColor() const
	{
		return Color;
	}

	bool IsValid() const
	{
		return !Name.IsNone();
	}

	bool Equals(const FName& InOther) const
	{
		return GetName().IsEqual(InOther, ENameCase::CaseSensitive);
	}

	bool Equals(const FRigVMStringTag& InOther) const
	{
		return Equals(InOther.GetName());
	}

private:
	FName Name;
	FLinearColor Color;
};

struct FRigVMStringWithTag
{
public:
	
	FRigVMStringWithTag() = default;

	FRigVMStringWithTag(const FString& InString, const FRigVMStringTag& InTag = FRigVMStringTag())
		: String(InString)
		, Tag(InTag)
	{
	}

	const FString& GetString() const
	{
		return String;
	}

	FString GetStringWithTag() const
	{
		if(HasTag())
		{
			static constexpr TCHAR Format[] = TEXT("%s (%s)");
			return FString::Printf(Format, *GetString(), *GetTag().GetName().ToString());
		}
		return GetString();
	}

	bool HasTag() const
	{
		return Tag.IsValid();
	}

	const FRigVMStringTag& GetTag() const
	{
		return Tag;
	}

	bool operator ==(const FRigVMStringWithTag& InOther) const
	{
		return Equals(InOther);
	}

	bool operator >(const FRigVMStringWithTag& InOther) const
	{
		return GetString() > InOther.GetString();
	}

	bool operator <(const FRigVMStringWithTag& InOther) const
	{
		return GetString() < InOther.GetString();
	}

	bool Equals(const FString& InOther) const
	{
		return GetString().Equals(InOther, ESearchCase::CaseSensitive);
	}

	bool Equals(const FRigVMStringWithTag& InOther) const
	{
		return Equals(InOther.GetString());
	}

private:
	FString String;
	FRigVMStringTag Tag;
};

DECLARE_MULTICAST_DELEGATE_OneParam(FRigVMEdGraphNodeClicked, URigVMEdGraphNode*);

UCLASS()
class RIGVMDEVELOPER_API URigVMEdGraph : public UEdGraph, public IRigVMEditorSideObject
{
	GENERATED_BODY()

public:
	URigVMEdGraph();

	/** IRigVMEditorSideObject interface */
	virtual FRigVMClient* GetRigVMClient() const override;
	virtual FString GetRigVMNodePath() const override;
	virtual void HandleRigVMGraphRenamed(const FString& InOldNodePath, const FString& InNewNodePath) override;

	/** Set up this graph */
	virtual void InitializeFromBlueprint(URigVMBlueprint* InBlueprint);

	/** Get the ed graph schema */
	const URigVMEdGraphSchema* GetRigVMEdGraphSchema();

#if WITH_EDITORONLY_DATA
	/** Customize blueprint changes based on backwards compatibility */
	virtual void Serialize(FArchive& Ar) override;
#endif

#if WITH_EDITOR

	bool bSuspendModelNotifications;
	bool bIsTemporaryGraphForCopyPaste;
	bool bIsSelecting;

	UEdGraphNode* FindNodeForModelNodeName(const FName& InModelNodeName, const bool bCacheIfRequired = true);

	URigVMBlueprint* GetBlueprint() const;
	URigVMGraph* GetModel() const;
	URigVMController* GetController() const;
	bool IsRootGraph() const { return GetRootGraph() == this; }
	const URigVMEdGraph* GetRootGraph() const;

	void HandleModifiedEvent(ERigVMGraphNotifType InNotifType, URigVMGraph* InGraph, UObject* InSubject);
	virtual bool HandleModifiedEvent_Internal(ERigVMGraphNotifType InNotifType, URigVMGraph* InGraph, UObject* InSubject);

	int32 GetInstructionIndex(const URigVMEdGraphNode* InNode, bool bAsInput);

	void CacheEntryNameList();
	const TArray<TSharedPtr<FRigVMStringWithTag>>* GetEntryNameList(URigVMPin* InPin = nullptr) const;

	virtual const TArray<TSharedPtr<FRigVMStringWithTag>>* GetNameListForWidget(const FString& InWidgetName) const { return nullptr; }

	UPROPERTY()
	FString ModelNodePath;

	UPROPERTY()
	bool bIsFunctionDefinition;

protected:
	using Super::AddNode;
	virtual void AddNode(UEdGraphNode* NodeToAdd, bool bUserAction = false, bool bSelectNewNode = true) override;

private:

	FRigVMEdGraphNodeClicked OnGraphNodeClicked;

	TMap<URigVMNode*, TPair<int32, int32>> CachedInstructionIndices;

	void RemoveNode(UEdGraphNode* InNode);

protected:
	void HandleVMCompiledEvent(UObject* InCompiledObject, URigVM* InVM, FRigVMExtendedExecuteContext& InContext);

private:
	TMap<FName, UEdGraphNode*> ModelNodePathToEdNode;
	mutable TWeakObjectPtr<URigVMGraph> CachedModelGraph;
	TArray<TSharedPtr<FRigVMStringWithTag>> EntryNameList;

#endif
	friend class URigVMEdGraphNode;
	friend class URigVMBlueprint;
	friend class FRigVMEditor;
	friend class SRigVMGraphNode;
	friend class URigVMBlueprint;

	friend class URigVMEdGraphUnitNodeSpawner;
	friend class URigVMEdGraphVariableNodeSpawner;
	friend class URigVMEdGraphParameterNodeSpawner;
	friend class URigVMEdGraphBranchNodeSpawner;
	friend class URigVMEdGraphIfNodeSpawner;
	friend class URigVMEdGraphSelectNodeSpawner;
	friend class URigVMEdGraphTemplateNodeSpawner;
	friend class URigVMEdGraphEnumNodeSpawner;
	friend class URigVMEdGraphFunctionRefNodeSpawner;
	friend class URigVMEdGraphArrayNodeSpawner;
	friend class URigVMEdGraphInvokeEntryNodeSpawner;
};
