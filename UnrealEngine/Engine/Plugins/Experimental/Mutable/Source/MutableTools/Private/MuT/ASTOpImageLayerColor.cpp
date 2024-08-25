// Copyright Epic Games, Inc. All Rights Reserved.

#include "MuT/ASTOpImageLayerColor.h"

#include "MuT/ASTOpSwitch.h"
#include "MuT/ASTOpImageSwizzle.h"
#include "MuT/ASTOpImageRasterMesh.h"
#include "Containers/Map.h"
#include "HAL/PlatformMath.h"
#include "MuR/ModelPrivate.h"
#include "MuR/RefCounted.h"
#include "MuR/Types.h"
#include "MuT/StreamsPrivate.h"


namespace mu
{

	ASTOpImageLayerColor::ASTOpImageLayerColor()
		: base(this)
		, color(this)
		, mask(this)
	{
	}


	//-------------------------------------------------------------------------------------------------
	ASTOpImageLayerColor::~ASTOpImageLayerColor()
	{
		// Explicit call needed to avoid recursive destruction
		ASTOp::RemoveChildren();
	}


	//-------------------------------------------------------------------------------------------------
	bool ASTOpImageLayerColor::IsEqual(const ASTOp& InOtherUntyped) const
	{
		if (InOtherUntyped.GetOpType()==GetOpType())
		{
			const ASTOpImageLayerColor* Other = static_cast<const ASTOpImageLayerColor*>(&InOtherUntyped);
			return base == Other->base &&
				color == Other->color &&
				mask == Other->mask &&
				blendType == Other->blendType &&
				blendTypeAlpha == Other->blendTypeAlpha &&
				BlendAlphaSourceChannel == Other->BlendAlphaSourceChannel &&
				Flags == Other->Flags;
		}
		return false;
	}


	//-------------------------------------------------------------------------------------------------
	uint64 ASTOpImageLayerColor::Hash() const
	{
		uint64 res = std::hash<OP_TYPE>()(GetOpType());
		hash_combine(res, base.child().get());
		hash_combine(res, color.child().get());
		hash_combine(res, mask.child().get());
		return res;
	}


	//-------------------------------------------------------------------------------------------------
	mu::Ptr<ASTOp> ASTOpImageLayerColor::Clone(MapChildFuncRef mapChild) const
	{
		Ptr<ASTOpImageLayerColor> n = new ASTOpImageLayerColor();
		n->base = mapChild(base.child());
		n->color = mapChild(color.child());
		n->mask = mapChild(mask.child());
		n->blendType = blendType;
		n->blendTypeAlpha = blendTypeAlpha;
		n->BlendAlphaSourceChannel = BlendAlphaSourceChannel;
		n->Flags = Flags;
		return n;
	}


	//-------------------------------------------------------------------------------------------------
	void ASTOpImageLayerColor::ForEachChild(const TFunctionRef<void(ASTChild&)> f)
	{
		f(base);
		f(color);
		f(mask);
	}


	//-------------------------------------------------------------------------------------------------
	void ASTOpImageLayerColor::Link(FProgram& program, FLinkerOptions*)
	{
		// Already linked?
		if (!linkedAddress)
		{
			OP::ImageLayerColourArgs args;
			FMemory::Memzero(&args, sizeof(args));

			args.blendType = (uint8)blendType;
			args.blendTypeAlpha = (uint8)blendTypeAlpha;
			args.BlendAlphaSourceChannel = BlendAlphaSourceChannel;
			args.flags = Flags;

			check(base);
			if (base) args.base = base->linkedAddress;
			if (color) args.colour = color->linkedAddress;
			if (mask) args.mask = mask->linkedAddress;

			linkedAddress = (OP::ADDRESS)program.m_opAddress.Num();
			program.m_opAddress.Add((uint32)program.m_byteCode.Num());
			AppendCode(program.m_byteCode, GetOpType());
			AppendCode(program.m_byteCode, args);
		}
	}


	//-------------------------------------------------------------------------------------------------
	FImageDesc ASTOpImageLayerColor::GetImageDesc(bool returnBestOption, FGetImageDescContext* context) const 
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
	void ASTOpImageLayerColor::GetLayoutBlockSize(int* pBlockX, int* pBlockY)
	{
		if (base)
		{
			base->GetLayoutBlockSize(pBlockX, pBlockY);
		}
	}


