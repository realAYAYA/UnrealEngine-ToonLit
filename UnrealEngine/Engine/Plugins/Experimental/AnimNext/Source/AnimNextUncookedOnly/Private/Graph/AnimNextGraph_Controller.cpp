// Copyright Epic Games, Inc. All Rights Reserved.

#include "Graph/AnimNextGraph_Controller.h"
#include "Graph/AnimNextGraph_UnitNode.h"

URigVMUnitNode* UAnimNextGraph_Controller::AddUnitNodeWithPins(UScriptStruct* InScriptStruct, const FRigVMPinInfoArray& PinArray, const FName& InMethodName, const FVector2D& InPosition, const FString& InNodeName, bool bSetupUndoRedo, bool bPrintPythonCommand)
{
	const bool bHasDynamicPins = PinArray.Num() != 0;

	if (bHasDynamicPins)
	{
		OpenUndoBracket(TEXT("Add unit node with pins"));
	}

	URigVMUnitNode* Node = AddUnitNode(InScriptStruct, UAnimNextGraph_UnitNode::StaticClass(), InMethodName, InPosition, InNodeName, bSetupUndoRedo, bPrintPythonCommand);

	if (Node == nullptr)
	{
		if (bHasDynamicPins)
		{
			CancelUndoBracket();
		}

		return nullptr;
	}

	if (bHasDynamicPins)
	{
		const FRigVMRegistry& Registry = FRigVMRegistry::Get();
		const FRigVMPinInfoArray PreviousPins(Node);

		for (int32 PinIndex = 0; PinIndex < PinArray.Num(); ++PinIndex)
		{
			const FString& PinPath = PinArray.GetPinPath(PinIndex);
			FString ParentPinPath, PinName;
			UObject* OuterForPin = Node;
			if (URigVMPin::SplitPinPathAtEnd(PinPath, ParentPinPath, PinName))
			{
				OuterForPin = Node->FindPin(ParentPinPath);
			}

			CreatePinFromPinInfo(Registry, PreviousPins, PinArray[PinIndex], PinPath, OuterForPin);
		}

		CloseUndoBracket();
	}

	return Node;
}
