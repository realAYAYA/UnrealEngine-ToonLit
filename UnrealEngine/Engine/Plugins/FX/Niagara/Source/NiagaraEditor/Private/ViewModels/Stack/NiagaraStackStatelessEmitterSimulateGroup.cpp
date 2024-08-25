// Copyright Epic Games, Inc. All Rights Reserved.

#include "ViewModels/Stack/NiagaraStackStatelessEmitterSimulateGroup.h"

#include "IDetailTreeNode.h"
#include "NiagaraEditorStyle.h"
#include "NiagaraStackEditorData.h"
#include "PropertyHandle.h"
#include "ScopedTransaction.h"
#include "Stateless/NiagaraDistributionPropertyCustomization.h"
#include "Stateless/NiagaraDistributionIntPropertyCustomization.h"
#include "Stateless/NiagaraStatelessCommon.h"
#include "Stateless/NiagaraStatelessEmitter.h"
#include "Stateless/NiagaraStatelessModule.h"
#include "ViewModels/NiagaraSystemSelectionViewModel.h"
#include "ViewModels/NiagaraSystemViewModel.h"
#include "ViewModels/Stack/NiagaraStackItemPropertyHeaderValueShared.h"
#include "ViewModels/Stack/NiagaraStackObject.h"

#define LOCTEXT_NAMESPACE "NiagaraEmitterStatelessSimulateGroup"

class FNiagaraStatelessEmitterAddModuleAction : public INiagaraStackItemGroupAddAction
{

public:
	FNiagaraStatelessEmitterAddModuleAction(UNiagaraStatelessModule* StatelessModule)
	{
		StatelessModuleWeak = StatelessModule;
		DisplayName = StatelessModule->GetClass()->GetDisplayNameText();
	}

	UNiagaraStatelessModule* GetModule() const { return StatelessModuleWeak.Get(); }

	virtual TArray<FString> GetCategories() const override { return Categories; }
	virtual FText GetDisplayName() const override { return DisplayName; }
	virtual FText GetDescription() const override { return FText(); }
	virtual FText GetKeywords() const override { return FText(); }

private:
	TWeakObjectPtr<UNiagaraStatelessModule> StatelessModuleWeak;
	TArray<FString> Categories;
	FText DisplayName;
};

class FNiagaraStatelessEmitterSimulateGroupAddUtilities : public TNiagaraStackItemGroupAddUtilities<UNiagaraStatelessModule*>
{
public:
	FNiagaraStatelessEmitterSimulateGroupAddUtilities(UNiagaraStatelessEmitter& StatelessEmitter, UNiagaraStackEditorData& StackEditorData, FOnItemAdded InOnItemAdded)
		: TNiagaraStackItemGroupAddUtilities<UNiagaraStatelessModule*>(LOCTEXT("ModuleName", "Module"), EAddMode::AddFromAction, true, false, InOnItemAdded)
	{
		StatelessEmitterWeak = &StatelessEmitter;
		StackEditorDataWeak = &StackEditorData;
	}

	virtual void AddItemDirectly() { unimplemented(); };

	virtual void GenerateAddActions(TArray<TSharedRef<INiagaraStackItemGroupAddAction>>& OutAddActions, const FNiagaraStackItemGroupAddOptions& AddProperties = FNiagaraStackItemGroupAddOptions()) const override
	{
		UNiagaraStatelessEmitter* StatelessEmitter = StatelessEmitterWeak.Get();
		UNiagaraStackEditorData* StackEditorData = StackEditorDataWeak.Get();
		if (StatelessEmitter != nullptr && StackEditorData != nullptr)
		{
			for (UNiagaraStatelessModule* StatelessModule : StatelessEmitter->GetModules())
			{
				if (StatelessModule->IsModuleEnabled() == false && 
					StackEditorData->GetStatelessModuleShowWhenDisabled(UNiagaraStackStatelessModuleItem::GenerateStackEditorDataKey(StatelessModule)) == false)
				{
					OutAddActions.Add(MakeShared<FNiagaraStatelessEmitterAddModuleAction>(StatelessModule));
				}
			}
		}
	}

