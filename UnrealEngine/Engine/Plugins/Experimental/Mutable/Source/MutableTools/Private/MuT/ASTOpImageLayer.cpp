// Copyright Epic Games, Inc. All Rights Reserved.

#include "MuT/ASTOpImageLayer.h"

#include "MuT/StreamsPrivate.h"
#include "MuT/ASTOpImagePatch.h"
#include "MuT/ASTOpImageLayerColor.h"
#include "MuT/ASTOpImageSwizzle.h"
#include "MuT/ASTOpImageRasterMesh.h"
#include "MuT/ASTOpImageCrop.h"
#include "MuT/ASTOpSwitch.h"
#include "MuR/ModelPrivate.h"
#include "MuR/RefCounted.h"
#include "MuR/Types.h"

#include "Containers/Map.h"
#include "HAL/PlatformMath.h"


namespace mu
{


	ASTOpImageLayer::ASTOpImageLayer()
		: base(this)
		, blend(this)
		, mask(this)
	{
	}


	//-------------------------------------------------------------------------------------------------
	ASTOpImageLayer::~ASTOpImageLayer()
	{
		// Explicit call needed to avoid recursive destruction
		ASTOp::RemoveChildren();
	}


	//-------------------------------------------------------------------------------------------------
	bool ASTOpImageLayer::IsEqual(const ASTOp& InOtherUntyped) const
	{
		if (InOtherUntyped.GetOpType() == GetOpType())
		{
			const ASTOpImageLayer* Other = static_cast<const ASTOpImageLayer*>(&InOtherUntyped);
			return base == Other->base &&
				blend == Other->blend &&
				mask == Other->mask &&
				blendType == Other->blendType &&
				blendTypeAlpha == Other->blendTypeAlpha &&
				BlendAlphaSourceChannel == Other->BlendAlphaSourceChannel &&
				Flags == Other->Flags;
		}
		return false;
	}


	//-------------------------------------------------------------------------------------------------
	uint64 ASTOpImageLayer::Hash() const
	{
		uint64 res = std::hash<OP_TYPE>()(GetOpType());
		hash_combine(res, base.child().get());
		hash_combine(res, blend.child().get());
		hash_combine(res, mask.child().get());
		return res;
	}


	//-------------------------------------------------------------------------------------------------
	mu::Ptr<ASTOp> ASTOpImageLayer::Clone(MapChildFuncRef mapChild) const
	{
		Ptr<ASTOpImageLayer> n = new ASTOpImageLayer();
		n->base = mapChild(base.child());
		n->blend = mapChild(blend.child());
		n->mask = mapChild(mask.child());
		n->blendType = blendType;
		n->blendTypeAlpha = blendTypeAlpha;
		n->BlendAlphaSourceChannel = BlendAlphaSourceChannel;
		n->Flags = Flags;
		return n;
	}


	//-------------------------------------------------------------------------------------------------
	void ASTOpImageLayer::ForEachChild(const TFunctionRef<void(ASTChild&)> f)
	{
		f(base);
		f(blend);
		f(mask);
	}


	//-------------------------------------------------------------------------------------------------
	void ASTOpImageLayer::Link(FProgram& program, FLinkerOptions*)
	{
		// Already linked?
		if (!linkedAddress)
		{
			OP::ImageLayerArgs args;
			FMemory::Memzero(&args, sizeof(args));

			args.blendType = (uint8)blendType;
			args.blendTypeAlpha = (uint8)blendTypeAlpha;
			args.BlendAlphaSourceChannel = BlendAlphaSourceChannel;
			args.flags = Flags;

			if (base) args.base = base->linkedAddress;
			if (blend) args.blended = blend->linkedAddress;
			if (mask) args.mask = mask->linkedAddress;

			linkedAddress = (OP::ADDRESS)program.m_opAddress.Num();
			program.m_opAddress.Add((uint32)program.m_byteCode.Num());
			AppendCode(program.m_byteCode, GetOpType());
			AppendCode(program.m_byteCode, args);
		}
	}


	//-------------------------------------------------------------------------------------------------
	FImageDesc ASTOpImageLayer::GetImageDesc(bool returnBestOption, FGetImageDescContext* context) const 
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
		if (base)
		{
			res = base->GetImageDesc(returnBestOption, context);
		}


		// Cache the result
		if (context)
		{
			context->m_results.Add(this, res);
		}

