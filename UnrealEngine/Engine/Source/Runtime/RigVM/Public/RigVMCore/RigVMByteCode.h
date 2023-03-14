// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/Array.h"
#include "Containers/ArrayView.h"
#include "Containers/Map.h"
#include "Containers/UnrealString.h"
#include "CoreMinimal.h"
#include "HAL/PlatformCrt.h"
#include "HAL/PlatformMath.h"
#include "HAL/UnrealMemory.h"
#include "Misc/AssertionMacros.h"
#include "RigVMCore/RigVMMemoryCommon.h"
#include "RigVMDefines.h"
#include "RigVMMemoryDeprecated.h"
#include "RigVMRegistry.h"
#include "RigVMStatistics.h"
#include "Templates/TypeHash.h"
#include "UObject/NameTypes.h"
#include "UObject/ObjectMacros.h"
#include "UObject/UnrealNames.h"

#include "RigVMByteCode.generated.h"

class FArchive;
class UObject;
struct FRigVMByteCode;

// The code for a single operation within the RigVM
UENUM()
enum class ERigVMOpCode : uint8
{
	Execute_0_Operands, // execute a rig function with 0 operands
	Execute_1_Operands, // execute a rig function with 1 operands
	Execute_2_Operands, // execute a rig function with 2 operands
	Execute_3_Operands, // execute a rig function with 3 operands
	Execute_4_Operands, // execute a rig function with 4 operands
	Execute_5_Operands, // execute a rig function with 5 operands
	Execute_6_Operands, // execute a rig function with 6 operands
	Execute_7_Operands, // execute a rig function with 7 operands
	Execute_8_Operands, // execute a rig function with 8 operands
	Execute_9_Operands, // execute a rig function with 9 operands
	Execute_10_Operands, // execute a rig function with 10 operands
	Execute_11_Operands, // execute a rig function with 11 operands
	Execute_12_Operands, // execute a rig function with 12 operands
	Execute_13_Operands, // execute a rig function with 13 operands
	Execute_14_Operands, // execute a rig function with 14 operands
	Execute_15_Operands, // execute a rig function with 15 operands
	Execute_16_Operands, // execute a rig function with 16 operands
	Execute_17_Operands, // execute a rig function with 17 operands
	Execute_18_Operands, // execute a rig function with 18 operands
	Execute_19_Operands, // execute a rig function with 19 operands
	Execute_20_Operands, // execute a rig function with 20 operands
	Execute_21_Operands, // execute a rig function with 21 operands
	Execute_22_Operands, // execute a rig function with 22 operands
	Execute_23_Operands, // execute a rig function with 23 operands
	Execute_24_Operands, // execute a rig function with 24 operands
	Execute_25_Operands, // execute a rig function with 25 operands
	Execute_26_Operands, // execute a rig function with 26 operands
	Execute_27_Operands, // execute a rig function with 27 operands
	Execute_28_Operands, // execute a rig function with 28 operands
	Execute_29_Operands, // execute a rig function with 29 operands
	Execute_30_Operands, // execute a rig function with 30 operands
	Execute_31_Operands, // execute a rig function with 31 operands
	Execute_32_Operands, // execute a rig function with 32 operands
	Execute_33_Operands, // execute a rig function with 33 operands
	Execute_34_Operands, // execute a rig function with 34 operands
	Execute_35_Operands, // execute a rig function with 35 operands
	Execute_36_Operands, // execute a rig function with 36 operands
	Execute_37_Operands, // execute a rig function with 37 operands
	Execute_38_Operands, // execute a rig function with 38 operands
	Execute_39_Operands, // execute a rig function with 39 operands
	Execute_40_Operands, // execute a rig function with 40 operands
	Execute_41_Operands, // execute a rig function with 41 operands
	Execute_42_Operands, // execute a rig function with 42 operands
	Execute_43_Operands, // execute a rig function with 43 operands
	Execute_44_Operands, // execute a rig function with 44 operands
	Execute_45_Operands, // execute a rig function with 45 operands
	Execute_46_Operands, // execute a rig function with 46 operands
	Execute_47_Operands, // execute a rig function with 47 operands
	Execute_48_Operands, // execute a rig function with 48 operands
	Execute_49_Operands, // execute a rig function with 49 operands
	Execute_50_Operands, // execute a rig function with 50 operands
	Execute_51_Operands, // execute a rig function with 51 operands
	Execute_52_Operands, // execute a rig function with 52 operands
	Execute_53_Operands, // execute a rig function with 53 operands
	Execute_54_Operands, // execute a rig function with 54 operands
	Execute_55_Operands, // execute a rig function with 55 operands
	Execute_56_Operands, // execute a rig function with 56 operands
	Execute_57_Operands, // execute a rig function with 57 operands
	Execute_58_Operands, // execute a rig function with 58 operands
	Execute_59_Operands, // execute a rig function with 59 operands
	Execute_60_Operands, // execute a rig function with 60 operands
	Execute_61_Operands, // execute a rig function with 61 operands
	Execute_62_Operands, // execute a rig function with 62 operands
	Execute_63_Operands, // execute a rig function with 63 operands
	Execute_64_Operands, // execute a rig function with 64 operands
	Zero, // zero the memory of a given register
	BoolFalse, // set a given register to false
	BoolTrue, // set a given register to true
	Copy, // copy the content of one register to another
	Increment, // increment a int32 register
	Decrement, // decrement a int32 register
	Equals, // fill a bool register with the result of (A == B)
	NotEquals, // fill a bool register with the result of (A != B)
	JumpAbsolute, // jump to an absolute instruction index
	JumpForward, // jump forwards given a relative instruction index offset
	JumpBackward, // jump backwards given a relative instruction index offset
	JumpAbsoluteIf, // jump to an absolute instruction index based on a condition register
	JumpForwardIf, // jump forwards given a relative instruction index offset based on a condition register
	JumpBackwardIf, // jump backwards given a relative instruction index offset based on a condition register
	ChangeType, // change the type of a register (deprecated)
	Exit, // exit the execution loop
	BeginBlock, // begins a new memory slice / block
	EndBlock, // ends the last memory slice / block
	ArrayReset, // clears an array and resets its content
	ArrayGetNum, // reads and returns the size of an array (binary op, in array, out int32) 
	ArraySetNum, // resizes an array (binary op, in out array, in int32)
	ArrayGetAtIndex, // returns an array element by index (ternary op, in array, in int32, out element)  
	ArraySetAtIndex, // sets an array element by index (ternary op, in out array, in int32, in element)
	ArrayAdd, // adds an element to an array (ternary op, in out array, in element, out int32 index)
	ArrayInsert, // inserts an element to an array (ternary op, in out array, in int32, in element)
	ArrayRemove, // removes an element from an array (binary op, in out array, in inindex)
	ArrayFind, // finds and returns the index of an element (quaternery op, in array, in element, out int32 index, out bool success)
	ArrayAppend, // appends an array to another (binary op, in out array, in array)
	ArrayClone, // clones an array (binary op, in array, out array)
	ArrayIterator, // iterates over an array (senary op, in array, out element, out index, out count, out ratio, out continue)
	ArrayUnion, // merges two arrays while avoiding duplicates (binary op, in out array, in other array)
	ArrayDifference, // returns a new array containing elements only found in one array (ternary op, in array, in array, out result)
	ArrayIntersection, // returns a new array containing elements found in both of the input arrays (ternary op, in array, in array, out result)
	ArrayReverse, // returns the reverse of the input array (unary op, in out array)
	InvokeEntry, // invokes an entry from the entry list
	Invalid,
	FirstArrayOpCode = ArrayReset,
	LastArrayOpCode = ArrayReverse,
};

