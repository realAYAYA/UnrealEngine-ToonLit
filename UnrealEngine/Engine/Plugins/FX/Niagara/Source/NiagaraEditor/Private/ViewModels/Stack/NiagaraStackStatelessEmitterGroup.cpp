// Copyright Epic Games, Inc. All Rights Reserved.

#include "ViewModels/Stack/NiagaraStackStatelessEmitterGroup.h"

#include "IDetailTreeNode.h"
#include "PropertyHandle.h"
#include "Stateless/NiagaraDistributionPropertyCustomization.h"
#include "Stateless/NiagaraDistributionIntPropertyCustomization.h"
#include "Stateless/NiagaraStatelessEmitter.h"
#include "Styling/AppStyle.h"
#include "ViewModels/Stack/NiagaraStackObject.h"

#define LOCTEXT_NAMESPACE "NiagaraEmitterStatelessGroup"

void UNiagaraStackStatelessEmitterGroup::Initialize(FRequiredEntryData InRequiredEntryData, UNiagaraStatelessEmitter* InStatelessEmitter)
{
	Super::Initialize(
		InRequiredEntryData,
		LOCTEXT("EmitterStatelessGroupDisplayName", "Properties"), 
		LOCTEXT("EmitterStatelessGroupToolTip", "Properties for this lightweight emitter"),
		nullptr);
	StatelessEmitterWeak = InStatelessEmitter;
}

const FSlateBrush* UNiagaraStackStatelessEmitterGroup::GetIconBrush() const
{
	return FAppStyle::Get().GetBrush("Icons.Details");
}

void UNiagaraStackStatelessEmitterGroup::RefreshChildrenInternal(const TArray<UNiagaraStackEntry*>& CurrentChildren, TArray<UNiagaraStackEntry*>& NewChildren, TArray<FStackIssue>& NewIssues)
{
	Super::RefreshChildrenInternal(CurrentChildren, NewChildren, NewIssues);

	UNiagaraStatelessEmitter* StatelessEmitter = StatelessEmitterWeak.Get();
	if (StatelessEmitter != nullptr)
	{
		UNiagaraStackStatelessEmitterObjectItem* RawObjectItem = RawObjectItemWeak.Get();
		if (RawObjectItem == nullptr || RawObjectItem->GetStatelessEmitter() != StatelessEmitter)
		{
			bool bExpandedByDefault = false;
			RawObjectItem = NewObject<UNiagaraStackStatelessEmitterObjectItem>(this);
			RawObjectItem->Initialize(
				CreateDefaultChildRequiredData(),
				StatelessEmitter,
				LOCTEXT("EmitterObjectDisplayName", "Raw Object"),
				bExpandedByDefault,
				FNiagaraStackObjectShared::FOnFilterDetailNodes());
			RawObjectItemWeak = RawObjectItem;
		}
		NewChildren.Add(RawObjectItem);

		UNiagaraStackStatelessEmitterObjectItem* FilteredObjectItem = FilteredObjectItemWeak.Get();
		if (FilteredObjectItem == nullptr || FilteredObjectItem->GetStatelessEmitter() != StatelessEmitter)
		{
			bool bExpandedByDefault = true;
			FilteredObjectItem = NewObject<UNiagaraStackStatelessEmitterObjectItem>(this);
			FilteredObjectItem->Initialize(
				CreateDefaultChildRequiredData(),
				StatelessEmitter,
				LOCTEXT("EmitterPropertiesDisplayName", "Emitter Properties"),
				bExpandedByDefault,
				FNiagaraStackObjectShared::FOnFilterDetailNodes::CreateStatic(&UNiagaraStackStatelessEmitterGroup::FilterDetailNodes));
			FilteredObjectItemWeak = FilteredObjectItem;
		}
		NewChildren.Add(FilteredObjectItem);
	}
	else
	{
		RawObjectItemWeak.Reset();
		FilteredObjectItemWeak.Reset();
	}
}

