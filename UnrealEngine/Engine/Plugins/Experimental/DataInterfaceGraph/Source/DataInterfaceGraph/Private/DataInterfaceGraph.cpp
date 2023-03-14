// Copyright Epic Games, Inc. All Rights Reserved.

#include "DataInterfaceGraph.h"
#include "RigVMCore/RigVMMemoryStorage.h"
#include "RigUnit_DataInterfaceBeginExecution.h"
#include "DataInterfaceUnitContext.h"

namespace UE::DataInterfaceGraph
{
const FName EntryPointName("GetData");
const FName ResultName("Result");
}

FName UDataInterfaceGraph::GetReturnTypeNameImpl() const
{
	// TODO: this needs to be user-defined & determined at creation time, using float for now
	return TNameOf<float>::GetName();
}

const UScriptStruct* UDataInterfaceGraph::GetReturnTypeStructImpl() const
{
	// TODO: this needs to be user-defined & determined at creation time, using nullptr (for float) for now
	return nullptr;
}

bool UDataInterfaceGraph::GetDataImpl(const UE::DataInterface::FContext& Context) const
{
	bool bResult = true;
	
	if(RigVM)
	{
		FRigUnitContext RigUnitContext;
		RigUnitContext.State = EControlRigState::Update;
		FDataInterfaceUnitContext DataInterfaceUnitContext(this, Context, bResult);
		void* AdditionalArguments[2] = { &RigUnitContext, &DataInterfaceUnitContext };
		bResult &= RigVM->Execute(TArray<URigVMMemoryStorage*>(), AdditionalArguments, FRigUnit_DataInterfaceBeginExecution::EventName);
	}
	
	return bResult;
}

TArray<FRigVMExternalVariable> UDataInterfaceGraph::GetRigVMExternalVariables()
{
	return TArray<FRigVMExternalVariable>(); 
}