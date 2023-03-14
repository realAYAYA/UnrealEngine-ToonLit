// Copyright Epic Games, Inc. All Rights Reserved.

#include "MuT/ASTOpImageCompose.h"

#include "Containers/Array.h"
#include "Containers/Map.h"
#include "HAL/PlatformMath.h"
#include "MuR/Layout.h"
#include "MuR/MemoryPrivate.h"
#include "MuR/ModelPrivate.h"
#include "MuR/MutableMath.h"
#include "MuR/MutableTrace.h"
#include "MuR/RefCounted.h"
#include "MuR/Types.h"
#include "MuT/ASTOpConstantResource.h"
#include "MuT/ASTOpImagePixelFormat.h"
#include "MuT/StreamsPrivate.h"

#include <memory>
#include <utility>


namespace mu
{


//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
ASTOpImageCompose::ASTOpImageCompose()
    : Layout(this)
    , Base(this)
	, BlockImage(this)
	, Mask(this)
{
}


//-------------------------------------------------------------------------------------------------
ASTOpImageCompose::~ASTOpImageCompose()
{
    // Explicit call needed to avoid recursive destruction
    ASTOp::RemoveChildren();
}


//-------------------------------------------------------------------------------------------------
bool ASTOpImageCompose::IsEqual(const ASTOp& otherUntyped) const
{
    if ( const ASTOpImageCompose* other = dynamic_cast<const ASTOpImageCompose*>(&otherUntyped) )
    {
        return Layout ==other->Layout &&
			Base ==other->Base &&
			BlockImage == other->BlockImage &&
			Mask == other->Mask &&
			BlockIndex == other->BlockIndex;
    }
    return false;
}


//-------------------------------------------------------------------------------------------------
uint64 ASTOpImageCompose::Hash() const
{
	uint64 res = std::hash<OP_TYPE>()(OP_TYPE::IM_COMPOSE);
    hash_combine( res, Layout.child().get() );
	hash_combine(res, Base.child().get());
	hash_combine(res, BlockImage.child().get());
	hash_combine(res, Mask.child().get());
	hash_combine(res, BlockIndex);
	return res;
}


//-------------------------------------------------------------------------------------------------
mu::Ptr<ASTOp> ASTOpImageCompose::Clone(MapChildFuncRef mapChild) const
{
	mu::Ptr<ASTOpImageCompose> n = new ASTOpImageCompose();
    n->Layout = mapChild(Layout.child());
	n->Base = mapChild(Base.child());
	n->BlockImage = mapChild(BlockImage.child());
	n->Mask = mapChild(Mask.child());
	n->BlockIndex = BlockIndex;
    return n;
}


//-------------------------------------------------------------------------------------------------
void ASTOpImageCompose::ForEachChild(const TFunctionRef<void(ASTChild&)> f )
{
	f(Layout);
	f(Base);
	f(BlockImage);
	f(Mask);
}


//-------------------------------------------------------------------------------------------------
void ASTOpImageCompose::Link( PROGRAM& program, const FLinkerOptions*)
{
    // Already linked?
    if (!linkedAddress)
    {
        OP::ImageComposeArgs args;
        memset( &args,0, sizeof(args) );

		if (Layout) args.layout = Layout->linkedAddress;
		if (Base) args.base = Base->linkedAddress;
		if (BlockImage) args.blockImage = BlockImage->linkedAddress;
		if (Mask) args.mask = Mask->linkedAddress;
        args.blockIndex = BlockIndex;

        linkedAddress = (OP::ADDRESS)program.m_opAddress.Num();
        program.m_opAddress.Add((uint32_t)program.m_byteCode.Num());
        AppendCode(program.m_byteCode,OP_TYPE::IM_COMPOSE);
        AppendCode(program.m_byteCode,args);
    }

}


FImageDesc ASTOpImageCompose::GetImageDesc( bool returnBestOption, GetImageDescContext* context )
{
    FImageDesc res;

    // Local context in case it is necessary
    GetImageDescContext localContext;
    if (!context)
    {
      context = &localContext;
    }
    else
    {
        // Cached result?
		FImageDesc* PtrValue = context->m_results.Find(this);
		if (PtrValue)
		{
			return *PtrValue;
		}
	}

    // Actual work
    if (Base)
    {
        res = Base->GetImageDesc( returnBestOption, context );
    }


    // Cache the result
    if (context)
    {
		context->m_results.Add(this, res);
	}

    return res;
}


mu::Ptr<ImageSizeExpression> ASTOpImageCompose::GetImageSizeExpression() const
{
    if ( Base )
    {
        return Base->GetImageSizeExpression();
    }

    return nullptr;
}


bool ASTOpImageCompose::IsImagePlainConstant(vec4<float>& colour) const
{
	bool res = false;

	if (BlockImage.child())
	{
		BlockImage->IsImagePlainConstant(colour);
	}

	if (res && Base.child())
	{
		vec4<float> baseColour;
		res = Base->IsImagePlainConstant(baseColour);
		res &= (colour == baseColour);
	}

	return res;
}


void ASTOpImageCompose::GetLayoutBlockSize(int* pBlockX, int* pBlockY)
{
	// We can find out in two ways: following the block image, or following the base image.
	// Let's try the block approach: We need the block size and the layout blocks
	int layoutBlocksX = 0;
	int layoutBlocksY = 0;
	if (Layout.child())
	{
		MUTABLE_CPUPROFILER_SCOPE(GetLayoutBlockSize_GetBlockLayoutSize);
		BLOCK_LAYOUT_SIZE_CACHE cache;
		Layout->GetBlockLayoutSizeCached(BlockIndex,
			&layoutBlocksX, &layoutBlocksY,
			&cache);
	}

	if (layoutBlocksX > 0 && layoutBlocksY > 0 && BlockImage.child())
	{
		FImageDesc blockDesc = BlockImage->GetImageDesc();

		*pBlockX = blockDesc.m_size[0] / layoutBlocksX;
		*pBlockY = blockDesc.m_size[1] / layoutBlocksY;
	}
	else
	{
		*pBlockX = 0;
		*pBlockY = 0;
	}
}


mu::Ptr<ASTOp> ASTOpImageCompose::OptimiseSemantic(const MODEL_OPTIMIZATION_OPTIONS& options) const
{
	mu::Ptr<ASTOp> at;

	auto baseAt = Base.child();
	auto blockAt = BlockImage.child();
	auto layoutAt = Layout.child();
	if (layoutAt
		&&
		layoutAt->GetOpType() == OP_TYPE::LA_CONSTANT
		&&
		baseAt
		&&
		blockAt)
	{
		auto typedLayout = dynamic_cast<const ASTOpConstantResource*>(layoutAt.get());
		mu::Ptr<const mu::Layout> pLayout = static_cast<const mu::Layout*>(typedLayout->GetValue().get());

		// Constant single-block full layout?
		if (pLayout->GetBlockCount() == 1
			&&
			pLayout->m_blocks[0].m_min == vec2<uint16>(0, 0)
			&&
			pLayout->m_blocks[0].m_size == pLayout->m_size
			&&
			pLayout->m_blocks[0].m_id == int(BlockIndex)
			)
		{
			// We could only take the block, but we must make sure it will have the format
			// and size of the base.
			auto baseDesc = baseAt->GetImageDesc(true);
			auto blockDesc = blockAt->GetImageDesc(true);

			at = blockAt;

			// \todo isn't this a common operation?
			if (baseDesc.m_format != blockDesc.m_format
				&&
				baseDesc.m_format != EImageFormat::IF_NONE)
			{
				mu::Ptr<ASTOpImagePixelFormat> reformat = new ASTOpImagePixelFormat;
				reformat->Format = baseDesc.m_format;
				reformat->FormatIfAlpha = EImageFormat::IF_NONE;
				reformat->Source = at;
				at = reformat;
			}

			if (baseDesc.m_size != blockDesc.m_size
				&&
				baseDesc.m_size[0] != 0
				&&
				baseDesc.m_size[1] != 0)
			{
				mu::Ptr<ASTOpFixed> resize = new ASTOpFixed;
				resize->op.type = OP_TYPE::IM_RESIZE;
				resize->op.args.ImageResize.size[0] = baseDesc.m_size[0];
				resize->op.args.ImageResize.size[1] = baseDesc.m_size[1];
				resize->SetChild(resize->op.args.ImageResize.source, at);
				at = resize;
			}
		}
	}

	return at;
}

}