// Copyright Epic Games, Inc. All Rights Reserved.

#include "MuT/ASTOpImageRasterMesh.h"

#include "MuT/ASTOpSwitch.h"
#include "MuT/ASTOpImageSwizzle.h"
#include "MuT/ASTOpMeshMorph.h"
#include "MuT/ASTOpConditional.h"
#include "MuT/ASTOpSwitch.h"
#include "Containers/Map.h"
#include "HAL/PlatformMath.h"
#include "MuR/ModelPrivate.h"
#include "MuR/RefCounted.h"
#include "MuR/Types.h"
#include "MuT/StreamsPrivate.h"


namespace mu
{


	ASTOpImageRasterMesh::ASTOpImageRasterMesh()
		: mesh(this)
		, image(this)
		, angleFadeProperties(this)
		, mask(this)
		, projector(this)
	{
		blockIndex = -1;
		sizeX = 0;
		sizeY = 0;
		bIsRGBFadingEnabled = 1;
		bIsAlphaFadingEnabled = 1;
	}


	//-------------------------------------------------------------------------------------------------
	ASTOpImageRasterMesh::~ASTOpImageRasterMesh()
	{
		// Explicit call needed to avoid recursive destruction
		ASTOp::RemoveChildren();
	}


	//-------------------------------------------------------------------------------------------------
	bool ASTOpImageRasterMesh::IsEqual(const ASTOp& InOtherUntyped) const
	{
		if (const ASTOpImageRasterMesh* Other = dynamic_cast<const ASTOpImageRasterMesh*>(&InOtherUntyped))
		{
			return mesh == Other->mesh &&
				image == Other->image &&
				angleFadeProperties == Other->angleFadeProperties &&
				mask == Other->mask &&
				projector == Other->projector &&
				blockIndex == Other->blockIndex &&
				sizeX == Other->sizeX &&
				sizeY == Other->sizeY &&
				bIsRGBFadingEnabled == Other->bIsRGBFadingEnabled &&
				bIsAlphaFadingEnabled == Other->bIsAlphaFadingEnabled;
		}
		return false;
	}


	//-------------------------------------------------------------------------------------------------
	uint64 ASTOpImageRasterMesh::Hash() const
	{
		uint64 res = std::hash<OP_TYPE>()(GetOpType());
		hash_combine(res, mesh.child().get());
		hash_combine(res, image.child().get());
		hash_combine(res, angleFadeProperties.child().get());
		hash_combine(res, mask.child().get());
		hash_combine(res, projector.child().get());
		hash_combine(res, blockIndex);
		return res;
	}


	//-------------------------------------------------------------------------------------------------
	mu::Ptr<ASTOp> ASTOpImageRasterMesh::Clone(MapChildFuncRef mapChild) const
	{
		Ptr<ASTOpImageRasterMesh> n = new ASTOpImageRasterMesh();
		n->mesh = mapChild(mesh.child());
		n->image = mapChild(image.child());
		n->angleFadeProperties = mapChild(angleFadeProperties.child());
		n->mask = mapChild(mask.child());
		n->projector = mapChild(projector.child());
		n->blockIndex = blockIndex;
		n->sizeX = sizeX;
		n->sizeY = sizeY;
		n->bIsRGBFadingEnabled = bIsRGBFadingEnabled;
		n->bIsAlphaFadingEnabled = bIsAlphaFadingEnabled;
		return n;
	}


	//-------------------------------------------------------------------------------------------------
	void ASTOpImageRasterMesh::ForEachChild(const TFunctionRef<void(ASTChild&)> f)
	{
		f(mesh);
		f(image);
		f(angleFadeProperties);
		f(mask);
		f(projector);
	}


