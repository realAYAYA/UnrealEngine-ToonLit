// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "NiagaraCommon.h"
#include "NiagaraEditorCommon.h"
#include "NiagaraNode.h"
#include "NiagaraNodeInput.generated.h"

class UEdGraphPin;
class UNiagaraGraph;


USTRUCT()
struct FNiagaraInputExposureOptions
{
	GENERATED_USTRUCT_BODY()
		
	FNiagaraInputExposureOptions()
		: bExposed(1)
		, bRequired(1)
		, bCanAutoBind(0)
		, bHidden(0)
	{}

	/** If this input is exposed to it's caller. */
	UPROPERTY(EditAnywhere, Category = Exposure)
	uint32 bExposed : 1;

	/** If this input is required to be bound. */
	UPROPERTY(EditAnywhere, Category = Exposure, meta = (editcondition = "bExposed"))
	uint32 bRequired : 1;

	/** If this input can auto-bind to system parameters and emitter attributes. Will never auto bind to custom parameters. */
	UPROPERTY(EditAnywhere, Category = Exposure, meta = (editcondition = "bExposed"))
	uint32 bCanAutoBind : 1;

	/** If this input should be hidden in the advanced pin section of it's caller. */
	UPROPERTY(EditAnywhere, Category = Exposure, meta = (editcondition = "bExposed"))
	uint32 bHidden : 1;
};


UCLASS(MinimalAPI)
class UNiagaraNodeInput : public UNiagaraNode
{
	GENERATED_UCLASS_BODY()

public:
	UPROPERTY(EditAnywhere, Category = Input, meta = (SkipForCompileHash = "true"))
	FNiagaraVariable Input;

	UPROPERTY(EditAnywhere, Category = Input)
	ENiagaraInputNodeUsage Usage;

	/** Controls where this input is relative to others in the calling node. */
	UPROPERTY(EditAnywhere, Category = Input, meta = (SkipForCompileHash = "true"))
	int32 CallSortPriority;

	/** Controls this inputs exposure to callers. */
	UPROPERTY(EditAnywhere, Category = Input)
	FNiagaraInputExposureOptions ExposureOptions;

	//~ Begin UObject Interface
	virtual void PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent) override;
	virtual void PostLoad() override;
	//~ End UObject Interface

	//~ Begin EdGraphNode Interface
	virtual void AllocateDefaultPins() override;
	virtual FText GetNodeTitle(ENodeTitleType::Type TitleType) const override;
	virtual FLinearColor GetNodeTitleColor() const override;
	virtual void AutowireNewNode(UEdGraphPin* FromPin)override;
	virtual TSharedPtr<SGraphNode> CreateVisualWidget() override;
	virtual void OnRenameNode(const FString& NewName) override;
	//~ End EdGraphNode Interface

	/** Notifies the node that the type of it's input has been changed externally. */
	void NotifyInputTypeChanged();

	/** Notifies the node that the exposure options have changed externally.*/
	void NotifyExposureOptionsChanged();

	virtual void Compile(class FHlslNiagaraTranslator* Translator, TArray<int32>& Outputs)override;
	
	bool IsExposed()const { return ExposureOptions.bExposed; }
	bool IsRequired()const { return ExposureOptions.bExposed && ExposureOptions.bRequired; }
	bool IsHidden()const { return ExposureOptions.bExposed && ExposureOptions.bHidden; }
	bool CanAutoBind()const { return ExposureOptions.bExposed && ExposureOptions.bCanAutoBind; }

	bool ReferencesSameInput(UNiagaraNodeInput* Other) const;

	virtual void BuildParameterMapHistory(FNiagaraParameterMapHistoryBuilder& OutHistory, bool bRecursive = true, bool bFilterForCompilation = true) const override;
	virtual void AppendFunctionAliasForContext(const FNiagaraGraphFunctionAliasContext& InFunctionAliasContext, FString& InOutFunctionAlias, bool& OutOnlyOncePerNodeType) override;

	static const FLinearColor TitleColor_Attribute;
	static const FLinearColor TitleColor_Constant;

	/** Generate a unique name based off of the existing names in the system.*/
	static FName GenerateUniqueName(const UNiagaraGraph* Graph, FName& ProposedName, ENiagaraInputNodeUsage Usage);

	/** Generate a unique sort index based off the existing nodes in the system.*/
	static int32 GenerateNewSortPriority(const UNiagaraGraph* Graph, FName& ProposedName, ENiagaraInputNodeUsage Usage);

	/** Given an array of nodes, sort them in place by their sort order, then lexicographically if the same.*/
	static void SortNodes(TArray<UNiagaraNodeInput*>& InOutNodes);

	UNiagaraDataInterface* GetDataInterface() const;

	void SetDataInterface(UNiagaraDataInterface* InDataInterface);

private:
	void DataInterfaceChanged();
	void ValidateDataInterface();

private:
	UPROPERTY(meta = (SkipForCompileHash = "true"))
	TObjectPtr<class UNiagaraDataInterface> DataInterface;
};

