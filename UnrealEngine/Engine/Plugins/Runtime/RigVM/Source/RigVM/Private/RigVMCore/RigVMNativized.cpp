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

void URigVMNativized::Reset(bool IsIgnoringArchetypeRef)
{
	// don't call super on purpose
	LazyMemoryHandles.Reset();
	LazyMemoryBranches.Reset();
	ByteCodeStorage.Reset();
}

bool URigVMNativized::Initialize(FRigVMExtendedExecuteContext& Context)
{
	// nothing to do here 
	return true;
}

ERigVMExecuteResult URigVMNativized::ExecuteVM(FRigVMExtendedExecuteContext& Context, const FName& InEntryName)
{
	// to be implemented by the generated code
	return ERigVMExecuteResult::Failed;
}

const FRigVMInstructionArray& URigVMNativized::GetInstructions()
{
	if(GetByteCode().GetNumInstructions() > 0)
	{
		return Super::GetInstructions();
	}
	static const FRigVMInstructionArray EmptyInstructions;
	return EmptyInstructions;
}

#if WITH_EDITOR

void URigVMNativized::SetByteCode(const FRigVMByteCode& InByteCode)
{
	ByteCodeStorage = InByteCode;
	ByteCodePtr = &ByteCodeStorage;
}

#endif

const FRigVMMemoryHandle& URigVMNativized::GetLazyMemoryHandle(int32 InIndex, uint8* InMemory, const FProperty* InProperty, TFunction<ERigVMExecuteResult()> InLambda)
{
	check(InIndex != INDEX_NONE);
	check(LazyMemoryHandles.IsValidIndex(InIndex));

	FRigVMMemoryHandle& Handle = LazyMemoryHandles[InIndex];
	if(!Handle.IsLazy())
	{
		check(LazyMemoryBranches.IsValidIndex(InIndex));
		FRigVMLazyBranch& LazyBranch = LazyMemoryBranches[InIndex];
		LazyBranch.VM = this;
		LazyBranch.FunctionPtr = InLambda;

		Handle = FRigVMMemoryHandle(InMemory, InProperty, nullptr, &LazyBranch);
	}

	return Handle;
}

void URigVMNativized::AllocateLazyMemoryHandles(int32 InCount)
{
	if(LazyMemoryHandles.Num() != InCount)
	{
		LazyMemoryHandles.SetNum(InCount);
		LazyMemoryBranches.SetNum(InCount);
	}
}
