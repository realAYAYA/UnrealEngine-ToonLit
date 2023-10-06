// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Templates/SharedPointerFwd.h"

enum class EStateTreeConditionEvaluationMode : uint8;
struct FStateTreeEditorNode;
struct EVisibility;
class FText;
class IPropertyHandle;
template <typename FuncType> class TFunctionRef;

namespace UE::StateTreeEditor::EditorNodeUtils
{

const FStateTreeEditorNode* GetCommonNode(const TSharedPtr<IPropertyHandle> StructProperty);
FStateTreeEditorNode* GetMutableCommonNode(const TSharedPtr<IPropertyHandle> StructProperty);

const FStateTreeEditorNode* GetCommonNode(const IPropertyHandle& StructProperty);
FStateTreeEditorNode* GetMutableCommonNode(IPropertyHandle& StructProperty);

EVisibility IsConditionVisible(TSharedPtr<IPropertyHandle> StructProperty);
EStateTreeConditionEvaluationMode GetConditionEvaluationMode(TSharedPtr<IPropertyHandle> StructProperty);
	
EVisibility IsTaskVisible(TSharedPtr<IPropertyHandle> StructProperty);
bool IsTaskDisabled(TSharedPtr<IPropertyHandle> StructProperty);

/**
 * Execute the provided function within a Transaction 
 * @param Description Description to associate to the transaction
 * @param StructProperty Property handle of the StateTreeEditorNode(s) 
 * @param Func Function that will be execute in the transaction on a valid Editor node property
 */
void ModifyNodeInTransaction(FText Description, TSharedPtr<IPropertyHandle> StructProperty, TFunctionRef<void(IPropertyHandle&)> Func);

}; // UE::StateTreeEditor::EditorNodeUtils
