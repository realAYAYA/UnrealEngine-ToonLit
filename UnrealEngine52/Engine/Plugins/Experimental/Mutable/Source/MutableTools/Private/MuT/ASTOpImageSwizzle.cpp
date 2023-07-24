// Copyright Epic Games, Inc. All Rights Reserved.

#include "MuT/ASTOpImageSwizzle.h"

#include "Containers/Map.h"
#include "HAL/UnrealMemory.h"
#include "Math/UnrealMathSSE.h"
#include "Misc/AssertionMacros.h"
#include "MuR/ImagePrivate.h"
#include "MuR/ModelPrivate.h"
#include "MuR/MutableMath.h"
#include "MuR/RefCounted.h"
#include "MuR/Types.h"
#include "MuT/ASTOpConditional.h"
#include "MuT/ASTOpImageCompose.h"
#include "MuT/ASTOpImagePatch.h"
#include "MuT/ASTOpImagePixelFormat.h"
#include "MuT/ASTOpImageMultiLayer.h"
#include "MuT/ASTOpImageLayer.h"
#include "MuT/ASTOpImageLayerColor.h"
#include "MuT/ASTOpImageRasterMesh.h"
#include "MuT/ASTOpSwitch.h"
#include "MuT/StreamsPrivate.h"

#include <memory>
#include <utility>


namespace mu
{

	ASTOpImageSwizzle::ASTOpImageSwizzle()
		: Sources{ ASTChild(this),ASTChild(this),ASTChild(this),ASTChild(this) }
	{
	}


	ASTOpImageSwizzle::~ASTOpImageSwizzle()
	{
		// Explicit call needed to avoid recursive destruction
		ASTOp::RemoveChildren();
	}


	bool ASTOpImageSwizzle::IsEqual(const ASTOp& otherUntyped) const
	{
		if (const ASTOpImageSwizzle* Other = dynamic_cast<const ASTOpImageSwizzle*>(&otherUntyped))
		{
			for (int32 i = 0; i<MUTABLE_OP_MAX_SWIZZLE_CHANNELS; ++i)
			{
				if (!(Sources[i] == Other->Sources[i] && SourceChannels[i] == Other->SourceChannels[i]))
				{
					return false;
				}
			}
			return Format == Other->Format;
		}
		return false;
	}


	uint64 ASTOpImageSwizzle::Hash() const
	{
		uint64 res = std::hash<void*>()(Sources[0].child().get());
		hash_combine(res, std::hash<void*>()(Sources[1].child().get()));
		hash_combine(res, std::hash<void*>()(Sources[2].child().get()));
		hash_combine(res, std::hash<void*>()(Sources[3].child().get()));
		hash_combine(res, std::hash<uint8>()(SourceChannels[0]));
		hash_combine(res, std::hash<uint8>()(SourceChannels[1]));
		hash_combine(res, std::hash<uint8>()(SourceChannels[2]));
		hash_combine(res, std::hash<uint8>()(SourceChannels[3]));
		hash_combine(res, Format);
		return res;
	}


	mu::Ptr<ASTOp> ASTOpImageSwizzle::Clone(MapChildFuncRef mapChild) const
	{
		mu::Ptr<ASTOpImageSwizzle> n = new ASTOpImageSwizzle();
		for (int32 i = 0; i < MUTABLE_OP_MAX_SWIZZLE_CHANNELS; ++i)
		{
			n->Sources[i] = mapChild(Sources[i].child());
			n->SourceChannels[i] = SourceChannels[i];
		}
		n->Format = Format;
		return n;
	}


	void ASTOpImageSwizzle::ForEachChild(const TFunctionRef<void(ASTChild&)> f)
	{
		for (int32 i = 0; i < MUTABLE_OP_MAX_SWIZZLE_CHANNELS; ++i)
		{
			f(Sources[i]);
		}
	}


	void ASTOpImageSwizzle::Link(FProgram& program, const FLinkerOptions*)
	{
		// Already linked?
		if (!linkedAddress)
		{
			OP::ImageSwizzleArgs args;
			FMemory::Memzero(&args, sizeof(args));

			args.format = Format;
			for (int32 i = 0; i < MUTABLE_OP_MAX_SWIZZLE_CHANNELS; ++i)
			{
				if (Sources[i]) args.sources[i] = Sources[i]->linkedAddress;
				args.sourceChannels[i] = SourceChannels[i];
			}			

			linkedAddress = (OP::ADDRESS)program.m_opAddress.Num();
			program.m_opAddress.Add((uint32_t)program.m_byteCode.Num());
			AppendCode(program.m_byteCode, GetOpType());
			AppendCode(program.m_byteCode, args);
		}
	}


