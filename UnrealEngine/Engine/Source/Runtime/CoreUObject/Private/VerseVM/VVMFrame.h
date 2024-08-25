// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "VerseVM/VVMEmergentTypeCreator.h"
#include "VerseVM/VVMGlobalTrivialEmergentTypePtr.h"
#include "VerseVM/VVMRestValue.h"
#include "VerseVM/VVMType.h"

namespace Verse
{
struct FOp;
struct VProcedure;

struct VFrame : VCell
{
	DECLARE_DERIVED_VCPPCLASSINFO(COREUOBJECT_API, VCell);
	COREUOBJECT_API static TGlobalTrivialEmergentTypePtr<&StaticCppClassInfo> GlobalTrivialEmergentType;

	enum class EReturnKind : uint8
	{
		Value,
		RestValue
	};

	const uint32 NumRegisters;
	EReturnKind ReturnKind;
	TWriteBarrier<VFrame> CallerFrame;
	FOp* CallerPC{nullptr};
	VRestValue ReturnEffectToken{0};
	TWriteBarrier<VProcedure> Procedure;
	union Union
	{
		TWriteBarrier<VValue> Value;
		VRestValue* RestValue; // This points into the CallerFrame, or the VRestValue is on the C++ stack, so we don't need to tell GC about it.
							   //
		Union()
			: RestValue(nullptr) {}
	} Return;
	VRestValue Registers[];

	template <typename ReturnSlotType>
	static VFrame& New(FAllocationContext Context, uint32 NumRegisters, VFrame* CallerFrame, FOp* CallerPC, VProcedure& Procedure, ReturnSlotType ReturnSlot)
	{
		return *new (Context.AllocateFastCell(offsetof(VFrame, Registers) + sizeof(VRestValue) * NumRegisters)) VFrame(Context, NumRegisters, CallerFrame, CallerPC, Procedure, ReturnSlot);
	}

	VFrame& CloneWithoutCallerInfo(FAllocationContext Context)
	{
		return VFrame::New(Context, *this);
	}

private:
	static VFrame& New(FAllocationContext Context, VFrame& Other)
	{
		return *new (Context.AllocateFastCell(offsetof(VFrame, Registers) + sizeof(VRestValue) * Other.NumRegisters)) VFrame(Context, Other);
	}

	template <typename ReturnSlotType>
	VFrame(FAllocationContext Context, uint32 InNumRegisters, VFrame* CallerFrame, FOp* CallerPC, VProcedure& Procedure, ReturnSlotType ReturnSlot)
		: VCell(Context, VEmergentTypeCreator::GetOrCreate(Context, VTrivialType::Singleton.Get(), &StaticCppClassInfo))
		, NumRegisters(InNumRegisters)
		, CallerFrame(Context, CallerFrame)
		, CallerPC(CallerPC)
		, Procedure(Context, Procedure)
	{
		static_assert(std::is_same_v<ReturnSlotType, VRestValue*> || std::is_same_v<ReturnSlotType, VValue>);
		if constexpr (std::is_same_v<ReturnSlotType, VRestValue*>)
		{
			Return.RestValue = ReturnSlot;
			ReturnKind = EReturnKind::RestValue;
		}
		else if constexpr (std::is_same_v<ReturnSlotType, VValue>)
		{
			Return.Value.Set(Context, ReturnSlot);
			ReturnKind = EReturnKind::Value;
		}

		for (uint32 RegisterIndex = 0; RegisterIndex < NumRegisters; ++RegisterIndex)
		{
			// TODO SOL-4222: Pipe through proper split depth here.
			new (&Registers[RegisterIndex]) VRestValue(0);
		}
	}

	// We don't copy the CallerFrame/CallerPC because during lenient execution
	// this won't return to the caller.
	VFrame(FAllocationContext Context, VFrame& Other)
		: VCell(Context, &GlobalTrivialEmergentType.Get(Context))
		, NumRegisters(Other.NumRegisters)
		, ReturnKind(EReturnKind::Value)
		, Procedure(Context, Other.Procedure.Get())
	{
		Return.Value = TWriteBarrier<VValue>();
		if (Other.ReturnKind == EReturnKind::RestValue)
		{
			if (Other.Return.RestValue)
			{
				Return.Value.Set(Context, Other.Return.RestValue->Get(Context));
			}
		}
		else
		{
			Return.Value.Set(Context, Other.Return.Value.Get());
		}

		ReturnEffectToken.Set(Context, Other.ReturnEffectToken.Get(Context));
		for (uint32 RegisterIndex = 0; RegisterIndex < NumRegisters; ++RegisterIndex)
		{
			// TODO SOL-4222: Pipe through proper split depth here.
			new (&Registers[RegisterIndex]) VRestValue(0);
			Registers[RegisterIndex].Set(Context, Other.Registers[RegisterIndex].Get(Context));
		}
	}
};

} // namespace Verse