// Base class for all VM operations
USTRUCT()
struct RIGVM_API FRigVMBaseOp
{
	GENERATED_USTRUCT_BODY()

	FRigVMBaseOp(ERigVMOpCode InOpCode = ERigVMOpCode::Invalid)
	: OpCode(InOpCode)
	{
	}

	ERigVMOpCode OpCode;

	friend FORCEINLINE uint32 GetTypeHash(const FRigVMBaseOp& Op)
	{
		return GetTypeHash(Op.OpCode);
	}
};


// execute a function
USTRUCT()
struct RIGVM_API FRigVMExecuteOp : public FRigVMBaseOp
{
	GENERATED_USTRUCT_BODY()

	FRigVMExecuteOp()
	: FRigVMBaseOp()
	, FunctionIndex(INDEX_NONE)
	{
	}

	FRigVMExecuteOp(uint16 InFunctionIndex, uint8 InArgumentCount)
	: FRigVMBaseOp((ERigVMOpCode)(uint8(ERigVMOpCode::Execute_0_Operands) + InArgumentCount))
	, FunctionIndex(InFunctionIndex)
	{
	}

	uint16 FunctionIndex;

	friend FORCEINLINE uint32 GetTypeHash(const FRigVMExecuteOp& Op)
	{
		return HashCombine(GetTypeHash((const FRigVMBaseOp&)Op), GetTypeHash(Op.FunctionIndex));
	}

	FORCEINLINE uint8 GetOperandCount() const { return uint8(OpCode) - uint8(ERigVMOpCode::Execute_0_Operands); }

	void Serialize(FArchive& Ar);
	FORCEINLINE friend FArchive& operator<<(FArchive& Ar, FRigVMExecuteOp& P)
	{
		P.Serialize(Ar);
		return Ar;
	}
};

// operator used for zero, false, true, increment, decrement
USTRUCT()
struct RIGVM_API FRigVMUnaryOp : public FRigVMBaseOp
{
	GENERATED_USTRUCT_BODY()

	FRigVMUnaryOp()
		: FRigVMBaseOp(ERigVMOpCode::Invalid)
		, Arg()
	{
	}

	FRigVMUnaryOp(ERigVMOpCode InOpCode, FRigVMOperand InArg)
		: FRigVMBaseOp(InOpCode)
		, Arg(InArg)
	{
		ensure(
			uint8(InOpCode) == uint8(ERigVMOpCode::Zero) ||
			uint8(InOpCode) == uint8(ERigVMOpCode::BoolFalse) ||
			uint8(InOpCode) == uint8(ERigVMOpCode::BoolTrue) ||
			uint8(InOpCode) == uint8(ERigVMOpCode::Increment) ||
			uint8(InOpCode) == uint8(ERigVMOpCode::Decrement) ||
			uint8(InOpCode) == uint8(ERigVMOpCode::JumpAbsoluteIf) ||
			uint8(InOpCode) == uint8(ERigVMOpCode::JumpForwardIf) ||
			uint8(InOpCode) == uint8(ERigVMOpCode::JumpBackwardIf) ||
			uint8(InOpCode) == uint8(ERigVMOpCode::ChangeType) ||
			uint8(InOpCode) == uint8(ERigVMOpCode::ArrayReset) ||
			uint8(InOpCode) == uint8(ERigVMOpCode::ArrayReverse)
		);
	}

	FRigVMOperand Arg;

	friend FORCEINLINE uint32 GetTypeHash(const FRigVMUnaryOp& Op)
	{
		return HashCombine(GetTypeHash((const FRigVMBaseOp&)Op), GetTypeHash(Op.Arg));
	}

	void Serialize(FArchive& Ar);
	FORCEINLINE friend FArchive& operator<<(FArchive& Ar, FRigVMUnaryOp& P)
	{
		P.Serialize(Ar);
		return Ar;
	}
};

// operator used for beginblock and array reset
USTRUCT()
struct RIGVM_API FRigVMBinaryOp : public FRigVMBaseOp
{
	GENERATED_USTRUCT_BODY()

	FRigVMBinaryOp()
		: FRigVMBaseOp(ERigVMOpCode::Invalid)
		, ArgA()
		, ArgB()
	{
	}

