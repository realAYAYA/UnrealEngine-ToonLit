// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UObject/PropertyPortFlags.h"
#include "RigVMController.h"
#if WITH_EDITOR
#include "ScopedTransaction.h"
#endif
#include "UObject/StructOnScope.h"
#include "RigVMControllerActions.generated.h"

// If this is set to one the action stack will log any created action
// and invoked undo / redo on those actions and print them as an indented log.
#ifndef RIGVM_ACTIONSTACK_VERBOSE_LOG
#define RIGVM_ACTIONSTACK_VERBOSE_LOG 0
#endif 

/**
 * ================================================================================
 * The RigVMController doesn't rely on the transaction system for performing,
 * tracking, undoing and redoing changes. Instead it uses an action stack which
 * stores small serialized structs for each occured action. The reason for this 
 * is the subscription model and Python support: We need the Graph to broadcast
 * events to all subscribers - independently from where the action is coming from.
 * This includes UI views, scripting or undo / redo.
 * Each action supports the concept of 'merging', so multiple color change actions
 * for example can be merged into a single action. This avoids the need for tracking
 * action scope - and makes the integration with UI code simple.
 * The Controller's ActionStack integrates into the editor's transaction stack
 * using the ActionIndex property: Transactions on that property cause the 
 * actionstack to consecutively undo or redo actions until the expected stack
 * size is reached.
 * ================================================================================
 */

struct FRigVMBaseAction;

/**
 * The action key is used for serializing and storing an action in the stack,
 * or within another action.
 */
USTRUCT()
struct FRigVMActionKey
{
	GENERATED_BODY()

	UPROPERTY()
	FString ScriptStructPath;

	UPROPERTY()
	FString ExportedText;

	UScriptStruct* GetScriptStruct() const;
	TSharedPtr<FStructOnScope> GetAction() const;

	template<class ActionType>
	void Set(const ActionType& InAction)
	{
		UScriptStruct* ScriptStruct = ActionType::StaticStruct();
		FRigVMActionKey Key;
		ScriptStructPath = ScriptStruct->GetPathName();
		
		TArray<uint8, TAlignedHeapAllocator<16>> DefaultStructData;
		DefaultStructData.AddZeroed(ScriptStruct->GetStructureSize());
		ScriptStruct->InitializeDefaultValue(DefaultStructData.GetData());
		
		ScriptStruct->ExportText(ExportedText, &InAction, DefaultStructData.GetData(), nullptr, PPF_None, nullptr);
	}
};

/**
 * The action wrapper is used to extract an action from a serialized key.
 */
struct FRigVMActionWrapper
{
public:
	FRigVMActionWrapper(const FRigVMActionKey& Key);
	~FRigVMActionWrapper();

	const UScriptStruct* GetScriptStruct() const;
	FRigVMBaseAction* GetAction() const;
	FString ExportText() const;

private:
	FRigVMActionWrapper(const FRigVMActionWrapper& Other) = delete;
	FRigVMActionWrapper& operator = (const FRigVMActionWrapper& Other) = delete;

	TSharedPtr<FStructOnScope> StructOnScope;
};

/**
 * The base action is the base struct for all actions, and provides
 * access to sub actions, merge functionality as well as undo and redo
 * base implementations.
 */
USTRUCT()
struct FRigVMBaseAction
{
	GENERATED_BODY()

	inline static const FString RedoPrefix = TEXT("Redo");
	inline static const FString UndoPrefix = TEXT("Undo");
	inline static const FString AddActionPrefix = TEXT("Add Action");
	inline static const FString BeginActionPrefix = TEXT("Begin Action");
	inline static const FString EndActionPrefix = TEXT("End Action");
	inline static const FString CancelActionPrefix = TEXT("Cancel Action");

	// Default constructor
	FRigVMBaseAction()
		 : Title(TEXT("Action"))
	{
	}

	// Default destructor
	virtual ~FRigVMBaseAction() {};

	// Access to the actions script struct. this needs to be overloaded
	virtual UScriptStruct* GetScriptStruct() const { return FRigVMBaseAction::StaticStruct(); }

	// Returns true if this action is empty has no effect
	virtual bool IsEmpty() const { return SubActions.IsEmpty(); }

	// Returns the title of the action - used for the Edit menu's undo / redo
	virtual FString GetTitle() const { return Title; }

	// Trys to merge the action with another action and 
	// returns true if successfull.
	virtual bool Merge(const FRigVMBaseAction* Other);

	// Un-does the action and returns true if successfull.
	virtual bool Undo(URigVMController* InController);

	// Re-does the action and returns true if successfull.
	virtual bool Redo(URigVMController* InController);

	// Adds a child / sub action to this one
	template<class ActionType>
	void AddAction(const ActionType& InAction, URigVMController* InController)
	{
		FRigVMActionKey Key;
		Key.Set<ActionType>(InAction);
		SubActions.Add(Key);
	}

#if RIGVM_ACTIONSTACK_VERBOSE_LOG
	
	// Logs the action to the console / output log
	void LogAction(URigVMController* InController, const FString& InPrefix) const;

#endif

	UPROPERTY()
	FString Title;

	UPROPERTY()
	TArray<FRigVMActionKey> SubActions;
};

/**
 * The Action Stack can be used to track actions happening on a
 * Graph. Currently the only owner of the ActionStack is the Controller.
 * Actions can be added to the stack, or they can be understood as
 * scopes / brackets. For this you can use BeginAction / EndAction / CancelAction
 * to open / close a bracket. Open brackets automatically record additional
 * actions occuring during the bracket's lifetime.
 */
UCLASS()
class URigVMActionStack : public UObject
{
	GENERATED_BODY()

	virtual ~URigVMActionStack()
	{
		ensure(CurrentActions.IsEmpty());
	}

public:

	// Begins an action and opens a bracket / scope.
	template<class ActionType>
	void BeginAction(ActionType& InAction)
	{
#if RIGVM_ACTIONSTACK_VERBOSE_LOG		
		TGuardValue<int32> TabDepthGuard(LogActionDepth, CurrentActions.Num());
		LogAction<ActionType>(InAction, FRigVMBaseAction::BeginActionPrefix);
#endif

		if (CurrentActions.Num() > 0)
		{
			// catch erroreous duplicate calls to begin action
			ensure(CurrentActions.Last() != &InAction);
		}
		CurrentActions.Add((FRigVMBaseAction*)&InAction);
		
		ModifiedEvent.Broadcast(ERigVMGraphNotifType::InteractionBracketOpened, nullptr, nullptr);
	}

	// Ends an action and closes a bracket / scope.
	template<class ActionType>
	void EndAction(ActionType& InAction, bool bPerformMerge = false)
	{
		ensure(CurrentActions.Num() > 0);
		ensure((FRigVMBaseAction*)&InAction == CurrentActions.Last());
		CurrentActions.Pop();

		{
#if RIGVM_ACTIONSTACK_VERBOSE_LOG		
			TGuardValue<bool> LogGuard(bSuspendLogActions, true);
#endif
			AddAction(InAction, bPerformMerge);
		}

#if RIGVM_ACTIONSTACK_VERBOSE_LOG		
		TGuardValue<int32> TabDepthGuard(LogActionDepth, CurrentActions.Num());
		LogAction<ActionType>(InAction, FRigVMBaseAction::EndActionPrefix);
#endif
		ModifiedEvent.Broadcast(ERigVMGraphNotifType::InteractionBracketClosed, nullptr, nullptr);
	}

