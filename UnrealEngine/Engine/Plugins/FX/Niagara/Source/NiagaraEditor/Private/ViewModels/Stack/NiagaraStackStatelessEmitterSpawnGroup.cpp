// Copyright Epic Games, Inc. All Rights Reserved.

#include "ViewModels/Stack/NiagaraStackStatelessEmitterSpawnGroup.h"

#include "IDetailTreeNode.h"
#include "NiagaraEditorStyle.h"
#include "PropertyHandle.h"
#include "ScopedTransaction.h"
#include "Stateless/NiagaraDistributionPropertyCustomization.h"
#include "Stateless/NiagaraDistributionIntPropertyCustomization.h"
#include "Stateless/NiagaraStatelessEmitter.h"
#include "Styling/AppStyle.h"
#include "ViewModels/NiagaraSystemSelectionViewModel.h"
#include "ViewModels/NiagaraSystemViewModel.h"
#include "ViewModels/Stack/NiagaraStackItemPropertyHeaderValueShared.h"
#include "ViewModels/Stack/NiagaraStackObject.h"

#define LOCTEXT_NAMESPACE "NiagaraEmitterStatelessSpawnGroup"

class FNiagaraStackStatelessEmitterSpawnGroupAddUtilities : public TNiagaraStackItemGroupAddUtilities<FGuid>
{
public:
	FNiagaraStackStatelessEmitterSpawnGroupAddUtilities(UNiagaraStatelessEmitter* StatelessEmitter, FOnItemAdded InOnItemAdded)
		: TNiagaraStackItemGroupAddUtilities(LOCTEXT("AddUtilitiesName", "Spawn Data"), EAddMode::AddDirectly, false, false, InOnItemAdded)
	{
		StatelessEmitterWeak = StatelessEmitter;
	}

	virtual void AddItemDirectly() override 
	{ 
		UNiagaraStatelessEmitter* StatelessEmitter = StatelessEmitterWeak.Get();
		if (StatelessEmitter != nullptr)
		{
			FScopedTransaction ScopedTransaction(LOCTEXT("AddNewSpawnInfoTransaction", "Add new spawn data"));
			StatelessEmitter->Modify();
			FNiagaraStatelessSpawnInfo& SpawnInfo = StatelessEmitter->AddSpawnInfo();
			SpawnInfo.SourceId = FGuid::NewGuid();
			OnItemAdded.ExecuteIfBound(SpawnInfo.SourceId);
		}
	}

	virtual void GenerateAddActions(TArray<TSharedRef<INiagaraStackItemGroupAddAction>>& OutAddActions, const FNiagaraStackItemGroupAddOptions& AddProperties) const override
	{
		unimplemented();
	}

	virtual void ExecuteAddAction(TSharedRef<INiagaraStackItemGroupAddAction> AddAction, int32 TargetIndex) override
	{
		unimplemented();
	}

private:
	TWeakObjectPtr<UNiagaraStatelessEmitter> StatelessEmitterWeak;
};

void UNiagaraStackStatelessEmitterSpawnGroup::Initialize(FRequiredEntryData InRequiredEntryData, UNiagaraStatelessEmitter* InStatelessEmitter)
{
	AddUtilities = MakeShared<FNiagaraStackStatelessEmitterSpawnGroupAddUtilities>(InStatelessEmitter,
		FNiagaraStackStatelessEmitterSpawnGroupAddUtilities::FOnItemAdded::CreateUObject(this, &UNiagaraStackStatelessEmitterSpawnGroup::OnSpawnInfoAdded));
	Super::Initialize(
		InRequiredEntryData,
		LOCTEXT("EmitterStatelessSpawningGroupDisplayName", "Spawn"),
		LOCTEXT("EmitterStatelessSpawningGroupToolTip", "Data related to spawning particles"),
		AddUtilities.Get());
	StatelessEmitterWeak = InStatelessEmitter;
}

const FSlateBrush* UNiagaraStackStatelessEmitterSpawnGroup::GetIconBrush() const
{
	return FNiagaraEditorStyle::Get().GetBrush("NiagaraEditor.Stateless.SpawnIcon");
}