	mu::Ptr<ASTOp> ASTOpImageSwizzle::OptimiseSemantic(const FModelOptimizationOptions&) const
	{
		Ptr<ASTOpImageSwizzle> sat;

		for (int c = 0; c < MUTABLE_OP_MAX_SWIZZLE_CHANNELS; ++c)
		{
			Ptr<ASTOp> candidate = Sources[c].child();
			if (!candidate)
			{
				continue;
			}

			switch (candidate->GetOpType())
			{
			// Swizzle
			case OP_TYPE::IM_SWIZZLE:
			{
				if (!sat)
				{
					sat = mu::Clone<ASTOpImageSwizzle>(this);
				}
				const ASTOpImageSwizzle* typedCandidate = dynamic_cast<const ASTOpImageSwizzle*>(candidate.get());
				int candidateChannel = SourceChannels[c];

				sat->Sources[c] = typedCandidate->Sources[candidateChannel].child();
				sat->SourceChannels[c] = typedCandidate->SourceChannels[candidateChannel];

				break;
			}

			// Format
			case OP_TYPE::IM_PIXELFORMAT:
			{
				// We can remove the format if its source is already an uncompressed format
				ASTOpImagePixelFormat* typedCandidate = dynamic_cast<ASTOpImagePixelFormat*>(candidate.get());
				Ptr<ASTOp> formatSource = typedCandidate->Source.child();

				if (formatSource)
				{
					FImageDesc desc = formatSource->GetImageDesc();
					if (desc.m_format != EImageFormat::IF_NONE && !IsCompressedFormat(desc.m_format))
					{
						if (!sat)
						{
							sat = mu::Clone<ASTOpImageSwizzle>(this);
						}
						sat->Sources[c] = formatSource;
					}
				}

				break;
			}

			default:
				break;
			}

		}

		return sat;
	}

	namespace
	{
		//---------------------------------------------------------------------------------------------
		//! Set al the non-null sources of an image swizzle operation to the given value
		//---------------------------------------------------------------------------------------------
		void ReplaceAllSources(Ptr<ASTOpImageSwizzle>& op, Ptr<ASTOp>& value)
		{
			check(op->GetOpType() == OP_TYPE::IM_SWIZZLE);
			for (int c = 0; c < MUTABLE_OP_MAX_SWIZZLE_CHANNELS; ++c)
			{
				if (op->Sources[c])
				{
					op->Sources[c] = value;
				}
			}
		}
	}