	//-------------------------------------------------------------------------------------------------
	mu::Ptr<ImageSizeExpression> ASTOpImageLayerColor::GetImageSizeExpression() const
	{
		if (base)
		{
			return base->GetImageSizeExpression();
		}

		return nullptr;
	}


	//---------------------------------------------------------------------------------------------
	Ptr<ASTOp> ASTOpImageLayerColor::OptimiseSemantic(const FModelOptimizationOptions& options, int32 Pass) const
	{
		Ptr<ASTOp> at;

		// Plain masks optimization
		if (mask.child() && !(Flags & OP::ImageLayerArgs::F_USE_MASK_FROM_BLENDED) )
		{
			FVector4f colour;
			if (mask.child()->IsImagePlainConstant(colour))
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
					Ptr<ASTOpImageLayerColor> NewOp = mu::Clone<ASTOpImageLayerColor>(this);
					NewOp->mask = nullptr;
					at = NewOp;
				}
			}
		}

		// Layer operations with constants that do nothing.
		if (!at)
		{
			bool bRGBUnchanged = blendType == EBlendType::BT_NONE;
			bool bAlphaUnchanged = blendTypeAlpha == EBlendType::BT_NONE;

			FVector4f ColorConst(0,0,0,1);
			if (!color || color.child()->GetOpType()==OP_TYPE::CO_CONSTANT)
			{
				if (color)
				{
					const ASTOpFixed* TypedColor = static_cast<const ASTOpFixed*>(color.child().get());
					const float* Value = TypedColor->op.args.ColourConstant.value;
					ColorConst.Set(Value[0], Value[1], Value[2], Value[3]);
				}

				if (!bAlphaUnchanged)
				{
					// TODO: Update when alpha may come from alpha in the color.
					switch (blendTypeAlpha)
					{
					case EBlendType::BT_LIGHTEN: bAlphaUnchanged = FMath::IsNearlyEqual(ColorConst[BlendAlphaSourceChannel], 0.0f); break;
					case EBlendType::BT_MULTIPLY: bAlphaUnchanged = FMath::IsNearlyEqual(ColorConst[BlendAlphaSourceChannel], 1.0f); break;
					default: break;
					}
				}

				if (!bRGBUnchanged)
				{
					if (Flags & OP::ImageLayerArgs::FLAGS::F_BASE_RGB_FROM_ALPHA)
					{
						switch (blendType)
						{
						case EBlendType::BT_LIGHTEN: bRGBUnchanged = FMath::IsNearlyZero(ColorConst[3]); break;
						case EBlendType::BT_MULTIPLY: bRGBUnchanged = FMath::IsNearlyEqual(ColorConst[3],1.0f); break;
						default: break;
						}
					}
					else
					{
						// How many channels are there in the base?
						FImageDesc BaseDesc = base->GetImageDesc();
						const FImageFormatData& FormatDesc = GetImageFormatData(BaseDesc.m_format);
						if (FormatDesc.Channels == 1)
						{
							// We only need to check R
							switch (blendType)
							{
							case EBlendType::BT_LIGHTEN: bRGBUnchanged = FMath::IsNearlyZero(ColorConst[0]); break;
							case EBlendType::BT_MULTIPLY: bRGBUnchanged = FMath::IsNearlyEqual(ColorConst[0],1.0); break;
							default: break;
							}
						}
						else
						{
							// Check RGB
							switch (blendType)
							{
							case EBlendType::BT_LIGHTEN: bRGBUnchanged = ColorConst.IsNearlyZero3(UE_SMALL_NUMBER); break;
							case EBlendType::BT_MULTIPLY: bRGBUnchanged = FVector3f(ColorConst).Equals(FVector3f(1, 1, 1)); break;
							default: break;
							}
						}
					}
				}
			}

			if (bRGBUnchanged && bAlphaUnchanged)
			{
				// Skip this operation.
				at = base.child();
			}
		}


		// Try to avoid child swizzle
		if (!at)
		{
			// Is the base a swizzle getting expanding alpha from the same texture?
			if (base.child()->GetOpType() == OP_TYPE::IM_SWIZZLE)
			{
				const ASTOpImageSwizzle* TypedBase = static_cast<const ASTOpImageSwizzle*>(base.child().get());
				bool bAreAllSameAlpha = true;
				Ptr<ASTOp> Source = nullptr;
				for (int32 c = 0; c < MUTABLE_OP_MAX_SWIZZLE_CHANNELS; ++c)
				{
					Ptr<ASTOp> ThisSource = TypedBase->Sources[c].child();
					if (!Source)
					{
						Source = ThisSource;
					}
					else if (ThisSource && ThisSource!=Source)
					{
						bAreAllSameAlpha = false;
						break;
					}


					if (ThisSource && TypedBase->SourceChannels[c] != 3)
					{
						bAreAllSameAlpha = false;
						break;
					}
				}

				if (bAreAllSameAlpha)
				{
					Ptr<ASTOpImageLayerColor> NewOp = mu::Clone<ASTOpImageLayerColor>(this);
					NewOp->Flags |= OP::ImageLayerArgs::FLAGS::F_BASE_RGB_FROM_ALPHA;
					NewOp->base = Source;
					at = NewOp;
				}
			}
		}

		return at;
	}


	//-------------------------------------------------------------------------------------------------
	Ptr<ASTOp> ASTOpImageLayerColor::OptimiseSink(const FModelOptimizationOptions& options, FOptimizeSinkContext& context) const
	{
		Ptr<ASTOp> at;

		// Layer effects may be worth sinking down switches and conditionals, to be able
		// to apply extra optimisations
		auto baseAt = base.child();
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
//                aOp.args.ImageLayerColour.base = program.m_code[baseAt].args.Conditional.yes;
//                op.args.Conditional.yes = program.AddOp( aOp );

//                OP bOp = program.m_code[at];
//                bOp.args.ImageLayerColour.base = program.m_code[baseAt].args.Conditional.no;
//                op.args.Conditional.no = program.AddOp( bOp );

//                at = program.AddOp( op );
//                break;
//            }

		case OP_TYPE::IM_SWITCH:
		{
			// Warning:
			// It seems to cause data explosion in optimizer in some models. Because
			// all switch branches become unique constants

			// See if the blended has an identical switch, to optimise it too
			const ASTOpSwitch* baseSwitch = static_cast<const ASTOpSwitch*>(baseAt.get());

			// Mask not supported yet
			if (maskAt)
			{
				break;
			}

			// Move the layer operation down base paths
			Ptr<ASTOpSwitch> nop = mu::Clone<ASTOpSwitch>(baseSwitch);

			if (nop->def)
			{
				Ptr<ASTOpImageLayerColor> defOp = mu::Clone<ASTOpImageLayerColor>(this);
				defOp->base = nop->def.child();
				nop->def = defOp;
			}

			for (int32 v = 0; v < nop->cases.Num(); ++v)
			{
				if (nop->cases[v].branch)
				{
					Ptr<ASTOpImageLayerColor> bOp = mu::Clone<ASTOpImageLayerColor>(this);
					bOp->base = nop->cases[v].branch.child();
					nop->cases[v].branch = bOp;
				}
			}

			at = nop;
			break;
		}

		case OP_TYPE::IM_DISPLACE:
		{
			// Mask not supported yet. If there is a mask it wouldn't be correct to sink
			// unless the mask was a similar displace.
			if (maskAt)
			{
				break;
			}

			Ptr<ASTOpFixed> NewDisplace = mu::Clone<ASTOpFixed>(baseAt);

			Ptr<ASTOp> sourceOp = NewDisplace->children[NewDisplace->op.args.ImageDisplace.source].child();
			Ptr<ASTOpImageLayerColor> NewSource = mu::Clone<ASTOpImageLayerColor>(this);
			NewSource->base = sourceOp;
			NewDisplace->SetChild(NewDisplace->op.args.ImageDisplace.source, NewSource);

			at = NewDisplace;
			break;
		}

		case OP_TYPE::IM_RASTERMESH:
		{
			// Mask not supported yet. If there is a mask it wouldn't be correct to sink.				
			if (maskAt)
			{
				break;
			}

			Ptr<ASTOpImageRasterMesh> NewRaster = mu::Clone<ASTOpImageRasterMesh>(baseAt);

			Ptr<ASTOp> sourceOp = NewRaster->image.child();
			Ptr<ASTOpImageLayerColor> NewSource = mu::Clone<ASTOpImageLayerColor>(this);
			NewSource->base = sourceOp;
			NewRaster->image = NewSource;

			at = NewRaster;
			break;
		}


		default:
			break;

		}

		return at;
	}

}