	// Cancels an action, closes a bracket / scope and discards all 
	// actions to this point.
	template<class ActionType>
	void CancelAction(ActionType& InAction, URigVMController* InController)
	{
		ensure(CurrentActions.Num() > 0);
		ensure((FRigVMBaseAction*)&InAction == CurrentActions.Last());
		CurrentActions.Pop();

		InAction.Undo(InController);

#if RIGVM_ACTIONSTACK_VERBOSE_LOG		
		TGuardValue<int32> TabDepthGuard(LogActionDepth, CurrentActions.Num());
		LogAction<ActionType>(InAction, FRigVMBaseAction::CancelActionPrefix);
#endif
		ModifiedEvent.Broadcast(ERigVMGraphNotifType::InteractionBracketCanceled, nullptr, nullptr);
	}

	// Adds an action to the stack. Optionally this can perform
	// a potential merge of this action with the previous action to
	// compact the stack.
	template<class ActionType>
	void AddAction(const ActionType& InAction, bool bPerformMerge = false)
	{
#if RIGVM_ACTIONSTACK_VERBOSE_LOG		
		TGuardValue<int32> TabDepthGuard(LogActionDepth, CurrentActions.Num());
		LogAction<ActionType>(InAction, FRigVMBaseAction::AddActionPrefix);
#endif
		TArray<FRigVMActionKey>* ActionList = &UndoActions;
		if (CurrentActions.Num() > 0)
		{
			ActionList = &CurrentActions[CurrentActions.Num()-1]->SubActions;
		}

		bool bMergeIfPossible = bPerformMerge;
		if (bMergeIfPossible)
		{
			bMergeIfPossible = false;
			if (ActionList->Num() > 0 && InAction.SubActions.Num() == 0)
			{
				if (ActionList->Last().ScriptStructPath == ActionType::StaticStruct()->GetPathName())
				{
					FRigVMActionWrapper Wrapper((*ActionList)[ActionList->Num() - 1]);
					if (Wrapper.GetAction()->SubActions.Num() == 0)
					{
						bMergeIfPossible = Wrapper.GetAction()->Merge(&InAction);
						if (bMergeIfPossible)
						{
							(*ActionList)[ActionList->Num()-1].ExportedText = Wrapper.ExportText();
						}
					}
				}
			}
		}

		if (!bMergeIfPossible)
		{
			FRigVMActionKey Key;
			Key.Set<ActionType>(InAction);

			ActionList->Add(Key);

			if (CurrentActions.Num() == 0)
			{
				RedoActions.Reset();

#if WITH_EDITOR
				const FScopedTransaction Transaction(FText::FromString(InAction.GetTitle()));
				SetFlags(RF_Transactional);
				Modify();
#endif

				ActionIndex = ActionIndex + 1;
			}
		}
	}

	// Opens an undo bracket / scope to record actions into.
	// This is primary useful for Python.
	UFUNCTION()
	bool OpenUndoBracket(const FString& InTitle);

	// Closes an undo bracket / scope.
	// This is primary useful for Python.
	UFUNCTION()
	bool CloseUndoBracket(URigVMController* InController);

	// Cancels an undo bracket / scope.
	// This is primary useful for Python.
	UFUNCTION()
	bool CancelUndoBracket(URigVMController* InController);

	// Pops the last action from the undo stack and perform undo on it.
	// Note: This should really only be used for unit tests,
	// use the GEditor's main Undo method instead.
	UFUNCTION()
	bool Undo(URigVMController* InController);

	// Pops the last action from the redo stack and perform redo on it.
	// Note: This should really only be used for unit tests,
	// use the GEditor's main Redo method instead.
	UFUNCTION()
	bool Redo(URigVMController* InController);

#if WITH_EDITOR
	virtual void PostTransacted(const FTransactionObjectEvent& TransactionEvent) override;
#endif

	FRigVMGraphModifiedEvent& OnModified() { return ModifiedEvent; }

private:

#if RIGVM_ACTIONSTACK_VERBOSE_LOG		
	template<class ActionType>
	FORCEINLINE void LogAction(const ActionType& InAction, const FString& InPrefix)
	{
		LogAction(ActionType::StaticStruct(), InAction, InPrefix);
	}
	
	void LogAction(const UScriptStruct* InActionStruct, const FRigVMBaseAction& InAction, const FString& InPrefix);
#endif
	
	UPROPERTY()
	int32 ActionIndex;

	UPROPERTY(NonTransactional)
	TArray<FRigVMActionKey> UndoActions;

	UPROPERTY(NonTransactional)
	TArray<FRigVMActionKey> RedoActions;

	TArray<FRigVMBaseAction*> CurrentActions;
	TArray<FRigVMBaseAction*> BracketActions;

	FRigVMGraphModifiedEvent ModifiedEvent;

#if RIGVM_ACTIONSTACK_VERBOSE_LOG		
	bool bSuspendLogActions = false;
	int32 LogActionDepth = 0;
#endif
	
	friend struct FRigVMBaseAction;
	friend class URigVMController;
};

/**
 * An action which inverses the child actions,
 * it performs undo on redo and vice versa.
 */
USTRUCT()
struct FRigVMInverseAction : public FRigVMBaseAction
{
	GENERATED_BODY()

	virtual ~FRigVMInverseAction() {};
	virtual UScriptStruct* GetScriptStruct() const override { return FRigVMInverseAction::StaticStruct(); }

	virtual bool Undo(URigVMController* InController);
	virtual bool Redo(URigVMController* InController);
};

/**
 * An action adding a unit node to the graph.
 */
USTRUCT()
struct FRigVMAddUnitNodeAction : public FRigVMBaseAction
{
	GENERATED_BODY()

public:

	FRigVMAddUnitNodeAction();
	FRigVMAddUnitNodeAction(URigVMUnitNode* InNode);
	virtual ~FRigVMAddUnitNodeAction() {};
	virtual UScriptStruct* GetScriptStruct() const override { return FRigVMAddUnitNodeAction::StaticStruct(); }
	virtual bool Undo(URigVMController* InController) override;
	virtual bool Redo(URigVMController* InController) override;

	UPROPERTY()
	FString ScriptStructPath;

	UPROPERTY()
	FName MethodName;
	
	UPROPERTY()
	FVector2D Position;

	UPROPERTY()
	FString NodePath;
};

/**
 * An action adding a variable node to the graph.
 */
USTRUCT()
struct FRigVMAddVariableNodeAction : public FRigVMBaseAction
{
	GENERATED_BODY()

public:

	FRigVMAddVariableNodeAction();
	FRigVMAddVariableNodeAction(URigVMVariableNode* InNode);
	virtual ~FRigVMAddVariableNodeAction() {};
	virtual UScriptStruct* GetScriptStruct() const override { return FRigVMAddVariableNodeAction::StaticStruct(); }
	virtual bool Undo(URigVMController* InController) override;
	virtual bool Redo(URigVMController* InController) override;

