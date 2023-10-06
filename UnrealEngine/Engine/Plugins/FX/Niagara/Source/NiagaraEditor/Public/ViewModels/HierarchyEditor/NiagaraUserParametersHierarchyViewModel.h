// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ViewModels/HierarchyEditor/NiagaraHierarchyViewModelBase.h"
#include "NiagaraScriptVariable.h"
#include "NiagaraUserParametersHierarchyViewModel.generated.h"

UCLASS()
class UNiagaraHierarchyUserParameterRefreshContext : public UNiagaraHierarchyDataRefreshContext
{
	GENERATED_BODY()

public:
	void SetSystem(UNiagaraSystem* InSystem) { System = InSystem; }
	const UNiagaraSystem* GetSystem() const { return System; }
private:
	UPROPERTY(Transient)
	TObjectPtr<UNiagaraSystem> System = nullptr;
};

UCLASS()
class UNiagaraHierarchyUserParameter : public UNiagaraHierarchyItem
{
	GENERATED_BODY()

public:
	UNiagaraHierarchyUserParameter() {}
	virtual ~UNiagaraHierarchyUserParameter() override {}
	
	void Initialize(UNiagaraScriptVariable& InUserParameterScriptVariable);
	
	const FNiagaraVariable& GetUserParameter() const { return UserParameterScriptVariable->Variable; }

	virtual FString ToString() const override { return GetUserParameter().GetName().ToString(); }

	const UNiagaraScriptVariable* GetScriptVariable() const { return UserParameterScriptVariable; }
private:
	UPROPERTY()
	TObjectPtr<UNiagaraScriptVariable> UserParameterScriptVariable;
};

struct FNiagaraHierarchyUserParameterViewModel : public FNiagaraHierarchyItemViewModel
{
	FNiagaraHierarchyUserParameterViewModel(UNiagaraHierarchyUserParameter* InItem, TSharedPtr<FNiagaraHierarchyItemViewModelBase> InParent, TWeakObjectPtr<UNiagaraHierarchyViewModelBase> InHierarchyViewModel, bool bInIsForHierarchy)
	: FNiagaraHierarchyItemViewModel(InItem, InParent, InHierarchyViewModel, bInIsForHierarchy) {}
	
	/** For editing in the details panel we want to handle the script variable that is represented by the hierarchy item, not the hierarchy item itself. */
	virtual UObject* GetDataForEditing() override;

	virtual bool RepresentsExternalData() const override { return true; }
	virtual bool DoesExternalDataStillExist(const UNiagaraHierarchyDataRefreshContext* Context) const override;
};

UCLASS()
class UNiagaraUserParametersHierarchyViewModel : public UNiagaraHierarchyViewModelBase
{
	GENERATED_BODY()
public:
	UNiagaraUserParametersHierarchyViewModel() {}
	virtual ~UNiagaraUserParametersHierarchyViewModel() override
	{
	}

	TSharedRef<FNiagaraSystemViewModel> GetSystemViewModel() const;

	void Initialize(TSharedRef<FNiagaraSystemViewModel> InSystemViewModel);
	
	virtual UNiagaraHierarchyRoot* GetHierarchyRoot() const override;
	virtual TSharedPtr<FNiagaraHierarchyItemViewModelBase> CreateViewModelForData(UNiagaraHierarchyItemBase* ItemBase, TSharedPtr<FNiagaraHierarchyItemViewModelBase> Parent) override;
	
	virtual void PrepareSourceItems(UNiagaraHierarchyRoot* SourceRoot, TSharedPtr<FNiagaraHierarchyRootViewModel>) override;
	virtual void SetupCommands() override;
	
	virtual TSharedRef<FNiagaraHierarchyDragDropOp> CreateDragDropOp(TSharedRef<FNiagaraHierarchyItemViewModelBase> Item) override;
	
	virtual bool SupportsDetailsPanel() override { return true; }
	virtual TArray<TTuple<UClass*, FOnGetDetailCustomizationInstance>> GetInstanceCustomizations() override;
protected:
	virtual void FinalizeInternal() override;
private:
	TWeakPtr<FNiagaraSystemViewModel> SystemViewModelWeak;
};

class FNiagaraUserParameterHierarchyDragDropOp : public FNiagaraHierarchyDragDropOp
{
public:
	DRAG_DROP_OPERATOR_TYPE(FNiagaraUserParameterDragDropOp, FNiagaraHierarchyDragDropOp)

	FNiagaraUserParameterHierarchyDragDropOp(TSharedPtr<FNiagaraHierarchyItemViewModelBase> UserParameterItem) : FNiagaraHierarchyDragDropOp(UserParameterItem) {}

	FNiagaraVariable GetUserParameter() const
	{
		const UNiagaraHierarchyUserParameter* HierarchyUserParameter = CastChecked<UNiagaraHierarchyUserParameter>(DraggedItem.Pin()->GetData());
		return HierarchyUserParameter->GetUserParameter();
	}
	
	virtual TSharedRef<SWidget> CreateCustomDecorator() const override;
};