	FRigVMBinaryOp(ERigVMOpCode InOpCode, FRigVMOperand InArgA, FRigVMOperand InArgB)
		: FRigVMBaseOp(InOpCode)
		, ArgA(InArgA)
		, ArgB(InArgB)
	{
		ensure(
			uint8(InOpCode) == uint8(ERigVMOpCode::BeginBlock) ||
			uint8(InOpCode) == uint8(ERigVMOpCode::ArrayGetNum) ||
			uint8(InOpCode) == uint8(ERigVMOpCode::ArraySetNum) ||
			uint8(InOpCode) == uint8(ERigVMOpCode::ArrayAppend) ||
			uint8(InOpCode) == uint8(ERigVMOpCode::ArrayClone) ||
			uint8(InOpCode) == uint8(ERigVMOpCode::ArrayRemove) ||
			uint8(InOpCode) == uint8(ERigVMOpCode::ArrayUnion)
		);
	}

	FRigVMOperand ArgA;
	FRigVMOperand ArgB;

	friend FORCEINLINE uint32 GetTypeHash(const FRigVMBinaryOp& Op)
	{
		uint32 Hash = GetTypeHash((const FRigVMBaseOp&)Op);
		Hash = HashCombine(Hash, GetTypeHash(Op.ArgA));
		Hash = HashCombine(Hash, GetTypeHash(Op.ArgB));
		return Hash;
	}

	void Serialize(FArchive& Ar);
	FORCEINLINE friend FArchive& operator<<(FArchive& Ar, FRigVMBinaryOp& P)
	{
		P.Serialize(Ar);
		return Ar;
	}
};

// operator used for some array operations
USTRUCT()
struct RIGVM_API FRigVMTernaryOp : public FRigVMBaseOp
{
	GENERATED_USTRUCT_BODY()

	FRigVMTernaryOp()
		: FRigVMBaseOp(ERigVMOpCode::Invalid)
		, ArgA()
		, ArgB()
		, ArgC()
	{
	}

	FRigVMTernaryOp(ERigVMOpCode InOpCode, FRigVMOperand InArgA, FRigVMOperand InArgB, FRigVMOperand InArgC)
		: FRigVMBaseOp(InOpCode)
		, ArgA(InArgA)
		, ArgB(InArgB)
		, ArgC(InArgC)
	{
		ensure(
			uint8(InOpCode) == uint8(ERigVMOpCode::ArrayAdd) ||
			uint8(InOpCode) == uint8(ERigVMOpCode::ArrayGetAtIndex) ||
			uint8(InOpCode) == uint8(ERigVMOpCode::ArraySetAtIndex) ||
			uint8(InOpCode) == uint8(ERigVMOpCode::ArrayInsert) ||
			uint8(InOpCode) == uint8(ERigVMOpCode::ArrayDifference) ||
			uint8(InOpCode) == uint8(ERigVMOpCode::ArrayIntersection)
		);
	}

	FRigVMOperand ArgA;
	FRigVMOperand ArgB;
	FRigVMOperand ArgC;

	friend FORCEINLINE uint32 GetTypeHash(const FRigVMTernaryOp& Op)
	{
		uint32 Hash = GetTypeHash((const FRigVMBaseOp&)Op);
		Hash = HashCombine(Hash, GetTypeHash(Op.ArgA));
		Hash = HashCombine(Hash, GetTypeHash(Op.ArgB));
		Hash = HashCombine(Hash, GetTypeHash(Op.ArgC));
		return Hash;
	}

	void Serialize(FArchive& Ar);
	FORCEINLINE friend FArchive& operator<<(FArchive& Ar, FRigVMTernaryOp& P)
	{
		P.Serialize(Ar);
		return Ar;
	}
};

// operator used for some array operations
USTRUCT()
struct RIGVM_API FRigVMQuaternaryOp : public FRigVMBaseOp
{
	GENERATED_USTRUCT_BODY()

	FRigVMQuaternaryOp()
		: FRigVMBaseOp(ERigVMOpCode::Invalid)
		, ArgA()
		, ArgB()
		, ArgC()
		, ArgD()
	{
	}

	FRigVMQuaternaryOp(ERigVMOpCode InOpCode, FRigVMOperand InArgA, FRigVMOperand InArgB, FRigVMOperand InArgC, FRigVMOperand InArgD)
		: FRigVMBaseOp(InOpCode)
		, ArgA(InArgA)
		, ArgB(InArgB)
		, ArgC(InArgC)
		, ArgD(InArgD)
	{
		ensure(
			uint8(InOpCode) == uint8(ERigVMOpCode::ArrayFind)
		);
	}

	FRigVMOperand ArgA;
	FRigVMOperand ArgB;
	FRigVMOperand ArgC;
	FRigVMOperand ArgD;

	friend FORCEINLINE uint32 GetTypeHash(const FRigVMQuaternaryOp& Op)
	{
		uint32 Hash = GetTypeHash((const FRigVMBaseOp&)Op);
		Hash = HashCombine(Hash, GetTypeHash(Op.ArgA));
		Hash = HashCombine(Hash, GetTypeHash(Op.ArgB));
		Hash = HashCombine(Hash, GetTypeHash(Op.ArgC));
		Hash = HashCombine(Hash, GetTypeHash(Op.ArgD));
		return Hash;
	}

	void Serialize(FArchive& Ar);
	FORCEINLINE friend FArchive& operator<<(FArchive& Ar, FRigVMQuaternaryOp& P)
	{
		P.Serialize(Ar);
		return Ar;
	}
};

// operator used for some array operations
USTRUCT()
struct RIGVM_API FRigVMQuinaryOp : public FRigVMBaseOp
{
	GENERATED_USTRUCT_BODY()

	FRigVMQuinaryOp()
		: FRigVMBaseOp(ERigVMOpCode::Invalid)
		, ArgA()
		, ArgB()
		, ArgC()
		, ArgD()
		, ArgE()
	{
	}

	FRigVMQuinaryOp(ERigVMOpCode InOpCode, FRigVMOperand InArgA, FRigVMOperand InArgB, FRigVMOperand InArgC, FRigVMOperand InArgD, FRigVMOperand InArgE)
		: FRigVMBaseOp(InOpCode)
		, ArgA(InArgA)
		, ArgB(InArgB)
		, ArgC(InArgC)
		, ArgD(InArgD)
		, ArgE(InArgE)
	{
	}