		return res;
	}


	//-------------------------------------------------------------------------------------------------
	void ASTOpImageLayer::GetLayoutBlockSize(int* pBlockX, int* pBlockY)
	{
		if (base)
		{
			base->GetLayoutBlockSize(pBlockX, pBlockY);
		}
	}


	//-------------------------------------------------------------------------------------------------
	mu::Ptr<ImageSizeExpression> ASTOpImageLayer::GetImageSizeExpression() const
	{
		if (base)
		{
			return base->GetImageSizeExpression();
		}

		return nullptr;
	}



	//---------------------------------------------------------------------------------------------
	Ptr<ASTOp> ASTOpImageLayer::OptimiseSemantic(const FModelOptimizationOptions& options, int32 Pass) const
	{
		Ptr<ASTOp> at;

		auto baseAt = base.child();
		auto blendAt = blend.child();
		auto maskAt = mask.child();

		// Convert to image layer color if blend is plain
		if (!at && blendAt && blendAt->GetOpType() == OP_TYPE::IM_PLAINCOLOUR)
		{
			// TODO: May some flags be supported?
			if (Flags == 0)
			{
				const ASTOpFixed* BlendPlainColor = static_cast<const ASTOpFixed*>(blendAt.get());

				Ptr<ASTOpImageLayerColor> NewLayerColor = new ASTOpImageLayerColor;
				NewLayerColor->base = baseAt;
				NewLayerColor->mask = maskAt;
				NewLayerColor->blendType = blendType;
				NewLayerColor->blendTypeAlpha = blendTypeAlpha;
				NewLayerColor->BlendAlphaSourceChannel = BlendAlphaSourceChannel;
				NewLayerColor->color = BlendPlainColor->children[BlendPlainColor->op.args.ImagePlainColour.colour].child();
				at = NewLayerColor;
			}
		}

		// Plain masks optimization
		if (!at && maskAt)
		{
			FVector4f colour;
			if (maskAt->IsImagePlainConstant(colour))
			{
				// For masks we only use one channel
				if (FMath::IsNearlyZero(colour[0]))
				{
					// If the mask is black, we can skip the entire operation
					at = base.child();
				}
				else if (FMath::IsNearlyEqual(colour[0], 1, UE_SMALL_NUMBER))
				{
					// If the mask is white, we can remove it
					Ptr<ASTOpImageLayer> NewOp = mu::Clone<ASTOpImageLayer>(this);
					NewOp->mask = nullptr;
					at = NewOp;
				}
			}
		}

		// See if the mask is actually already in the alpha channel of the blended. In that case,
		// remove the mask and enable the flag to use the alpha from blended.
		// This sounds very specific but, experimentally, it seems to happen often.
		if (!at && maskAt && blendAt 
			&& 
			Flags==0
			)
		{
			// Traverse down the expression while the expressions match
			Ptr<ASTOp> CurrentMask = maskAt;
			Ptr<ASTOp> CurrentBlend = blendAt;
			
			bool bMatchingAlphaExpression = true;
			while (bMatchingAlphaExpression)
			{
				bMatchingAlphaExpression = false;

				// Skip blend ops that wouldn't change the alpha
				bool bUpdated = true;
				while (bUpdated)
				{
					bUpdated = false;
					if (!CurrentBlend)
					{
						break;
					}

					switch (CurrentBlend->GetOpType())
					{
					case OP_TYPE::IM_LAYERCOLOUR:
					{
						const ASTOpImageLayerColor* BlendLayer = static_cast<const ASTOpImageLayerColor*>(CurrentBlend.get());
						if (BlendLayer->blendTypeAlpha == EBlendType::BT_NONE)
						{
							CurrentBlend = BlendLayer->base.child();
							bUpdated = true;
						}
						break;
					}

					case OP_TYPE::IM_LAYER:
					{
						const ASTOpImageLayer* BlendLayer = static_cast<const ASTOpImageLayer*>(CurrentBlend.get());
						if (BlendLayer->blendTypeAlpha == EBlendType::BT_NONE)
						{
							CurrentBlend = BlendLayer->base.child();
							bUpdated = true;
						}
						break;
					}

					default:
						break;
					}
				}

				// Matching ops?
				if (CurrentMask->GetOpType() != CurrentBlend->GetOpType())
				{
					break;
				}

				switch (CurrentMask->GetOpType())
				{

				case OP_TYPE::IM_DISPLACE:
				{
					const ASTOpFixed* MaskDisplace = static_cast<const ASTOpFixed*>(CurrentMask.get());
					const ASTOpFixed* BlendDisplace = static_cast<const ASTOpFixed*>(CurrentBlend.get());
					if (MaskDisplace && BlendDisplace 
						&&
						MaskDisplace->children[MaskDisplace->op.args.ImageDisplace.displacementMap].child()
						==
						BlendDisplace->children[BlendDisplace->op.args.ImageDisplace.displacementMap].child())
					{
						CurrentMask = MaskDisplace->children[MaskDisplace->op.args.ImageDisplace.source].child();
						CurrentBlend = BlendDisplace->children[BlendDisplace->op.args.ImageDisplace.source].child();
						bMatchingAlphaExpression = true;
					}
					break;
				}

				case OP_TYPE::IM_RASTERMESH:
				{
					const ASTOpImageRasterMesh* MaskRaster = static_cast<const ASTOpImageRasterMesh*>(CurrentMask.get());
					const ASTOpImageRasterMesh* BlendRaster = static_cast<const ASTOpImageRasterMesh*>(CurrentBlend.get());
					if (MaskRaster && BlendRaster
						&&
						MaskRaster->mesh.child() == BlendRaster->mesh.child()
						&&
						MaskRaster->projector.child() == BlendRaster->projector.child()
						&&
						MaskRaster->mask.child() == BlendRaster->mask.child()
						&&
						MaskRaster->angleFadeProperties.child() == BlendRaster->angleFadeProperties.child()
						&&
						MaskRaster->BlockId == BlendRaster->BlockId
						&&
						MaskRaster->LayoutIndex == BlendRaster->LayoutIndex
						)
					{
						CurrentMask = MaskRaster->image.child();
						CurrentBlend = BlendRaster->image.child();
						bMatchingAlphaExpression = true;
					}
					break;
				}

				case OP_TYPE::IM_RESIZE:
				{
					const ASTOpFixed* MaskResize = static_cast<const ASTOpFixed*>(CurrentMask.get());
					const ASTOpFixed* BlendResize = static_cast<const ASTOpFixed*>(CurrentBlend.get());
					if (MaskResize && BlendResize 
						&&
						MaskResize->op.args.ImageResize.size[0] == BlendResize->op.args.ImageResize.size[0]
						&&
						MaskResize->op.args.ImageResize.size[1] == BlendResize->op.args.ImageResize.size[1])
					{
						CurrentMask = MaskResize->children[MaskResize->op.args.ImageResize.source].child();
						CurrentBlend = BlendResize->children[BlendResize->op.args.ImageResize.source].child();
						bMatchingAlphaExpression = true;
					}
					break;
				}

				default:
					// Case not supported, so don't optimize.
					break;
				}
			}

			if (CurrentMask && CurrentMask->GetOpType() == OP_TYPE::IM_SWIZZLE)
			{
				// End of the possible mask expression chain match should have a swizzle selecting the alpha.
				const ASTOpImageSwizzle* MaskSwizzle = static_cast<const ASTOpImageSwizzle*>(CurrentMask.get());
				if (MaskSwizzle->SourceChannels[0] == 3
					&&
					!MaskSwizzle->SourceChannels[1]
					&&
					!MaskSwizzle->SourceChannels[2]
					&&
					!MaskSwizzle->SourceChannels[3]
					&&
					MaskSwizzle->Sources[0].child() == CurrentBlend
					)
				{
					// we can do something good here
					Ptr<ASTOpImageLayer> NewLayer = mu::Clone<ASTOpImageLayer>(this);
					NewLayer->mask = nullptr;
					NewLayer->Flags |= OP::ImageLayerArgs::F_USE_MASK_FROM_BLENDED;
					at = NewLayer;
				}
			}
		}

		// Try to avoid child swizzle
		if (!at)
		{
			// Is the base a swizzle expanding alpha from a texture?
			if (baseAt->GetOpType() == OP_TYPE::IM_SWIZZLE)
			{
				const ASTOpImageSwizzle* TypedBase = static_cast<const ASTOpImageSwizzle*>(baseAt.get());
				bool bAreAllAlpha = true;
				for (int32 c=0; c<MUTABLE_OP_MAX_SWIZZLE_CHANNELS; ++c)
				{
					if (TypedBase->Sources[c] && TypedBase->SourceChannels[c] != 3)
					{
						bAreAllAlpha = false;
						break;
					}
				}

				if (bAreAllAlpha)
				{
					// TODO
				}
			}

			// Is the mask a swizzle expanding alpha from a texture?
			if (!at && maskAt && maskAt->GetOpType() == OP_TYPE::IM_SWIZZLE)
			{
				const ASTOpImageSwizzle* TypedBase = static_cast<const ASTOpImageSwizzle*>(maskAt.get());
				bool bAreAllAlpha = true;
				for (int32 c = 0; c < MUTABLE_OP_MAX_SWIZZLE_CHANNELS; ++c)
				{
					if (TypedBase->Sources[c] && TypedBase->SourceChannels[c] != 3)
					{
						bAreAllAlpha = false;
						break;
					}
				}

				if (bAreAllAlpha)
				{
					// TODO
				}
			}

			// Is the blend a swizzle selecting alpha?
			if (Pass>0 && !at && blendAt && blendAt->GetOpType() == OP_TYPE::IM_SWIZZLE && Flags==0)
			{
				const ASTOpImageSwizzle* TypedBlend = static_cast<const ASTOpImageSwizzle*>(blendAt.get());
				if (TypedBlend->Format==EImageFormat::IF_L_UBYTE
					&&
					TypedBlend->SourceChannels[0]==3)
				{
					Ptr<ASTOpImageLayer> nop = mu::Clone<ASTOpImageLayer>(this);
					nop->Flags = Flags | OP::ImageLayerArgs::FLAGS::F_BLENDED_RGB_FROM_ALPHA;
					nop->blend = TypedBlend->Sources[0].child();
					auto Desc = nop->blend->GetImageDesc(true);
					check(Desc.m_format==EImageFormat::IF_RGBA_UBYTE);
					at = nop;					
				}
			}
		}

		// Introduce crop if mask is constant and smaller than the base
		if (!at && maskAt)
		{
			FImageRect sourceMaskUsage;
			FImageDesc maskDesc;

			bool validUsageRect = false;
			{
				//MUTABLE_CPUPROFILER_SCOPE(EvaluateAreasForCrop);

				validUsageRect = maskAt->GetNonBlackRect(sourceMaskUsage);
				if (validUsageRect)
				{
					check(sourceMaskUsage.size[0] > 0);
					check(sourceMaskUsage.size[1] > 0);

					FGetImageDescContext context;
					maskDesc = maskAt->GetImageDesc(false, &context);
				}
			}

			if (validUsageRect)
			{
				// Adjust for compressed blocks (4), and some extra mips (2 more mips, which is 4)
				// \TODO: block size may be different in ASTC
				constexpr int blockSize = 4 * 4;

				FImageRect maskUsage;
				maskUsage.min[0] = (sourceMaskUsage.min[0] / blockSize) * blockSize;
				maskUsage.min[1] = (sourceMaskUsage.min[1] / blockSize) * blockSize;
				FImageSize minOffset = sourceMaskUsage.min - maskUsage.min;
				maskUsage.size[0] = ((sourceMaskUsage.size[0] + minOffset[0] + blockSize - 1) / blockSize) * blockSize;
				maskUsage.size[1] = ((sourceMaskUsage.size[1] + minOffset[1] + blockSize - 1) / blockSize) * blockSize;

				// Is it worth?
				float ratio = float(maskUsage.size[0] * maskUsage.size[1])
					/ float(maskDesc.m_size[0] * maskDesc.m_size[1]);
				float acceptableCropRatio = options.AcceptableCropRatio;
				if (ratio < acceptableCropRatio)
				{
					check(maskUsage.size[0] > 0);
					check(maskUsage.size[1] > 0);

					Ptr<ASTOpImageCrop> cropMask = new ASTOpImageCrop();
					cropMask->Source = mask.child();
					cropMask->Min[0] = maskUsage.min[0];
					cropMask->Min[1] = maskUsage.min[1];
					cropMask->Size[0] = maskUsage.size[0];
					cropMask->Size[1] = maskUsage.size[1];

					Ptr<ASTOpImageCrop> cropBlended = new ASTOpImageCrop();
					cropBlended->Source = blend.child();
					cropBlended->Min[0] = maskUsage.min[0];
					cropBlended->Min[1] = maskUsage.min[1];
					cropBlended->Size[0] = maskUsage.size[0];
					cropBlended->Size[1] = maskUsage.size[1];

					Ptr<ASTOpImageCrop> cropBase = new ASTOpImageCrop();
					cropBase->Source = base.child();
					cropBase->Min[0] = maskUsage.min[0];
					cropBase->Min[1] = maskUsage.min[1];
					cropBase->Size[0] = maskUsage.size[0];
					cropBase->Size[1] = maskUsage.size[1];

					Ptr<ASTOpImageLayer> newLayer = mu::Clone<ASTOpImageLayer>(this);
					newLayer->base = cropBase;
					newLayer->blend = cropBlended;
					newLayer->mask = cropMask;

					Ptr<ASTOpImagePatch> patch = new ASTOpImagePatch();
					patch->base = baseAt;
					patch->patch = newLayer;
					patch->location[0] = maskUsage.min[0];
					patch->location[1] = maskUsage.min[1];
					at = patch;
				}
			}
		}

		return at;
	}


	Ptr<ASTOp> ASTOpImageLayer::OptimiseSink(const FModelOptimizationOptions& options, FOptimizeSinkContext& context) const
	{
		Ptr<ASTOp> at;

		// Layer effects may be worth sinking down switches and conditionals, to be able
		// to apply extra optimisations
		auto baseAt = base.child();
		auto blendAt = blend.child();
		auto maskAt = mask.child();

		// Promote conditions from the base
		OP_TYPE baseType = baseAt->GetOpType();
		switch (baseType)
		{
			// Seems to cause operation explosion in optimizer in bandit model.
			// moved to generic sink in the default.
//            case OP_TYPE::IM_CONDITIONAL:
//            {
//                m_modified = true;

//                OP op = program.m_code[baseAt];

//                OP aOp = program.m_code[at];
//                aOp.args.ImageLayer.base = program.m_code[baseAt].args.Conditional.yes;
//                op.args.Conditional.yes = program.AddOp( aOp );

//                OP bOp = program.m_code[at];
//                bOp.args.ImageLayer.base = program.m_code[baseAt].args.Conditional.no;
//                op.args.Conditional.no = program.AddOp( bOp );

//                at = program.AddOp( op );
//                break;
//            }

		case OP_TYPE::IM_SWITCH:
		{
			// Warning:
			// It seems to cause data explosion in optimizer in some models. Because
			// all switch branches become unique constants

//                // See if the blended has an identical switch, to optimise it too
//                auto baseSwitch = dynamic_cast<const ASTOpSwitch*>( baseAt.get() );
//                auto blendedSwitch = dynamic_cast<const ASTOpSwitch*>( blendAt.get() );
//                auto maskSwitch = dynamic_cast<const ASTOpSwitch*>( maskAt.get() );

//                bool blendedToo = baseSwitch->IsCompatibleWith( blendedSwitch );
//                bool maskToo = baseSwitch->IsCompatibleWith( maskSwitch );

//                // Move the layer operation down all the paths
//                auto nop = mu::Clone<ASTOpSwitch>(baseSwitch);

//                if (nop->def)
//                {
//                    auto defOp = mu::Clone<ASTOpFixed>(this);
//                    defOp->SetChild( defOp->op.args.ImageLayer.base, nop->def );
//                    if (blendedToo)
//                    {
//                        defOp->SetChild( defOp->op.args.ImageLayer.blended, blendedSwitch->def );
//                    }
//                    if (maskToo)
//                    {
//                        defOp->SetChild( defOp->op.args.ImageLayer.mask, maskSwitch->def );
//                    }
//                    nop->def = defOp;
//                }

//                for ( size_t v=0; v<nop->cases.Num(); ++v )
//                {
//                    if ( nop->cases[v].branch )
//                    {
//                        auto bOp = mu::Clone<ASTOpFixed>(this);
//                        bOp->SetChild( bOp->op.args.ImageLayer.base, nop->cases[v].branch );

//                        if (blendedToo)
//                        {
//                            bOp->SetChild( bOp->op.args.ImageLayer.blended, blendedSwitch->FindBranch( nop->cases[v].condition ) );
//                        }

//                        if (maskToo)
//                        {
//                            bOp->SetChild( bOp->op.args.ImageLayer.mask, maskSwitch->FindBranch( nop->cases[v].condition ) );
//                        }

//                        nop->cases[v].branch = bOp;
//                    }
//                }

//                at = nop;

			break;
		}

		default:
		{
			// Try generic base sink
//                OP templateOp = program.m_code[at];

//                CloneNeutralPartialTreeVisitor neutral;
//                OP_TYPE supportedOps[] = {
//                    OP_TYPE::IM_CONDITIONAL,
//                    OP_TYPE::NONE
//                };
//                auto newRoot = neutral.Apply
//                         (
//                            program.m_code[at].args.ImageLayer.base, program,
//                            DT_IMAGE, supportedOps,
//                            &templateOp, &templateOp.args.ImageLayer.base);

//                // If there is a change
//                if (newRoot!=program.m_code[at].args.ImageLayer.base)
//                {
//                    m_modified = true;
//                    at = newRoot;
//                }

			break;
		}

		}

		// If we failed to optimize so far, see if it is worth optimizing the blended branch only.
		if (!at && blendAt)
		{
			OP_TYPE BlendType = blendAt->GetOpType();
			switch (BlendType)
			{

			case OP_TYPE::IM_SWITCH:
			{
				const ASTOpSwitch* BlendSwitch = static_cast<const ASTOpSwitch*>(blendAt.get());

				// If at least a switch option is a plain colour, sink the layer into the switch
				bool bWorthSinking = false;
				for (int32 v = 0; v < BlendSwitch->cases.Num(); ++v)
				{
					if (BlendSwitch->cases[v].branch)
					{
						// \TODO: Use the smarter query function to detect plain images?
						if (BlendSwitch->cases[v].branch->GetOpType() == OP_TYPE::IM_PLAINCOLOUR)
						{
							bWorthSinking = true;
							break;
						}
					}
				}

				if (bWorthSinking)
				{
					bool bMaskIsCompatibleSwitch = false;
					const ASTOpSwitch* MaskSwitch = nullptr;
					if (maskAt && maskAt->GetOpType()== OP_TYPE::IM_SWITCH)
					{
						MaskSwitch = static_cast<const ASTOpSwitch*>(maskAt.get());
						bMaskIsCompatibleSwitch = MaskSwitch->IsCompatibleWith(BlendSwitch);
					}

					Ptr<ASTOpSwitch> NewSwitch = mu::Clone<ASTOpSwitch>(BlendSwitch);

					if (NewSwitch->def)
					{
						Ptr<ASTOpImageLayer> defOp = mu::Clone<ASTOpImageLayer>(this);
						defOp->blend = BlendSwitch->def.child();
						if (bMaskIsCompatibleSwitch)
						{
							defOp->mask = MaskSwitch->def.child();
						}
						NewSwitch->def = defOp;
					}

					for (int32 v = 0; v < NewSwitch->cases.Num(); ++v)
					{
						if (NewSwitch->cases[v].branch)
						{
							Ptr<ASTOpImageLayer> BranchOp = mu::Clone<ASTOpImageLayer>(this);
							BranchOp->blend = BlendSwitch->cases[v].branch.child();
							if (bMaskIsCompatibleSwitch)
							{
								BranchOp->mask = MaskSwitch->cases[v].branch.child();
							}
							NewSwitch->cases[v].branch = BranchOp;
						}
					}

					at = NewSwitch;
				}

				break;
			}

			default:
				break;

			}
		}

		// If we failed to optimize so far, see if it is worth optimizing the mask branch only.
		if (!at && maskAt)
		{
			OP_TYPE MaskType = maskAt->GetOpType();
			switch (MaskType)
			{

			case OP_TYPE::IM_SWITCH:
			{
				const ASTOpSwitch* MaskSwitch = static_cast<const ASTOpSwitch*>(maskAt.get());

				// If at least a switch option is a plain colour, sink the layer into the switch
				bool bWorthSinking = false;
				for (int32 v = 0; v < MaskSwitch->cases.Num(); ++v)
				{
					if (MaskSwitch->cases[v].branch)
					{
						// \TODO: Use the smarter query function to detect plain images?
						if (MaskSwitch->cases[v].branch->GetOpType() == OP_TYPE::IM_PLAINCOLOUR)
						{
							bWorthSinking = true;
							break;
						}
					}
				}

				if (bWorthSinking)
				{
					Ptr<ASTOpSwitch> NewSwitch = mu::Clone<ASTOpSwitch>(MaskSwitch);

					if (NewSwitch->def)
					{
						Ptr<ASTOpImageLayer> defOp = mu::Clone<ASTOpImageLayer>(this);
						defOp->mask = MaskSwitch->def.child();
						NewSwitch->def = defOp;
					}

					for (int32 v = 0; v < NewSwitch->cases.Num(); ++v)
					{
						if (NewSwitch->cases[v].branch)
						{
							Ptr<ASTOpImageLayer> BranchOp = mu::Clone<ASTOpImageLayer>(this);
							BranchOp->mask = MaskSwitch->cases[v].branch.child();
							NewSwitch->cases[v].branch = BranchOp;
						}
					}

					at = NewSwitch;
				}

				break;
			}

			default:
				break;

			}
		}

		return at;
	}


}
