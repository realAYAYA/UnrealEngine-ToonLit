// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IDetailCustomNodeBuilder.h"
#include "NiagaraTypes.h"
#include "NiagaraMetaDataCustomNodeBuilder.generated.h"

class UNiagaraGraph;
class FDetailWidgetRow;
class IDetailChildrenBuilder;

USTRUCT()
struct FNiagaraVariableMetaDataContainer
{
	GENERATED_BODY();

	UPROPERTY(EditAnywhere, Category=MetaData)
	FNiagaraVariableMetaData MetaData;
};

class FNiagaraMetaDataCustomNodeBuilder : public IDetailCustomNodeBuilder, public TSharedFromThis<FNiagaraMetaDataCustomNodeBuilder>
{
public:
	FNiagaraMetaDataCustomNodeBuilder();
	
	void Initialize(UNiagaraGraph* InScriptGraph);

	~FNiagaraMetaDataCustomNodeBuilder();

	virtual void SetOnRebuildChildren(FSimpleDelegate InOnRegenerateChildren) override;

	virtual void GenerateHeaderRowContent(FDetailWidgetRow& NodeRow) {}
	virtual void Tick(float DeltaTime) override {}
	virtual bool RequiresTick() const override { return false; }
	virtual bool InitiallyCollapsed() const { return true; }
	
	virtual FName GetName() const  override;

	virtual void GenerateChildContent(IDetailChildrenBuilder& ChildrenBuilder) override;

	void Rebuild();

private:
	void OnGraphChanged(const struct FEdGraphEditAction& Action);

	void MetaDataPropertyHandleChanged(FNiagaraVariable ParameterVariable, TSharedRef<FStructOnScope> MetaDataContainerStruct);

private:
	TWeakObjectPtr<UNiagaraGraph> ScriptGraph;
	FDelegateHandle OnGraphChangedHandle;
	FDelegateHandle OnGraphNeedsRecompileHandle;
	FSimpleDelegate OnRebuildChildren;
};