	FRigVMOperand ArgA;
	FRigVMOperand ArgB;
	FRigVMOperand ArgC;
	FRigVMOperand ArgD;
	FRigVMOperand ArgE;

	friend FORCEINLINE uint32 GetTypeHash(const FRigVMQuinaryOp& Op)
	{
		uint32 Hash = GetTypeHash((const FRigVMBaseOp&)Op);
		Hash = HashCombine(Hash, GetTypeHash(Op.ArgA));
		Hash = HashCombine(Hash, GetTypeHash(Op.ArgB));
		Hash = HashCombine(Hash, GetTypeHash(Op.ArgC));
		Hash = HashCombine(Hash, GetTypeHash(Op.ArgD));
		Hash = HashCombine(Hash, GetTypeHash(Op.ArgE));
		return Hash;
	}

	void Serialize(FArchive& Ar);
	FORCEINLINE friend FArchive& operator<<(FArchive& Ar, FRigVMQuinaryOp& P)
	{
		P.Serialize(Ar);
		return Ar;
	}
};

// operator used for some array operations
USTRUCT()
struct RIGVM_API FRigVMSenaryOp : public FRigVMBaseOp
{
	GENERATED_USTRUCT_BODY()

	FRigVMSenaryOp()
		: FRigVMBaseOp(ERigVMOpCode::Invalid)
		, ArgA()
		, ArgB()
		, ArgC()
		, ArgD()
		, ArgE()
		, ArgF()
	{
	}

	FRigVMSenaryOp(ERigVMOpCode InOpCode, FRigVMOperand InArgA, FRigVMOperand InArgB, FRigVMOperand InArgC, FRigVMOperand InArgD, FRigVMOperand InArgE, FRigVMOperand InArgF)
		: FRigVMBaseOp(InOpCode)
		, ArgA(InArgA)
		, ArgB(InArgB)
		, ArgC(InArgC)
		, ArgD(InArgD)
		, ArgE(InArgE)
		, ArgF(InArgF)
	{
		ensure(
			uint8(InOpCode) == uint8(ERigVMOpCode::ArrayIterator)
		);
	}

	FRigVMOperand ArgA;
	FRigVMOperand ArgB;
	FRigVMOperand ArgC;
	FRigVMOperand ArgD;
	FRigVMOperand ArgE;
	FRigVMOperand ArgF;

	friend FORCEINLINE uint32 GetTypeHash(const FRigVMSenaryOp& Op)
	{
		uint32 Hash = GetTypeHash((const FRigVMBaseOp&)Op);
		Hash = HashCombine(Hash, GetTypeHash(Op.ArgA));
		Hash = HashCombine(Hash, GetTypeHash(Op.ArgB));
		Hash = HashCombine(Hash, GetTypeHash(Op.ArgC));
		Hash = HashCombine(Hash, GetTypeHash(Op.ArgD));
		Hash = HashCombine(Hash, GetTypeHash(Op.ArgE));
		Hash = HashCombine(Hash, GetTypeHash(Op.ArgF));
		return Hash;
	}

	void Serialize(FArchive& Ar);
	FORCEINLINE friend FArchive& operator<<(FArchive& Ar, FRigVMSenaryOp& P)
	{
		P.Serialize(Ar);
		return Ar;
	}
};

// The kind of copy operation to perform
UENUM()
enum class ERigVMCopyType : uint8
{
	Default,
	FloatToDouble,
	DoubleToFloat
};

// copy the content of one register to another
USTRUCT()
struct RIGVM_API FRigVMCopyOp : public FRigVMBaseOp
{
	GENERATED_USTRUCT_BODY()

public:

	FRigVMCopyOp()
	: FRigVMBaseOp(ERigVMOpCode::Copy)
	, Source()
	, Target()
	, NumBytes(0)
	, RegisterType(ERigVMRegisterType::Invalid)
	, CopyType(ERigVMCopyType::Default)
	{
	}

	FRigVMCopyOp(
		FRigVMOperand InSource,
		FRigVMOperand InTarget
	)
		: FRigVMBaseOp(ERigVMOpCode::Copy)
		, Source(InSource)
		, Target(InTarget)
		, NumBytes(0)
		, RegisterType(ERigVMRegisterType::Invalid)
		, CopyType(ERigVMCopyType::Default)
	{
	}

	bool IsValid() const
	{
		return
			Source.IsValid() &&
			Target.IsValid() &&
			(Source != Target) &&
			(Target.GetMemoryType() != ERigVMMemoryType::Literal);
	}

	FRigVMOperand Source;
	FRigVMOperand Target;

	friend FORCEINLINE uint32 GetTypeHash(const FRigVMCopyOp& Op)
	{
		uint32 Hash = GetTypeHash((const FRigVMBaseOp&)Op);
		Hash = HashCombine(Hash, GetTypeHash(Op.Source));
		Hash = HashCombine(Hash, GetTypeHash(Op.Target));
		return Hash;
	}

private:
	uint16 NumBytes;
	ERigVMRegisterType RegisterType;
	ERigVMCopyType CopyType;

public:
	void Serialize(FArchive& Ar);
	FORCEINLINE friend FArchive& operator<<(FArchive& Ar, FRigVMCopyOp& P)
	{
		P.Serialize(Ar);
		return Ar;
	}
};

// used for equals and not equals comparisons
USTRUCT()
struct RIGVM_API FRigVMComparisonOp : public FRigVMBaseOp
{
	GENERATED_USTRUCT_BODY()

	FRigVMComparisonOp()
		: FRigVMBaseOp(ERigVMOpCode::Invalid)
		, A()
		, B()
		, Result()
	{
	}

	FRigVMComparisonOp(
		ERigVMOpCode InOpCode,
		FRigVMOperand InA,
		FRigVMOperand InB,
		FRigVMOperand InResult
	)
		: FRigVMBaseOp(InOpCode)
		, A(InA)
		, B(InB)
		, Result(InResult)
	{
		ensure(
			uint8(InOpCode) == uint8(ERigVMOpCode::Equals) ||
			uint8(InOpCode) == uint8(ERigVMOpCode::NotEquals)
			);
	}

