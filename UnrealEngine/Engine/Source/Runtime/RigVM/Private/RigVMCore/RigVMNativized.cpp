// Copyright Epic Games, Inc. All Rights Reserved.

#include "RigVMCore/RigVMNativized.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(RigVMNativized)

URigVMNativized::URigVMNativized()
	: URigVM()
	, TemporaryArrayIndex(INDEX_NONE)
{
}

URigVMNativized::~URigVMNativized()
{
}

void URigVMNativized::Serialize(FArchive& Ar)
{
	UObject::Serialize(Ar);
}

bool URigVMNativized::Initialize(TArrayView<URigVMMemoryStorage*> Memory, TArrayView<void*> AdditionalArguments,
	bool bInitializeMemory)
{
	// nothing to do here 
	return true;
}

bool URigVMNativized::Execute(TArrayView<URigVMMemoryStorage*> Memory, TArrayView<void*> AdditionalArguments,
	const FName& InEntryName)
{
	// to be implemented by the generated code
	return false;
}

const FRigVMInstructionArray& URigVMNativized::GetInstructions()
{
	static const FRigVMInstructionArray EmptyInstructions;
	return EmptyInstructions;
}

const FRigVMExecuteContext& URigVMNativized::UpdateContext(TArrayView<void*> AdditionalArguments, int32 InNumberInstructions, const FName& InEntryName)
{
	UpdateExternalVariables();
	
	Context.Reset();
	Context.OpaqueArguments = AdditionalArguments;
	Context.SliceOffsets.AddZeroed(InNumberInstructions);
	Context.PublicData.EventName = InEntryName;
	return Context.PublicData;
}