	//-------------------------------------------------------------------------------------------------
	void ASTOpImageRasterMesh::Link(FProgram& program, const FLinkerOptions*)
	{
		// Already linked?
		if (!linkedAddress)
		{
			OP::ImageRasterMeshArgs args;
			FMemory::Memzero(&args, sizeof(args));

			args.blockIndex = blockIndex;
			args.sizeX = sizeX;
			args.sizeY = sizeY;
			args.bIsRGBFadingEnabled = bIsRGBFadingEnabled;
			args.bIsAlphaFadingEnabled = bIsAlphaFadingEnabled;

			if (mesh) args.mesh = mesh->linkedAddress;
			if (image) args.image = image->linkedAddress;
			if (angleFadeProperties) args.angleFadeProperties = angleFadeProperties->linkedAddress;
			if (mask) args.mask = mask->linkedAddress;
			if (projector) args.projector = projector->linkedAddress;

			linkedAddress = (OP::ADDRESS)program.m_opAddress.Num();
			program.m_opAddress.Add((uint32)program.m_byteCode.Num());
			AppendCode(program.m_byteCode, GetOpType());
			AppendCode(program.m_byteCode, args);
		}
	}


	//-------------------------------------------------------------------------------------------------
	FImageDesc ASTOpImageRasterMesh::GetImageDesc(bool returnBestOption, FGetImageDescContext* context) const 
	{
		FImageDesc res;

		// Local context in case it is necessary
		FGetImageDescContext localContext;
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
		if (image)
		{
			res = image->GetImageDesc( returnBestOption, context);
			res.m_size[0] = sizeX;
			res.m_size[1] = sizeY;
		}


		// Cache the result
		if (context)
		{
			context->m_results.Add(this, res);
		}

		return res;
	}


	//-------------------------------------------------------------------------------------------------
	Ptr<ImageSizeExpression> ASTOpImageRasterMesh::GetImageSizeExpression() const
	{
		Ptr<ImageSizeExpression> pRes = new ImageSizeExpression;
		pRes->type = ImageSizeExpression::ISET_CONSTANT;
		pRes->size[0] = sizeX ? sizeX : 256;
		pRes->size[1] = sizeY ? sizeY : 256;
		return pRes;
	}


	//---------------------------------------------------------------------------------------------
	Ptr<ASTOp> ASTOpImageRasterMesh::OptimiseSemantic(const FModelOptimizationOptions& options) const
	{
		Ptr<ASTOp> at;

		// TODO

		return at;
	}


