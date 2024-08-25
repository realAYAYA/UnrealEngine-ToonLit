// Copyright Epic Games, Inc. All Rights Reserved.

#include "MuT/ASTOpConstantString.h"

#include "HAL/PlatformMath.h"
#include "MuR/ModelPrivate.h"
#include "MuR/RefCounted.h"
#include "MuR/Types.h"


namespace mu
{


	//-------------------------------------------------------------------------------------------------
	//-------------------------------------------------------------------------------------------------
	//-------------------------------------------------------------------------------------------------
	void ASTOpConstantString::ForEachChild(const TFunctionRef<void(ASTChild&)>)
	{
	}


	uint64 ASTOpConstantString::Hash() const
	{
		uint64 res = std::hash<int32>()(value.Len());
		return res;
	}


	bool ASTOpConstantString::IsEqual(const ASTOp& otherUntyped) const
	{
		if (otherUntyped.GetOpType() == GetOpType())
		{
			const ASTOpConstantString* other = static_cast<const ASTOpConstantString*>(&otherUntyped);
			return value == other->value;
		}
		return false;
	}


	mu::Ptr<ASTOp> ASTOpConstantString::Clone(MapChildFuncRef) const
	{
		Ptr<ASTOpConstantString> n = new ASTOpConstantString();
		n->value = value;
		return n;
	}


	void ASTOpConstantString::Link(FProgram& program, FLinkerOptions*)
	{
		if (!linkedAddress)
		{
			OP::ResourceConstantArgs args;
			FMemory::Memset(&args, 0, sizeof(args));
			args.value = program.AddConstant(value);

			linkedAddress = (OP::ADDRESS)program.m_opAddress.Num();
			program.m_opAddress.Add((uint32_t)program.m_byteCode.Num());
			AppendCode(program.m_byteCode, OP_TYPE::ST_CONSTANT);
			AppendCode(program.m_byteCode, args);
		}
	}

}