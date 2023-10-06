// Copyright Epic Games, Inc. All Rights Reserved.

#include "ViewModels/HierarchyEditor/NiagaraUserParametersHierarchyViewModel.h"
#include "NiagaraEditorUtilities.h"
#include "NiagaraEmitterEditorData.h"
#include "NiagaraNodeFunctionCall.h"
#include "NiagaraScriptVariable.h"
#include "NiagaraSystem.h"
#include "NiagaraSystemEditorData.h"
#include "SDropTarget.h"
#include "Customizations/NiagaraScriptVariableCustomization.h"
#include "ViewModels/NiagaraSystemViewModel.h"

#define LOCTEXT_NAMESPACE "NiagaraUserParametersHierarchyEditor"

void UNiagaraHierarchyUserParameter::Initialize(UNiagaraScriptVariable& InUserParameterScriptVariable)
{
	UserParameterScriptVariable = &InUserParameterScriptVariable;
	SetIdentity(FNiagaraHierarchyIdentity({InUserParameterScriptVariable.Metadata.GetVariableGuid()}, {}));
}

UObject* FNiagaraHierarchyUserParameterViewModel::GetDataForEditing()
{
	UNiagaraHierarchyUserParameter* HierarchyUserParameter = Cast<UNiagaraHierarchyUserParameter>(GetDataMutable<UNiagaraHierarchyUserParameter>());
	FNiagaraVariable ContainedVariable = HierarchyUserParameter->GetUserParameter();
	UNiagaraUserParametersHierarchyViewModel* UserParametersHierarchyViewModel = Cast<UNiagaraUserParametersHierarchyViewModel>(GetHierarchyViewModel());
	UserParametersHierarchyViewModel->GetSystemViewModel()->GetSystem().GetExposedParameters().RedirectUserVariable(ContainedVariable);
	TObjectPtr<UNiagaraScriptVariable> ScriptVariable = FNiagaraEditorUtilities::GetScriptVariableForUserParameter(ContainedVariable, UserParametersHierarchyViewModel->GetSystemViewModel());
	return ScriptVariable.Get();
}

bool FNiagaraHierarchyUserParameterViewModel::DoesExternalDataStillExist(const UNiagaraHierarchyDataRefreshContext* Context) const
{
	const UNiagaraHierarchyUserParameterRefreshContext* UserParameterRefreshContext = CastChecked<UNiagaraHierarchyUserParameterRefreshContext>(Context);
	const UNiagaraSystem* System = UserParameterRefreshContext->GetSystem();
	return FNiagaraEditorUtilities::FindScriptVariableForUserParameter(GetData()->GetPersistentIdentity().Guids[0], *System) != nullptr;
}

TSharedRef<FNiagaraSystemViewModel> UNiagaraUserParametersHierarchyViewModel::GetSystemViewModel() const
{
	TSharedPtr<FNiagaraSystemViewModel> SystemViewModelPinned = SystemViewModelWeak.Pin();
	checkf(SystemViewModelPinned.IsValid(), TEXT("System view model destroyed before user parameters hierarchy view model."));
	return SystemViewModelPinned.ToSharedRef();
}

void UNiagaraUserParametersHierarchyViewModel::Initialize(TSharedRef<FNiagaraSystemViewModel> InSystemViewModel)
{
	SystemViewModelWeak = InSystemViewModel;
	UNiagaraHierarchyViewModelBase::Initialize();

	UNiagaraHierarchyUserParameterRefreshContext* UserParameterRefreshContext = NewObject<UNiagaraHierarchyUserParameterRefreshContext>(this);
	UserParameterRefreshContext->SetSystem(&InSystemViewModel->GetSystem());

	SetRefreshContext(UserParameterRefreshContext);
	
	UNiagaraSystemEditorData& SystemEditorData = InSystemViewModel->GetEditorData();
	SystemEditorData.OnUserParameterScriptVariablesSynced().AddUObject(this, &UNiagaraUserParametersHierarchyViewModel::ForceFullRefresh);
}

void UNiagaraUserParametersHierarchyViewModel::FinalizeInternal()
{
	TSharedPtr<FNiagaraSystemViewModel> SystemViewModel = SystemViewModelWeak.Pin();
	if(SystemViewModel.IsValid())
	{
		GetSystemViewModel()->GetSystem().GetExposedParameters().RemoveAllOnChangedHandlers(this);
		UNiagaraSystemEditorData* SystemEditorData = Cast<UNiagaraSystemEditorData>(GetSystemViewModel()->GetSystem().GetEditorData());
		SystemEditorData->OnUserParameterScriptVariablesSynced().RemoveAll(this);
	}
}

TSharedRef<SWidget> FNiagaraUserParameterHierarchyDragDropOp::CreateCustomDecorator() const
{
	return FNiagaraParameterUtilities::GetParameterWidget(GetUserParameter(), true, false);
}

