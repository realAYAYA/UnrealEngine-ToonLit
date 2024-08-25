// Copyright Epic Games, Inc. All Rights Reserved.

#include "MuT/ASTOpConstantExtensionData.h"

#include "HAL/UnrealMemory.h"
#include "MuR/ModelPrivate.h"
#include "MuT/StreamsPrivate.h"


namespace mu
{


//-------------------------------------------------------------------------------------------------
bool ASTOpConstantExtensionData::IsEqual(const ASTOp& OtherUntyped) const
{
	if (OtherUntyped.GetOpType() == GetOpType())
	{
		const ASTOpConstantExtensionData* Other = static_cast<const ASTOpConstantExtensionData*>(&OtherUntyped);
		return Other->Value == Value;
	}

	return false;
}


//-------------------------------------------------------------------------------------------------
mu::Ptr<ASTOp> ASTOpConstantExtensionData::Clone(MapChildFuncRef MapChild) const
{
	Ptr<ASTOpConstantExtensionData> Result = new ASTOpConstantExtensionData();

	Result->Value = Value;

	return Result;
}


//-------------------------------------------------------------------------------------------------
uint64 ASTOpConstantExtensionData::Hash() const
{
	return std::hash<uint64>()(Value.get() != nullptr ? Value->Hash() : 0ll);
}


//-------------------------------------------------------------------------------------------------
void ASTOpConstantExtensionData::Link(FProgram& Program, FLinkerOptions* Options)
{
	if (!linkedAddress)
	{
		OP::ResourceConstantArgs Args;
		FMemory::Memset(Args, 0);

		Args.value = Program.AddConstant(Value);

		linkedAddress = (OP::ADDRESS)Program.m_opAddress.Num();
		Program.m_opAddress.Add((uint32_t)Program.m_byteCode.Num());
		AppendCode(Program.m_byteCode, OP_TYPE::ED_CONSTANT);
		AppendCode(Program.m_byteCode, Args);
	}
}

}