// Copyright Epic Games, Inc. All Rights Reserved.

#include "MuT/ASTOpImageTransform.h"

#include "Containers/Map.h"
#include "HAL/PlatformMath.h"
#include "MuR/ModelPrivate.h"
#include "MuR/RefCounted.h"
#include "MuR/Types.h"
#include "MuT/StreamsPrivate.h"


namespace mu
{

	//-------------------------------------------------------------------------------------------------
	ASTOpImageTransform::ASTOpImageTransform()
		: Base(this)
		, OffsetX(this)
		, OffsetY(this)
		, ScaleX(this)
		, ScaleY(this)
		, Rotation(this)
	{
	}


	//-------------------------------------------------------------------------------------------------
	ASTOpImageTransform::~ASTOpImageTransform()
	{
		// Explicit call needed to avoid recursive destruction
		ASTOp::RemoveChildren();
	}


	//-------------------------------------------------------------------------------------------------
	bool ASTOpImageTransform::IsEqual(const ASTOp& OtherUntyped) const
	{
		if (OtherUntyped.GetOpType()==GetOpType())
		{
			auto Other = static_cast<const ASTOpImageTransform*>(&OtherUntyped);
			return 
				Base 	 == Other->Base &&
				OffsetX  == Other->OffsetX &&
				OffsetY  == Other->OffsetY &&
				ScaleX 	 == Other->ScaleX &&
				ScaleY 	 == Other->ScaleY &&
				Rotation == Other->Rotation &&
				AddressMode == Other->AddressMode &&
				SizeX == Other->SizeX &&
				SizeY == Other->SizeY &&
				SourceSizeX == Other->SourceSizeX &&
				SourceSizeY == Other->SourceSizeY &&
				bKeepAspectRatio == Other->bKeepAspectRatio;
		}
		return false;
	}


	//-------------------------------------------------------------------------------------------------
	uint64 ASTOpImageTransform::Hash() const
	{
		uint64 Res = std::hash<OP_TYPE>()(OP_TYPE::IM_TRANSFORM);
		hash_combine(Res, Base.child().get());
		hash_combine(Res, OffsetX.child().get());
		hash_combine(Res, OffsetY.child().get());
		hash_combine(Res, ScaleX.child().get());
		hash_combine(Res, ScaleY.child().get());
		hash_combine(Res, Rotation.child().get());
		hash_combine(Res, std::hash<uint32>()(static_cast<uint32>(AddressMode)));
		hash_combine(Res, SizeX);
		hash_combine(Res, SizeY);
		hash_combine(Res, SourceSizeX);
		hash_combine(Res, SourceSizeY);
		hash_combine(Res, bKeepAspectRatio);
		return Res;
	}


	//-------------------------------------------------------------------------------------------------
	mu::Ptr<ASTOp> ASTOpImageTransform::Clone(MapChildFuncRef MapChild) const
	{
		Ptr<ASTOpImageTransform> NewOp = new ASTOpImageTransform();
		NewOp->Base     = MapChild(Base.child());
		NewOp->OffsetX  = MapChild(OffsetX.child());
		NewOp->OffsetY  = MapChild(OffsetY.child());
		NewOp->ScaleX   = MapChild(ScaleX.child());
		NewOp->ScaleY   = MapChild(ScaleY.child());
		NewOp->Rotation = MapChild(Rotation.child());
		NewOp->AddressMode = AddressMode;
		NewOp->SizeX = SizeX;
		NewOp->SizeY = SizeY;
		NewOp->SourceSizeX = SourceSizeX;
		NewOp->SourceSizeY = SourceSizeY;
		NewOp->bKeepAspectRatio = bKeepAspectRatio;
		return NewOp;
	}


	//-------------------------------------------------------------------------------------------------
	void ASTOpImageTransform::ForEachChild(const TFunctionRef<void(ASTChild&)> Func)
	{
		Func(Base);
		Func(OffsetX);
		Func(OffsetY);
		Func(ScaleX);
		Func(ScaleY);
		Func(Rotation);
	}


	//-------------------------------------------------------------------------------------------------
	void ASTOpImageTransform::Link(FProgram& Program, FLinkerOptions*)
	{
		// Already linked?
		if (!linkedAddress)
		{
			OP::ImageTransformArgs Args;
			FMemory::Memzero(Args);

			Args.Base     = Base     ? Base->linkedAddress     : 0;
			Args.OffsetX  = OffsetX  ? OffsetX->linkedAddress  : 0;
			Args.OffsetY  = OffsetY  ? OffsetY->linkedAddress  : 0;
			Args.ScaleX   = ScaleX   ? ScaleX->linkedAddress   : 0;
			Args.ScaleY   = ScaleY   ? ScaleY->linkedAddress   : 0;
			Args.Rotation = Rotation ? Rotation->linkedAddress : 0;
			Args.AddressMode = static_cast<uint32>(AddressMode);
			Args.bKeepAspectRatio = bKeepAspectRatio;
			Args.SizeX = SizeX;
			Args.SizeY = SizeY;
			Args.SourceSizeX = SourceSizeX;
			Args.SourceSizeY = SourceSizeY;

			linkedAddress = (OP::ADDRESS)Program.m_opAddress.Num();
			Program.m_opAddress.Add((uint32_t)Program.m_byteCode.Num());
			AppendCode(Program.m_byteCode, OP_TYPE::IM_TRANSFORM);
			AppendCode(Program.m_byteCode, Args);
		}
	}

	//-------------------------------------------------------------------------------------------------
	FImageDesc ASTOpImageTransform::GetImageDesc(bool bReturnBestOption, FGetImageDescContext* Context) const
	{
		FImageDesc Result;

		// Local context in case it is necessary
		FGetImageDescContext LocalContext;
		if (!Context)
		{
			Context = &LocalContext;
		}
		else
		{
			// Cached result?
			FImageDesc* PtrValue = Context->m_results.Find(this);
			if (PtrValue)
			{
				return *PtrValue;
			}
		}


		// Actual work
		if (Base)
		{
			Result = Base->GetImageDesc(bReturnBestOption, Context);
			
			Result.m_format = GetUncompressedFormat(Result.m_format); 
			Result.m_lods = 1;
			
			if (!(SizeX == 0 && SizeY == 0))
			{
				Result.m_size = FImageSize(SizeX, SizeY);
			}
		}


		// Cache the result
		if (Context)
		{
			Context->m_results.Add(this, Result);
		}

		return Result;
	}


	//-------------------------------------------------------------------------------------------------
	void ASTOpImageTransform::GetLayoutBlockSize(int* pBlockX, int* pBlockY)
	{
		if (Base)
		{
			Base->GetLayoutBlockSize(pBlockX, pBlockY);
		}
	}


	//-------------------------------------------------------------------------------------------------
	mu::Ptr<ImageSizeExpression> ASTOpImageTransform::GetImageSizeExpression() const
	{
		if (Base)
		{
			if (!(SizeX == 0 && SizeY == 0))
			{
				Ptr<ImageSizeExpression> SizeExpr = new ImageSizeExpression;
				SizeExpr->type = ImageSizeExpression::ISET_CONSTANT;
				SizeExpr->size[0] = SizeX;
				SizeExpr->size[1] = SizeY;

				return SizeExpr;
			}
			else
			{
				return Base->GetImageSizeExpression();
			}
		}

		return nullptr;
	}

}