	UPROPERTY()
	FName VariableName;
	
	UPROPERTY()
	FString CPPType;

	UPROPERTY()
	FString CPPTypeObjectPath;

	UPROPERTY()
	bool bIsGetter;

	UPROPERTY()
	FString DefaultValue;

	UPROPERTY()
	FVector2D Position;

	UPROPERTY()
	FString NodePath;
};

/**
 * An action adding a comment node to the graph.
 */
USTRUCT()
struct FRigVMAddCommentNodeAction : public FRigVMBaseAction
{
	GENERATED_BODY()

public:

	FRigVMAddCommentNodeAction();
	FRigVMAddCommentNodeAction(URigVMCommentNode* InNode);
	virtual ~FRigVMAddCommentNodeAction() {};
	virtual UScriptStruct* GetScriptStruct() const override { return FRigVMAddCommentNodeAction::StaticStruct(); }
	virtual bool Undo(URigVMController* InController) override;
	virtual bool Redo(URigVMController* InController) override;

	UPROPERTY()
	FString CommentText;

	UPROPERTY()
	FVector2D Position;

	UPROPERTY()
	FVector2D Size;

	UPROPERTY()
	FLinearColor Color;

	UPROPERTY()
	FString NodePath;
};

/**
 * An action adding a reroute node to the graph.
 */
USTRUCT()
struct FRigVMAddRerouteNodeAction : public FRigVMBaseAction
{
	GENERATED_BODY()

public:

	FRigVMAddRerouteNodeAction();
	FRigVMAddRerouteNodeAction(URigVMRerouteNode* InNode);
	virtual ~FRigVMAddRerouteNodeAction() {};
	virtual UScriptStruct* GetScriptStruct() const override { return FRigVMAddRerouteNodeAction::StaticStruct(); }
	virtual bool Undo(URigVMController* InController) override;
	virtual bool Redo(URigVMController* InController) override;

	UPROPERTY()
	bool bShowAsFullNode;

	UPROPERTY()
	FString CPPType;

	UPROPERTY()
	FName CPPTypeObjectPath;

	UPROPERTY()
	FString DefaultValue;

	UPROPERTY()
	bool bIsConstant;

	UPROPERTY()
	FName CustomWidgetName;

	UPROPERTY()
	FVector2D Position;

	UPROPERTY()
	FString NodePath;
};

/**
 * An action adding a branch node to the graph.
 */
USTRUCT()
struct FRigVMAddBranchNodeAction : public FRigVMBaseAction
{
	GENERATED_BODY()

public:

	FRigVMAddBranchNodeAction();
	FRigVMAddBranchNodeAction(URigVMBranchNode* InNode);
	virtual ~FRigVMAddBranchNodeAction() {};
	virtual UScriptStruct* GetScriptStruct() const override { return FRigVMAddBranchNodeAction::StaticStruct(); }
	virtual bool Undo(URigVMController* InController) override;
	virtual bool Redo(URigVMController* InController) override;

	UPROPERTY()
	FVector2D Position;

	UPROPERTY()
	FString NodePath;
};

/**
 * An action adding an if node to the graph.
 */
USTRUCT()
struct FRigVMAddIfNodeAction : public FRigVMBaseAction
{
	GENERATED_BODY()

public:

	FRigVMAddIfNodeAction();
	FRigVMAddIfNodeAction(URigVMIfNode* InNode);
	virtual ~FRigVMAddIfNodeAction() {};
	virtual UScriptStruct* GetScriptStruct() const override { return FRigVMAddIfNodeAction::StaticStruct(); }
	virtual bool Undo(URigVMController* InController) override;
	virtual bool Redo(URigVMController* InController) override;

	UPROPERTY()
	FString CPPType;

	UPROPERTY()
	FName CPPTypeObjectPath;

	UPROPERTY()
	FVector2D Position;

	UPROPERTY()
	FString NodePath;
};

/**
 * An action adding a select node to the graph.
 */
USTRUCT()
struct FRigVMAddSelectNodeAction : public FRigVMBaseAction
{
	GENERATED_BODY()

public:

	FRigVMAddSelectNodeAction();
	FRigVMAddSelectNodeAction(URigVMSelectNode* InNode);
	virtual ~FRigVMAddSelectNodeAction() {};
	virtual UScriptStruct* GetScriptStruct() const override { return FRigVMAddSelectNodeAction::StaticStruct(); }
	virtual bool Undo(URigVMController* InController) override;
	virtual bool Redo(URigVMController* InController) override;

	UPROPERTY()
	FString CPPType;

	UPROPERTY()
	FName CPPTypeObjectPath;

	UPROPERTY()
	FVector2D Position;

	UPROPERTY()
	FString NodePath;
};

/**
 * An action adding an enum node to the graph.
 */
USTRUCT()
struct FRigVMAddEnumNodeAction : public FRigVMBaseAction
{
	GENERATED_BODY()

public:

	FRigVMAddEnumNodeAction();
	FRigVMAddEnumNodeAction(URigVMEnumNode* InNode);
	virtual ~FRigVMAddEnumNodeAction() {};
	virtual UScriptStruct* GetScriptStruct() const override { return FRigVMAddEnumNodeAction::StaticStruct(); }
	virtual bool Undo(URigVMController* InController) override;
	virtual bool Redo(URigVMController* InController) override;

	UPROPERTY()
	FString CPPType;

	UPROPERTY()
	FName CPPTypeObjectPath;

	UPROPERTY()
	FVector2D Position;

	UPROPERTY()
	FString NodePath;
};

/**
 * An action adding a template node to the graph.
 */
USTRUCT()
struct FRigVMAddTemplateNodeAction : public FRigVMBaseAction
{
	GENERATED_BODY()

public:

	FRigVMAddTemplateNodeAction();
	FRigVMAddTemplateNodeAction(URigVMTemplateNode* InNode);
	virtual ~FRigVMAddTemplateNodeAction() {};
	virtual UScriptStruct* GetScriptStruct() const override { return FRigVMAddTemplateNodeAction::StaticStruct(); }
	virtual bool Undo(URigVMController* InController) override;
	virtual bool Redo(URigVMController* InController) override;

	UPROPERTY()
	FName TemplateNotation;

	UPROPERTY()
	FVector2D Position;

	UPROPERTY()
	FString NodePath;
};

/**
 * An action injecting a node into a pin
 */
USTRUCT()
struct FRigVMInjectNodeIntoPinAction : public FRigVMBaseAction
{
	GENERATED_BODY()

public:

	FRigVMInjectNodeIntoPinAction();
	FRigVMInjectNodeIntoPinAction(URigVMInjectionInfo* InInjectionInfo);
	virtual ~FRigVMInjectNodeIntoPinAction() {};
	virtual UScriptStruct* GetScriptStruct() const override { return FRigVMInjectNodeIntoPinAction::StaticStruct(); }
	virtual bool Undo(URigVMController* InController) override;
	virtual bool Redo(URigVMController* InController) override;

	UPROPERTY()
	FString PinPath;

	UPROPERTY()
	bool bAsInput;

	UPROPERTY()
	FName InputPinName;

	UPROPERTY()
	FName OutputPinName;

