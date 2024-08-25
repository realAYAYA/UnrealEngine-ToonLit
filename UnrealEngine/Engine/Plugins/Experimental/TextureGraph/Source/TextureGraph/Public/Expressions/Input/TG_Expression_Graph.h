// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "Expressions/TG_Expression.h"

#include "TG_Expression_Graph.generated.h"

class UTextureGraph;

//////////////////////////////////////////////////////////////////////////
//// Generic subgraph
//////////////////////////////////////////////////////////////////////////

UCLASS(Hidden)
class TEXTUREGRAPH_API UTG_Expression_Graph : public UTG_Expression
{
	GENERATED_BODY()
public:
	TG_DECLARE_DYNAMIC_EXPRESSION(TG_Category::Input);
	virtual void Evaluate(FTG_EvaluationContext* InContext) override;

protected:
	
	mutable TArray<FTG_Id> InParamIds;
	mutable TArray<FTG_Id> OutParamIds;

	virtual UTG_Graph* GetGraph() const { return nullptr; }
	void NotifyGraphChanged();

	virtual void SetupAndEvaluate(FTG_EvaluationContext* InContext) override;
};

//////////////////////////////////////////////////////////////////////////
// TextureGraph Expression
//////////////////////////////////////////////////////////////////////////

UCLASS()
class TEXTUREGRAPH_API UTG_Expression_TextureGraph : public UTG_Expression_Graph
{
	GENERATED_BODY()
public:

	UTG_Expression_TextureGraph();
	virtual ~UTG_Expression_TextureGraph();
	

#if WITH_EDITOR
protected:
	// Only in editor mode, in the case the TextureGraph is assigned from Details panel we need to remember the previous version
	TWeakObjectPtr<UTextureGraph>	PreEditTextureGraph; 
public:
	virtual void PreEditChange(FProperty* PropertyAboutToChange) override;
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
	virtual void PostEditUndo() override;
#endif

	virtual void Evaluate(FTG_EvaluationContext* InContext) override;
	virtual bool Validate(MixUpdateCyclePtr	Cycle) override;

	UPROPERTY(EditAnywhere, Setter, Category = NoCategory, meta = (TGType = "TG_Setting"))
	TObjectPtr<UTextureGraph>	TextureGraph;				// The TextureGraph to use for the sub-graph

	void SetTextureGraph(UTextureGraph* InTextureGraph);

	virtual bool CanHandleAsset(UObject* Asset) override;
	virtual void SetAsset(UObject* Asset) override;
	
protected:
	virtual void Initialize() override;
	virtual void CopyVarGeneric(const FTG_Argument& Arg, FTG_Var* InVar, bool CopyVarToArg) override;

	virtual UTG_Graph* GetGraph() const override;

	void OnTextureGraphPreSave(UObject* Object, FObjectPreSaveContext SaveContext);
private:
	UPROPERTY(Transient)
	TObjectPtr<UTG_Graph>	RuntimeGraph;		// The runtime graph instance to use for the sub-graph

	FDelegateHandle PreSaveHandle;				// delegate handle for global ObjectPreSave event 
	
	bool CheckDependencies(const UTextureGraph* InTextureGraph) const;
	void SetTextureGraphInternal(UTextureGraph* InTextureGraph);

public:

	virtual FText GetTooltipText() const override { return FText::FromString(TEXT("Encapsulates another graph as a node with the graph's input and output parameters exposed as the node input and output pins.")); }

};