	FRigVMOperand A;
	FRigVMOperand B;
	FRigVMOperand Result;

	friend FORCEINLINE uint32 GetTypeHash(const FRigVMComparisonOp& Op)
	{
		uint32 Hash = GetTypeHash((const FRigVMBaseOp&)Op);
		Hash = HashCombine(Hash, GetTypeHash(Op.A));
		Hash = HashCombine(Hash, GetTypeHash(Op.B));
		Hash = HashCombine(Hash, GetTypeHash(Op.Result));
		return Hash;
	}

	void Serialize(FArchive& Ar);
	FORCEINLINE friend FArchive& operator<<(FArchive& Ar, FRigVMComparisonOp& P)
	{
		P.Serialize(Ar);
		return Ar;
	}
};

// jump to a new instruction index.
// the instruction can be absolute, relative forward or relative backward
// based on the opcode 
USTRUCT()
struct RIGVM_API FRigVMJumpOp : public FRigVMBaseOp
{
	GENERATED_USTRUCT_BODY()

	FRigVMJumpOp()
	: FRigVMBaseOp(ERigVMOpCode::Invalid)
	, InstructionIndex(INDEX_NONE)
	{
	}

	FRigVMJumpOp(ERigVMOpCode InOpCode, int32 InInstructionIndex)
	: FRigVMBaseOp(InOpCode)
	, InstructionIndex(InInstructionIndex)
	{
		ensure(uint8(InOpCode) >= uint8(ERigVMOpCode::JumpAbsolute));
		ensure(uint8(InOpCode) <= uint8(ERigVMOpCode::JumpBackward));
	}

	int32 InstructionIndex;

	friend FORCEINLINE uint32 GetTypeHash(const FRigVMJumpOp& Op)
	{
		uint32 Hash = GetTypeHash((const FRigVMBaseOp&)Op);
		Hash = HashCombine(Hash, GetTypeHash(Op.InstructionIndex));
		return Hash;
	}

	void Serialize(FArchive& Ar);
	FORCEINLINE friend FArchive& operator<<(FArchive& Ar, FRigVMJumpOp& P)
	{
		P.Serialize(Ar);
		return Ar;
	}
};

// jump to a new instruction index based on a condition.
// the instruction can be absolute, relative forward or relative backward
// based on the opcode 
USTRUCT()
struct RIGVM_API FRigVMJumpIfOp : public FRigVMUnaryOp
{
	GENERATED_USTRUCT_BODY()

	FRigVMJumpIfOp()
		: FRigVMUnaryOp()
		, InstructionIndex(INDEX_NONE)
		, Condition(true)
	{
	}

	FRigVMJumpIfOp(ERigVMOpCode InOpCode, FRigVMOperand InConditionArg, int32 InInstructionIndex, bool InCondition = false)
		: FRigVMUnaryOp(InOpCode, InConditionArg)
		, InstructionIndex(InInstructionIndex)
		, Condition(InCondition)
	{
		ensure(uint8(InOpCode) >= uint8(ERigVMOpCode::JumpAbsoluteIf));
		ensure(uint8(InOpCode) <= uint8(ERigVMOpCode::JumpBackwardIf));
	}

	int32 InstructionIndex;
	bool Condition;

	friend FORCEINLINE uint32 GetTypeHash(const FRigVMJumpIfOp& Op)
	{
		uint32 Hash = GetTypeHash((const FRigVMBaseOp&)Op);
		Hash = HashCombine(Hash, GetTypeHash(Op.InstructionIndex));
		Hash = HashCombine(Hash, GetTypeHash(Op.Condition));
		return Hash;
	}

	void Serialize(FArchive& Ar);
	FORCEINLINE friend FArchive& operator<<(FArchive& Ar, FRigVMJumpIfOp& P)
	{
		P.Serialize(Ar);
		return Ar;
	}
};

// change the type of a register
USTRUCT()
struct RIGVM_API FRigVMChangeTypeOp : public FRigVMUnaryOp
{
	GENERATED_USTRUCT_BODY()

	void Serialize(FArchive& Ar);
	FORCEINLINE friend FArchive& operator<<(FArchive& Ar, FRigVMChangeTypeOp& P)
	{
		P.Serialize(Ar);
		return Ar;
	}
};

// invoke another entry
USTRUCT()
struct RIGVM_API FRigVMInvokeEntryOp : public FRigVMBaseOp
{
	GENERATED_USTRUCT_BODY()

	FRigVMInvokeEntryOp()
		: FRigVMBaseOp(ERigVMOpCode::InvokeEntry)
		, EntryName(NAME_None)
	{}

	FRigVMInvokeEntryOp(FName InEntryName)
		: FRigVMBaseOp(ERigVMOpCode::InvokeEntry)
		, EntryName(InEntryName)
	{}

	FName EntryName;

	friend FORCEINLINE uint32 GetTypeHash(const FRigVMInvokeEntryOp& Op)
	{
		return HashCombine(GetTypeHash((const FRigVMBaseOp&)Op), GetTypeHash(Op.EntryName));
	}

	void Serialize(FArchive& Ar);
	FORCEINLINE friend FArchive& operator<<(FArchive& Ar, FRigVMInvokeEntryOp& P)
	{
		P.Serialize(Ar);
		return Ar;
	}
};

/**
 * The FRigVMInstruction represents
 * a single instruction within the VM.
 */
USTRUCT()
struct RIGVM_API FRigVMInstruction
{
	GENERATED_USTRUCT_BODY()

	FRigVMInstruction(ERigVMOpCode InOpCode = ERigVMOpCode::Invalid, uint64 InByteCodeIndex = UINT64_MAX, uint8 InOperandAlignment = 0)
		: ByteCodeIndex(InByteCodeIndex)
		, OpCode(InOpCode)
		, OperandAlignment(InOperandAlignment)
	{
	}

	UPROPERTY()
	uint64 ByteCodeIndex;

	UPROPERTY()
	ERigVMOpCode OpCode;

	UPROPERTY()
	uint8 OperandAlignment;
};

/**
 * The FRigVMInstructionArray represents all current instructions
 * within a RigVM and can be used to iterate over all operators and retrieve
 * each instruction's data.
 */