	//-------------------------------------------------------------------------------------------------
	Ptr<ASTOp> ASTOpImageRasterMesh::OptimiseSink(const FModelOptimizationOptions& options, FOptimizeSinkContext& context) const
	{
		Ptr<ASTOp> at;

		Ptr<ASTOp> OriginalAt = at;
		Ptr<ASTOp> sourceAt = mesh.child();
		Ptr<ASTOp> imageAt = image.child();

		OP_TYPE sourceType = sourceAt->GetOpType();
		switch (sourceType)
		{

		case OP_TYPE::ME_PROJECT:
		{
			// If we are rastering just the UV layout (to create a mask) we don't care about
			// mesh project operations, which modify only the positions.
			// This optimisation helps with states removing fake dependencies on projector
			// parameters that may be runtime.
			if (!imageAt)
			{
				// We remove the project from the raster children
				// \todo: maybe this clone is not necessary
				const ASTOpFixed* typedSource = dynamic_cast<const ASTOpFixed*>(sourceAt.get());
				Ptr<ASTOpImageRasterMesh> nop = mu::Clone<ASTOpImageRasterMesh>(this);
				nop->mesh = typedSource->children[typedSource->op.args.MeshProject.mesh].child();
				at = nop;
			}
			break;
		}

		case OP_TYPE::ME_INTERPOLATE:
		{
			auto typedSource = dynamic_cast<const ASTOpFixed*>(sourceAt.get());
			Ptr<ASTOpImageRasterMesh> rasterOp = mu::Clone<ASTOpImageRasterMesh>(this);
			rasterOp->mesh = typedSource->children[typedSource->op.args.MeshInterpolate.base].child();
			at = rasterOp;
			break;
		}

		case OP_TYPE::ME_MORPH2:
		{
			const ASTOpMeshMorph* typedSource = dynamic_cast<const ASTOpMeshMorph*>(sourceAt.get());
			Ptr<ASTOpImageRasterMesh> rasterOp = mu::Clone<ASTOpImageRasterMesh>(this);
			rasterOp->mesh = typedSource->Base.child();
			at = rasterOp;
			break;
		}

		case OP_TYPE::ME_CONDITIONAL:
		{
			auto nop = mu::Clone<ASTOpConditional>(sourceAt.get());
			nop->type = OP_TYPE::IM_CONDITIONAL;

			Ptr<ASTOpImageRasterMesh> aOp = mu::Clone<ASTOpImageRasterMesh>(this);
			aOp->mesh = nop->yes.child();
			nop->yes = aOp;

			Ptr<ASTOpImageRasterMesh> bOp = mu::Clone<ASTOpImageRasterMesh>(this);
			bOp->mesh = nop->no.child();
			nop->no = bOp;

			at = nop;
			break;
		}

		case OP_TYPE::ME_SWITCH:
		{
			// Make an image for every path
			auto nop = mu::Clone<ASTOpSwitch>(sourceAt.get());
			nop->type = OP_TYPE::IM_SWITCH;

			if (nop->def)
			{
				Ptr<ASTOpImageRasterMesh> defOp = mu::Clone<ASTOpImageRasterMesh>(this);
				defOp->mesh = nop->def.child();
				nop->def = defOp;
			}

			// We need to copy the options because we change them
			for (size_t o = 0; o < nop->cases.Num(); ++o)
			{
				if (nop->cases[o].branch)
				{
					Ptr<ASTOpImageRasterMesh> bOp = mu::Clone<ASTOpImageRasterMesh>(this);
					bOp->mesh = nop->cases[o].branch.child();
					nop->cases[o].branch = bOp;
				}
			}

			at = nop;
			break;
		}

		default:
			break;
		}

		// If we didn't optimize the mesh child, try to optimize the image child.
		if (OriginalAt == at && imageAt)
		{
			OP_TYPE imageType = imageAt->GetOpType();
			switch (imageType)
			{

				// TODO: Implement for image conditionals.
				//case OP_TYPE::ME_CONDITIONAL:
				//{
				//	auto nop = mu::Clone<ASTOpConditional>(sourceAt.get());
				//	nop->type = OP_TYPE::IM_CONDITIONAL;

				//	Ptr<ASTOpFixed> aOp = mu::Clone<ASTOpFixed>(this);
				//	aOp->SetChild(aOp->op.args.ImageRasterMesh.mesh, nop->yes);
				//	nop->yes = aOp;

				//	Ptr<ASTOpFixed> bOp = mu::Clone<ASTOpFixed>(this);
				//	bOp->SetChild(bOp->op.args.ImageRasterMesh.mesh, nop->no);
				//	nop->no = bOp;

				//	at = nop;
				//	break;
				//}

			case OP_TYPE::IM_SWITCH:
			{
				// TODO: Do this only if the projector is constant?

				// Make a project for every path
				auto nop = mu::Clone<ASTOpSwitch>(imageAt.get());

				if (nop->def)
				{
					Ptr<ASTOpImageRasterMesh> defOp = mu::Clone<ASTOpImageRasterMesh>(this);
					defOp->image = nop->def.child();
					nop->def = defOp;
				}

				// We need to copy the options because we change them
				for (size_t o = 0; o < nop->cases.Num(); ++o)
				{
					if (nop->cases[o].branch)
					{
						Ptr<ASTOpImageRasterMesh> bOp = mu::Clone<ASTOpImageRasterMesh>(this);
						bOp->image = nop->cases[o].branch.child();
						nop->cases[o].branch = bOp;
					}
				}

				at = nop;
				break;
			}

			default:
				break;
			}
		}

		return at;
	}

}
