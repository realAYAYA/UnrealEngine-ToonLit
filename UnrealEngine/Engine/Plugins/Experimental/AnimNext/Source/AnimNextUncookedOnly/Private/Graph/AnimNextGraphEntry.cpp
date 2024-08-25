// Copyright Epic Games, Inc. All Rights Reserved.

#include "Graph/AnimNextGraphEntry.h"
#include "Graph/AnimNextGraph_EdGraph.h"
#include "Graph/RigUnit_AnimNextGraphRoot.h"

FName UAnimNextGraphEntry::GetEntryName() const
{
	return GraphName;
}

void UAnimNextGraphEntry::SetEntryName(FName InName, bool bSetupUndoRedo)
{
	if(bSetupUndoRedo)
	{
		Modify();
	};

	GraphName = InName;

	// Forward to entry point node
	URigVMController* Controller = GetImplementingOuter<IRigVMClientHost>()->GetController(Graph);
	for(URigVMNode* Node : Graph->GetNodes())
	{
		if(URigVMUnitNode* UnitNode = Cast<URigVMUnitNode>(Node))
		{
			if(UnitNode->GetScriptStruct() == FRigUnit_AnimNextGraphRoot::StaticStruct())
			{
				URigVMPin* EntryPointPin = UnitNode->FindPin(GET_MEMBER_NAME_STRING_CHECKED(FRigUnit_AnimNextGraphRoot, EntryPoint));
				check(EntryPointPin);
				check(EntryPointPin->GetDirection() == ERigVMPinDirection::Hidden);

				Controller->SetPinDefaultValue(EntryPointPin->GetPinPath(), InName.ToString());
			}
		}
	}
	
	BroadcastModified();
}

URigVMGraph* UAnimNextGraphEntry::GetRigVMGraph() const
{
	return Graph;
}

URigVMEdGraph* UAnimNextGraphEntry::GetEdGraph() const
{
	return EdGraph;
}
