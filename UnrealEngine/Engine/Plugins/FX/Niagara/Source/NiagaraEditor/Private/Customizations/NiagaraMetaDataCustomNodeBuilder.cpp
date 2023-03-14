// Copyright Epic Games, Inc. All Rights Reserved.

#include "NiagaraMetaDataCustomNodeBuilder.h"
#include "NiagaraGraph.h"
#include "ViewModels/Stack/NiagaraParameterHandle.h"
#include "IDetailPropertyRow.h"
#include "IDetailChildrenBuilder.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(NiagaraMetaDataCustomNodeBuilder)

FNiagaraMetaDataCustomNodeBuilder::FNiagaraMetaDataCustomNodeBuilder()
	: ScriptGraph(nullptr)
{
}
	
void FNiagaraMetaDataCustomNodeBuilder::Initialize(UNiagaraGraph* InScriptGraph)
{
	ScriptGraph = InScriptGraph;

	// We need to add handlers for both graph changed and graph needs compile since needs compile changes will
	// not broadcast the generic graph changed but they still might modify the metadata collection.
	OnGraphChangedHandle = ScriptGraph->AddOnGraphChangedHandler(FOnGraphChanged::FDelegate::CreateSP(this, &FNiagaraMetaDataCustomNodeBuilder::OnGraphChanged));
	OnGraphNeedsRecompileHandle = ScriptGraph->AddOnGraphNeedsRecompileHandler(FOnGraphChanged::FDelegate::CreateSP(this, &FNiagaraMetaDataCustomNodeBuilder::OnGraphChanged));
}

FNiagaraMetaDataCustomNodeBuilder::~FNiagaraMetaDataCustomNodeBuilder()
{
	if (ScriptGraph.IsValid())
	{
		ScriptGraph->RemoveOnGraphChangedHandler(OnGraphChangedHandle);
		ScriptGraph->RemoveOnGraphNeedsRecompileHandler(OnGraphNeedsRecompileHandle);
	}
}

void FNiagaraMetaDataCustomNodeBuilder::SetOnRebuildChildren(FSimpleDelegate InOnRegenerateChildren)
{
	OnRebuildChildren = InOnRegenerateChildren;
}

FName FNiagaraMetaDataCustomNodeBuilder::GetName() const
{
	static const FName NiagaraMetadataCustomNodeBuilder("NiagaraMetadataCustomNodeBuilder");
	return NiagaraMetadataCustomNodeBuilder;
}