	UPROPERTY()
	FString NodePath;
};

/**
 * An action removing a node from the graph.
 */
USTRUCT()
struct FRigVMRemoveNodeAction : public FRigVMBaseAction
{
	GENERATED_BODY()

public:

	FRigVMRemoveNodeAction() {}
	FRigVMRemoveNodeAction(URigVMNode* InNode, URigVMController* InController);
	virtual ~FRigVMRemoveNodeAction() {};
	virtual UScriptStruct* GetScriptStruct() const override { return FRigVMRemoveNodeAction::StaticStruct(); }
	virtual bool Undo(URigVMController* InController) override;
	virtual bool Redo(URigVMController* InController) override;

	UPROPERTY()
	FRigVMActionKey InverseActionKey;
};

/**
 * An action selecting or deselecting a node in the graph.
 */
USTRUCT()
struct FRigVMSetNodeSelectionAction : public FRigVMBaseAction
{
	GENERATED_BODY()

public:

	FRigVMSetNodeSelectionAction();
	FRigVMSetNodeSelectionAction(URigVMGraph* InGraph, TArray<FName> InNewSelection);
	virtual ~FRigVMSetNodeSelectionAction() {};
	virtual UScriptStruct* GetScriptStruct() const override { return FRigVMSetNodeSelectionAction::StaticStruct(); }
	virtual bool Undo(URigVMController* InController) override;
	virtual bool Redo(URigVMController* InController) override;

	UPROPERTY()
	TArray<FName> NewSelection;

	UPROPERTY()
	TArray<FName> OldSelection;
};

/**
 * An action setting a node's position in the graph.
 */
USTRUCT()
struct FRigVMSetNodePositionAction : public FRigVMBaseAction
{
	GENERATED_BODY()

public:

	FRigVMSetNodePositionAction()
	{
		OldPosition = NewPosition = FVector2D::ZeroVector;
	}
	FRigVMSetNodePositionAction(URigVMNode* InNode, const FVector2D& InNewPosition);
	virtual ~FRigVMSetNodePositionAction() {};
	virtual UScriptStruct* GetScriptStruct() const override { return FRigVMSetNodePositionAction::StaticStruct(); }
	virtual bool Merge(const FRigVMBaseAction* Other);
	virtual bool Undo(URigVMController* InController) override;
	virtual bool Redo(URigVMController* InController) override;

	UPROPERTY()
	FString NodePath;

	UPROPERTY()
	FVector2D OldPosition;

	UPROPERTY()
	FVector2D NewPosition;
};

/**
 * An action setting a node's size in the graph.
 */
USTRUCT()
struct FRigVMSetNodeSizeAction : public FRigVMBaseAction
{
	GENERATED_BODY()

public:

	FRigVMSetNodeSizeAction()
	{
		OldSize = NewSize = FVector2D::ZeroVector;
	}
	FRigVMSetNodeSizeAction(URigVMNode* InNode, const FVector2D& InNewSize);
	virtual ~FRigVMSetNodeSizeAction() {};
	virtual UScriptStruct* GetScriptStruct() const override { return FRigVMSetNodeSizeAction::StaticStruct(); }
	virtual bool Merge(const FRigVMBaseAction* Other);
	virtual bool Undo(URigVMController* InController) override;
	virtual bool Redo(URigVMController* InController) override;

	UPROPERTY()
	FString NodePath;

	UPROPERTY()
	FVector2D OldSize;

	UPROPERTY()
	FVector2D NewSize;
};

/**
 * An action setting a node's color in the graph.
 */
USTRUCT()
struct FRigVMSetNodeColorAction : public FRigVMBaseAction
{
	GENERATED_BODY()

public:

	FRigVMSetNodeColorAction()
	{
		OldColor = NewColor = FLinearColor::Black;
	}
	FRigVMSetNodeColorAction(URigVMNode* InNode, const FLinearColor& InNewColor);
	virtual ~FRigVMSetNodeColorAction() {};
	virtual UScriptStruct* GetScriptStruct() const override { return FRigVMSetNodeColorAction::StaticStruct(); }
	virtual bool Merge(const FRigVMBaseAction* Other);
	virtual bool Undo(URigVMController* InController) override;
	virtual bool Redo(URigVMController* InController) override;

	UPROPERTY()
	FString NodePath;

	UPROPERTY()
	FLinearColor OldColor;

	UPROPERTY()
	FLinearColor NewColor;
};

/**
 * An action setting a node's category in the graph.
 */
USTRUCT()
struct FRigVMSetNodeCategoryAction : public FRigVMBaseAction
{
	GENERATED_BODY()

public:

	FRigVMSetNodeCategoryAction()
	{
		OldCategory = NewCategory = FString();
	}
	FRigVMSetNodeCategoryAction(URigVMCollapseNode* InNode, const FString& InNewCategory);
	virtual ~FRigVMSetNodeCategoryAction() {};
	virtual UScriptStruct* GetScriptStruct() const override { return FRigVMSetNodeCategoryAction::StaticStruct(); }
	virtual bool Merge(const FRigVMBaseAction* Other);
	virtual bool Undo(URigVMController* InController) override;
	virtual bool Redo(URigVMController* InController) override;

	UPROPERTY()
	FString NodePath;

	UPROPERTY()
	FString OldCategory;

	UPROPERTY()
	FString NewCategory;
};


/**
 * An action setting a node's keywords in the graph.
 */
USTRUCT()
struct FRigVMSetNodeKeywordsAction : public FRigVMBaseAction
{
	GENERATED_BODY()

public:

	FRigVMSetNodeKeywordsAction()
	{
		OldKeywords = NewKeywords = FString();
	}
	FRigVMSetNodeKeywordsAction(URigVMCollapseNode* InNode, const FString& InNewKeywords);
	virtual ~FRigVMSetNodeKeywordsAction() {};
	virtual UScriptStruct* GetScriptStruct() const override { return FRigVMSetNodeKeywordsAction::StaticStruct(); }
	virtual bool Merge(const FRigVMBaseAction* Other);
	virtual bool Undo(URigVMController* InController) override;
	virtual bool Redo(URigVMController* InController) override;

	UPROPERTY()
	FString NodePath;

	UPROPERTY()
	FString OldKeywords;

	UPROPERTY()
	FString NewKeywords;
};

/**
* An action setting a node's description in the graph.
*/
USTRUCT()
struct FRigVMSetNodeDescriptionAction : public FRigVMBaseAction
{
	GENERATED_BODY()

public:

	FRigVMSetNodeDescriptionAction()
	{
		OldDescription = NewDescription = FString();
	}
	FRigVMSetNodeDescriptionAction(URigVMCollapseNode* InNode, const FString& InNewDescription);
	virtual ~FRigVMSetNodeDescriptionAction() {};
	virtual UScriptStruct* GetScriptStruct() const override { return FRigVMSetNodeDescriptionAction::StaticStruct(); }
	virtual bool Merge(const FRigVMBaseAction* Other);
	virtual bool Undo(URigVMController* InController) override;
	virtual bool Redo(URigVMController* InController) override;

	UPROPERTY()
	FString NodePath;

	UPROPERTY()
	FString OldDescription;

	UPROPERTY()
	FString NewDescription;
};