USTRUCT()
struct RIGVM_API FRigVMInstructionArray
{
	GENERATED_USTRUCT_BODY()

public:

	FRigVMInstructionArray();

	// Resets the data structure and maintains all storage.
	void Reset();

	// Resets the data structure and removes all storage.
	void Empty();

	// Returns true if a given instruction index is valid.
	FORCEINLINE bool IsValidIndex(int32 InIndex) const { return Instructions.IsValidIndex(InIndex); }

	// Returns the number of instructions.
	FORCEINLINE int32 Num() const { return Instructions.Num(); }

	// const accessor for an instruction given its index
	FORCEINLINE const FRigVMInstruction& operator[](int32 InIndex) const { return Instructions[InIndex]; }

	FORCEINLINE_DEBUGGABLE TArray<FRigVMInstruction>::RangedForConstIteratorType begin() const { return Instructions.begin(); }
	FORCEINLINE_DEBUGGABLE TArray<FRigVMInstruction>::RangedForConstIteratorType end() const { return Instructions.end(); }

private:

	// hide utility constructor
	FRigVMInstructionArray(const FRigVMByteCode& InByteCode, bool bByteCodeIsAligned = true);

	UPROPERTY()
	TArray<FRigVMInstruction> Instructions;

	friend struct FRigVMByteCode;
};

USTRUCT()
struct RIGVM_API FRigVMByteCodeEntry
{
	GENERATED_USTRUCT_BODY()

	FRigVMByteCodeEntry()
		: Name(NAME_None)
		, InstructionIndex(0)
	{}

	UPROPERTY()
	FName Name;

	UPROPERTY()
	int32 InstructionIndex;

	FString GetSanitizedName() const;
};

/**
 * The FRigVMByteCode is a container to store a list of instructions with
 * their corresponding data. The byte code is then used within a VM to 
 * execute. To iterate over the instructions within the byte code you can 
 * use GetInstructions() to retrieve a FRigVMInstructionArray.
 */
USTRUCT()
struct RIGVM_API FRigVMByteCode
{
	GENERATED_USTRUCT_BODY()

public:

	FRigVMByteCode();

	void Serialize(FArchive& Ar);
	void Save(FArchive& Ar) const;
	void Load(FArchive& Ar);
	FORCEINLINE friend FArchive& operator<<(FArchive& Ar, FRigVMByteCode& P)
	{
		P.Serialize(Ar);
		return Ar;
	}

	// resets the container and maintains all memory
	void Reset();

	// resets the container and removes all memory
	void Empty();

	// returns a unique hash identifying this bytecode
	uint32 GetByteCodeHash() const;

	// returns a unique hash for an operator at a given instruction index
	uint32 GetOperatorHash(const FRigVMInstruction& InInstruction) const;

	// overload for GetTypeHash
	friend FORCEINLINE uint32 GetTypeHash(const FRigVMByteCode& InByteCode)
	{
		return InByteCode.GetByteCodeHash();
	}
	
	// returns the number of instructions in this container
	uint64 Num() const;

	// const accessor for a byte given its index
	FORCEINLINE const uint8& operator[](int32 InIndex) const { return ByteCode[InIndex]; }

	// returns the number of entries
	uint64 NumEntries() const;

	// returns the entry with a given index
	const FRigVMByteCodeEntry& GetEntry(int32 InEntryIndex) const;

	// returns the index of an entry given a name or INDEX_NONE
	int32 FindEntryIndex(const FName& InEntryName) const;

	// adds an execute operator given its function index operands
	uint64 AddExecuteOp(uint16 InFunctionIndex, const FRigVMOperandArray& InOperands);

	// adds a zero operator to zero the memory of a given argument
	uint64 AddZeroOp(const FRigVMOperand& InArg);

	// adds a false operator to set a given argument to false
	uint64 AddFalseOp(const FRigVMOperand& InArg);

	// adds a true operator to set a given argument to true
	uint64 AddTrueOp(const FRigVMOperand& InArg);

	// adds a copy operator to copy the content of a source argument to a target argument
	uint64 AddCopyOp(const FRigVMOperand& InSource, const FRigVMOperand& InTarget);

	// adds a copy operator to copy the content of a source argument to a target argument
	uint64 AddCopyOp(const FRigVMCopyOp& InCopyOp);

	// adds an increment operator to increment a int32 argument
	uint64 AddIncrementOp(const FRigVMOperand& InArg);

	// adds an decrement operator to decrement a int32 argument
	uint64 AddDecrementOp(const FRigVMOperand& InArg);

	// adds an equals operator to store the comparison result of A and B into a Result argument
	uint64 AddEqualsOp(const FRigVMOperand& InA, const FRigVMOperand& InB, const FRigVMOperand& InResult);

	// adds an not-equals operator to store the comparison result of A and B into a Result argument
	uint64 AddNotEqualsOp(const FRigVMOperand& InA, const FRigVMOperand& InB, const FRigVMOperand& InResult);

	// adds an absolute, forward or backward jump operator
	uint64 AddJumpOp(ERigVMOpCode InOpCode, uint16 InInstructionIndex);

	// adds an absolute, forward or backward jump operator based on a condition argument
	uint64 AddJumpIfOp(ERigVMOpCode InOpCode, uint16 InInstructionIndex, const FRigVMOperand& InConditionArg, bool bJumpWhenConditionIs = false);

	// adds an exit operator to exit the execution loop
	uint64 AddExitOp();

	// adds an operator to end the last memory slice
	uint64 AddBeginBlockOp(FRigVMOperand InCountArg, FRigVMOperand InIndexArg);

	// adds an operator to end the last memory slice
	uint64 AddEndBlockOp();

	// adds an array reset operator
	uint64 AddArrayResetOp(FRigVMOperand InArrayArg);

	// adds an array get num operator
	uint64 AddArrayGetNumOp(FRigVMOperand InArrayArg, FRigVMOperand InNumArg);

	// adds an array set num operator
	uint64 AddArraySetNumOp(FRigVMOperand InArrayArg, FRigVMOperand InNumArg);

