// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "NiagaraGraph.h"
#include "EdGraph/EdGraphSchema.h"
#include "GraphEditorDragDropAction.h"
#include "DragAndDrop/DecoratedDragDropOp.h"
#include "NiagaraEditorCommon.h"
#include "ViewModels/Stack/NiagaraParameterHandle.h"
#include "NiagaraActions.generated.h"

UENUM()
enum class ENiagaraMenuSections : uint8
{
	Suggested = 0,
	General = 1
};

UENUM()
enum class EScriptSource : uint8
{
	Niagara,
	Game,
	Plugins,
	Developer,
	Unknown
};

USTRUCT()
struct NIAGARAEDITOR_API FNiagaraActionSourceData
{
	GENERATED_BODY()
	
	FNiagaraActionSourceData()
	{}
	FNiagaraActionSourceData(const EScriptSource& InSource, const FText& InSourceText, bool bInDisplaySource = false)
	{
		Source = InSource;
		SourceText = InSourceText;
		bDisplaySource = bInDisplaySource;
	}	
	
	EScriptSource Source = EScriptSource::Unknown;
	FText SourceText = FText::GetEmpty();
	bool bDisplaySource = false;	
};

USTRUCT()
struct NIAGARAEDITOR_API FNiagaraMenuAction : public FEdGraphSchemaAction
{
	GENERATED_USTRUCT_BODY();

	DECLARE_DELEGATE(FOnExecuteStackAction);
	DECLARE_DELEGATE_RetVal(bool, FCanExecuteStackAction);

	FNiagaraMenuAction() {}
	FNiagaraMenuAction(FText InNodeCategory, FText InMenuDesc, FText InToolTip, const int32 InGrouping, FText InKeywords, FOnExecuteStackAction InAction, int32 InSectionID = 0);
	FNiagaraMenuAction(FText InNodeCategory, FText InMenuDesc, FText InToolTip, const int32 InGrouping, FText InKeywords, FOnExecuteStackAction InAction, FCanExecuteStackAction InCanPerformAction, int32 InSectionID = 0);

	void ExecuteAction()
	{
		if (CanExecute())
		{
			Action.ExecuteIfBound();
		}
	}

	bool CanExecute() const
	{
		// Fire the 'can execute' delegate if we have one, otherwise always return true
		return CanPerformAction.IsBound() ? CanPerformAction.Execute() : true;
	}

	bool IsExperimental = false;

	TOptional<FNiagaraVariable> GetParameterVariable() const;
	void SetParameterVariable(const FNiagaraVariable& InParameterVariable);
	void SetSectionId(const int32 NewSectionId) { SectionID = NewSectionId; };

private:
	TOptional<FNiagaraVariable> ParameterVariable;
	FOnExecuteStackAction Action;
	FCanExecuteStackAction CanPerformAction;
};

class FNiagaraMenuActionCollector
{
public:
	void AddAction(TSharedPtr<FNiagaraMenuAction> Action, int32 SortOrder, const FString& Category = FString());
	void AddAllActionsTo(FGraphActionListBuilderBase& ActionBuilder);

private:
	struct FCollectedAction
	{
		TSharedPtr<FNiagaraMenuAction> Action;
		int32 SortOrder;
		FString Category;
	};

	TArray<FCollectedAction> Actions;
};

// new action hierarchy for the new menus. Prefer inheriting from this rather than the above
// this action does not have any use; inherit from it and provide your own functionality
USTRUCT()
struct NIAGARAEDITOR_API FNiagaraMenuAction_Base
{
	GENERATED_USTRUCT_BODY();

	DECLARE_DELEGATE(FOnExecuteAction);
	DECLARE_DELEGATE_RetVal(bool, FCanExecuteAction);

	FNiagaraMenuAction_Base() {}
	FNiagaraMenuAction_Base(FText DisplayName, ENiagaraMenuSections Section, TArray<FString> InNodeCategories, FText InToolTip, FText InKeywords, float InIntrinsicWeightMultiplier = 1.f);

	void UpdateFullSearchText();

	bool bIsExperimental = false;

	bool bIsInLibrary = true;

	/** Top level section this action belongs to. */
	ENiagaraMenuSections Section;
	
	/** Nested categories below a top level section. Can be empty */
	TArray<FString> Categories;

	/** The DisplayName used in lists */
	FText DisplayName;

	/** The Tooltip text for this action */
	FText ToolTip;

	/** Additional keywords that should be considered for searching */
	FText Keywords;

	/** Additional data about where this action originates. Useful to display additional data such as the owning module. */
	FNiagaraActionSourceData SourceData;

	/** A string that combines all kinds of search terms */
	FString FullSearchString;

	/** A multiplier intrinsic to the action. Can be used to tweak search relevance depending on context (like module actions in a module script) */
	float SearchWeightMultiplier = 1.f;
};

USTRUCT()
struct NIAGARAEDITOR_API FNiagaraMenuAction_Generic : public FNiagaraMenuAction_Base
{
	GENERATED_BODY()

	FNiagaraMenuAction_Generic() {}