/**
 * An action setting a comment node's text in the graph.
 */
USTRUCT()
struct FRigVMSetCommentTextAction : public FRigVMBaseAction
{
	GENERATED_BODY()

public:

	FRigVMSetCommentTextAction();
	FRigVMSetCommentTextAction(URigVMCommentNode* InNode, const FString& InNewText, const int32& InNewFontSize, const bool& bInNewBubbleVisible, const bool& bInNewColorBubble);
	virtual ~FRigVMSetCommentTextAction() {};
	virtual UScriptStruct* GetScriptStruct() const override { return FRigVMSetCommentTextAction::StaticStruct(); }
	virtual bool Undo(URigVMController* InController) override;
	virtual bool Redo(URigVMController* InController) override;

	UPROPERTY()
	FString NodePath;

	UPROPERTY()
	FString OldText;

	UPROPERTY()
	FString NewText;

	UPROPERTY()
	int32 OldFontSize;

	UPROPERTY()
	int32 NewFontSize;

	UPROPERTY()
	bool bOldBubbleVisible;

	UPROPERTY()
	bool bNewBubbleVisible;

	UPROPERTY()
	bool bOldColorBubble;

	UPROPERTY()
	bool bNewColorBubble;
};

/**
 * An action setting a reroute node's compactness in the graph.
 */
USTRUCT()
struct FRigVMSetRerouteCompactnessAction : public FRigVMBaseAction
{
	GENERATED_BODY()

public:

	FRigVMSetRerouteCompactnessAction();
	FRigVMSetRerouteCompactnessAction(URigVMRerouteNode* InNode, bool InShowAsFullNode);
	virtual ~FRigVMSetRerouteCompactnessAction() {};
	virtual UScriptStruct* GetScriptStruct() const override { return FRigVMSetRerouteCompactnessAction::StaticStruct(); }
	virtual bool Undo(URigVMController* InController) override;
	virtual bool Redo(URigVMController* InController) override;

	UPROPERTY()
	FString NodePath;

	UPROPERTY()
	bool OldShowAsFullNode;

	UPROPERTY()
	bool NewShowAsFullNode;
};

/**
 * An action renaming a variable in the graph.
 */
USTRUCT(meta=(Deprecated = "5.1"))
struct FRigVMRenameVariableAction : public FRigVMBaseAction
{
	GENERATED_BODY()

public:

	FRigVMRenameVariableAction() {}
	FRigVMRenameVariableAction(const FName& InOldVariableName, const FName& InNewVariableName);
	virtual ~FRigVMRenameVariableAction() {};
	virtual UScriptStruct* GetScriptStruct() const override { return FRigVMRenameVariableAction::StaticStruct(); }
	virtual bool Undo(URigVMController* InController) override;
	virtual bool Redo(URigVMController* InController) override;

	UPROPERTY()
	FString OldVariableName;

	UPROPERTY()
	FString NewVariableName;
};

/**
 * An action setting a pin's expansion state in the graph.
 */
USTRUCT()
struct FRigVMSetPinExpansionAction : public FRigVMBaseAction
{
	GENERATED_BODY()

public:

	FRigVMSetPinExpansionAction()
	{
		PinPath = FString();
		OldIsExpanded = NewIsExpanded = false;
	}

	FRigVMSetPinExpansionAction(URigVMPin* InPin, bool bNewIsExpanded);
	virtual ~FRigVMSetPinExpansionAction() {};
	virtual UScriptStruct* GetScriptStruct() const override { return FRigVMSetPinExpansionAction::StaticStruct(); }
	virtual bool Undo(URigVMController* InController) override;
	virtual bool Redo(URigVMController* InController) override;

	UPROPERTY()
	FString PinPath;

	UPROPERTY()
	bool OldIsExpanded;

	UPROPERTY()
	bool NewIsExpanded;
};

/**
 * An action setting a pin's watch state in the graph.
 */
USTRUCT()
struct FRigVMSetPinWatchAction : public FRigVMBaseAction
{
	GENERATED_BODY()

public:

	FRigVMSetPinWatchAction()
	{
		OldIsWatched = NewIsWatched = false;
	}

	FRigVMSetPinWatchAction(URigVMPin* InPin, bool bNewIsWatched);
	virtual ~FRigVMSetPinWatchAction() {};
	virtual UScriptStruct* GetScriptStruct() const override { return FRigVMSetPinWatchAction::StaticStruct(); }
	virtual bool Undo(URigVMController* InController) override;
	virtual bool Redo(URigVMController* InController) override;

	UPROPERTY()
	FString PinPath;

	UPROPERTY()
	bool OldIsWatched;

	UPROPERTY()
	bool NewIsWatched;
};

/**
 * An action setting a pin's default value in the graph.
 */
USTRUCT()
struct FRigVMSetPinDefaultValueAction : public FRigVMBaseAction
{
	GENERATED_BODY()

public:

	FRigVMSetPinDefaultValueAction() {}
	FRigVMSetPinDefaultValueAction(URigVMPin* InPin, const FString& InNewDefaultValue);
	virtual ~FRigVMSetPinDefaultValueAction() {};
	virtual UScriptStruct* GetScriptStruct() const override { return FRigVMSetPinDefaultValueAction::StaticStruct(); }
	virtual bool Merge(const FRigVMBaseAction* Other);
	virtual bool Undo(URigVMController* InController) override;
	virtual bool Redo(URigVMController* InController) override;

	UPROPERTY()
	FString PinPath;

	UPROPERTY()
	FString OldDefaultValue;

	UPROPERTY()
	FString NewDefaultValue;
};

/**
 * An action setting the filtered permutations on a template node
 */
USTRUCT()
struct FRigVMSetTemplateFilteredPermutationsAction : public FRigVMBaseAction
{
	GENERATED_BODY()

public:

	FRigVMSetTemplateFilteredPermutationsAction() {}
	FRigVMSetTemplateFilteredPermutationsAction(URigVMTemplateNode* InNode, const TArray<int32>& InOldFilteredPermutations);
	virtual ~FRigVMSetTemplateFilteredPermutationsAction() {};
	virtual UScriptStruct* GetScriptStruct() const override { return FRigVMSetTemplateFilteredPermutationsAction::StaticStruct(); }
	virtual bool Merge(const FRigVMBaseAction* Other);
	virtual bool Undo(URigVMController* InController) override;
	virtual bool Redo(URigVMController* InController) override;

	UPROPERTY()
	FString NodePath;

	UPROPERTY()
	TArray<int32> OldFilteredPermutations;

	UPROPERTY()
	TArray<int32> NewFilteredPermutations;
};

/**
 * An action setting the filtered permutations on a template node
 */
USTRUCT()
struct FRigVMSetPreferredTemplatePermutationsAction : public FRigVMBaseAction
{
	GENERATED_BODY()

public:

	FRigVMSetPreferredTemplatePermutationsAction() {}
	FRigVMSetPreferredTemplatePermutationsAction(URigVMTemplateNode* InNode, const TArray<FRigVMTemplatePreferredType>& InPreferredTypes);
	virtual ~FRigVMSetPreferredTemplatePermutationsAction() {};
	virtual UScriptStruct* GetScriptStruct() const override { return FRigVMSetTemplateFilteredPermutationsAction::StaticStruct(); }
	virtual bool Merge(const FRigVMBaseAction* Other);
	virtual bool Undo(URigVMController* InController) override;
	virtual bool Redo(URigVMController* InController) override;

