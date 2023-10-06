// Copyright Epic Games, Inc. All Rights Reserved.

#include "StateTreeEditorNodeUtils.h"

#include "Layout/Visibility.h"
#include "PropertyHandle.h"
#include "ScopedTransaction.h"
#include "StateTreeConditionBase.h"
#include "StateTreeEditorNode.h"
#include "StateTreeTaskBase.h"

namespace UE::StateTreeEditor::EditorNodeUtils
{

EStateTreeConditionEvaluationMode GetConditionEvaluationMode(TSharedPtr<IPropertyHandle> StructProperty)
{
	if (const FStateTreeEditorNode* Node = GetCommonNode(StructProperty))
	{
		if (const FStateTreeConditionBase* ConditionBase = Node->Node.GetPtr<FStateTreeConditionBase>())
		{
			return ConditionBase->EvaluationMode;
		}
	}
	// Evaluate as default value
	return EStateTreeConditionEvaluationMode::Evaluated;
}

EVisibility IsConditionVisible(TSharedPtr<IPropertyHandle> StructProperty)
{
	const UScriptStruct* ScriptStruct = nullptr;
	if (const FStateTreeEditorNode* Node = GetCommonNode(StructProperty))
	{
		ScriptStruct = Node->Node.GetScriptStruct();
	}

	return ScriptStruct != nullptr && ScriptStruct->IsChildOf(FStateTreeConditionBase::StaticStruct()) ? EVisibility::Visible : EVisibility::Collapsed;
}

EVisibility IsTaskVisible(TSharedPtr<IPropertyHandle> StructProperty)
{
	const UScriptStruct* ScriptStruct = nullptr;
	if (const FStateTreeEditorNode* Node = GetCommonNode(StructProperty))
	{
		ScriptStruct = Node->Node.GetScriptStruct();
	}

	return ScriptStruct != nullptr && ScriptStruct->IsChildOf(FStateTreeTaskBase::StaticStruct()) ? EVisibility::Visible : EVisibility::Collapsed;
}

bool IsTaskDisabled(TSharedPtr<IPropertyHandle> StructProperty)
{
	if (const FStateTreeEditorNode* Node = GetCommonNode(StructProperty))
	{
		if (const FStateTreeTaskBase* TaskBase = Node->Node.GetPtr<FStateTreeTaskBase>())
		{
			return !TaskBase->bTaskEnabled;
		}
	}

	return false;
}

void ModifyNodeInTransaction(FText Description, TSharedPtr<IPropertyHandle> StructProperty, TFunctionRef<void(IPropertyHandle&)> Func)
{
	check(StructProperty);

	FScopedTransaction ScopedTransaction(Description);

	StructProperty->NotifyPreChange();

	Func(*StructProperty);

	StructProperty->NotifyPostChange(EPropertyChangeType::ValueSet);
	StructProperty->NotifyFinishedChangingProperties();
}

const FStateTreeEditorNode* GetCommonNode(const TSharedPtr<IPropertyHandle> StructProperty)
{
	if (const IPropertyHandle* PropertyHandle = StructProperty.Get())
	{
		return GetCommonNode(*PropertyHandle);
	}

	return nullptr;
}

FStateTreeEditorNode* GetMutableCommonNode(const TSharedPtr<IPropertyHandle> StructProperty)
{
	if (IPropertyHandle* PropertyHandle = StructProperty.Get())
	{
		return GetMutableCommonNode(*PropertyHandle);
	}

	return nullptr;
}

const FStateTreeEditorNode* GetCommonNode(const IPropertyHandle& InStructProperty)
{
	TArray<const void*> RawNodeData;
	InStructProperty.AccessRawData(RawNodeData);

	const FStateTreeEditorNode* CommonNode = nullptr;

	for (const void* Data : RawNodeData)
	{
		if (const FStateTreeEditorNode* Node = static_cast<const FStateTreeEditorNode*>(Data))
		{
			if (CommonNode == nullptr)
			{
				CommonNode = Node;
			}
			else if (CommonNode != Node)
			{
				CommonNode = nullptr;
				break;
			}
		}
	}

	return CommonNode;
}

FStateTreeEditorNode* GetMutableCommonNode(IPropertyHandle& InStructProperty)
{
	TArray<void*> RawNodeData;
	InStructProperty.AccessRawData(RawNodeData);

	FStateTreeEditorNode* CommonNode = nullptr;

	for (void* Data : RawNodeData)
	{
		if (FStateTreeEditorNode* Node = static_cast<FStateTreeEditorNode*>(Data))
		{
			if (CommonNode == nullptr)
			{
				CommonNode = Node;
			}
			else if (CommonNode != Node)
			{
				CommonNode = nullptr;
				break;
			}
		}
	}

	return CommonNode;
}

} // namespace UE::StateTreeEditor::EditorNodeUtils