	mu::Ptr<ASTOp> ASTOpImageSwizzle::OptimiseSink(const FModelOptimizationOptions& options, FOptimizeSinkContext& context) const
	{
		MUTABLE_CPUPROFILER_SCOPE(OptimiseSwizzleAST);

		//! Basic optimisation first
		Ptr<ASTOp> at = OptimiseSemantic(options);
		if (at)
		{
			return at;
		}

		// If all sources are the same, we can sink the instruction
		bool bAllChannelsAreTheSame = true;
		bool bAllChannelsAreTheSameType = true;
		Ptr<ASTOp> channelSourceAt;
		for (int c = 0; c < MUTABLE_OP_MAX_SWIZZLE_CHANNELS; ++c)
		{
			Ptr<ASTOp> candidate = Sources[c].child();
			if (candidate)
			{
				if (!channelSourceAt)
				{
					channelSourceAt = candidate;
				}
				else
				{
					bAllChannelsAreTheSame = bAllChannelsAreTheSame && (channelSourceAt == candidate);
					bAllChannelsAreTheSameType = bAllChannelsAreTheSameType && (channelSourceAt->GetOpType() == candidate->GetOpType());
				}
			}
		}

		// If we are not changing channel order, just remove the swizzle and adjust the format.
		bool bSameChannelOrder = true;
		int32 NumChannelsInFormat = GetImageFormatData(Format).m_channels;
		for (int32 c = 0; c < NumChannelsInFormat; ++c)
		{
			if (Sources[c] && SourceChannels[c] != c)
			{
				bSameChannelOrder = false;
			}
		}

		if (!channelSourceAt)
		{
			return at;
		}

		OP_TYPE sourceType = channelSourceAt->GetOpType();

		if (bAllChannelsAreTheSame && channelSourceAt)
		{
			// The instruction can be sunk
			switch (sourceType)
			{

			case OP_TYPE::IM_SWITCH:
			{
				// Move the swizzle down all the paths
				Ptr<ASTOpSwitch> nop = mu::Clone<ASTOpSwitch>(channelSourceAt);

				if (nop->def)
				{
					Ptr<ASTOpImageSwizzle> defOp = mu::Clone<ASTOpImageSwizzle>(this);
					ReplaceAllSources(defOp, nop->def.child());
					nop->def = defOp;
				}

				for (int32 v = 0; v < nop->cases.Num(); ++v)
				{
					if (nop->cases[v].branch)
					{
						Ptr<ASTOpImageSwizzle> bOp = mu::Clone<ASTOpImageSwizzle>(this);
						ReplaceAllSources(bOp, nop->cases[v].branch.child());
						nop->cases[v].branch = bOp;
					}
				}

				at = nop;
				break;
			}

			case OP_TYPE::IM_CONDITIONAL:
			{
				// We move the swizzle down the two paths
				Ptr<ASTOpConditional> nop = mu::Clone<ASTOpConditional>(channelSourceAt);

				Ptr<ASTOpImageSwizzle> aOp = mu::Clone<ASTOpImageSwizzle>(this);
				ReplaceAllSources(aOp, nop->yes.child());
				nop->yes = aOp;

				Ptr<ASTOpImageSwizzle> bOp = mu::Clone<ASTOpImageSwizzle>(this);
				ReplaceAllSources(bOp, nop->no.child());
				nop->no = bOp;

				at = nop;
				break;
			}

			case OP_TYPE::IM_LAYER:
			{
				// We move the swizzle down the two paths
				Ptr<ASTOpImageLayer> nop = mu::Clone<ASTOpImageLayer>(channelSourceAt);

				Ptr<ASTOpImageSwizzle> aOp = mu::Clone<ASTOpImageSwizzle>(this);
				ReplaceAllSources(aOp, nop->base.child());
				nop->base = aOp;

				Ptr<ASTOpImageSwizzle> bOp = mu::Clone<ASTOpImageSwizzle>(this);
				ReplaceAllSources(bOp, nop->blend.child());
				nop->blend = bOp;

				at = nop;
				break;
			}

			case OP_TYPE::IM_LAYERCOLOUR:
			{
				// We move the swizzle down the base path
				Ptr<ASTOpImageLayerColor> nop = mu::Clone<ASTOpImageLayerColor>(channelSourceAt);

				Ptr<ASTOpImageSwizzle> NewSwizzle = mu::Clone<ASTOpImageSwizzle>(this);
				ReplaceAllSources(NewSwizzle, nop->base.child());
				nop->base = NewSwizzle;

				// We need to swizzle the colour too
				Ptr<ASTOpFixed> cOp = new ASTOpFixed;
				cOp->op.type = OP_TYPE::CO_SWIZZLE;
				for (int i = 0; i < MUTABLE_OP_MAX_SWIZZLE_CHANNELS; ++i)
				{
					cOp->SetChild(cOp->op.args.ColourSwizzle.sources[i], nop->color);
					cOp->op.args.ColourSwizzle.sourceChannels[i] = SourceChannels[i];
				}
				nop->color = cOp;

				at = nop;
				break;
			}

			case OP_TYPE::IM_DISPLACE:
			{
				Ptr<ASTOpFixed> NewDisplace = mu::Clone<ASTOpFixed>(channelSourceAt);
				Ptr<ASTOpImageSwizzle> NewSwizzle = mu::Clone<ASTOpImageSwizzle>(this);
				ReplaceAllSources(NewSwizzle, NewDisplace->children[NewDisplace->op.args.ImageDisplace.source].child());
				NewDisplace->SetChild(NewDisplace->op.args.ImageDisplace.source, NewSwizzle);
				at = NewDisplace;
				break;
			}

			case OP_TYPE::IM_RASTERMESH:
			{
				Ptr<ASTOpImageRasterMesh> NewRaster = mu::Clone<ASTOpImageRasterMesh>(channelSourceAt);
				Ptr<ASTOpImageSwizzle> NewSwizzle = mu::Clone<ASTOpImageSwizzle>(this);
				ReplaceAllSources(NewSwizzle, NewRaster->image.child());
				NewRaster->image = NewSwizzle;
				at = NewRaster;
				break;
			}

			case OP_TYPE::IM_RESIZE:
			{
				Ptr<ASTOpFixed> NewResize = mu::Clone<ASTOpFixed>(channelSourceAt);
				Ptr<ASTOpImageSwizzle> NewSwizzle = mu::Clone<ASTOpImageSwizzle>(this);
				ReplaceAllSources(NewSwizzle, NewResize->children[NewResize->op.args.ImageResize.source].child());
				NewResize->SetChild(NewResize->op.args.ImageResize.source, NewSwizzle);
				at = NewResize;
				break;
			}

			// This is not valid: binarize always forces format L8
			//case OP_TYPE::IM_BINARISE:
			//{
			//	Ptr<ASTOpFixed> NewBinarise = mu::Clone<ASTOpFixed>(channelSourceAt);
			//	Ptr<ASTOpImageSwizzle> NewSwizzle = mu::Clone<ASTOpImageSwizzle>(this);
			//	ReplaceAllSources(NewSwizzle, NewBinarise->children[NewBinarise->op.args.ImageBinarise.base].child());
			//	NewBinarise->SetChild(NewBinarise->op.args.ImageBinarise.base, NewSwizzle);
			//	at = NewBinarise;
			//	break;
			//}

			case OP_TYPE::IM_PIXELFORMAT:
			{
				if (bSameChannelOrder)
				{
					Ptr<ASTOpImagePixelFormat> NewFormat = mu::Clone<ASTOpImagePixelFormat>(channelSourceAt);
					NewFormat->Format = Format;
					at = NewFormat;
				}
				break;
			}

			default:
				bAllChannelsAreTheSame = false;
				break;
			}
		}

		if (!bAllChannelsAreTheSame && bAllChannelsAreTheSameType)
		{
			// Maybe we can still sink the instruction in some cases

			// If we have RGB being the same IM_MULTILAYER, and alpha a compatible IM_MULTILAYER we can optimize with
			// a special multilayer blend mode. This happens often because of higher level group projector nodes.
			if (!at
				&&
				Format == EImageFormat::IF_RGBA_UBYTE
				&&
				Sources[0] == Sources[1] && Sources[0] == Sources[2]
				&&
				Sources[0] && Sources[0]->GetOpType() == OP_TYPE::IM_MULTILAYER
				&&
				Sources[3] && Sources[3]->GetOpType() == OP_TYPE::IM_MULTILAYER
				&&
				SourceChannels[0] == 0 && SourceChannels[1] == 1 && SourceChannels[2] == 2 && SourceChannels[3] == 0
				)
			{
				const ASTOpImageMultiLayer* ColorMultiLayer = dynamic_cast<const ASTOpImageMultiLayer*>(Sources[0].child().get());
				check(ColorMultiLayer);
				const ASTOpImageMultiLayer* AlphaMultiLayer = dynamic_cast<const ASTOpImageMultiLayer*>(Sources[3].child().get());
				check(AlphaMultiLayer);

				bool bIsSpecialMultiLayer = !AlphaMultiLayer->mask
					&&
					ColorMultiLayer->range == AlphaMultiLayer->range;

				if (bIsSpecialMultiLayer)
				{
					// We can combine the 2 multilayers into the composite blend+lighten mode

					Ptr<ASTOpImageSwizzle> NewBase = mu::Clone<ASTOpImageSwizzle>(this);
					NewBase->Sources[0] = ColorMultiLayer->base.child();
					NewBase->Sources[1] = ColorMultiLayer->base.child();
					NewBase->Sources[2] = ColorMultiLayer->base.child();
					NewBase->Sources[3] = AlphaMultiLayer->base.child();

					Ptr<ASTOpImageSwizzle> NewBlended = mu::Clone<ASTOpImageSwizzle>(this);
					NewBlended->Sources[0] = ColorMultiLayer->blend.child();
					NewBlended->Sources[1] = ColorMultiLayer->blend.child();
					NewBlended->Sources[2] = ColorMultiLayer->blend.child();
					NewBlended->Sources[3] = AlphaMultiLayer->blend.child();

					Ptr<ASTOpImageMultiLayer> NewMultiLayer = mu::Clone<ASTOpImageMultiLayer>(ColorMultiLayer);
					NewMultiLayer->blendTypeAlpha = AlphaMultiLayer->blendType;
					NewMultiLayer->base = NewBase;
					NewMultiLayer->blend = NewBlended;

					if ( NewMultiLayer->mask.child() == AlphaMultiLayer->blend.child()
						&&
						NewBlended->Format==EImageFormat::IF_RGBA_UBYTE
						)
					{
						// Additional optimization is possible here.
						NewMultiLayer->bUseMaskFromBlended = true;
						NewMultiLayer->mask = nullptr;
					}

					at = NewMultiLayer;
				}
			}

			// If we have RGB being the same IM_LAYER, and alpha a compatible IM_LAYER we can optimize with a special layer blend mode.
			if (!at
				&&
				Format == EImageFormat::IF_RGBA_UBYTE
				&&
				Sources[0] == Sources[1] && (Sources[0] == Sources[2] || !Sources[2])
				&&
				Sources[0] && Sources[0]->GetOpType() == OP_TYPE::IM_LAYER
				&&
				Sources[3] && Sources[3]->GetOpType() == OP_TYPE::IM_LAYER
				&&
				SourceChannels[0] == 0 && SourceChannels[1] == 1 && (SourceChannels[2] == 2 || !Sources[2] ) && SourceChannels[3] == 0
				)
			{
				const ASTOpImageLayer* ColorLayer = dynamic_cast<const ASTOpImageLayer*>(Sources[0].child().get());
				check(ColorLayer);
				const ASTOpImageLayer* AlphaLayer = dynamic_cast<const ASTOpImageLayer*>(Sources[3].child().get());
				check(AlphaLayer);

				bool bIsSpecialMultiLayer = !AlphaLayer->mask && !ColorLayer->Flags && !AlphaLayer->Flags;

				if (bIsSpecialMultiLayer)
				{
					// We can combine the 2 image_layers into the composite blend+lighten mode

					Ptr<ASTOpImageSwizzle> NewBase = mu::Clone<ASTOpImageSwizzle>(this);
					NewBase->Sources[0] = ColorLayer->base.child();
					NewBase->Sources[1] = ColorLayer->base.child();
					NewBase->Sources[2] = Sources[2] ? ColorLayer->base.child() : nullptr;
					NewBase->Sources[3] = AlphaLayer->base.child();

					Ptr<ASTOpImageSwizzle> NewBlended = mu::Clone<ASTOpImageSwizzle>(this);
					NewBlended->Sources[0] = ColorLayer->blend.child();
					NewBlended->Sources[1] = ColorLayer->blend.child();
					NewBlended->Sources[2] = Sources[2] ? ColorLayer->blend.child() : nullptr;
					NewBlended->Sources[3] = AlphaLayer->blend.child();

					Ptr<ASTOpImageLayer> NewLayer = mu::Clone<ASTOpImageLayer>(ColorLayer);
					NewLayer->blendTypeAlpha = AlphaLayer->blendType;
					NewLayer->base = NewBase;
					NewLayer->blend = NewBlended;

					if (NewLayer->mask.child() == AlphaLayer->blend.child()
						&&
						NewBlended->Format == EImageFormat::IF_RGBA_UBYTE
						)
					{
						// Additional optimization is possible here.
						NewLayer->Flags |= OP::ImageLayerArgs::FLAGS::F_USE_MASK_FROM_BLENDED;
						NewLayer->mask = nullptr;
					}

					at = NewLayer;
				}
			}


			// If the channels are compatible switches, we can still sink the swizzle.
			if (!at && sourceType == OP_TYPE::IM_SWITCH)
			{
				const ASTOpSwitch* FirstSwitch = dynamic_cast<const ASTOpSwitch*>(Sources[0].child().get());
				check(FirstSwitch);

				bool bAreAllSwitchesCompatible = true;
				for (int32 c = 1; c < MUTABLE_OP_MAX_SWIZZLE_CHANNELS; ++c)
				{
					if (Sources[c])
					{
						const ASTOpSwitch* Typed = dynamic_cast<const ASTOpSwitch*>(Sources[c].child().get());
						check(Typed);
						if (!Typed->IsCompatibleWith(FirstSwitch))
						{
							bAreAllSwitchesCompatible = false;
							break;
						}
					}
				}

				if (bAreAllSwitchesCompatible)
				{
					// Move the swizzle down all the paths
					Ptr<ASTOpSwitch> nop = mu::Clone<ASTOpSwitch>(channelSourceAt);

					if (nop->def)
					{
						Ptr<ASTOpImageSwizzle> defOp = mu::Clone<ASTOpImageSwizzle>(this);
						for (int c = 0; c < MUTABLE_OP_MAX_SWIZZLE_CHANNELS; ++c)
						{
							const ASTOpSwitch* ChannelSwitch = dynamic_cast<const ASTOpSwitch*>(Sources[c].child().get());
							if (ChannelSwitch)
							{
								defOp->Sources[c] = ChannelSwitch->def.child();
							}
						}
						nop->def = defOp;
					}

					for (int32 v = 0; v < nop->cases.Num(); ++v)
					{
						if (nop->cases[v].branch)
						{
							Ptr<ASTOpImageSwizzle> branchOp = mu::Clone<ASTOpImageSwizzle>(this);
							for (int c = 0; c < MUTABLE_OP_MAX_SWIZZLE_CHANNELS; ++c)
							{
								const ASTOpSwitch* ChannelSwitch = dynamic_cast<const ASTOpSwitch*>(Sources[c].child().get());
								if (ChannelSwitch)
								{
									branchOp->Sources[c] = ChannelSwitch->cases[v].branch.child();
								}
							}
							nop->cases[v].branch = branchOp;
						}
					}

					at = nop;
				}
			}

			// Swizzle down compatible displaces.
			if (!at && sourceType == OP_TYPE::IM_DISPLACE)
			{
				const ASTOpFixed* FirstDisplace = dynamic_cast<const ASTOpFixed*>(Sources[0].child().get());
				check(FirstDisplace);

				bool bAreAllDisplacesCompatible = true;
				for (int32 c = 1; c < MUTABLE_OP_MAX_SWIZZLE_CHANNELS; ++c)
				{
					if (Sources[c])
					{
						const ASTOpFixed* Typed = dynamic_cast<const ASTOpFixed*>(Sources[c].child().get());
						check(Typed);
						if (FirstDisplace->op.args.ImageDisplace.displacementMap != Typed->op.args.ImageDisplace.displacementMap)
						{
							bAreAllDisplacesCompatible = false;
							break;
						}
					}
				}

				if (bAreAllDisplacesCompatible)
				{
					// Move the swizzle down all the paths
					Ptr<ASTOpFixed> NewDisplace = mu::Clone<ASTOpFixed>(FirstDisplace);

					Ptr<ASTOpImageSwizzle> SourceOp = mu::Clone<ASTOpImageSwizzle>(this);
					for (int c = 0; c < MUTABLE_OP_MAX_SWIZZLE_CHANNELS; ++c)
					{
						const ASTOpFixed* ChannelDisplace = dynamic_cast<const ASTOpFixed*>(Sources[c].child().get());
						if (ChannelDisplace)
						{
							SourceOp->Sources[c] = ChannelDisplace->children[ChannelDisplace->op.args.ImageDisplace.source].child();
						}
					}

					NewDisplace->SetChild(NewDisplace->op.args.ImageDisplace.source, SourceOp);

					at = NewDisplace;
				}

			}

			// Swizzle down compatible raster meshes.
			if (!at && sourceType == OP_TYPE::IM_RASTERMESH)
			{
				const ASTOpImageRasterMesh* FirstRasterMesh = dynamic_cast<const ASTOpImageRasterMesh*>(Sources[0].child().get());
				check(FirstRasterMesh);

				bool bAreAllRasterMeshesCompatible = true;
				for (int32 c = 1; c < MUTABLE_OP_MAX_SWIZZLE_CHANNELS; ++c)
				{
					if (Sources[c])
					{
						const ASTOpImageRasterMesh* Typed = dynamic_cast<const ASTOpImageRasterMesh*>(Sources[c].child().get());
						check(Typed);

						// Compare all args but the source image
						if (Typed->mesh.child() != FirstRasterMesh->mesh.child()
							|| Typed->angleFadeProperties.child() != FirstRasterMesh->angleFadeProperties.child()
							|| Typed->mask.child() != FirstRasterMesh->mask.child()
							|| Typed->projector.child() != FirstRasterMesh->projector.child()
							|| Typed->blockIndex != FirstRasterMesh->blockIndex
							|| Typed->sizeX != FirstRasterMesh->sizeX
							|| Typed->sizeY != FirstRasterMesh->sizeY
							|| Typed->bIsRGBFadingEnabled != FirstRasterMesh->bIsRGBFadingEnabled
							|| Typed->bIsAlphaFadingEnabled != FirstRasterMesh->bIsAlphaFadingEnabled
							)
						{
							bAreAllRasterMeshesCompatible = false;
							break;
						}
					}
				}

				if (bAreAllRasterMeshesCompatible)
				{
					// Move the swizzle down all the paths
					Ptr<ASTOpImageRasterMesh> NewRaster = mu::Clone<ASTOpImageRasterMesh>(FirstRasterMesh);

					Ptr<ASTOpImageSwizzle> NewSwizzle = mu::Clone<ASTOpImageSwizzle>(this);
					for (int c = 0; c < MUTABLE_OP_MAX_SWIZZLE_CHANNELS; ++c)
					{
						const ASTOpImageRasterMesh* ChannelRaster = dynamic_cast<const ASTOpImageRasterMesh*>(Sources[c].child().get());
						if (ChannelRaster)
						{
							NewSwizzle->Sources[c] = ChannelRaster->image.child();
						}
					}

					NewRaster->image = NewSwizzle;

					at = NewRaster;
				}

			}

			// Swizzle down compatible resizes.
			if (!at && sourceType == OP_TYPE::IM_RESIZE)
			{
				const ASTOpFixed* FirstResize = dynamic_cast<const ASTOpFixed*>(Sources[0].child().get());
				check(FirstResize);

				bool bAreAllResizesCompatible = true;
				for (int32 c = 1; c < MUTABLE_OP_MAX_SWIZZLE_CHANNELS; ++c)
				{
					if (Sources[c])
					{
						const ASTOpFixed* Typed = dynamic_cast<const ASTOpFixed*>(Sources[c].child().get());
						check(Typed);
						// Compare all args but the source image
						OP::ImageResizeArgs ArgCopy = FirstResize->op.args.ImageResize;
						ArgCopy.source = Typed->op.args.ImageResize.source;

						if (FMemory::Memcmp(&ArgCopy, &Typed->op.args.ImageResize, sizeof(OP::ImageResizeArgs)) != 0)
						{
							bAreAllResizesCompatible = false;
							break;
						}
					}
				}

				if (bAreAllResizesCompatible)
				{
					// Move the swizzle down all the paths
					Ptr<ASTOpFixed> NewResize = mu::Clone<ASTOpFixed>(FirstResize);

					Ptr<ASTOpImageSwizzle> NewSwizzle = mu::Clone<ASTOpImageSwizzle>(this);
					for (int c = 0; c < MUTABLE_OP_MAX_SWIZZLE_CHANNELS; ++c)
					{
						const ASTOpFixed* ChannelResize = dynamic_cast<const ASTOpFixed*>(Sources[c].child().get());
						if (ChannelResize)
						{
							NewSwizzle->Sources[c] = ChannelResize->children[ChannelResize->op.args.ImageResize.source].child();
						}
					}

					NewResize->SetChild(NewResize->op.args.ImageResize.source, NewSwizzle);

					at = NewResize;
				}

			}

			// Swizzle down compatible pixelformats.
			if (!at && sourceType == OP_TYPE::IM_PIXELFORMAT && bSameChannelOrder)
			{
				const ASTOpImagePixelFormat* FirstFormat = dynamic_cast<const ASTOpImagePixelFormat*>(Sources[0].child().get());
				check(FirstFormat);

				bool bAreAllFormatsCompatible = true;
				for (int32 c = 1; c < MUTABLE_OP_MAX_SWIZZLE_CHANNELS; ++c)
				{
					if (Sources[c])
					{
						const ASTOpImagePixelFormat* Typed = dynamic_cast<const ASTOpImagePixelFormat*>(Sources[c].child().get());
						check(Typed);

						if (Typed->Source.child() != FirstFormat->Source.child())
						{
							bAreAllFormatsCompatible = false;
							break;
						}
					}
				}

				if (bAreAllFormatsCompatible)
				{
					// Move the swizzle down all the paths
					Ptr<ASTOpImagePixelFormat> NewFormat = mu::Clone<ASTOpImagePixelFormat>(FirstFormat);
					NewFormat->Format = Format;
					at = NewFormat;
				}
			}

			// Swizzle down plaincolours.
			if (!at && sourceType == OP_TYPE::IM_PLAINCOLOUR)
			{
				Ptr<ASTOpFixed> NewPlain = mu::Clone<ASTOpFixed>(channelSourceAt);
				Ptr<ASTOpFixed> NewSwizzle = new ASTOpFixed;
				NewSwizzle->op.type = OP_TYPE::CO_SWIZZLE;
				for (int c = 0; c < MUTABLE_OP_MAX_SWIZZLE_CHANNELS; ++c)
				{
					if (Sources[c])
					{
						const ASTOpFixed* TypedPlain = dynamic_cast<const ASTOpFixed*>(Sources[c].child().get());

						NewSwizzle->SetChild(NewSwizzle->op.args.ColourSwizzle.sources[c], TypedPlain->children[TypedPlain->op.args.ImagePlainColour.colour]);
					}
					NewSwizzle->op.args.ColourSwizzle.sourceChannels[c] = SourceChannels[c];
				}
				NewPlain->SetChild(NewPlain->op.args.ImagePlainColour.colour, NewSwizzle);
				NewPlain->op.args.ImagePlainColour.format = Format;
				at = NewPlain;
			}

		}

		// Swizzle of RGB from a source + A from a layer colour
		// This can be optimized to apply the layer colour on-base directly to the alpha channel to skip the swizzle
		if ( !at 
			&&
			Sources[0] && Sources[0]==Sources[1] && Sources[0]==Sources[2]
			&&
			Sources[3] && Sources[3]->GetOpType()==OP_TYPE::IM_LAYERCOLOUR )
		{
			// Move the swizzle down all the paths
			Ptr<ASTOpImageLayerColor> NewLayerColour = mu::Clone<ASTOpImageLayerColor>(Sources[3].child());

			Ptr<ASTOpImageSwizzle> NewSwizzle = mu::Clone<ASTOpImageSwizzle>(this);
			NewSwizzle->Sources[3] = NewLayerColour->base.child();

			NewLayerColour->blendTypeAlpha = NewLayerColour->blendType;
			NewLayerColour->blendType = EBlendType::BT_NONE;
			NewLayerColour->base = NewSwizzle;

			at = NewLayerColour;

		}

		// Swizzle of RGB from a layer colour + A from a different source
		// This can be optimized to apply the layer colour on-base directly to the rgb channel to skip the swizzle
		if (!at
			&&
			Sources[0] 
			&& 
			(!Sources[1] || Sources[0] == Sources[1]) 
			&& 
			(!Sources[2] || Sources[0] == Sources[2])
			&&
			Sources[0]->GetOpType() == OP_TYPE::IM_LAYERCOLOUR)
		{
			// Move the swizzle down all the rgb path
			Ptr<ASTOpImageLayerColor> NewLayerColour = mu::Clone<ASTOpImageLayerColor>(Sources[0].child());
			check(NewLayerColour);

			Ptr<ASTOpImageSwizzle> NewSwizzle = mu::Clone<ASTOpImageSwizzle>(this);
			NewSwizzle->Sources[0] = NewLayerColour->base.child();
			NewSwizzle->Sources[1] = Sources[1] ? NewLayerColour->base.child() : nullptr;
			NewSwizzle->Sources[2] = Sources[2] ? NewLayerColour->base.child() : nullptr;

			NewLayerColour->blendTypeAlpha = EBlendType::BT_NONE;
			NewLayerColour->base = NewSwizzle;

			at = NewLayerColour;
		}

		// Swizzle of RGB from a binarise + A from a different source
		// This can be optimized to apply the layer colour on-base directly to the rgb channel to skip the swizzle
		// This is not valid because binarise always forces L8 format.
		//if (!at
		//	&&
		//	Sources[0] && (!Sources[0] ||Sources[0] == Sources[1]) && (!Sources[2] || Sources[0] == Sources[2])
		//	&&
		//	Sources[0]->GetOpType() == OP_TYPE::IM_BINARISE)
		//{
		//	// Move the swizzle down all the rgb path
		//	Ptr<ASTOpFixed> NewBinarise = mu::Clone<ASTOpFixed>(Sources[0].child());
		//	check(NewBinarise);

		//	Ptr<ASTOpImageSwizzle> NewSwizzle = mu::Clone<ASTOpImageSwizzle>(this);
		//	NewSwizzle->Sources[0] = NewBinarise->children[NewBinarise->op.args.ImageBinarise.base].child();
		//	NewSwizzle->Sources[1] = Sources[1] ? NewSwizzle->Sources[0].child() : nullptr;
		//	NewSwizzle->Sources[2] = Sources[2] ? NewSwizzle->Sources[0].child() : nullptr;

		//	NewBinarise->SetChild(NewBinarise->op.args.ImageBinarise.base, NewSwizzle);

		//	at = NewBinarise;
		//}


		return at;
	}