	UPROPERTY()
	FString NodePath;

	UPROPERTY()
	TArray<FRigVMTemplatePreferredType> OldPreferredPermutationTypes;

	UPROPERTY()
	TArray<FRigVMTemplatePreferredType> NewPreferredPermutationTypes;
};

/**
 * An action setting the filtered permutations on a template node
 */
USTRUCT()
struct FRigVMSetLibraryTemplateAction : public FRigVMBaseAction
{
	GENERATED_BODY()

public:

	FRigVMSetLibraryTemplateAction() {}
	FRigVMSetLibraryTemplateAction(URigVMLibraryNode* InNode, FRigVMTemplate& InNewTemplate);
	virtual UScriptStruct* GetScriptStruct() const override { return FRigVMSetTemplateFilteredPermutationsAction::StaticStruct(); }
	virtual bool Undo(URigVMController* InController) override;
	virtual bool Redo(URigVMController* InController) override;

	UPROPERTY()
	FString NodePath;

	UPROPERTY()
	TArray<uint8> OldTemplateBytes;

	UPROPERTY()
	TArray<uint8> NewTemplateBytes;
};

/**
 * An action inserting a new array pin in the graph.
 */
USTRUCT()
struct FRigVMInsertArrayPinAction : public FRigVMBaseAction
{
	GENERATED_BODY()

public:

	FRigVMInsertArrayPinAction()
		: Index(0)
	{
	}
	FRigVMInsertArrayPinAction(URigVMPin* InArrayPin, int32 InIndex, const FString& InNewDefaultValue);
	virtual ~FRigVMInsertArrayPinAction() {};
	virtual UScriptStruct* GetScriptStruct() const override { return FRigVMInsertArrayPinAction::StaticStruct(); }
	virtual bool Undo(URigVMController* InController) override;
	virtual bool Redo(URigVMController* InController) override;

	UPROPERTY()
	FString ArrayPinPath;

	UPROPERTY()
	int32 Index;

	UPROPERTY()
	FString NewDefaultValue;
};

/**
 * An action removing an array pin from the graph.
 */
USTRUCT()
struct FRigVMRemoveArrayPinAction : public FRigVMBaseAction
{
	GENERATED_BODY()

public:

	FRigVMRemoveArrayPinAction()
		: Index(0)
	{
	}
	FRigVMRemoveArrayPinAction(URigVMPin* InArrayElementPin);
	virtual ~FRigVMRemoveArrayPinAction() {};
	virtual UScriptStruct* GetScriptStruct() const override { return FRigVMRemoveArrayPinAction::StaticStruct(); }
	virtual bool Undo(URigVMController* InController) override;
	virtual bool Redo(URigVMController* InController) override;

	UPROPERTY()
	FString ArrayPinPath;

	UPROPERTY()
	int32 Index;

	UPROPERTY()
	FString DefaultValue;
};

/**
 * An action adding a new link to the graph.
 */
USTRUCT()
struct FRigVMAddLinkAction : public FRigVMBaseAction
{
	GENERATED_BODY()

public:

	FRigVMAddLinkAction() {}
	FRigVMAddLinkAction(URigVMPin* InOutputPin, URigVMPin* InInputPin);
	virtual ~FRigVMAddLinkAction() {};
	virtual UScriptStruct* GetScriptStruct() const override { return FRigVMAddLinkAction::StaticStruct(); }
	virtual bool Undo(URigVMController* InController) override;
	virtual bool Redo(URigVMController* InController) override;

	UPROPERTY()
	FString OutputPinPath;

	UPROPERTY()
	FString InputPinPath;
};

/**
 * An action removing a link from the graph.
 */
USTRUCT()
struct FRigVMBreakLinkAction : public FRigVMBaseAction
{
	GENERATED_BODY()

public:

	FRigVMBreakLinkAction() {}
	FRigVMBreakLinkAction(URigVMPin* InOutputPin, URigVMPin* InInputPin);
	virtual ~FRigVMBreakLinkAction() {};
	virtual UScriptStruct* GetScriptStruct() const override { return FRigVMBreakLinkAction::StaticStruct(); }
	virtual bool Undo(URigVMController* InController) override;
	virtual bool Redo(URigVMController* InController) override;

	UPROPERTY()
	FSoftObjectPath GraphPath;

	UPROPERTY()
	FString OutputPinPath;

	UPROPERTY()
	FString InputPinPath;
};

/**
 * An action changing a pin type
 */
USTRUCT()
struct FRigVMChangePinTypeAction : public FRigVMBaseAction
{
	GENERATED_BODY()

public:

	FRigVMChangePinTypeAction();
	FRigVMChangePinTypeAction(URigVMPin* InPin, int32 InTypeIndex, bool InSetupOrphanPins, bool InBreakLinks, bool InRemoveSubPins);
	virtual ~FRigVMChangePinTypeAction() {};
	virtual UScriptStruct* GetScriptStruct() const override { return FRigVMChangePinTypeAction::StaticStruct(); }
	virtual bool Undo(URigVMController* InController) override;
	virtual bool Redo(URigVMController* InController) override;

	UPROPERTY()
	FString PinPath;

	UPROPERTY()
	int32 OldTypeIndex;

	UPROPERTY()
	int32 NewTypeIndex;

	UPROPERTY()
	bool bSetupOrphanPins;

	UPROPERTY()
	bool bBreakLinks;

	UPROPERTY()
	bool bRemoveSubPins;
};

/**
 * An action to add a node from a text buffer
 */
USTRUCT()
struct FRigVMImportNodeFromTextAction : public FRigVMBaseAction
{
	GENERATED_BODY()

public:

	FRigVMImportNodeFromTextAction();
	FRigVMImportNodeFromTextAction(URigVMNode* InNode, URigVMController* InController);
	virtual ~FRigVMImportNodeFromTextAction() {};
	virtual UScriptStruct* GetScriptStruct() const override { return FRigVMImportNodeFromTextAction::StaticStruct(); }
	virtual bool Undo(URigVMController* InController) override;
	virtual bool Redo(URigVMController* InController) override;

	UPROPERTY()
	FVector2D Position;

	UPROPERTY()
	FString NodePath;

	UPROPERTY()
	FString ExportedText;
};

/**
 * An action to collapse a selection of nodes
 */
USTRUCT()
struct FRigVMCollapseNodesAction : public FRigVMBaseAction
{
	GENERATED_BODY()

public:

	FRigVMCollapseNodesAction();
	FRigVMCollapseNodesAction(URigVMController* InController, const TArray<URigVMNode*>& InNodes, const FString& InNodePath, const bool bIsAggregate);
	virtual ~FRigVMCollapseNodesAction() {};
	virtual UScriptStruct* GetScriptStruct() const override { return FRigVMCollapseNodesAction::StaticStruct(); }
	virtual bool Undo(URigVMController* InController) override;
	virtual bool Redo(URigVMController* InController) override;

	UPROPERTY()
	FString LibraryNodePath;

