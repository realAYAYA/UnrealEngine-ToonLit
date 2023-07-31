// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "NiagaraCommon.h"
#include "NiagaraEmitter.h"
#include "ViewModels/Stack/NiagaraStackItem.h"
#include "ViewModels/Stack/NiagaraStackScriptItemGroup.h"
#include "NiagaraStackSimulationStageGroup.generated.h"

class FNiagaraScriptViewModel;
class UNiagaraStackObject;
struct FNiagaraEventScriptProperties;
class IDetailTreeNode;

UCLASS()
class NIAGARAEDITOR_API UNiagaraStackSimulationStagePropertiesItem : public UNiagaraStackItem
{
	GENERATED_BODY()
		 
public:
	void Initialize(FRequiredEntryData InRequiredEntryData, UNiagaraSimulationStageBase* InSimulationStage);

	virtual FText GetDisplayName() const override;

	virtual bool SupportsResetToBase() const override { return true; }
	virtual bool TestCanResetToBaseWithMessage(FText& OutCanResetToBaseMessage) const override;
	virtual void ResetToBase() override;

	void SetSimulationStageEnabled(bool bIsEnabled);

protected:
	virtual void FinalizeInternal() override;

	virtual void RefreshChildrenInternal(const TArray<UNiagaraStackEntry*>& CurrentChildren, TArray<UNiagaraStackEntry*>& NewChildren, TArray<FStackIssue>& NewIssues) override;

private:
	void SimulationStagePropertiesChanged();

	bool HasBaseSimulationStage() const;

private:
	TWeakObjectPtr<UNiagaraSimulationStageBase> SimulationStage;

	mutable TOptional<bool> bHasBaseSimulationStageCache;

	mutable TOptional<bool> bCanResetToBaseCache;

	UPROPERTY()
	TObjectPtr<UNiagaraStackObject> SimulationStageObject;
};

UCLASS()
/** Meant to contain a single binding of a Emitter::EventScriptProperties to the stack.*/
class NIAGARAEDITOR_API UNiagaraStackSimulationStageGroup : public UNiagaraStackScriptItemGroup
{
	GENERATED_BODY()

public:
	DECLARE_DELEGATE(FOnModifiedSimulationStages);

public:
	void Initialize(
		FRequiredEntryData InRequiredEntryData,
		TSharedRef<FNiagaraScriptViewModel> InScriptViewModel,
		UNiagaraSimulationStageBase* InSimulationStage);

	UNiagaraSimulationStageBase* GetSimulationStage() const;
	const TObjectPtr<UNiagaraStackSimulationStagePropertiesItem>& GetSimulationStagePropertiesItem() const { return SimulationStageProperties; }
	void SetOnModifiedSimulationStages(FOnModifiedSimulationStages OnModifiedSimulationStages);

	virtual bool SupportsDelete() const override { return true; }
	virtual bool TestCanDeleteWithMessage(FText& OutCanDeleteMessage) const override;
	virtual void Delete() override;

	virtual bool SupportsInheritance() const override { return true; }
	virtual bool GetIsInherited() const override;
	virtual FText GetInheritanceMessage() const override;

	virtual bool CanDrag() const override { return true; }

	virtual bool GetIsEnabled() const override;
	virtual void SetIsEnabled(bool bEnabled) override;
	virtual bool SupportsChangeEnabled() const override { return true; }

protected:
	virtual void FinalizeInternal() override;

	virtual void RefreshChildrenInternal(const TArray<UNiagaraStackEntry*>& CurrentChildren, TArray<UNiagaraStackEntry*>& NewChildren, TArray<FStackIssue>& NewIssues) override;

	virtual TOptional<FDropRequestResponse> CanDropInternal(const FDropRequest& DropRequest) override;

	virtual TOptional<FDropRequestResponse> DropInternal(const FDropRequest& DropRequest) override;

private:
	void SimulationStagePropertiesChanged();

	bool HasBaseSimulationStage() const;
	
private:
	TWeakObjectPtr<UNiagaraSimulationStageBase> SimulationStage;

	mutable TOptional<bool> bHasBaseSimulationStageCache;

	FOnModifiedSimulationStages OnModifiedSimulationStagesDelegate;

	UPROPERTY()
	TObjectPtr<UNiagaraStackSimulationStagePropertiesItem> SimulationStageProperties;
};