void UNiagaraStackStatelessEmitterSpawnGroup::RefreshChildrenInternal(const TArray<UNiagaraStackEntry*>& CurrentChildren, TArray<UNiagaraStackEntry*>& NewChildren, TArray<FStackIssue>& NewIssues)
{
	Super::RefreshChildrenInternal(CurrentChildren, NewChildren, NewIssues);

	UNiagaraStatelessEmitter* StatelessEmitter = StatelessEmitterWeak.Get();
	if (StatelessEmitter != nullptr)
	{
		for (int32 SpawnInfoIndex = 0; SpawnInfoIndex < StatelessEmitter->GetNumSpawnInfos(); SpawnInfoIndex++)
		{
			UNiagaraStackStatelessEmitterSpawnItem* SpawningItem = FindCurrentChildOfTypeByPredicate<UNiagaraStackStatelessEmitterSpawnItem>(CurrentChildren,
				[StatelessEmitter, SpawnInfoIndex](const UNiagaraStackStatelessEmitterSpawnItem* CurrentChild)
			{ 
				return
					CurrentChild->GetStatelessEmitter() == StatelessEmitter &&
					CurrentChild->GetIndex() == SpawnInfoIndex &&
					CurrentChild->GetSourceId() == StatelessEmitter->GetSpawnInfoByIndex(SpawnInfoIndex)->SourceId;
			});
			if (SpawningItem == nullptr)
			{
				SpawningItem = NewObject<UNiagaraStackStatelessEmitterSpawnItem>(this);
				SpawningItem->Initialize(CreateDefaultChildRequiredData(), StatelessEmitter, SpawnInfoIndex);
				SpawningItem->OnRequestDelete().BindUObject(this, &UNiagaraStackStatelessEmitterSpawnGroup::OnChildRequestDelete);
			}
			NewChildren.Add(SpawningItem);
		}
	}
}

void UNiagaraStackStatelessEmitterSpawnGroup::OnSpawnInfoAdded(FGuid AddedItemId)
{
	GetSystemViewModel()->GetSelectionViewModel()->EmptySelection();
	GetSystemViewModel()->GetSelectionViewModel()->AddEntryToSelectionBySelectionIdDeferred(AddedItemId);
	UNiagaraStatelessEmitter* StatelessEmitter = StatelessEmitterWeak.Get();
	if (StatelessEmitter != nullptr)
	{
		OnDataObjectModified().Broadcast({ StatelessEmitter }, ENiagaraDataObjectChange::Changed);
	}
	RefreshChildren();
}

void UNiagaraStackStatelessEmitterSpawnGroup::OnChildRequestDelete(FGuid DeleteItemId)
{
	UNiagaraStatelessEmitter* StatelessEmitter = StatelessEmitterWeak.Get();
	if (StatelessEmitter != nullptr)
	{
		int32 IndexToDelete = StatelessEmitter->IndexOfSpawnInfoBySourceId(DeleteItemId);
		if (IndexToDelete != INDEX_NONE)
		{
			StatelessEmitter->Modify();
			StatelessEmitter->RemoveSpawnInfoBySourceId(DeleteItemId);
			RefreshChildren();
		}
	}
}

void UNiagaraStackStatelessEmitterSpawnItem::Initialize(FRequiredEntryData InRequiredEntryData, UNiagaraStatelessEmitter* InStatelessEmitter, int32 InIndex)
{
	Super::Initialize(InRequiredEntryData, FString::Printf(TEXT("StatelessEmitterSpawnItem-%i"), InIndex));
	StatelessEmitterWeak = InStatelessEmitter;
	Index = InIndex;
	SourceId = InStatelessEmitter->GetSpawnInfoByIndex(InIndex)->SourceId;
	OnDataObjectModified().AddUObject(this, &UNiagaraStackStatelessEmitterSpawnItem::OnSpawnInfoModified);
}

FText UNiagaraStackStatelessEmitterSpawnItem::GetDisplayName() const
{
	const FNiagaraStatelessSpawnInfo* SpawnInfo = GetSpawnInfo();
	const ENiagaraStatelessSpawnInfoType SpawnInfoType = SpawnInfo ? SpawnInfo->Type : ENiagaraStatelessSpawnInfoType::Burst;
	switch (SpawnInfoType)
	{
		case ENiagaraStatelessSpawnInfoType::Burst:
			return LOCTEXT("EmitterSpawnBurstDisplayName", "Spawn Burst Instantaneous");

		case ENiagaraStatelessSpawnInfoType::Rate:
			return LOCTEXT("EmitterSpawnRateDisplayName", "Spawn Rate");

		default:
			checkNoEntry();
			return LOCTEXT("EmitterSpawnUnknownDisplayName", "Unknown");
	}
}

FGuid UNiagaraStackStatelessEmitterSpawnItem::GetSelectionId() const
{
	return SourceId;
}

