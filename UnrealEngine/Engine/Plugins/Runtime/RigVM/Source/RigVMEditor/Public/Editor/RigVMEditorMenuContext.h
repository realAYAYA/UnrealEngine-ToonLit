// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "EdGraph/EdGraph.h"

#include "RigVMEditorMenuContext.generated.h"

class URigVMHost;
class URigVMBlueprint;
class FRigVMEditor;
class URigVMGraph;
class URigVMNode;
class URigVMPin;

USTRUCT(BlueprintType)
struct RIGVMEDITOR_API FRigVMEditorGraphMenuContext
{
	GENERATED_BODY()

	FRigVMEditorGraphMenuContext()
		: Graph(nullptr)
		, Node(nullptr)
		, Pin(nullptr)
	{
	}
	
	FRigVMEditorGraphMenuContext(TObjectPtr<const URigVMGraph> InGraph, TObjectPtr<const URigVMNode> InNode, TObjectPtr<const URigVMPin> InPin)
		: Graph(InGraph)
		, Node(InNode)
		, Pin(InPin)
	{
	}
	
	/** The graph associated with this context. */
	UPROPERTY(BlueprintReadOnly, Category = RigVMEditor)
	TObjectPtr<const URigVMGraph> Graph;

	/** The node associated with this context. */
	UPROPERTY(BlueprintReadOnly, Category = RigVMEditor)
	TObjectPtr<const URigVMNode> Node;

	/** The pin associated with this context; may be NULL when over a node. */
	UPROPERTY(BlueprintReadOnly, Category = RigVMEditor)
	TObjectPtr<const URigVMPin> Pin;
};

UCLASS(BlueprintType)
class RIGVMEDITOR_API URigVMEditorMenuContext : public UObject
{
	GENERATED_BODY()

public:
	/**
	 *	Initialize the Context
	 * @param InRigVMEditor 	    The RigVM Editor hosting the menus
	 * @param InGraphMenuContext 	Additional context for specific menus
	*/
	void Init(TWeakPtr<FRigVMEditor> InRigVMEditor, const FRigVMEditorGraphMenuContext& InGraphMenuContext = FRigVMEditorGraphMenuContext());

	/** Get the rigvm blueprint that we are editing */
	UFUNCTION(BlueprintCallable, Category = RigVMEditor)
    URigVMBlueprint* GetRigVMBlueprint() const;
	
	/** Get the active rigvm host instance in the viewport */
	UFUNCTION(BlueprintCallable, Category = RigVMEditor)
    URigVMHost* GetRigVMHost() const;

	/** Returns true if either alt key is down */
	UFUNCTION(BlueprintCallable, Category = RigVMEditor)
	bool IsAltDown() const;
	
	/** Returns context for graph node context menu */
	UFUNCTION(BlueprintCallable, Category = RigVMEditor)
	FRigVMEditorGraphMenuContext GetGraphMenuContext();
	
	FRigVMEditor* GetRigVMEditor() const;

private:
	TWeakPtr<FRigVMEditor> WeakRigVMEditor;

	FRigVMEditorGraphMenuContext GraphMenuContext;
};