void UNiagaraStackStatelessEmitterGroup::FilterDetailNodes(const TArray<TSharedRef<IDetailTreeNode>>& InSourceNodes, TArray<TSharedRef<IDetailTreeNode>>& OutFilteredNodes)
{
	for (const TSharedRef<IDetailTreeNode>& SourceNode : InSourceNodes)
	{
		bool bIncludeNode = true;
		if (SourceNode->GetNodeType() == EDetailNodeType::Item)
		{
			TSharedPtr<IPropertyHandle> NodePropertyHandle = SourceNode->CreatePropertyHandle();
			if (NodePropertyHandle.IsValid() && NodePropertyHandle->HasMetaData("HideInStack"))
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

void UNiagaraStackStatelessEmitterObjectItem::Initialize(FRequiredEntryData InRequiredEntryData, UNiagaraStatelessEmitter* InStatelessEmitter, FText InDisplayName, bool bInIsExpandedByDefault, FNiagaraStackObjectShared::FOnFilterDetailNodes InOnFilterDetailNodes)
{
	Super::Initialize(InRequiredEntryData, InStatelessEmitter->GetPathName() + TEXT("StatelessEmitterStackObjectItem"));
	StatelessEmitterWeak = InStatelessEmitter;
	DisplayName = InDisplayName;
	bIsExpandedByDefault = bInIsExpandedByDefault;
	OnFilterDetailNodes = InOnFilterDetailNodes;
}

void UNiagaraStackStatelessEmitterObjectItem::RefreshChildrenInternal(const TArray<UNiagaraStackEntry*>& CurrentChildren, TArray<UNiagaraStackEntry*>& NewChildren, TArray<FStackIssue>& NewIssues)
{
	Super::RefreshChildrenInternal(CurrentChildren, NewChildren, NewIssues);

	UNiagaraStatelessEmitter* StatelessEmitter = StatelessEmitterWeak.Get();
	if (StatelessEmitter != nullptr)
	{
		UNiagaraStackObject* StatelessEmitterStackObject = StatelessEmitterStackObjectWeak.Get();
		if (StatelessEmitterStackObject == nullptr)
		{
			StatelessEmitterStackObject = NewObject<UNiagaraStackObject>(this);
			bool bIsTopLevelObject = true;
			bool bHideTopLevelCategories = false;
			StatelessEmitterStackObject->Initialize(CreateDefaultChildRequiredData(), StatelessEmitter, bIsTopLevelObject, bHideTopLevelCategories, GetStackEditorDataKey());
			StatelessEmitterStackObject->SetOnFilterDetailNodes(OnFilterDetailNodes, UNiagaraStackObject::EDetailNodeFilterMode::FilterAllNodes);
			StatelessEmitterStackObject->RegisterInstancedCustomPropertyTypeLayout(FNiagaraDistributionFloat::StaticStruct()->GetFName(), FOnGetPropertyTypeCustomizationInstance::CreateStatic(&FNiagaraDistributionPropertyCustomization::MakeFloatInstance));
			StatelessEmitterStackObject->RegisterInstancedCustomPropertyTypeLayout(FNiagaraDistributionVector2::StaticStruct()->GetFName(), FOnGetPropertyTypeCustomizationInstance::CreateStatic(&FNiagaraDistributionPropertyCustomization::MakeVector2Instance));
			StatelessEmitterStackObject->RegisterInstancedCustomPropertyTypeLayout(FNiagaraDistributionVector3::StaticStruct()->GetFName(), FOnGetPropertyTypeCustomizationInstance::CreateStatic(&FNiagaraDistributionPropertyCustomization::MakeVector3Instance));
			StatelessEmitterStackObject->RegisterInstancedCustomPropertyTypeLayout(FNiagaraDistributionColor::StaticStruct()->GetFName(), FOnGetPropertyTypeCustomizationInstance::CreateStatic(&FNiagaraDistributionPropertyCustomization::MakeColorInstance));
			StatelessEmitterStackObject->RegisterInstancedCustomPropertyTypeLayout(FNiagaraDistributionRangeFloat::StaticStruct()->GetFName(), FOnGetPropertyTypeCustomizationInstance::CreateStatic(&FNiagaraDistributionPropertyCustomization::MakeFloatInstance));
			StatelessEmitterStackObject->RegisterInstancedCustomPropertyTypeLayout(FNiagaraDistributionRangeVector2::StaticStruct()->GetFName(), FOnGetPropertyTypeCustomizationInstance::CreateStatic(&FNiagaraDistributionPropertyCustomization::MakeVector2Instance));
			StatelessEmitterStackObject->RegisterInstancedCustomPropertyTypeLayout(FNiagaraDistributionRangeVector3::StaticStruct()->GetFName(), FOnGetPropertyTypeCustomizationInstance::CreateStatic(&FNiagaraDistributionPropertyCustomization::MakeVector3Instance));
			StatelessEmitterStackObject->RegisterInstancedCustomPropertyTypeLayout(FNiagaraDistributionRangeColor::StaticStruct()->GetFName(), FOnGetPropertyTypeCustomizationInstance::CreateStatic(&FNiagaraDistributionPropertyCustomization::MakeColorInstance));
			StatelessEmitterStackObject->RegisterInstancedCustomPropertyTypeLayout(FNiagaraDistributionRangeInt::StaticStruct()->GetFName(), FOnGetPropertyTypeCustomizationInstance::CreateStatic(&FNiagaraDistributionIntPropertyCustomization::MakeIntInstance));
			StatelessEmitterStackObjectWeak = StatelessEmitterStackObject;
		}
		NewChildren.Add(StatelessEmitterStackObject);
	}
	else
	{
		StatelessEmitterStackObjectWeak.Reset();
	}
}

#undef LOCTEXT_NAMESPACE