	virtual void ExecuteAddAction(TSharedRef<INiagaraStackItemGroupAddAction> AddAction, int32 TargetIndex) override
	{
		TSharedRef<FNiagaraStatelessEmitterAddModuleAction> AddModuleAction = StaticCastSharedRef<FNiagaraStatelessEmitterAddModuleAction>(AddAction);
		UNiagaraStatelessModule* StatelessModule = AddModuleAction->GetModule();
		UNiagaraStackEditorData* StackEditorData = StackEditorDataWeak.Get();
		if (StatelessModule != nullptr && StackEditorData != nullptr)
		{
			FScopedTransaction ScopedTransaction(LOCTEXT("AddStatelessModuleTransaction", "Add module."));
			StatelessModule->Modify();
			StatelessModule->SetIsModuleEnabled(true);
			StatelessModule->PostEditChange();
			OnItemAdded.ExecuteIfBound(StatelessModule);
		}
	}

protected:
	TWeakObjectPtr<UNiagaraStatelessEmitter> StatelessEmitterWeak;
	TWeakObjectPtr<UNiagaraStackEditorData> StackEditorDataWeak;
};

void UNiagaraStackStatelessEmitterSimulateGroup::Initialize(FRequiredEntryData InRequiredEntryData, UNiagaraStatelessEmitter* InStatelessEmitter)
{
	AddUtilities = MakeShared<FNiagaraStatelessEmitterSimulateGroupAddUtilities>(*InStatelessEmitter, *InRequiredEntryData.StackEditorData,
		FNiagaraStatelessEmitterSimulateGroupAddUtilities::FOnItemAdded::CreateUObject(this, &UNiagaraStackStatelessEmitterSimulateGroup::ModuleAdded));
	Super::Initialize(
		InRequiredEntryData, 
		LOCTEXT("EmitterStatelessSimulateGroupDisplayName", "Simulate"),
		LOCTEXT("EmitterStatelessSimulateGroupToolTip", "Data related to the simulation of the particles"),
		AddUtilities.Get());
	StatelessEmitterWeak = InStatelessEmitter;
}

const FSlateBrush* UNiagaraStackStatelessEmitterSimulateGroup::GetIconBrush() const
{
	return FNiagaraEditorStyle::Get().GetBrush("NiagaraEditor.Stateless.UpdateIcon");
}

void UNiagaraStackStatelessEmitterSimulateGroup::RefreshChildrenInternal(const TArray<UNiagaraStackEntry*>& CurrentChildren, TArray<UNiagaraStackEntry*>& NewChildren, TArray<FStackIssue>& NewIssues)
{
	Super::RefreshChildrenInternal(CurrentChildren, NewChildren, NewIssues);

	UNiagaraStatelessEmitter* StatelessEmitter = StatelessEmitterWeak.Get();
	if (StatelessEmitter != nullptr)
	{
		for (UNiagaraStatelessModule* StatelessModule : StatelessEmitter->GetModules())
		{
			FString ModuleStackEditorDataKey = UNiagaraStackStatelessModuleItem::GenerateStackEditorDataKey(StatelessModule);
			if (StatelessModule->CanDisableModule() && 
				StatelessModule->IsModuleEnabled() == false &&
				GetStackEditorData().GetStatelessModuleShowWhenDisabled(ModuleStackEditorDataKey) == false)
			{
				// If a module is disabled and doesn't have the show when disabled flag set, filter it from the UI.
				continue;
			}

			UNiagaraStackStatelessModuleItem* ModuleItem = FindCurrentChildOfTypeByPredicate<UNiagaraStackStatelessModuleItem>(CurrentChildren,
				[StatelessModule](const UNiagaraStackStatelessModuleItem* CurrentChild) { return CurrentChild->GetStatelessModule() == StatelessModule; });
			if (ModuleItem == nullptr)
			{
				ModuleItem = NewObject<UNiagaraStackStatelessModuleItem>(this);
				ModuleItem->Initialize(CreateDefaultChildRequiredData(), StatelessModule);
				ModuleItem->OnModifiedGroupItems().AddUObject(this, &UNiagaraStackStatelessEmitterSimulateGroup::ModuleModifiedGroupItems);
			}
			NewChildren.Add(ModuleItem);
		}
	}
}