UNiagaraHierarchyRoot* UNiagaraUserParametersHierarchyViewModel::GetHierarchyRoot() const
{
	UNiagaraHierarchyRoot* RootItem = GetSystemViewModel()->GetEditorData().UserParameterHierarchy;

	ensure(RootItem != nullptr);
	return RootItem;
}

TSharedPtr<FNiagaraHierarchyItemViewModelBase> UNiagaraUserParametersHierarchyViewModel::CreateViewModelForData(UNiagaraHierarchyItemBase* ItemBase, TSharedPtr<FNiagaraHierarchyItemViewModelBase> Parent)
{
	if(UNiagaraHierarchyUserParameter* UserParameter = Cast<UNiagaraHierarchyUserParameter>(ItemBase))
	{
		return MakeShared<FNiagaraHierarchyUserParameterViewModel>(UserParameter, Parent, this, Parent.IsValid() ? Parent->IsForHierarchy() : false);
	}
	else if(UNiagaraHierarchyCategory* Category = Cast<UNiagaraHierarchyCategory>(ItemBase))
	{
		return MakeShared<FNiagaraHierarchyCategoryViewModel>(Category, Parent, this, Parent.IsValid() ? Parent->IsForHierarchy() : false);
	}

	check(false);
	return nullptr;
}

void UNiagaraUserParametersHierarchyViewModel::PrepareSourceItems(UNiagaraHierarchyRoot* SourceRoot, TSharedPtr<FNiagaraHierarchyRootViewModel>)
{	
	TArray<FNiagaraVariable> UserParameters;
	GetSystemViewModel()->GetSystem().GetExposedParameters().GetUserParameters(UserParameters);

	TArray<UNiagaraHierarchyItemBase*> OldChildren = SourceRoot->GetChildrenMutable();
	SourceRoot->GetChildrenMutable().Empty();

	// we ensure we have one data child per user parameter, and we get rid of deleted ones
	for(FNiagaraVariable& UserParameter : UserParameters)
	{
		FNiagaraVariable RedirectedUserParameter = UserParameter;
		GetSystemViewModel()->GetSystem().GetExposedParameters().RedirectUserVariable(RedirectedUserParameter);
		TObjectPtr<UNiagaraScriptVariable> ScriptVariable = FNiagaraEditorUtilities::GetScriptVariableForUserParameter(RedirectedUserParameter, GetSystemViewModel());
		
		if(UNiagaraHierarchyItemBase** ExistingChild = OldChildren.FindByPredicate([ScriptVariable](UNiagaraHierarchyItemBase* ItemBase)
		{
			return ItemBase->GetPersistentIdentity().Guids[0] == ScriptVariable->Metadata.GetVariableGuid();
		}))
		{
			SourceRoot->GetChildrenMutable().Add(*ExistingChild);
			continue;
		}
		
		// since the source items are transient we need to create them here and keep them around until the end of the tool's lifetime
		UNiagaraHierarchyUserParameter* UserParameterHierarchyObject = NewObject<UNiagaraHierarchyUserParameter>(SourceRoot);
		UserParameterHierarchyObject->Initialize(*ScriptVariable.Get());
		SourceRoot->GetChildrenMutable().Add(UserParameterHierarchyObject);
	}
}

void UNiagaraUserParametersHierarchyViewModel::SetupCommands()
{
	// no custom commands yet
}

TSharedRef<FNiagaraHierarchyDragDropOp> UNiagaraUserParametersHierarchyViewModel::CreateDragDropOp(TSharedRef<FNiagaraHierarchyItemViewModelBase> Item)
{
	if(const UNiagaraHierarchyUserParameter* UserParameter = Cast<UNiagaraHierarchyUserParameter>(Item->GetData()))
	{
		TSharedRef<FNiagaraUserParameterHierarchyDragDropOp> ParameterDragDropOp = MakeShared<FNiagaraUserParameterHierarchyDragDropOp>(Item);
		ParameterDragDropOp->Construct();
		return ParameterDragDropOp;
	}

	if(UNiagaraHierarchyCategory* HierarchyCategory = Cast<UNiagaraHierarchyCategory>(Item->GetDataMutable()))
	{
		TSharedRef<FNiagaraHierarchyDragDropOp> CategoryDragDropOp = MakeShared<FNiagaraHierarchyDragDropOp>(Item);
		CategoryDragDropOp->SetAdditionalLabel(FText::FromName(HierarchyCategory->GetCategoryName()));
		CategoryDragDropOp->Construct();
		return CategoryDragDropOp;
	}

	return MakeShared<FNiagaraHierarchyDragDropOp>(nullptr);
}

TArray<TTuple<UClass*, FOnGetDetailCustomizationInstance>> UNiagaraUserParametersHierarchyViewModel::GetInstanceCustomizations()
{
	return {{UNiagaraScriptVariable::StaticClass(), FOnGetDetailCustomizationInstance::CreateStatic(&FNiagaraScriptVariableHierarchyDetails::MakeInstance)}};
}

#undef LOCTEXT_NAMESPACE
