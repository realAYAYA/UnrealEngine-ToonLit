// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#if WITH_VERSE_VM || defined(__INTELLISENSE__)

#include "Templates/TypeCompatibleBytes.h"
#include "VVMBytecode.h"
#include "VVMGlobalTrivialEmergentTypePtr.h"
#include "VVMType.h"

namespace Verse
{
struct VProcedure : VCell
{
	DECLARE_DERIVED_VCPPCLASSINFO(COREUOBJECT_API, VCell);
	COREUOBJECT_API static TGlobalTrivialEmergentTypePtr<&StaticCppClassInfo> GlobalTrivialEmergentType;

	const uint32 NumParameters;
	const uint32 NumRegisters;
	const uint32 NumOpBytes;

	const uint32 NumConstants;
	TWriteBarrier<VValue> Constants[];

	FOp* GetOpsBegin() { return BitCast<FOp*>(GetConstantsEnd()); }
	FOp* GetOpsEnd() { return BitCast<FOp*>(BitCast<uint8*>(GetOpsBegin()) + NumOpBytes); }

	// In bytes.
	uint32 BytecodeOffset(const FOp& Bytecode)
	{
		checkSlow(GetOpsBegin() <= &Bytecode && &Bytecode < GetOpsEnd());
		return static_cast<uint32>(BitCast<char*>(&Bytecode) - BitCast<char*>(GetOpsBegin()));
	}

	TWriteBarrier<VValue>* GetConstantsBegin()
	{
		return Constants;
	}
	TWriteBarrier<VValue>* GetConstantsEnd()
	{
		return GetConstantsBegin() + NumConstants;
	}

	void SetConstant(FAllocationContext Context, FConstantIndex ConstantIndex, VValue Value)
	{
		checkSlow(ConstantIndex.Index < NumConstants);
		Constants[ConstantIndex.Index].Set(Context, Value);
	}

	VValue GetConstant(FConstantIndex ConstantIndex)
	{
		checkSlow(ConstantIndex.Index < NumConstants);
		return Constants[ConstantIndex.Index].Get();
	}

	static VProcedure& New(FAllocationContext Context, uint32 NumParameters, uint32 NumRegisters, uint32 NumConstants, size_t NumOpBytes)
	{
		const size_t NumBytes = offsetof(VProcedure, Constants)
							  + sizeof(Constants[0]) * NumConstants
							  + NumOpBytes;
		return *new (Context.Allocate(Verse::FHeap::DestructorSpace, NumBytes)) VProcedure(Context, NumParameters, NumRegisters, NumConstants, NumOpBytes);
	}

private:
	VProcedure(FAllocationContext Context, uint32 InNumArguments, uint32 InNumRegisters, uint32 InNumConstants, uint32 InNumOpBytes)
		: VCell(Context, &GlobalTrivialEmergentType.Get(Context))
		, NumParameters(InNumArguments)
		, NumRegisters(InNumRegisters)
		, NumOpBytes(InNumOpBytes)
		, NumConstants(InNumConstants)
	{
		for (uint32 ConstantIndex = 0; ConstantIndex < NumConstants; ++ConstantIndex)
		{
			new (&Constants[ConstantIndex]) TWriteBarrier<VValue>{};
		}
	}

	/// Overridden from `VCell` because we want to ensure that the variadic arguments allocated in the function get de-allocated
	/// once the function object lifetime ends. Otherwise they would not get their destructors called normally.
	~VProcedure();
};
} // namespace Verse
#endif // WITH_VERSE_VM
