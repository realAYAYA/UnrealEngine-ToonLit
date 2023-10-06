// Copyright Epic Games, Inc. All Rights Reserved.

#include "MuT/ASTOpReferenceResource.h"

#include "Containers/Array.h"
#include "HAL/PlatformMath.h"
#include "Hash/CityHash.h"
#include "Misc/AssertionMacros.h"
#include "MuR/Types.h"
#include "MuT/StreamsPrivate.h"


namespace mu
{

	void ASTOpReferenceResource::ForEachChild(const TFunctionRef<void(ASTChild&)>)
	{
	}


	//-------------------------------------------------------------------------------------------------
	bool ASTOpReferenceResource::IsEqual(const ASTOp& otherUntyped) const
	{
		if (const ASTOpReferenceResource* other = dynamic_cast<const ASTOpReferenceResource*>(&otherUntyped))
		{
			return type == other->type && ID == other->ID;
		}
		return false;
	}


	//-------------------------------------------------------------------------------------------------
	mu::Ptr<ASTOp> ASTOpReferenceResource::Clone(MapChildFuncRef) const
	{
		Ptr<ASTOpReferenceResource> n = new ASTOpReferenceResource();
		n->type = type;
		n->ID = ID;
		return n;
	}


	//-------------------------------------------------------------------------------------------------
	uint64 ASTOpReferenceResource::Hash() const
	{
		uint64 res = std::hash<uint64>()(uint64(type));
		hash_combine(res, ID);
		return res;
	}


	//-------------------------------------------------------------------------------------------------
	void ASTOpReferenceResource::Link(FProgram& program, FLinkerOptions* Options)
	{
		if (!linkedAddress)
		{			
			OP::ResourceReferenceArgs Args;
			FMemory::Memset(&Args, 0, sizeof(Args));
			Args.ID = ID;

			linkedAddress = (OP::ADDRESS)program.m_opAddress.Num();
			program.m_opAddress.Add((uint32)program.m_byteCode.Num());
			AppendCode(program.m_byteCode, type);
			AppendCode(program.m_byteCode, Args);
		}
	}


	//-------------------------------------------------------------------------------------------------
	FImageDesc ASTOpReferenceResource::GetImageDesc(bool, class FGetImageDescContext*) const
	{
		FImageDesc res;
		return res;
	}


	//-------------------------------------------------------------------------------------------------
	void ASTOpReferenceResource::GetBlockLayoutSize(int blockIndex, int* pBlockX, int* pBlockY, FBlockLayoutSizeCache*)
	{
		switch (type)
		{

		case OP_TYPE::IM_REFERENCE:
		{
			*pBlockX = 0;
			*pBlockY = 0;
			break;
		}

		default:
			checkf(false, TEXT("Instruction not supported"));
		}
	}


	//-------------------------------------------------------------------------------------------------
	void ASTOpReferenceResource::GetLayoutBlockSize(int* pBlockX, int* pBlockY)
	{
		switch (type)
		{

		case OP_TYPE::IM_REFERENCE:
		{
			// We didn't find any layout.
			*pBlockX = 0;
			*pBlockY = 0;
			break;
		}

		default:
			checkf(false, TEXT("Instruction not supported"));
		}
	}


	//-------------------------------------------------------------------------------------------------
	bool ASTOpReferenceResource::GetNonBlackRect(FImageRect&) const
	{
		return false;
	}


	//-------------------------------------------------------------------------------------------------
	ASTOpReferenceResource::~ASTOpReferenceResource()
	{
	}


	//-------------------------------------------------------------------------------------------------
	mu::Ptr<ImageSizeExpression> ASTOpReferenceResource::GetImageSizeExpression() const
	{
		Ptr<ImageSizeExpression> pRes = new ImageSizeExpression;
		pRes->type = ImageSizeExpression::ISET_UNKNOWN;

		return pRes;
	}

}