	FNiagaraMenuAction_Generic(FOnExecuteAction ExecuteAction, FCanExecuteAction InCanExecuteAction,
		FText InDisplayName, ENiagaraMenuSections Section, TArray<FString> InNodeCategories, FText InToolTip, FText InKeywords, float InIntrinsicWeightMultiplier = 1.f)
    : FNiagaraMenuAction_Base(InDisplayName, Section, InNodeCategories, InToolTip, InKeywords, InIntrinsicWeightMultiplier)
	{
		Action = ExecuteAction;
		CanExecuteAction = InCanExecuteAction;
	}

	FNiagaraMenuAction_Generic(FOnExecuteAction ExecuteAction,
        FText InDisplayName, ENiagaraMenuSections Section, TArray<FString> InNodeCategories, FText InToolTip, FText InKeywords, float InIntrinsicWeightMultiplier = 1.f)
    : FNiagaraMenuAction_Base(InDisplayName, Section, InNodeCategories, InToolTip, InKeywords, InIntrinsicWeightMultiplier)
	{
		Action = ExecuteAction;
	}

	void Execute()
	{
		if(CanExecuteAction.IsBound())
		{
			if(CanExecuteAction.Execute())
			{
				Action.ExecuteIfBound();
			}
		}
		else
		{
			Action.ExecuteIfBound();
		}
	}

	TOptional<FNiagaraVariable> GetParameterVariable() const;
	void SetParameterVariable(const FNiagaraVariable& InParameterVariable);
protected:
	FOnExecuteAction Action;
	FCanExecuteAction CanExecuteAction;

	TOptional<FNiagaraVariable> ParameterVariable;
};

USTRUCT()
struct NIAGARAEDITOR_API FNiagaraAction_NewNode : public FNiagaraMenuAction_Generic
{
	GENERATED_BODY()

	FNiagaraAction_NewNode() {}
	FNiagaraAction_NewNode(
		FText InDisplayName, ENiagaraMenuSections Section, TArray<FString> InNodeCategories, FText InToolTip, FText InKeywords, float InIntrinsicWeightMultiplier = 1.f)
		: FNiagaraMenuAction_Generic(FOnExecuteAction(), InDisplayName, Section, InNodeCategories, InToolTip, InKeywords, InIntrinsicWeightMultiplier)
	{		
	}

	class UEdGraphNode* CreateNode(UEdGraph* Graph, UEdGraphPin* FromPin, FVector2D NodePosition, bool bSelectNewNode = true) const;
	class UEdGraphNode* CreateNode(UEdGraph* Graph, TArray<UEdGraphPin*>& FromPins, FVector2D NodePosition, bool bSelectNewNode = true) const;

	UPROPERTY()
	TObjectPtr<class UEdGraphNode> NodeTemplate = nullptr;
};

struct NIAGARAEDITOR_API FNiagaraParameterAction : public FEdGraphSchemaAction
{
	FNiagaraParameterAction()
		: ScriptVar(nullptr)
		, Parameter(FNiagaraVariable())
		, ReferenceCollection()
		, bIsExternallyReferenced(false)
		, bIsSourcedFromCustomStackContext(false)
		, ParametersWithNamespaceModifierRenamePendingWeak()
	{
	}

	FNiagaraParameterAction(const FNiagaraVariable& InParameter,
		const TArray<FNiagaraGraphParameterReferenceCollection>& InReferenceCollection,
		FText InNodeCategory, FText InMenuDesc, FText InToolTip, const int32 InGrouping, FText InKeywords,
		TSharedPtr<TArray<FName>> ParametersWithNamespaceModifierRenamePending,
		int32 InSectionID = 0);

	FNiagaraParameterAction(const FNiagaraVariable& InParameter, 
		FText InNodeCategory, FText InMenuDesc, FText InToolTip, const int32 InGrouping, FText InKeywords,
		TSharedPtr<TArray<FName>> ParametersWithNamespaceModifierRenamePending,
		int32 InSectionID = 0);

	FNiagaraParameterAction(const UNiagaraScriptVariable* InScriptVar,
		FText InNodeCategory, FText InMenuDesc, FText InToolTip, const int32 InGrouping, FText InKeywords,
		int32 InSectionID = 0);

	/** Simple type info. */
	static FName StaticGetTypeId() { return FNiagaraEditorStrings::FNiagaraParameterActionId; };
	virtual FName GetTypeId() const { return StaticGetTypeId(); };

	const UNiagaraScriptVariable* GetScriptVar() const;

	const FNiagaraVariable& GetParameter() const;

	TArray<FNiagaraGraphParameterReferenceCollection>& GetReferenceCollection();

	bool GetIsNamespaceModifierRenamePending() const;

	void SetIsNamespaceModifierRenamePending(bool bIsNamespaceModifierRenamePending);

	bool GetIsExternallyReferenced() const;

	void SetIsExternallyReferenced(bool bInIsExternallyReferenced);

	bool GetIsSourcedFromCustomStackContext() const;

	void SetIsSourcedFromCustomStackContext(bool bInIsSourcedFromCustomStackContext);

private:
	const UNiagaraScriptVariable* ScriptVar;