	UPROPERTY()
	FString CollapsedNodesContent;

	UPROPERTY()
	TArray<FString> CollapsedNodesPaths;

	UPROPERTY()
	TArray<FString> CollapsedNodesLinks;

	UPROPERTY()
	bool bIsAggregate;
};

/**
 * An action to expand a library node into its content
 */
USTRUCT()
struct FRigVMExpandNodeAction : public FRigVMBaseAction
{
	GENERATED_BODY()

public:

	FRigVMExpandNodeAction();
	FRigVMExpandNodeAction(URigVMController* InController, URigVMLibraryNode* InLibraryNode);
	virtual ~FRigVMExpandNodeAction() {};
	virtual UScriptStruct* GetScriptStruct() const override { return FRigVMExpandNodeAction::StaticStruct(); }
	virtual bool Undo(URigVMController* InController) override;
	virtual bool Redo(URigVMController* InController) override;

	UPROPERTY()
	FString LibraryNodePath;

	UPROPERTY()
	FString LibraryNodeContent;

	UPROPERTY()
	TArray<FString> LibraryNodeLinks;

	UPROPERTY()
	TArray<FString> ExpandedNodePaths;
};

/**
 * An action renaming a node in the graph.
 */
USTRUCT()
struct FRigVMRenameNodeAction : public FRigVMBaseAction
{
	GENERATED_BODY()

public:

	FRigVMRenameNodeAction() {}
	FRigVMRenameNodeAction(const FName& InOldNodeName, const FName& InNewNodeName);
	virtual ~FRigVMRenameNodeAction() {};
	virtual UScriptStruct* GetScriptStruct() const override { return FRigVMRenameNodeAction::StaticStruct(); }
	virtual bool Undo(URigVMController* InController) override;
	virtual bool Redo(URigVMController* InController) override;

	UPROPERTY()
	FString OldNodeName;

	UPROPERTY()
	FString NewNodeName;
};

/**
 * An action pushing a graph to the graph stack of the controller
 */
USTRUCT()
struct FRigVMPushGraphAction : public FRigVMBaseAction
{
	GENERATED_BODY()

public:

	FRigVMPushGraphAction() {}
	FRigVMPushGraphAction(UObject* InGraph);
	virtual ~FRigVMPushGraphAction() {};
	virtual UScriptStruct* GetScriptStruct() const override { return FRigVMPushGraphAction::StaticStruct(); }
	virtual bool Undo(URigVMController* InController) override;
	virtual bool Redo(URigVMController* InController) override;

	UPROPERTY()
	FSoftObjectPath GraphPath;
};

/**
 * An action popping a graph from the graph stack of the controller
 */
USTRUCT()
struct FRigVMPopGraphAction : public FRigVMBaseAction
{
	GENERATED_BODY()

public:

	FRigVMPopGraphAction() {}
	FRigVMPopGraphAction(UObject* InGraph);
	virtual ~FRigVMPopGraphAction() {};
	virtual UScriptStruct* GetScriptStruct() const override { return FRigVMPopGraphAction::StaticStruct(); }
	virtual bool Undo(URigVMController* InController) override;
	virtual bool Redo(URigVMController* InController) override;

	UPROPERTY()
	FSoftObjectPath GraphPath;
};


/**
 * An action exposing a pin as a parameter
 */
USTRUCT()
struct FRigVMAddExposedPinAction : public FRigVMBaseAction
{
	GENERATED_BODY()

public:

	FRigVMAddExposedPinAction() {}
	FRigVMAddExposedPinAction(URigVMPin* InPin);
	virtual ~FRigVMAddExposedPinAction() {};
	virtual UScriptStruct* GetScriptStruct() const override { return FRigVMAddExposedPinAction::StaticStruct(); }
	virtual bool Undo(URigVMController* InController) override;
	virtual bool Redo(URigVMController* InController) override;

	UPROPERTY()
	FString PinName;
	
	UPROPERTY()
	ERigVMPinDirection Direction = ERigVMPinDirection::Input;

	UPROPERTY()
	FString CPPType;

	UPROPERTY()
	FString CPPTypeObjectPath;
	
	UPROPERTY()
	FString DefaultValue;
};

/**
 * An action exposing a pin as a parameter
 */
USTRUCT()
struct FRigVMRemoveExposedPinAction : public FRigVMBaseAction
{
	GENERATED_BODY()

public:

	FRigVMRemoveExposedPinAction();
	FRigVMRemoveExposedPinAction(URigVMPin* InPin);
	virtual ~FRigVMRemoveExposedPinAction() {};
	virtual UScriptStruct* GetScriptStruct() const override { return FRigVMRemoveExposedPinAction::StaticStruct(); }
	virtual bool Undo(URigVMController* InController) override;
	virtual bool Redo(URigVMController* InController) override;

	UPROPERTY()
	FString PinName;
	
	UPROPERTY()
	ERigVMPinDirection Direction = ERigVMPinDirection::Input;

	UPROPERTY()
	FString CPPType;

	UPROPERTY()
	FString CPPTypeObjectPath;

	UPROPERTY()
	FString DefaultValue;

	UPROPERTY()
	int32 PinIndex;
};

/**
 * An action renaming an exposed in the graph.
 */
USTRUCT()
struct FRigVMRenameExposedPinAction : public FRigVMBaseAction
{
	GENERATED_BODY()

public:

	FRigVMRenameExposedPinAction() {}
	FRigVMRenameExposedPinAction(const FName& InOldPinName, const FName& InNewPinName);
	virtual ~FRigVMRenameExposedPinAction() {};
	virtual UScriptStruct* GetScriptStruct() const override { return FRigVMRenameExposedPinAction::StaticStruct(); }
	virtual bool Undo(URigVMController* InController) override;
	virtual bool Redo(URigVMController* InController) override;

	UPROPERTY()
	FString OldPinName;

	UPROPERTY()
	FString NewPinName;
};

/**
 * An action to reorder pins on a node
 */
USTRUCT()
struct FRigVMSetPinIndexAction : public FRigVMBaseAction
{
	GENERATED_BODY()

public:

	FRigVMSetPinIndexAction();
	FRigVMSetPinIndexAction(URigVMPin* InPin, int32 InNewIndex);
	virtual ~FRigVMSetPinIndexAction() {};
	virtual UScriptStruct* GetScriptStruct() const override { return FRigVMSetPinIndexAction::StaticStruct(); }
	virtual bool Undo(URigVMController* InController) override;
	virtual bool Redo(URigVMController* InController) override;

	UPROPERTY()
	FString PinPath;

	UPROPERTY()
	int32 OldIndex;

	UPROPERTY()
	int32 NewIndex;
};

/**
* An action to remap a variable inside of a function reference node renaming a node in the graph.
*/
USTRUCT()
struct FRigVMSetRemappedVariableAction : public FRigVMBaseAction
{
	GENERATED_BODY()

public:

	FRigVMSetRemappedVariableAction() {}
	FRigVMSetRemappedVariableAction(URigVMFunctionReferenceNode* InFunctionRefNode, const FName& InInnerVariableName,
		const FName& InOldOuterVariableName, const FName& InNewOuterVariableName);
	virtual ~FRigVMSetRemappedVariableAction() {};
	virtual UScriptStruct* GetScriptStruct() const override { return FRigVMSetRemappedVariableAction::StaticStruct(); }
	virtual bool Undo(URigVMController* InController) override;
	virtual bool Redo(URigVMController* InController) override;