void UNiagaraStackStatelessEmitterSimulateGroup::ModuleAdded(UNiagaraStatelessModule* StatelessModule)
{
	GetSystemViewModel()->GetSelectionViewModel()->EmptySelection();
	GetSystemViewModel()->GetSelectionViewModel()->AddEntryToSelectionByDisplayedObjectDeferred(StatelessModule);
	OnDataObjectModified().Broadcast({ StatelessModule }, ENiagaraDataObjectChange::Changed);
	RefreshChildren();
}

void UNiagaraStackStatelessEmitterSimulateGroup::ModuleModifiedGroupItems()
{
	RefreshChildren();
}

FString UNiagaraStackStatelessModuleItem::GenerateStackEditorDataKey(const UNiagaraStatelessModule* InStatelessModule)
{
	return FString::Printf(TEXT("StatelessModuleItem-%s"), *InStatelessModule->GetName());
}

void UNiagaraStackStatelessModuleItem::Initialize(FRequiredEntryData InRequiredEntryData, UNiagaraStatelessModule* InStatelessModule)
{
	Super::Initialize(InRequiredEntryData, GenerateStackEditorDataKey(InStatelessModule));
	StatelessModuleWeak = InStatelessModule;
	DisplayName = InStatelessModule->GetClass()->GetDisplayNameText();
}

bool UNiagaraStackStatelessModuleItem::TestCanDeleteWithMessage(FText& OutCanDeleteMessage) const
{
	UNiagaraStatelessModule* StatelessModule = StatelessModuleWeak.Get();
	if (StatelessModule != nullptr)
	{
		if (StatelessModule->CanDisableModule())
		{
			OutCanDeleteMessage = LOCTEXT("DeleteStatelessModule", "Delete this module.");
			return true;
		}
	}
	OutCanDeleteMessage = LOCTEXT("DeleteStatelessModuleUnsupported", "This module does not support being deleted.");
	return false;
}

FText UNiagaraStackStatelessModuleItem::GetDeleteTransactionText() const
{
	return LOCTEXT("DeleteStatelessModuleTransaction", "Delete module from lightweight emitter.");
}

void UNiagaraStackStatelessModuleItem::Delete()
{
	UNiagaraStatelessModule* StatelessModule = StatelessModuleWeak.Get();
	if (StatelessModule != nullptr && StatelessModule->CanDisableModule())
	{
		StatelessModule->Modify();
		StatelessModule->SetIsModuleEnabled(false);
		StatelessModule->PostEditChange();
		OnDataObjectModified().Broadcast({ StatelessModule }, ENiagaraDataObjectChange::Changed);
		GetStackEditorData().Modify();
		GetStackEditorData().SetStatelessModuleShowWhenDisabled(GetStackEditorDataKey(), false);
		OnModifiedGroupItems().Broadcast();
	}
}

bool UNiagaraStackStatelessModuleItem::SupportsChangeEnabled() const
{
	UNiagaraStatelessModule* StatelessModule = StatelessModuleWeak.Get();
	return StatelessModule != nullptr && StatelessModule->CanDisableModule();
}

bool UNiagaraStackStatelessModuleItem::GetIsEnabled() const
{
	UNiagaraStatelessModule* StatelessModule = StatelessModuleWeak.Get();
	return StatelessModule != nullptr && StatelessModule->IsModuleEnabled();
}