	//!
	FImageDesc ASTOpImageSwizzle::GetImageDesc(bool returnBestOption, FGetImageDescContext* context) const
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

		if (Sources[0].child())
		{
			res = Sources[0]->GetImageDesc(returnBestOption, context);
			res.m_format = Format;
			check(res.m_format != EImageFormat::IF_NONE);
		}

		// Cache the result
		if (context)
		{
			context->m_results.Add(this, res);
		}

		return res;
	}


	void ASTOpImageSwizzle::GetLayoutBlockSize(int* pBlockX, int* pBlockY)
	{
		if (Sources[0].child())
		{
			// Assume the block size of the biggest mip
			Sources[0].child()->GetLayoutBlockSize(pBlockX, pBlockY);
		}
	}


	bool ASTOpImageSwizzle::IsImagePlainConstant(FVector4f& colour) const
	{
		// TODO: Maybe something could be done here.
		return false;
	}


	mu::Ptr<ImageSizeExpression> ASTOpImageSwizzle::GetImageSizeExpression() const
	{
		mu::Ptr<ImageSizeExpression> pRes;

		if (Sources[0].child())
		{
			pRes = Sources[0].child()->GetImageSizeExpression();
		}
		else
		{
			pRes = new ImageSizeExpression;
		}

		return pRes;
	}

}