	// adds an array get at index operator
	uint64 AddArrayGetAtIndexOp(FRigVMOperand InArrayArg, FRigVMOperand InIndexArg, FRigVMOperand InElementArg);

	// adds an array set at index operator
	uint64 AddArraySetAtIndexOp(FRigVMOperand InArrayArg, FRigVMOperand InIndexArg, FRigVMOperand InElementArg);

	// adds an array add operator
	uint64 AddArrayAddOp(FRigVMOperand InArrayArg, FRigVMOperand InElementArg, FRigVMOperand InIndexArg);

	// adds an array insert operator
	uint64 AddArrayInsertOp(FRigVMOperand InArrayArg, FRigVMOperand InIndexArg, FRigVMOperand InElementArg);

	// adds an array remove operator
	uint64 AddArrayRemoveOp(FRigVMOperand InArrayArg, FRigVMOperand InIndexArg);

	// adds an array find operator
	uint64 AddArrayFindOp(FRigVMOperand InArrayArg, FRigVMOperand InElementArg, FRigVMOperand InIndexArg, FRigVMOperand InSuccessArg);

	// adds an array append operator
	uint64 AddArrayAppendOp(FRigVMOperand InArrayArg, FRigVMOperand InOtherArrayArg);

	// adds an array clone operator
	uint64 AddArrayCloneOp(FRigVMOperand InArrayArg, FRigVMOperand InClonedArrayArg);

	// adds an array iterator operator
	uint64 AddArrayIteratorOp(FRigVMOperand InArrayArg, FRigVMOperand InElementArg, FRigVMOperand InIndexArg, FRigVMOperand InCountArg, FRigVMOperand InRatioArg, FRigVMOperand InContinueArg);

	// adds an array union operator
	uint64 AddArrayUnionOp(FRigVMOperand InArrayArg, FRigVMOperand InOtherArrayArg);
	
	// adds an array difference operator
	uint64 AddArrayDifferenceOp(FRigVMOperand InArrayArg, FRigVMOperand InOtherArrayArg, FRigVMOperand InResultArrayArg);
	
	// adds an array intersection operator
	uint64 AddArrayIntersectionOp(FRigVMOperand InArrayArg, FRigVMOperand InOtherArrayArg, FRigVMOperand InResultArrayArg);
	
	// adds an array reverse operator
	uint64 AddArrayReverseOp(FRigVMOperand InArrayArg);

	// adds an invoke entry operator
	uint64 AddInvokeEntryOp(const FName& InEntryName);

	// returns an instruction array for iterating over all operators
	FORCEINLINE FRigVMInstructionArray GetInstructions() const
	{
		return FRigVMInstructionArray(*this, bByteCodeIsAligned);
	}

	// returns the opcode at a given byte index
	FORCEINLINE ERigVMOpCode GetOpCodeAt(uint64 InByteCodeIndex) const
	{
		ensure(InByteCodeIndex >= 0 && InByteCodeIndex < ByteCode.Num());
		return (ERigVMOpCode)ByteCode[InByteCodeIndex];
	}

	// returns the size of the operator in bytes at a given byte index
	uint64 GetOpNumBytesAt(uint64 InByteCodeIndex, bool bIncludeOperands) const;

	// returns an operator at a given byte code index
	template<class OpType>
	FORCEINLINE const OpType& GetOpAt(uint64 InByteCodeIndex) const
	{
		ensure(InByteCodeIndex >= 0 && InByteCodeIndex <= ByteCode.Num() - sizeof(OpType));
		return *(const OpType*)(ByteCode.GetData() + InByteCodeIndex);
	}

	// returns an operator for a given instruction
	template<class OpType>
	FORCEINLINE const OpType& GetOpAt(const FRigVMInstruction& InInstruction) const
	{
		return GetOpAt<OpType>(InInstruction.ByteCodeIndex);
	}

	// returns an operator at a given byte code index
	template<class OpType>
	FORCEINLINE OpType& GetOpAt(uint64 InByteCodeIndex)
	{
		ensure(InByteCodeIndex >= 0 && InByteCodeIndex <= ByteCode.Num() - sizeof(OpType));
		return *(OpType*)(ByteCode.GetData() + InByteCodeIndex);
	}

	// returns an operator for a given instruction
	template<class OpType>
	FORCEINLINE OpType& GetOpAt(const FRigVMInstruction& InInstruction)
	{
		return GetOpAt<OpType>(InInstruction.ByteCodeIndex);
	}

	// returns a list of operands at a given byte code index
	FORCEINLINE FRigVMOperandArray GetOperandsAt(uint64 InByteCodeIndex, uint16 InArgumentCount) const
	{
		ensure(InByteCodeIndex >= 0 && InByteCodeIndex <= ByteCode.Num() - sizeof(FRigVMOperand) * InArgumentCount);
		return FRigVMOperandArray((FRigVMOperand*)(ByteCode.GetData() + InByteCodeIndex), InArgumentCount);
	}

	// returns the operands for a given execute instruction
	FORCEINLINE_DEBUGGABLE FRigVMOperandArray GetOperandsForExecuteOp(const FRigVMInstruction& InInstruction) const
	{
		uint64 ByteCodeIndex = InInstruction.ByteCodeIndex;
		const FRigVMExecuteOp& ExecuteOp = GetOpAt<FRigVMExecuteOp>(ByteCodeIndex);
		// if the bytecode is not aligned the OperandAlignment needs to be 0
		ensure(bByteCodeIsAligned || InInstruction.OperandAlignment == 0);
		ByteCodeIndex += sizeof(FRigVMExecuteOp) + (uint64)InInstruction.OperandAlignment;
		return GetOperandsAt(ByteCodeIndex, ExecuteOp.GetOperandCount());
	}

	// returns all of the operands for a given instruction
	FRigVMOperandArray GetOperandsForOp(const FRigVMInstruction& InInstruction) const;

	// returns all of the operands for a given instruction
	TArray<int32> GetInstructionsForOperand(const FRigVMOperand& InOperand) const;

	// returns true if the operator in question is used by multiple instructions
	bool IsOperandShared(const FRigVMOperand& InOperand) const { return GetInstructionsForOperand(InOperand).Num() > 1; }