void UNiagaraStackStatelessModuleItem::RefreshChildrenInternal(const TArray<UNiagaraStackEntry*>& CurrentChildren, TArray<UNiagaraStackEntry*>& NewChildren, TArray<FStackIssue>& NewIssues)
{
	Super::RefreshChildrenInternal(CurrentChildren, NewChildren, NewIssues);

	UNiagaraStatelessModule* StatelessModule = StatelessModuleWeak.Get();
	if (StatelessModule != nullptr)
	{
		UNiagaraStackObject* ModuleObject = ModuleObjectWeak.Get();
		if (ModuleObject == nullptr || ModuleObject->GetObject() != StatelessModule)
		{
			bool bIsInTopLevelObject = true;
			bool bHideTopLevelCategories = true;
			ModuleObject = NewObject<UNiagaraStackObject>(this);
			ModuleObject->Initialize(CreateDefaultChildRequiredData(), StatelessModule, bIsInTopLevelObject, bHideTopLevelCategories, GetStackEditorDataKey());
			ModuleObject->SetOnFilterDetailNodes(FNiagaraStackObjectShared::FOnFilterDetailNodes::CreateStatic(&UNiagaraStackStatelessModuleItem::FilterDetailNodes), UNiagaraStackObject::EDetailNodeFilterMode::FilterAllNodes);
			ModuleObject->RegisterInstancedCustomPropertyTypeLayout(FNiagaraDistributionFloat::StaticStruct()->GetFName(), FOnGetPropertyTypeCustomizationInstance::CreateStatic(&FNiagaraDistributionPropertyCustomization::MakeFloatInstance));
			ModuleObject->RegisterInstancedCustomPropertyTypeLayout(FNiagaraDistributionVector2::StaticStruct()->GetFName(), FOnGetPropertyTypeCustomizationInstance::CreateStatic(&FNiagaraDistributionPropertyCustomization::MakeVector2Instance));
			ModuleObject->RegisterInstancedCustomPropertyTypeLayout(FNiagaraDistributionVector3::StaticStruct()->GetFName(), FOnGetPropertyTypeCustomizationInstance::CreateStatic(&FNiagaraDistributionPropertyCustomization::MakeVector3Instance));
			ModuleObject->RegisterInstancedCustomPropertyTypeLayout(FNiagaraDistributionColor::StaticStruct()->GetFName(), FOnGetPropertyTypeCustomizationInstance::CreateStatic(&FNiagaraDistributionPropertyCustomization::MakeColorInstance));
			ModuleObject->RegisterInstancedCustomPropertyTypeLayout(FNiagaraDistributionRangeFloat::StaticStruct()->GetFName(), FOnGetPropertyTypeCustomizationInstance::CreateStatic(&FNiagaraDistributionPropertyCustomization::MakeFloatInstance));
			ModuleObject->RegisterInstancedCustomPropertyTypeLayout(FNiagaraDistributionRangeVector2::StaticStruct()->GetFName(), FOnGetPropertyTypeCustomizationInstance::CreateStatic(&FNiagaraDistributionPropertyCustomization::MakeVector2Instance));
			ModuleObject->RegisterInstancedCustomPropertyTypeLayout(FNiagaraDistributionRangeVector3::StaticStruct()->GetFName(), FOnGetPropertyTypeCustomizationInstance::CreateStatic(&FNiagaraDistributionPropertyCustomization::MakeVector3Instance));
			ModuleObject->RegisterInstancedCustomPropertyTypeLayout(FNiagaraDistributionRangeColor::StaticStruct()->GetFName(), FOnGetPropertyTypeCustomizationInstance::CreateStatic(&FNiagaraDistributionPropertyCustomization::MakeColorInstance));
			ModuleObject->RegisterInstancedCustomPropertyTypeLayout(FNiagaraDistributionRangeInt::StaticStruct()->GetFName(), FOnGetPropertyTypeCustomizationInstance::CreateStatic(&FNiagaraDistributionIntPropertyCustomization::MakeIntInstance));
			ModuleObjectWeak = ModuleObject;
		}
		NewChildren.Add(ModuleObject);

		if (bGeneratedHeaderValueHandlers == false)
		{
			bGeneratedHeaderValueHandlers = true;
			FNiagaraStackItemPropertyHeaderValueShared::GenerateHeaderValueHandlers(*StatelessModule, nullptr, *StatelessModule->GetClass(), FSimpleDelegate::CreateUObject(this, &UNiagaraStackStatelessModuleItem::OnHeaderValueChanged), HeaderValueHandlers);
			if (StatelessModule->CanDebugDraw())
			{
				// Debug draw is handled separately here since it's visibility needs to be determined by the results of the CanDebugDraw function, rather than by property metadata.
				FBoolProperty* DebugDrawProperty = nullptr;
				for (TFieldIterator<FProperty> PropertyIt(StatelessModule->GetClass(), EFieldIteratorFlags::SuperClassFlags::IncludeSuper, EFieldIteratorFlags::DeprecatedPropertyFlags::ExcludeDeprecated); PropertyIt; ++PropertyIt)
				{
					if (PropertyIt->GetFName() == UNiagaraStatelessModule::PrivateMemberNames::bDebugDrawEnabled)
					{
						DebugDrawProperty = CastField<FBoolProperty>(*PropertyIt);
						break;
					}
				}
				if (DebugDrawProperty != nullptr)
				{
					HeaderValueHandlers.Add(MakeShared<FNiagaraStackItemPropertyHeaderValue>(
						*StatelessModule, nullptr, *DebugDrawProperty, 
						FSimpleDelegate::CreateUObject(this, &UNiagaraStackStatelessModuleItem::OnHeaderValueChanged)));
				}
			}
		}
		else
		{
			for (TSharedRef<FNiagaraStackItemPropertyHeaderValue> HeaderValueHandler : HeaderValueHandlers)
			{
				HeaderValueHandler->Refresh();
			}
		}
	}
	else
	{
		ModuleObjectWeak.Reset();
		HeaderValueHandlers.Empty();
	}
}