void FNiagaraMetaDataCustomNodeBuilder::GenerateChildContent(IDetailChildrenBuilder& ChildrenBuilder)
{
	// We only store metadata information if it's actually set but we want to display metadata for all module parameters so
	// we iterate over that collection here.
	TArray<TTuple<FNiagaraVariable, FNiagaraParameterHandle, TSharedRef<FStructOnScope>>> MetaDataRowData;
	const TMap<FNiagaraVariable, FNiagaraGraphParameterReferenceCollection>& ParameterReferenceMap = ScriptGraph->GetParameterReferenceMap();
	for (const auto& ParameterToReferences : ParameterReferenceMap)
	{
		const FNiagaraVariable& ParameterVariable = ParameterToReferences.Key;
		FNiagaraParameterHandle ParameterHandle(ParameterVariable.GetName());
		if (ParameterHandle.IsModuleHandle())
		{
			TSharedRef<FStructOnScope> MetaDataContainerStruct = MakeShared<FStructOnScope>(FNiagaraVariableMetaDataContainer::StaticStruct());
			TOptional<FNiagaraVariableMetaData> MetaData = ScriptGraph->GetMetaData(ParameterVariable);
			if (MetaData.IsSet())
			{
				FNiagaraVariableMetaDataContainer* MetaDataContainer = (FNiagaraVariableMetaDataContainer*)MetaDataContainerStruct->GetStructMemory();
				MetaDataContainer->MetaData = MetaData.GetValue();
			}

			MetaDataRowData.Add(TTuple<FNiagaraVariable, FNiagaraParameterHandle, TSharedRef<FStructOnScope>>(ParameterVariable, ParameterHandle, MetaDataContainerStruct));
		}
	}

	// Sort the metadata using the same sorting that the function inputs use in the stack.
	MetaDataRowData.Sort([](const TTuple<FNiagaraVariable, FNiagaraParameterHandle, TSharedRef<FStructOnScope>>& MetaDataRowDataA, const TTuple<FNiagaraVariable, FNiagaraParameterHandle, TSharedRef<FStructOnScope>>& MetaDataRowDataB)
	{
		const FNiagaraVariableMetaData& MetaDataA = ((FNiagaraVariableMetaDataContainer*)MetaDataRowDataA.Get<2>()->GetStructMemory())->MetaData;
		const FNiagaraVariableMetaData& MetaDataB = ((FNiagaraVariableMetaDataContainer*)MetaDataRowDataB.Get<2>()->GetStructMemory())->MetaData;
		if (MetaDataA.CategoryName.IsEmpty() && MetaDataB.CategoryName.IsEmpty() == false)
		{
			return true;
		}
		if (MetaDataA.CategoryName.IsEmpty() == false && MetaDataB.CategoryName.IsEmpty())
		{
			return false;
		}
		if (MetaDataA.EditorSortPriority != MetaDataB.EditorSortPriority)
		{
			return  MetaDataA.EditorSortPriority < MetaDataB.EditorSortPriority;
		}
		else
		{
			const FNiagaraVariable& ParameterVariableA = MetaDataRowDataA.Get<0>();
			const FNiagaraVariable& ParameterVariableB = MetaDataRowDataB.Get<0>();
			return FNameLexicalLess()(ParameterVariableA.GetName(), ParameterVariableB.GetName());
		}
	});

	// Add the rows for each metadata entry now that they have been sorted.
	for (TTuple<FNiagaraVariable, FNiagaraParameterHandle, TSharedRef<FStructOnScope>>& MetaDataRowDataItem : MetaDataRowData)
	{
		const FNiagaraVariable& ParameterVariable = MetaDataRowDataItem.Get<0>();
		const FNiagaraParameterHandle& ParameterHandle = MetaDataRowDataItem.Get<1>();
		TSharedRef<FStructOnScope>& MetaDataContainerStruct = MetaDataRowDataItem.Get<2>();
		
		IDetailPropertyRow* MetaDataRow = ChildrenBuilder.AddExternalStructureProperty(MetaDataContainerStruct, GET_MEMBER_NAME_CHECKED(FNiagaraVariableMetaDataContainer, MetaData), FAddPropertyParams().UniqueId(ParameterVariable.GetName()));
		MetaDataRow->DisplayName(FText::FromName(ParameterHandle.GetName()));
		if (MetaDataRow->GetPropertyHandle().IsValid())
		{
			MetaDataRow->GetPropertyHandle()->SetOnPropertyValueChanged(FSimpleDelegate::CreateSP(this, &FNiagaraMetaDataCustomNodeBuilder::MetaDataPropertyHandleChanged, ParameterVariable, MetaDataContainerStruct));
			MetaDataRow->GetPropertyHandle()->SetOnChildPropertyValueChanged(FSimpleDelegate::CreateSP(this, &FNiagaraMetaDataCustomNodeBuilder::MetaDataPropertyHandleChanged, ParameterVariable, MetaDataContainerStruct));
		}
	}
}

void FNiagaraMetaDataCustomNodeBuilder::Rebuild()
{
	OnRebuildChildren.ExecuteIfBound();
}

void FNiagaraMetaDataCustomNodeBuilder::OnGraphChanged(const FEdGraphEditAction& Action)
{
	OnRebuildChildren.ExecuteIfBound();
}

void FNiagaraMetaDataCustomNodeBuilder::MetaDataPropertyHandleChanged(FNiagaraVariable ParameterVariable, TSharedRef<FStructOnScope> MetaDataContainerStruct)
{
	if (ScriptGraph.IsValid())
	{
		FNiagaraVariableMetaDataContainer* MetaDataContainer = (FNiagaraVariableMetaDataContainer*)MetaDataContainerStruct->GetStructMemory();
		ScriptGraph->SetMetaData(ParameterVariable, MetaDataContainer->MetaData);
	}
}