bool UNiagaraStackStatelessEmitterSpawnItem::TestCanDeleteWithMessage(FText& OutCanDeleteMessage) const
{
	OutCanDeleteMessage = LOCTEXT("DeleteSpawnDataMessage", "Delete this spawn data.");
	return true;
}

FText UNiagaraStackStatelessEmitterSpawnItem::GetDeleteTransactionText() const
{
	return LOCTEXT("DeleteSpawnTransaction", "Delete spawn data");
}

void UNiagaraStackStatelessEmitterSpawnItem::Delete()
{
	OnRequestDeleteDelegate.ExecuteIfBound(SourceId);
}

void UNiagaraStackStatelessEmitterSpawnItem::GetHeaderValueHandlers(TArray<TSharedRef<INiagaraStackItemHeaderValueHandler>>& OutHeaderValueHandlers) const
{
	if (GetSpawnInfo() != nullptr)
	{
		OutHeaderValueHandlers.Append(HeaderValueHandlers);
	}
}

void UNiagaraStackStatelessEmitterSpawnItem::RefreshChildrenInternal(const TArray<UNiagaraStackEntry*>& CurrentChildren, TArray<UNiagaraStackEntry*>& NewChildren, TArray<FStackIssue>& NewIssues)
{
	Super::RefreshChildrenInternal(CurrentChildren, NewChildren, NewIssues);

	FNiagaraStatelessSpawnInfo* SpawnInfo = GetSpawnInfo();
	if (SpawnInfo != nullptr)
	{
		if (SpawnInfoStructOnScope.IsValid() == false || SpawnInfoStructOnScope->GetStructMemory() != (uint8*)SpawnInfo)
		{
			SpawnInfoStructOnScope = MakeShared<FStructOnScope>(FNiagaraStatelessSpawnInfo::StaticStruct(), (uint8*)SpawnInfo);
		}

		UNiagaraStackObject* SpawnInfoObject = SpawnInfoObjectWeak.Get();
		UObject* StatelessEmitterObject = StatelessEmitterWeak.Get();
		if (SpawnInfoObject == nullptr || SpawnInfoObject->GetObject() != StatelessEmitterObject ||
			SpawnInfoObject->GetDisplayedStruct().IsValid() == false || SpawnInfoObject->GetDisplayedStruct()->GetStructMemory() != (uint8*)SpawnInfo)
		{
			bool bIsInTopLevelStruct = true;
			bool bHideTopLevelCategories = true;
			SpawnInfoObject = NewObject<UNiagaraStackObject>(this);
			SpawnInfoObject->Initialize(CreateDefaultChildRequiredData(), StatelessEmitterObject, SpawnInfoStructOnScope.ToSharedRef(), TEXT("SpawnInfo"), bIsInTopLevelStruct, bHideTopLevelCategories, GetStackEditorDataKey());
			SpawnInfoObject->SetOnFilterDetailNodes(
				FNiagaraStackObjectShared::FOnFilterDetailNodes::CreateStatic(&UNiagaraStackStatelessEmitterSpawnItem::FilterDetailNodes),
				UNiagaraStackObject::EDetailNodeFilterMode::FilterAllNodes);
			//SpawnInfoObject->RegisterInstancedCustomPropertyTypeLayout(FNiagaraDistributionFloat::StaticStruct()->GetFName(), FOnGetPropertyTypeCustomizationInstance::CreateStatic(&FNiagaraDistributionPropertyCustomization::MakeFloatInstance, StatelessEmitterObject));
			//SpawnInfoObject->RegisterInstancedCustomPropertyTypeLayout(FNiagaraDistributionVector2::StaticStruct()->GetFName(), FOnGetPropertyTypeCustomizationInstance::CreateStatic(&FNiagaraDistributionPropertyCustomization::MakeVector2Instance));
			//SpawnInfoObject->RegisterInstancedCustomPropertyTypeLayout(FNiagaraDistributionVector3::StaticStruct()->GetFName(), FOnGetPropertyTypeCustomizationInstance::CreateStatic(&FNiagaraDistributionPropertyCustomization::MakeVector3Instance));
			//SpawnInfoObject->RegisterInstancedCustomPropertyTypeLayout(FNiagaraDistributionColor::StaticStruct()->GetFName(), FOnGetPropertyTypeCustomizationInstance::CreateStatic(&FNiagaraDistributionPropertyCustomization::MakeColorInstance));
			SpawnInfoObject->RegisterInstancedCustomPropertyTypeLayout(FNiagaraDistributionRangeFloat::StaticStruct()->GetFName(), FOnGetPropertyTypeCustomizationInstance::CreateStatic(&FNiagaraDistributionPropertyCustomization::MakeFloatInstance, StatelessEmitterObject));
			//SpawnInfoObject->RegisterInstancedCustomPropertyTypeLayout(FNiagaraDistributionRangeVector2::StaticStruct()->GetFName(), FOnGetPropertyTypeCustomizationInstance::CreateStatic(&FNiagaraDistributionPropertyCustomization::MakeVector2Instance));
			//SpawnInfoObject->RegisterInstancedCustomPropertyTypeLayout(FNiagaraDistributionRangeVector3::StaticStruct()->GetFName(), FOnGetPropertyTypeCustomizationInstance::CreateStatic(&FNiagaraDistributionPropertyCustomization::MakeVector3Instance));
			//SpawnInfoObject->RegisterInstancedCustomPropertyTypeLayout(FNiagaraDistributionRangeColor::StaticStruct()->GetFName(), FOnGetPropertyTypeCustomizationInstance::CreateStatic(&FNiagaraDistributionPropertyCustomization::MakeColorInstance));
			SpawnInfoObject->RegisterInstancedCustomPropertyTypeLayout(FNiagaraDistributionRangeInt::StaticStruct()->GetFName(), FOnGetPropertyTypeCustomizationInstance::CreateStatic(&FNiagaraDistributionIntPropertyCustomization::MakeIntInstance, StatelessEmitterObject));

			SpawnInfoObjectWeak = SpawnInfoObject;
		}
		NewChildren.Add(SpawnInfoObject);

		if (bGeneratedHeaderValueHandlers == false)
		{
			bGeneratedHeaderValueHandlers = true;
			FNiagaraStackItemPropertyHeaderValueShared::GenerateHeaderValueHandlers(*StatelessEmitterObject, (uint8*)SpawnInfo, *SpawnInfo->StaticStruct(), FSimpleDelegate::CreateUObject(this, &UNiagaraStackStatelessEmitterSpawnItem::OnHeaderValueChanged), HeaderValueHandlers);
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
		SpawnInfoStructOnScope.Reset();
		SpawnInfoObjectWeak.Reset();
		HeaderValueHandlers.Empty();
	}
}

FNiagaraStatelessSpawnInfo* UNiagaraStackStatelessEmitterSpawnItem::GetSpawnInfo() const
{
	UNiagaraStatelessEmitter* StatelessEmitter = StatelessEmitterWeak.Get();
	if (StatelessEmitter != nullptr && Index >= 0 && Index < StatelessEmitter->GetNumSpawnInfos())
	{
		return StatelessEmitter->GetSpawnInfoByIndex(Index);
	}
	return nullptr;
}

void UNiagaraStackStatelessEmitterSpawnItem::FilterDetailNodes(const TArray<TSharedRef<IDetailTreeNode>>& InSourceNodes, TArray<TSharedRef<IDetailTreeNode>>& OutFilteredNodes)
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

void UNiagaraStackStatelessEmitterSpawnItem::OnHeaderValueChanged()
{
	UNiagaraStatelessEmitter* StatelessEmitter = StatelessEmitterWeak.Get();
	if (StatelessEmitter != nullptr)
	{
		TArray<UObject*> ChangedObjects;
		ChangedObjects.Add(StatelessEmitter);
		OnDataObjectModified().Broadcast(ChangedObjects, ENiagaraDataObjectChange::Changed);
	}
}

//-TODO:Stateless: There should be cleaner way of doing this
void UNiagaraStackStatelessEmitterSpawnItem::OnSpawnInfoModified(TArray<UObject*> Objects, ENiagaraDataObjectChange ChangeType)
{
	UNiagaraStatelessEmitter* StatelessEmitter = StatelessEmitterWeak.Get();
	if (Objects.Num() == 1 && Objects[0] == StatelessEmitter && StatelessEmitter)
	{
		FNiagaraStatelessSpawnInfo* SpawnInfo = GetSpawnInfo();
		SpawnInfo->Rate.UpdateValuesFromDistribution();

		FPropertyChangedEvent EmptyPropertyUpdateStruct(nullptr);
		StatelessEmitter->PostEditChangeProperty(EmptyPropertyUpdateStruct);
	}
}

#undef LOCTEXT_NAMESPACE