	FNiagaraVariable Parameter;

	TArray<FNiagaraGraphParameterReferenceCollection> ReferenceCollection;

	bool bIsExternallyReferenced;

	bool bIsSourcedFromCustomStackContext;

	TWeakPtr<TArray<FName>> ParametersWithNamespaceModifierRenamePendingWeak;
};

struct NIAGARAEDITOR_API FNiagaraScriptParameterAction : public FEdGraphSchemaAction
{
	FNiagaraScriptParameterAction() {}
	FNiagaraScriptParameterAction(const FNiagaraVariable& InVariable, const FNiagaraVariableMetaData& InVariableMetaData);
};

class NIAGARAEDITOR_API FNiagaraParameterGraphDragOperation : public FGraphSchemaActionDragDropAction
{
public:
	DRAG_DROP_OPERATOR_TYPE(FNiagaraParameterGraphDragOperation, FGraphSchemaActionDragDropAction)

	static TSharedRef<FNiagaraParameterGraphDragOperation> New(const TSharedPtr<FEdGraphSchemaAction>& InActionNode);

	// FGraphEditorDragDropAction interface
	virtual void HoverTargetChanged() override;
	virtual FReply DroppedOnNode(FVector2D ScreenPosition, FVector2D GraphPosition) override;
	virtual FReply DroppedOnPanel(const TSharedRef< class SWidget >& Panel, FVector2D ScreenPosition, FVector2D GraphPosition, UEdGraph& Graph) override;
	// End of FGraphEditorDragDropAction

	/** Set if operation is modified by alt */
	void SetAltDrag(bool InIsAltDrag) { bAltDrag = InIsAltDrag; }

	/** Set if operation is modified by the ctrl key */
	void SetCtrlDrag(bool InIsCtrlDrag) { bControlDrag = InIsCtrlDrag; }

	/** Returns true if the drag operation is currently hovering over the supplied node */
	bool IsCurrentlyHoveringNode(const UEdGraphNode* TestNode) const;

	const TSharedPtr<FEdGraphSchemaAction>& GetSourceAction() const { return SourceAction; }

protected:
	/** Constructor */
	FNiagaraParameterGraphDragOperation();

	/** Structure for required node construction parameters */
	struct FNiagaraParameterNodeConstructionParams
	{
		FNiagaraParameterNodeConstructionParams() = delete;

		FNiagaraParameterNodeConstructionParams(
			const FVector2D& InGraphPosition,
			UEdGraph* InGraph,
			const FNiagaraVariable& InParameter,
			const UNiagaraScriptVariable* InScriptVar)
			: GraphPosition(InGraphPosition)
			, Graph(InGraph)
			, Parameter(InParameter)
			, ScriptVar(InScriptVar)
		{};

		const FVector2D GraphPosition;
		UEdGraph* Graph;
		const FNiagaraVariable Parameter;
		const UNiagaraScriptVariable* ScriptVar;
	};

	static void MakeGetMap(FNiagaraParameterNodeConstructionParams InParams);
	static void MakeSetMap(FNiagaraParameterNodeConstructionParams InParams);
	static void MakeStaticSwitch(FNiagaraParameterNodeConstructionParams InParams, const UNiagaraScriptVariable* ScriptVariable);

	virtual EVisibility GetIconVisible() const override;
	virtual EVisibility GetErrorIconVisible() const override;

	/** Was ctrl held down at start of drag */
	bool bControlDrag;
	/** Was alt held down at the start of drag */
	bool bAltDrag;
};

class NIAGARAEDITOR_API FNiagaraParameterDragOperation : public FDecoratedDragDropOp
{
public:
	DRAG_DROP_OPERATOR_TYPE(FNiagaraParameterDragOperation, FDecoratedDragDropOp)

	FNiagaraParameterDragOperation(TSharedPtr<FEdGraphSchemaAction> InSourceAction)
		: SourceAction(InSourceAction)
	{
	}

	const TSharedPtr<FEdGraphSchemaAction>& GetSourceAction() const { return SourceAction; }

	virtual TSharedPtr<SWidget> GetDefaultDecorator() const override;

	void SetAdditionalText(FText InAdditionalText) { CurrentHoverText = InAdditionalText; }
	EVisibility IsTextVisible() const;
private:
	TSharedPtr<FEdGraphSchemaAction> SourceAction;
	TOptional<FNiagaraVariable> TargetParameter;
};

class NIAGARAEDITOR_API FNiagaraScriptDragOperation : public FDecoratedDragDropOp
{
public:
	DRAG_DROP_OPERATOR_TYPE(FNiagaraScriptDragOperation, FDecoratedDragDropOp)

		FNiagaraScriptDragOperation(UNiagaraScript* InPayloadScript, const FGuid& InVersion, const FText& InFriendlyName)
		: Script(InPayloadScript), Version(InVersion), FriendlyName(InFriendlyName)
	{
	}

	TWeakObjectPtr<UNiagaraScript> Script;
	FGuid Version;
	FText FriendlyName;
};