void UNiagaraStackStatelessModuleItem::SetIsEnabledInternal(bool bInIsEnabled)
{
	UNiagaraStatelessModule* StatelessModule = StatelessModuleWeak.Get();
	if (StatelessModule != nullptr && StatelessModule->CanDisableModule() && StatelessModule->IsModuleEnabled() != bInIsEnabled)
	{
		FScopedTransaction ScopedTransaction(LOCTEXT("ChangeStatelessModuleEnabledTransaction", "Change module enabled"));
		StatelessModule->Modify();
		StatelessModule->SetIsModuleEnabled(bInIsEnabled);
		StatelessModule->PostEditChange();
		GetStackEditorData().SetStatelessModuleShowWhenDisabled(GetStackEditorDataKey(), true);
		OnDataObjectModified().Broadcast({ StatelessModule }, ENiagaraDataObjectChange::Changed);
		RefreshChildren();
	}
}

void UNiagaraStackStatelessModuleItem::GetHeaderValueHandlers(TArray<TSharedRef<INiagaraStackItemHeaderValueHandler>>& OutHeaderValueHandlers) const
{
	UNiagaraStatelessModule* StatelessModule = StatelessModuleWeak.Get();
	if (StatelessModule != nullptr)
	{
		OutHeaderValueHandlers.Append(HeaderValueHandlers);
	}
}

void UNiagaraStackStatelessModuleItem::FilterDetailNodes(const TArray<TSharedRef<IDetailTreeNode>>& InSourceNodes, TArray<TSharedRef<IDetailTreeNode>>& OutFilteredNodes)
{
	for (const TSharedRef<IDetailTreeNode>& SourceNode : InSourceNodes)
	{
		bool bIncludeNode = true;
		if (SourceNode->GetNodeType() == EDetailNodeType::Item)
		{
			TSharedPtr<IPropertyHandle> NodePropertyHandle = SourceNode->CreatePropertyHandle();
			if (NodePropertyHandle.IsValid() && (NodePropertyHandle->HasMetaData("HideInStack") || NodePropertyHandle->HasMetaData("ShowInStackItemHeader")))
			{
				bIncludeNode = false;
			}
		}
		if (bIncludeNode)
		{
			OutFilteredNodes.Add(SourceNode);
		}
	}
}

void UNiagaraStackStatelessModuleItem::OnHeaderValueChanged()
{
	UNiagaraStatelessModule* StatelessModule = StatelessModuleWeak.Get();
	if (StatelessModule != nullptr)
	{
		TArray<UObject*> ChangedObjects;
		ChangedObjects.Add(StatelessModule);
		OnDataObjectModified().Broadcast(ChangedObjects, ENiagaraDataObjectChange::Changed);
	}
}

#undef LOCTEXT_NAMESPACE
