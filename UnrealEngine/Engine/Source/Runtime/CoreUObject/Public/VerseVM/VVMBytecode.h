// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#if WITH_VERSE_VM || defined(__INTELLISENSE__)

#include "CoreTypes.h"
#include "Misc/AssertionMacros.h"
#include "Templates/TypeCompatibleBytes.h"
#include "VVMBytecodeOps.h"

namespace Verse
{
struct VProcedure;

using FOpcodeInt = uint16_t;

enum class EOpcode : FOpcodeInt
{
#define VISIT_OP(Name) Name,
	VERSE_ENUM_OPS(VISIT_OP)
#undef VISIT_OP
};

COREUOBJECT_API const char* ToString(EOpcode Opcode);

/// This _must_ match up with the codegen in `VerseVMBytecodeGenerator.cs`.
enum class EOperandRole : uint8
{
	Use,
	Immediate,
	ClobberDef,
	UnifyDef,
};

// We align the bytecode stream to 8 bytes so we don't see tearing from the collector,
// and in the future other concurrent threads, when writing to a VValue/pointer sized
// entry.
struct alignas(8) FOp
{
	const EOpcode Opcode;

	explicit FOp(const EOpcode InOpcode)
		: Opcode(InOpcode) {}
};

struct FRegisterIndex
{
	uint32 Index;
};

struct FConstantIndex
{
	uint32 Index;
};

struct FValueOperand
{
	int32 Index = INT32_MIN;

	FValueOperand() = default;

	FValueOperand(FConstantIndex Constant)
		: Index(-1 - Constant.Index)
	{
		check(Constant.Index <= INT32_MAX);
		check(IsConstant());
	}
	FValueOperand(FRegisterIndex Register)
		: Index(Register.Index)
	{
		check(Register.Index <= INT32_MAX);
		check(!IsConstant());
	}

	bool IsConstant() const { return Index < 0; }
	bool IsRegister() const { return Index >= 0; }

	FRegisterIndex AsRegister() const
	{
		checkSlow(IsRegister());
		return FRegisterIndex{static_cast<uint32>(Index)};
	}
	FConstantIndex AsConstant() const
	{
		checkSlow(IsConstant());
		return FConstantIndex{static_cast<uint32>(-Index) - 1};
	}
};

struct FLabelOffset
{
	int32 Offset; // In bytes, relative to the address of this FLabelOffset

	FOp* GetLabeledPC() const
	{
		return const_cast<FOp*>(BitCast<const FOp*>(BitCast<const uint8*>(this) + Offset));
	}
};
} // namespace Verse
#endif // WITH_VERSE_VM