	UPROPERTY()
	FString NodePath;

	UPROPERTY()
	FName InnerVariableName;

	UPROPERTY()
	FName OldOuterVariableName;

	UPROPERTY()
	FName NewOuterVariableName;
};

/**
* An action to add a local variable.
*/
USTRUCT()
struct FRigVMAddLocalVariableAction : public FRigVMBaseAction
{
	GENERATED_BODY()

public:

	FRigVMAddLocalVariableAction() {}
	FRigVMAddLocalVariableAction(const FRigVMGraphVariableDescription& InLocalVariable);
	virtual ~FRigVMAddLocalVariableAction() {};
	virtual UScriptStruct* GetScriptStruct() const override { return FRigVMAddLocalVariableAction::StaticStruct(); }
	virtual bool Undo(URigVMController* InController) override;
	virtual bool Redo(URigVMController* InController) override;

	UPROPERTY()
	FRigVMGraphVariableDescription LocalVariable;
};

/**
* An action to remove a local variable.
*/
USTRUCT()
struct FRigVMRemoveLocalVariableAction : public FRigVMBaseAction
{
	GENERATED_BODY()

public:

	FRigVMRemoveLocalVariableAction() {}
	FRigVMRemoveLocalVariableAction(const FRigVMGraphVariableDescription& InLocalVariable);
	virtual ~FRigVMRemoveLocalVariableAction() {};
	virtual UScriptStruct* GetScriptStruct() const override { return FRigVMRemoveLocalVariableAction::StaticStruct(); }
	virtual bool Undo(URigVMController* InController) override;
	virtual bool Redo(URigVMController* InController) override;

	UPROPERTY()
	FRigVMGraphVariableDescription LocalVariable;
};

/**
* An action to rename a local variable.
*/
USTRUCT()
struct FRigVMRenameLocalVariableAction : public FRigVMBaseAction
{
	GENERATED_BODY()

public:

	FRigVMRenameLocalVariableAction() {}
	FRigVMRenameLocalVariableAction(const FName& InOldName, const FName& InNewName);
	virtual ~FRigVMRenameLocalVariableAction() {};
	virtual UScriptStruct* GetScriptStruct() const override { return FRigVMRenameLocalVariableAction::StaticStruct(); }
	virtual bool Undo(URigVMController* InController) override;
	virtual bool Redo(URigVMController* InController) override;

	UPROPERTY()
	FName OldVariableName;

	UPROPERTY()
	FName NewVariableName;
};

/**
* An action to change the type of a local variable.
*/
USTRUCT()
struct FRigVMChangeLocalVariableTypeAction : public FRigVMBaseAction
{
	GENERATED_BODY()

public:
	FRigVMChangeLocalVariableTypeAction();
	FRigVMChangeLocalVariableTypeAction(const FRigVMGraphVariableDescription& InLocalVariable, const FString& InCPPType, UObject* InCPPTypeObject);
	virtual ~FRigVMChangeLocalVariableTypeAction() {};
	virtual UScriptStruct* GetScriptStruct() const override { return FRigVMChangeLocalVariableTypeAction::StaticStruct(); }
	virtual bool Undo(URigVMController* InController) override;
	virtual bool Redo(URigVMController* InController) override;

	UPROPERTY()
	FRigVMGraphVariableDescription LocalVariable;

	UPROPERTY()
	FString CPPType;

	UPROPERTY()
	TObjectPtr<UObject> CPPTypeObject;
};

/**
* An action to change the default value of a local variable.
*/
USTRUCT()
struct FRigVMChangeLocalVariableDefaultValueAction : public FRigVMBaseAction
{
	GENERATED_BODY()

public:
	FRigVMChangeLocalVariableDefaultValueAction();
	FRigVMChangeLocalVariableDefaultValueAction(const FRigVMGraphVariableDescription& InLocalVariable, const FString& InDefaultValue);
	virtual ~FRigVMChangeLocalVariableDefaultValueAction() {};
	virtual UScriptStruct* GetScriptStruct() const override { return FRigVMChangeLocalVariableDefaultValueAction::StaticStruct(); }
	virtual bool Undo(URigVMController* InController) override;
	virtual bool Redo(URigVMController* InController) override;

	UPROPERTY()
	FRigVMGraphVariableDescription LocalVariable;

	UPROPERTY()
	FString DefaultValue;
};

/**
* An action adding a array node to the graph.
*/
USTRUCT()
struct FRigVMAddArrayNodeAction : public FRigVMBaseAction
{
	GENERATED_BODY()

public:

	FRigVMAddArrayNodeAction();
	FRigVMAddArrayNodeAction(URigVMArrayNode* InNode);
	virtual ~FRigVMAddArrayNodeAction() {};
	virtual UScriptStruct* GetScriptStruct() const override { return FRigVMAddArrayNodeAction::StaticStruct(); }
	virtual bool Undo(URigVMController* InController) override;
	virtual bool Redo(URigVMController* InController) override;

	UPROPERTY()
	ERigVMOpCode OpCode;
	
	UPROPERTY()
	FString CPPType;

	UPROPERTY()
	FString CPPTypeObjectPath;

	UPROPERTY()
	FVector2D Position;

	UPROPERTY()
	FString NodePath;
};

/**
 * An action to promote a function to collapse node or vice versa
 */
USTRUCT()
struct FRigVMPromoteNodeAction : public FRigVMBaseAction
{
	GENERATED_BODY()

public:

	FRigVMPromoteNodeAction();
	FRigVMPromoteNodeAction(const URigVMNode* InNodeToPromote, const FString& InNodePath, const FString& InFunctionDefinitionPath);
	virtual ~FRigVMPromoteNodeAction() {};
	virtual UScriptStruct* GetScriptStruct() const override { return FRigVMPromoteNodeAction::StaticStruct(); }
	virtual bool Undo(URigVMController* InController) override;
	virtual bool Redo(URigVMController* InController) override;

	UPROPERTY()
	FString LibraryNodePath;

	UPROPERTY()
	FString FunctionDefinitionPath;

	UPROPERTY()
	bool bFromFunctionToCollapseNode;
};

/**
 * An action adding an invoke entry node to the graph.
 */
USTRUCT()
struct FRigVMAddInvokeEntryNodeAction : public FRigVMBaseAction
{
	GENERATED_BODY()

public:

	FRigVMAddInvokeEntryNodeAction();
	FRigVMAddInvokeEntryNodeAction(URigVMInvokeEntryNode* InNode);
	virtual ~FRigVMAddInvokeEntryNodeAction() {};
	virtual UScriptStruct* GetScriptStruct() const override { return FRigVMAddInvokeEntryNodeAction::StaticStruct(); }
	virtual bool Undo(URigVMController* InController) override;
	virtual bool Redo(URigVMController* InController) override;

	UPROPERTY()
	FName EntryName;
	
	UPROPERTY()
	FVector2D Position;

	UPROPERTY()
	FString NodePath;
};