	// returns the raw data of the byte code
	FORCEINLINE const TArrayView<const uint8> GetByteCode() const
	{
		const uint8* Data = ByteCode.GetData();
		return TArrayView<const uint8>((uint8*)Data, ByteCode.Num());
	}

	// returns the statistics information
	FRigVMByteCodeStatistics GetStatistics() const
	{
		FRigVMByteCodeStatistics Statistics;
		Statistics.InstructionCount = GetInstructions().Num();
		Statistics.DataBytes = ByteCode.GetAllocatedSize();
		return Statistics;
	}

	// returns the number of instructions within this byte code
	int32 GetNumInstructions() const { return NumInstructions; }

	// returns the alignment for an operator given its opcode
	uint64 GetOpAlignment(ERigVMOpCode InOpCode) const;

	// returns the alignment for an operand
	uint64 GetOperandAlignment() const;

	FString DumpToText() const;

#if WITH_EDITOR

	// returns the subject which was used to inject a given instruction
	UObject* GetSubjectForInstruction(int32 InInstructionIndex) const;

	// returns the first hit instruction index for a given subject (or INDEX_NONE)
	int32 GetFirstInstructionIndexForSubject(UObject* InSubject) const;

	// returns all found instruction indices for a given subject
	const TArray<int32>& GetAllInstructionIndicesForSubject(UObject* InSubject) const;

	// returns the callpath which was used to inject a given instruction
	FString GetCallPathForInstruction(int32 InInstructionIndex) const;

	// returns the first hit instruction index for a given callpath (or INDEX_NONE)
	int32 GetFirstInstructionIndexForCallPath(const FString& InCallPath, bool bStartsWith = false, bool bEndsWith = false) const;

	// returns all found instruction indices for a given callpath
	TArray<int32> GetAllInstructionIndicesForCallPath(const FString& InCallPath, bool bStartsWith = false, bool bEndsWith = false) const;

	// returns the first hit instruction index for a given callpath (or INDEX_NONE)
	int32 GetFirstInstructionIndexForCallstack(const TArray<UObject*>& InCallstack) const;

	// returns all found instruction indices for a given callpath
	const TArray<int32>& GetAllInstructionIndicesForCallstack(const TArray<UObject*>& InCallstack) const;

	// returns the callstack which was used to inject a given instruction
	const TArray<UObject*>* GetCallstackForInstruction(int32 InInstructionIndex) const;

	// returns the callstack hash which was used to inject a given instruction
	uint32 GetCallstackHashForInstruction(int32 InInstructionIndex) const;

	// computes a hash for a given callstack
	static uint32 GetCallstackHash(const TArray<UObject*>& InCallstack);
	static uint32 GetCallstackHash(const TArrayView<UObject* const>& InCallstack);

	// returns the input operands of a given instruction
	FORCEINLINE_DEBUGGABLE FRigVMOperandArray GetInputOperands(int32 InInstructionIndex) const
	{
		if(InputOperandsPerInstruction.IsValidIndex(InInstructionIndex))
		{
			if(InputOperandsPerInstruction[InInstructionIndex].Num() > 0)
			{
				return FRigVMOperandArray((FRigVMOperand*)(InputOperandsPerInstruction[InInstructionIndex].GetData()), InputOperandsPerInstruction[InInstructionIndex].Num());
			}
		}
		return FRigVMOperandArray();
	}

	// returns the output operands of a given instruction
	FORCEINLINE_DEBUGGABLE FRigVMOperandArray GetOutputOperands(int32 InInstructionIndex) const
	{
		if(OutputOperandsPerInstruction.IsValidIndex(InInstructionIndex))
		{
			if(OutputOperandsPerInstruction[InInstructionIndex].Num() > 0)
			{
				return FRigVMOperandArray((FRigVMOperand*)(OutputOperandsPerInstruction[InInstructionIndex].GetData()), OutputOperandsPerInstruction[InInstructionIndex].Num());
			}
		}
		return FRigVMOperandArray();
	}

	void SetOperandsForInstruction(int32 InInstructionIndex, const FRigVMOperandArray& InputOperands, const FRigVMOperandArray& OutputOperands);

#endif

private:

	template<class OpType>
	FORCEINLINE_DEBUGGABLE uint64 AddOp(const OpType& InOp)
	{
		uint64 ByteIndex = (uint64)ByteCode.AddZeroed(sizeof(OpType));
		uint8* Pointer = &ByteCode[ByteIndex];
		FMemory::Memcpy(Pointer, &InOp, sizeof(OpType));
		NumInstructions++;
		return ByteIndex;
	}

	void AlignByteCode();

	// memory for all instructions
	UPROPERTY()
	TArray<uint8> ByteCode;

	// number of instructions stored here
	UPROPERTY()
	int32 NumInstructions;
	
#if WITH_EDITORONLY_DATA

	TArray<UObject*> SubjectPerInstruction;
	TMap<UObject*, TArray<int32>> SubjectToInstructions;
	TArray<FString> CallPathPerInstruction;
	TMap<FString, TArray<int32>> CallPathToInstructions;
	TArray<TArray<UObject*>> CallstackPerInstruction;
	TMap<uint32, TArray<int32>> CallstackHashToInstructions;
	TArray<uint32> CallstackHashPerInstruction;
	TArray<TArray<FRigVMOperand>> InputOperandsPerInstruction;
	TArray<TArray<FRigVMOperand>> OutputOperandsPerInstruction;

#endif

#if WITH_EDITOR

	void SetSubject(int32 InInstructionIndex, const FString& InCallPath, const TArray<UObject*>& InCallstack);

#endif

	// a look up table from entry name to instruction index
	UPROPERTY()
	TArray<FRigVMByteCodeEntry> Entries;

	// if this is set to true the stored bytecode is aligned / padded
	bool bByteCodeIsAligned;

	static TArray<int32> EmptyInstructionIndices;

	friend class URigVMCompiler;
	friend class FRigVMByteCodeTest;
	friend class FRigVMInvokeEntryTest;
	friend class URigVM;
};
