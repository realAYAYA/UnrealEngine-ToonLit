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
#include "MuT/ASTOpImageTransform.h"
#include "MuT/ASTOpImageMipmap.h"
#include "MuT/ASTOpSwitch.h"
#include "MuT/StreamsPrivate.h"

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
		if (otherUntyped.GetOpType()==GetOpType())
		{
			const ASTOpImageSwizzle* Other = static_cast<const ASTOpImageSwizzle*>(&otherUntyped);
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


	void ASTOpImageSwizzle::Link(FProgram& program, FLinkerOptions*)
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
			program.m_opAddress.Add((uint32)program.m_byteCode.Num());
			AppendCode(program.m_byteCode, GetOpType());
			AppendCode(program.m_byteCode, args);
		}
	}


	mu::Ptr<ASTOp> ASTOpImageSwizzle::OptimiseSemantic(const FModelOptimizationOptions&, int32 Pass) const
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
				const ASTOpImageSwizzle* typedCandidate = static_cast<const ASTOpImageSwizzle*>(candidate.get());
				int candidateChannel = SourceChannels[c];

				sat->Sources[c] = typedCandidate->Sources[candidateChannel].child();
				sat->SourceChannels[c] = typedCandidate->SourceChannels[candidateChannel];

				break;
			}

			// Format
			case OP_TYPE::IM_PIXELFORMAT:
			{
				// We can remove the format if its source is already an uncompressed format
				ASTOpImagePixelFormat* typedCandidate = static_cast<ASTOpImagePixelFormat*>(candidate.get());
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
		Ptr<ASTOp> at = OptimiseSemantic(options, 0);
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

		if (!channelSourceAt)
		{
			return at;
		}

		// If we are not changing channel order, just remove the swizzle and adjust the format.
		bool bSameChannelOrder = true;
		int32 NumChannelsInFormat = GetImageFormatData(Format).Channels;
		for (int32 c = 0; c < NumChannelsInFormat; ++c)
		{
			if (Sources[c] && SourceChannels[c] != c)
			{
				bSameChannelOrder = false;
			}
		}
		
		// If all channels are the same, and in the same order, and the source format is the same that we are
		// setting in the swizzle, then the swizzle won't do anything.
		if (bAllChannelsAreTheSame && bSameChannelOrder)
		{
			FImageDesc SourceDesc = channelSourceAt->GetImageDesc();
			if (SourceDesc.m_format == Format)
			{
				return channelSourceAt;
			}
		}

		OP_TYPE sourceType = channelSourceAt->GetOpType();

		if (bAllChannelsAreTheSame)
		{
			at = context.ImageSwizzleSinker.Apply(this);
		}

		if (!at && bAllChannelsAreTheSameType)
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
				const ASTOpImageMultiLayer* ColorMultiLayer = static_cast<const ASTOpImageMultiLayer*>(Sources[0].child().get());
				check(ColorMultiLayer);
				const ASTOpImageMultiLayer* AlphaMultiLayer = static_cast<const ASTOpImageMultiLayer*>(Sources[3].child().get());
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
					NewMultiLayer->BlendAlphaSourceChannel = 3;
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
				const ASTOpImageLayer* ColorLayer = static_cast<const ASTOpImageLayer*>(Sources[0].child().get());
				check(ColorLayer);
				const ASTOpImageLayer* AlphaLayer = static_cast<const ASTOpImageLayer*>(Sources[3].child().get());
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
					NewLayer->BlendAlphaSourceChannel = 3;
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
				const ASTOpSwitch* FirstSwitch = static_cast<const ASTOpSwitch*>(Sources[0].child().get());
				check(FirstSwitch);

				bool bAreAllSwitchesCompatible = true;
				for (int32 c = 1; c < MUTABLE_OP_MAX_SWIZZLE_CHANNELS; ++c)
				{
					if (Sources[c])
					{
						const ASTOpSwitch* Typed = static_cast<const ASTOpSwitch*>(Sources[c].child().get());
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
							const ASTOpSwitch* ChannelSwitch = static_cast<const ASTOpSwitch*>(Sources[c].child().get());
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
								const ASTOpSwitch* ChannelSwitch = static_cast<const ASTOpSwitch*>(Sources[c].child().get());
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
				const ASTOpFixed* FirstDisplace = static_cast<const ASTOpFixed*>(Sources[0].child().get());
				check(FirstDisplace);

				bool bAreAllDisplacesCompatible = true;
				for (int32 c = 1; c < MUTABLE_OP_MAX_SWIZZLE_CHANNELS; ++c)
				{
					if (Sources[c])
					{
						const ASTOpFixed* Typed = static_cast<const ASTOpFixed*>(Sources[c].child().get());
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
						const ASTOpFixed* ChannelDisplace = static_cast<const ASTOpFixed*>(Sources[c].child().get());
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
				const ASTOpImageRasterMesh* FirstRasterMesh = static_cast<const ASTOpImageRasterMesh*>(Sources[0].child().get());
				check(FirstRasterMesh);

				bool bAreAllRasterMeshesCompatible = true;
				for (int32 c = 1; c < MUTABLE_OP_MAX_SWIZZLE_CHANNELS; ++c)
				{
					if (Sources[c])
					{
						const ASTOpImageRasterMesh* Typed = static_cast<const ASTOpImageRasterMesh*>(Sources[c].child().get());
						check(Typed);

						// Compare all args but the source image
						if (Typed->mesh.child() != FirstRasterMesh->mesh.child()
							|| Typed->angleFadeProperties.child() != FirstRasterMesh->angleFadeProperties.child()
							|| Typed->mask.child() != FirstRasterMesh->mask.child()
							|| Typed->projector.child() != FirstRasterMesh->projector.child()
							|| Typed->BlockId != FirstRasterMesh->BlockId
							|| Typed->LayoutIndex != FirstRasterMesh->LayoutIndex
							|| Typed->SizeX != FirstRasterMesh->SizeX
							|| Typed->SizeY != FirstRasterMesh->SizeY
							|| Typed->UncroppedSizeX != FirstRasterMesh->UncroppedSizeX
							|| Typed->UncroppedSizeY != FirstRasterMesh->UncroppedSizeY
							|| Typed->CropMinX != FirstRasterMesh->CropMinX
							|| Typed->CropMinY != FirstRasterMesh->CropMinY
							// Also ignore the fading flags. They are dealt with below.
							//|| Typed->bIsRGBFadingEnabled != FirstRasterMesh->bIsRGBFadingEnabled
							//|| Typed->bIsAlphaFadingEnabled != FirstRasterMesh->bIsAlphaFadingEnabled
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
						const ASTOpImageRasterMesh* ChannelRaster = static_cast<const ASTOpImageRasterMesh*>(Sources[c].child().get());
						if (ChannelRaster)
						{
							NewSwizzle->Sources[c] = ChannelRaster->image.child();
						}
					}

					NewRaster->image = NewSwizzle;

					// If we are swapping rgb and alphas, we need to correct some flags
					{
						// We should only find these two cases
						if (SourceChannels[0] == 3 || SourceChannels[1] == 3 || SourceChannels[2] == 3)
						{
							NewRaster->bIsRGBFadingEnabled = NewRaster->bIsAlphaFadingEnabled;
						}
						else if (Sources[3] && SourceChannels[3] < 3)
						{
							const ASTOpImageRasterMesh* ChannelRaster = static_cast<const ASTOpImageRasterMesh*>(Sources[3].child().get());
							NewRaster->bIsAlphaFadingEnabled = ChannelRaster->bIsRGBFadingEnabled;
						}
					}

					at = NewRaster;
				}

			}

			// Swizzle down compatible image transforms.
			if (!at && sourceType == OP_TYPE::IM_TRANSFORM)
			{
				const ASTOpImageTransform* FirstTransform = static_cast<const ASTOpImageTransform*>(Sources[0].child().get());
				check(FirstTransform);

				bool bAreAllTransformsCompatible = true;
				for (int32 c = 1; c < MUTABLE_OP_MAX_SWIZZLE_CHANNELS; ++c)
				{
					if (Sources[c])
					{
						const ASTOpImageTransform* Typed = static_cast<const ASTOpImageTransform*>(Sources[c].child().get());
						check(Typed);

						// Compare all args but the source image
						if (Typed->ScaleX.child() != FirstTransform->ScaleX.child()
							|| Typed->ScaleY.child() != FirstTransform->ScaleY.child()
							|| Typed->OffsetX.child() != FirstTransform->OffsetX.child()
							|| Typed->OffsetY.child() != FirstTransform->OffsetY.child()
							|| Typed->Rotation.child() != FirstTransform->Rotation.child()
							|| Typed->SizeX != FirstTransform->SizeX
							|| Typed->SizeY != FirstTransform->SizeY
							|| Typed->SourceSizeX != FirstTransform->SourceSizeX
							|| Typed->SourceSizeY != FirstTransform->SourceSizeY
							|| Typed->AddressMode != FirstTransform->AddressMode
							|| Typed->bKeepAspectRatio != FirstTransform->bKeepAspectRatio
							)
						{
							bAreAllTransformsCompatible = false;
							break;
						}
					}
				}

				if (bAreAllTransformsCompatible)
				{
					// Move the swizzle down all the paths
					Ptr<ASTOpImageTransform> NewTransform = mu::Clone<ASTOpImageTransform>(FirstTransform);

					Ptr<ASTOpImageSwizzle> NewSwizzle = mu::Clone<ASTOpImageSwizzle>(this);
					for (int c = 0; c < MUTABLE_OP_MAX_SWIZZLE_CHANNELS; ++c)
					{
						const ASTOpImageTransform* ChannelTransform = static_cast<const ASTOpImageTransform*>(Sources[c].child().get());
						if (ChannelTransform)
						{
							NewSwizzle->Sources[c] = ChannelTransform->Base.child();
						}
					}

					NewTransform->Base = NewSwizzle;

					at = NewTransform;
				}
			}

			// Swizzle down compatible resizes.
			//if (!at && sourceType == OP_TYPE::IM_RESIZE)
			//{
			//	const ASTOpFixed* FirstResize = static_cast<const ASTOpFixed*>(Sources[0].child().get());
			//	check(FirstResize);

			//	bool bAreAllResizesCompatible = true;
			//	for (int32 c = 1; c < MUTABLE_OP_MAX_SWIZZLE_CHANNELS; ++c)
			//	{
			//		if (Sources[c])
			//		{
			//			const ASTOpFixed* Typed = static_cast<const ASTOpFixed*>(Sources[c].child().get());
			//			check(Typed);
			//			// Compare all args but the source image
			//			OP::ImageResizeArgs ArgCopy = FirstResize->op.args.ImageResize;
			//			ArgCopy.source = Typed->op.args.ImageResize.source;

			//			if (FMemory::Memcmp(&ArgCopy, &Typed->op.args.ImageResize, sizeof(OP::ImageResizeArgs)) != 0)
			//			{
			//				bAreAllResizesCompatible = false;
			//				break;
			//			}
			//		}
			//	}

			//	if (bAreAllResizesCompatible)
			//	{
			//		// Move the swizzle down all the paths
			//		Ptr<ASTOpFixed> NewResize = mu::Clone<ASTOpFixed>(FirstResize);

			//		Ptr<ASTOpImageSwizzle> NewSwizzle = mu::Clone<ASTOpImageSwizzle>(this);
			//		for (int c = 0; c < MUTABLE_OP_MAX_SWIZZLE_CHANNELS; ++c)
			//		{
			//			const ASTOpFixed* ChannelResize = static_cast<const ASTOpFixed*>(Sources[c].child().get());
			//			if (ChannelResize)
			//			{
			//				NewSwizzle->Sources[c] = ChannelResize->children[ChannelResize->op.args.ImageResize.source].child();
			//			}
			//		}

			//		NewResize->SetChild(NewResize->op.args.ImageResize.source, NewSwizzle);

			//		at = NewResize;
			//	}

			//}

			// Swizzle down compatible pixelformats.
			if (!at && sourceType == OP_TYPE::IM_PIXELFORMAT && bSameChannelOrder)
			{
				const ASTOpImagePixelFormat* FirstFormat = static_cast<const ASTOpImagePixelFormat*>(Sources[0].child().get());
				check(FirstFormat);

				bool bAreAllFormatsCompatible = true;
				for (int32 c = 1; c < MUTABLE_OP_MAX_SWIZZLE_CHANNELS; ++c)
				{
					if (Sources[c])
					{
						const ASTOpImagePixelFormat* Typed = static_cast<const ASTOpImagePixelFormat*>(Sources[c].child().get());
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
						const ASTOpFixed* TypedPlain = static_cast<const ASTOpFixed*>(Sources[c].child().get());

						NewSwizzle->SetChild(NewSwizzle->op.args.ColourSwizzle.sources[c], TypedPlain->children[TypedPlain->op.args.ImagePlainColour.colour]);
					}
					NewSwizzle->op.args.ColourSwizzle.sourceChannels[c] = SourceChannels[c];
				}
				NewPlain->SetChild(NewPlain->op.args.ImagePlainColour.colour, NewSwizzle);
				NewPlain->op.args.ImagePlainColour.format = Format;
				at = NewPlain;
			}

		}

		// TODO \warning: probably wrong because it doesn't check if the layer colour is doing a separated alpha operation.
		// Swizzle of RGB from a source + A from a layer colour
		// This can be optimized to apply the layer colour on-base directly to the alpha channel to skip the swizzle
		//if ( !at 
		//	&&
		//	Sources[0] && Sources[0]==Sources[1] && Sources[0]==Sources[2]
		//	&&
		//	Sources[3] && Sources[3]->GetOpType()==OP_TYPE::IM_LAYERCOLOUR
		//	)
		//{
		//	// Move the swizzle down all the paths
		//	Ptr<ASTOpImageLayerColor> NewLayerColour = mu::Clone<ASTOpImageLayerColor>(Sources[3].child());

		//	Ptr<ASTOpImageSwizzle> NewSwizzle = mu::Clone<ASTOpImageSwizzle>(this);
		//	NewSwizzle->Sources[3] = NewLayerColour->base.child();

		//	NewLayerColour->blendTypeAlpha = NewLayerColour->blendType;
		//	NewLayerColour->BlendAlphaSourceChannel = SourceChannels[3];
		//	NewLayerColour->blendType = EBlendType::BT_NONE;
		//	NewLayerColour->base = NewSwizzle;

		//	at = NewLayerColour;

		//}

		// Swizzle of RGB from a source + A from a layer
		// This can be optimized to apply the layer on-base directly to the alpha channel to skip the swizzle
		// \TODO: wrong: the new layer colour will always use the alpha from the colour, instead of the channel that the swizzle is selecting.
		// \TODO: wrong: it ignores the possibility of separate alpha operation
		//if (!at
		//	&&
		//	Sources[0] && Sources[0] == Sources[1] && Sources[0] == Sources[2]
		//	&&
		//	Sources[3] && Sources[3]->GetOpType() == OP_TYPE::IM_LAYER)
		//{
		//	// Move the swizzle down all the paths
		//	Ptr<ASTOpImageLayer> NewLayer = mu::Clone<ASTOpImageLayer>(Sources[3].child());

		//	Ptr<ASTOpImageSwizzle> NewSwizzle = mu::Clone<ASTOpImageSwizzle>(this);
		//	NewSwizzle->Sources[3] = NewLayer->base.child();

		//	NewLayer->blendTypeAlpha = NewLayer->blendType;
		//	NewLayer->BlendAlphaSourceChannel = SourceChannels[3];
		//	NewLayer->blendType = EBlendType::BT_NONE;
		//	NewLayer->base = NewSwizzle;

		//	at = NewLayer;
		//}

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
			Sources[0]->GetOpType() == OP_TYPE::IM_LAYERCOLOUR
			&& 
			!(Sources[3]==Sources[0]) )
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

		// Swizzle getting an A from a saturate
		// The saturate doesn't affect A channel so it can be removed.
		if (!at)
		{
			Ptr<ASTOpImageSwizzle> NewSwizzle;

			for (int32 Channel = 0; Channel < MUTABLE_OP_MAX_SWIZZLE_CHANNELS; ++Channel)
			{
				if (Sources[Channel] && SourceChannels[Channel]==3 && Sources[Channel]->GetOpType() == OP_TYPE::IM_SATURATE)
				{
					// Remove the saturate for this channel
					if (!NewSwizzle)
					{
						NewSwizzle = mu::Clone<ASTOpImageSwizzle>(this);
					}

					const ASTOpFixed* OldSaturate = static_cast<const ASTOpFixed*>(Sources[Channel].child().get());
					Ptr<ASTOp> OldSaturateBase = OldSaturate->children[OldSaturate->op.args.ImageSaturate.base].child();

					NewSwizzle->Sources[Channel] = OldSaturateBase;
					at = NewSwizzle;
				}
			}
		}

		// Swizzle of RGB from a saturate + A from a different source
		// This can be optimized to apply the saturate after the swizzle, since it doesn't touch A
		if (!at
			&&
			Sources[0]->GetOpType() == OP_TYPE::IM_SATURATE 
			&&
			Sources[0]
			&&
			(Sources[0] == Sources[1]) && (Sources[0] == Sources[2])
			&&
			// Actually it would be enough with all the RGB channels to be present in any order
			SourceChannels[0]==0 && SourceChannels[1] == 1 && SourceChannels[2] == 2
			)
		{
			// Move the swizzle down 
			Ptr<ASTOpFixed> NewSaturate = mu::Clone<ASTOpFixed>(Sources[0].child());
			check(NewSaturate);

			Ptr<ASTOpImageSwizzle> NewSwizzle = mu::Clone<ASTOpImageSwizzle>(this);
			Ptr<ASTOp> OldSaturateBase = NewSaturate->children[NewSaturate->op.args.ImageSaturate.base].child();
			NewSwizzle->Sources[0] = OldSaturateBase;
			NewSwizzle->Sources[1] = OldSaturateBase;
			NewSwizzle->Sources[2] = OldSaturateBase;

			// Remove the saturate from the alpha if it is there.
			if (Sources[3] == Sources[0] && SourceChannels[3] == 3)
			{
				NewSwizzle->Sources[3] = OldSaturateBase;
			}

			NewSaturate->SetChild(NewSaturate->op.args.ImageSaturate.base, NewSwizzle);

			at = NewSaturate;
		}
		
		// Swizzle with the same op as identity in RGB, a Layer op in the A that has one of the operands matching 
		// the one in the swizzle RGB, but using its A. 
		// The Layer operation can be flagged as alpha only and moved up the swizzle, then the swizzle is identity 
		// and can be removed, so remove it here anyway.
		// This is another very specific optimization case that happens with certain combination of operations.
		// from:
		//- SWIZZLE
		//	r -> r from A
		//	g -> g from A
		//	b -> b from A
		//	a -> (r or a) from LAYER
		//		- 3 from A (on alpha only using flags)
		//		- B
		// to:
		//- LAYER (on alpha only)
		//	- A
		//	- B
		// In addition, if the blend operation done by LAYER is commutative, see if X is 3 from I instead.
		if (!at
			&&
			(Sources[0] == Sources[1]) && (Sources[0] == Sources[2])
			&&
			SourceChannels[0] == 0 && SourceChannels[1] == 1 && SourceChannels[2] == 2
			&&
			Sources[3] && Sources[3]->GetOpType() == OP_TYPE::IM_LAYER
			)
		{
			const ASTOpImageLayer* OldLayer = static_cast<const ASTOpImageLayer*>(Sources[3].child().get());

			Ptr<ASTOp> SwizzleRGBOp = Sources[0].child();
			Ptr<ASTOp> OldLayerBlendOp = OldLayer->blend.child();
			{
				auto DiscardNeutralOps = [](Ptr<ASTOp> Op)
				{
					bool bUpdated = true;
					while (bUpdated)
					{
						bUpdated = false;
						switch (Op->GetOpType())
						{
						case OP_TYPE::IM_PIXELFORMAT:
						{
							const ASTOpImagePixelFormat* Typed = static_cast<const ASTOpImagePixelFormat*>(Op.get());
							Op = Typed->Source.child();
							bUpdated = true;
							break;
						}

						default: break;
						}
					}
					return Op;
				};

				SwizzleRGBOp = DiscardNeutralOps(SwizzleRGBOp);
				OldLayerBlendOp = DiscardNeutralOps(OldLayerBlendOp);
			}
			bool bOldLayerBlendIsCompatibleWithSwizzleRGBs = OldLayerBlendOp == SwizzleRGBOp;

			// For now just check the case that we are observing in the working data: 
			// A is in the blended of a multiply, and we take its alpha channel
			// \TODO: Implement the other cases when we find instances of them.
			if ( OldLayer->Flags==OP::ImageLayerArgs::FLAGS::F_BLENDED_RGB_FROM_ALPHA
				&&
				bOldLayerBlendIsCompatibleWithSwizzleRGBs
				&& 
				OldLayer->blendType==EBlendType::BT_MULTIPLY 
				&&
				OldLayer->blendTypeAlpha == EBlendType::BT_NONE 
				&&
				SourceChannels[3]==0)
			{
				// The new base needs to have the format of the root swizzle
				Ptr<ASTOpImagePixelFormat> NewBase = new ASTOpImagePixelFormat;
				NewBase->Source = Sources[0].child();
				NewBase->Format = Format;

				Ptr<ASTOpImageLayer> NewLayer = mu::Clone<ASTOpImageLayer>(OldLayer);
				NewLayer->blend = OldLayer->base.child();
				NewLayer->base = NewBase;
				NewLayer->blendTypeAlpha = NewLayer->blendType;
				NewLayer->blendType = EBlendType::BT_NONE;
				NewLayer->BlendAlphaSourceChannel = 0;
				NewLayer->Flags = 0;

				at = NewLayer;
			}
		}


		// If we have an alpha channel that has as children something that expands a single channel texture
		// skip the expansion,. since we know we just want one channel.
		// Very specific, based on observed code patterns.
		// \TODO: Make more general.
		if (!at
			&&
			Sources[3] && Sources[3]->GetOpType() == OP_TYPE::IM_LAYER
			)
		{
			ASTOpImageLayer* OldLayer = static_cast<ASTOpImageLayer*>(Sources[3].child().get());

			// For now just check the case that we are observing in the working data: 
			if (OldLayer->Flags == 0
				&&
				SourceChannels[3] == 0
				&&
				OldLayer->blend->GetOpType()==OP_TYPE::IM_PIXELFORMAT )
			{
				const ASTOpImagePixelFormat* OldFormat = static_cast<const ASTOpImagePixelFormat*>(OldLayer->blend.child().get());
				if (OldFormat->Source->GetOpType() == OP_TYPE::IM_SWIZZLE
					&&
					(
						OldFormat->Format == EImageFormat::IF_RGB_UBYTE
						||
						OldFormat->Format == EImageFormat::IF_RGBA_UBYTE
						) 
					)
				{
					const ASTOpImageSwizzle* OldChildSwizzle = static_cast<const ASTOpImageSwizzle*>(OldFormat->Source.child().get());
					if (OldChildSwizzle->Format == EImageFormat::IF_L_UBYTE)
					{
						Ptr<ASTOpImageSwizzle> NewBaseSwizzle = new ASTOpImageSwizzle;
						NewBaseSwizzle->Format = EImageFormat::IF_L_UBYTE;
						NewBaseSwizzle->Sources[0] = OldLayer->base.child();
						NewBaseSwizzle->SourceChannels[0] = SourceChannels[3];

						Ptr<ASTOpImageSwizzle> NewBlendSwizzle = new ASTOpImageSwizzle;
						NewBlendSwizzle->Format = EImageFormat::IF_L_UBYTE;
						NewBlendSwizzle->Sources[0] = OldLayer->blend.child();
						NewBlendSwizzle->SourceChannels[0] = SourceChannels[3];

						Ptr<ASTOpImageLayer> NewLayer = mu::Clone<ASTOpImageLayer>(OldLayer);
						NewLayer->base = NewBaseSwizzle;
						NewLayer->blend = NewBlendSwizzle;

						Ptr<ASTOpImageSwizzle> NewSwizzle = mu::Clone<ASTOpImageSwizzle>(this);
						NewSwizzle->Sources[3] = NewLayer;

						at = NewSwizzle;
					}
				}
			}
		}


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




	//---------------------------------------------------------------------------------------------
	//---------------------------------------------------------------------------------------------
	//---------------------------------------------------------------------------------------------
	mu::Ptr<ASTOp> Sink_ImageSwizzleAST::Apply(const ASTOpImageSwizzle* InRoot)
	{
		Root = InRoot;
		OldToNew.Reset();

		check(Root->GetOpType() == OP_TYPE::IM_SWIZZLE);

		// This sinker only works assuming all swizzle channels come from the same image operation.
		bool bAllChannelsAreTheSame = true;
		Ptr<ASTOp> Source;
		for (int c = 0; c < MUTABLE_OP_MAX_SWIZZLE_CHANNELS; ++c)
		{
			Ptr<ASTOp> Candidate = Root->Sources[c].child();
			if (Candidate)
			{
				if (!Source)
				{
					Source = Candidate;
				}
				else
				{
					bAllChannelsAreTheSame = bAllChannelsAreTheSame && (Source == Candidate);
				}
			}
		}

		if (!bAllChannelsAreTheSame || !Source)
		{
			return nullptr;
		}

		InitialSource = Source;
		mu::Ptr<ASTOp> NewSource = Visit(InitialSource, Root);

		Root = nullptr;

		// If there is any change, it is the new root.
		if (NewSource != InitialSource)
		{
			return NewSource;
		}

		return nullptr;
	}


	//---------------------------------------------------------------------------------------------
	mu::Ptr<ASTOp> Sink_ImageSwizzleAST::Visit(mu::Ptr<ASTOp> at, const ASTOpImageSwizzle* CurrentSwizzleOp)
	{
		if (!at) return nullptr;

		// Already visited?
		const Ptr<ASTOp>* Cached = OldToNew.Find({ at,CurrentSwizzleOp });
		if (Cached)
		{
			return *Cached;
		}

		mu::Ptr<ASTOp> newAt = at;
		switch (at->GetOpType())
		{

		case OP_TYPE::IM_CONDITIONAL:
		{
			// We move the op down the two paths
			auto newOp = mu::Clone<ASTOpConditional>(at);
			newOp->yes = Visit(newOp->yes.child(), CurrentSwizzleOp);
			newOp->no = Visit(newOp->no.child(), CurrentSwizzleOp);
			newAt = newOp;
			break;
		}

		case OP_TYPE::IM_SWITCH:
		{
			// We move the op down all the paths
			Ptr<ASTOpSwitch> newOp = mu::Clone<ASTOpSwitch>(at);
			newOp->def = Visit(newOp->def.child(), CurrentSwizzleOp);
			for (auto& c : newOp->cases)
			{
				c.branch = Visit(c.branch.child(), CurrentSwizzleOp);
			}
			newAt = newOp;
			break;
		}

		case OP_TYPE::IM_COMPOSE:
		{
			Ptr<ASTOpImageCompose> newOp = mu::Clone<ASTOpImageCompose>(at);
			newOp->Base = Visit(newOp->Base.child(), CurrentSwizzleOp);
			newOp->BlockImage = Visit(newOp->BlockImage.child(), CurrentSwizzleOp);
			newAt = newOp;
			break;
		}

		case OP_TYPE::IM_PATCH:
		{
			Ptr<ASTOpImagePatch> newOp = mu::Clone<ASTOpImagePatch>(at);
			newOp->base = Visit(newOp->base.child(), CurrentSwizzleOp);
			newOp->patch = Visit(newOp->patch.child(), CurrentSwizzleOp);
			newAt = newOp;
			break;
		}

		case OP_TYPE::IM_MIPMAP:
		{
			Ptr<ASTOpImageMipmap> newOp = mu::Clone<ASTOpImageMipmap>(at);
			newOp->Source = Visit(newOp->Source.child(), CurrentSwizzleOp);
			newAt = newOp;
			break;
		}

		case OP_TYPE::IM_INTERPOLATE:
		{
			Ptr<ASTOpFixed> newOp = mu::Clone<ASTOpFixed>(at);

			for (int32 v = 0; v < MUTABLE_OP_MAX_INTERPOLATE_COUNT; ++v)
			{
				Ptr<ASTOp> child = newOp->children[newOp->op.args.ImageInterpolate.targets[v]].child();
				Ptr<ASTOp> bOp = Visit(child, CurrentSwizzleOp);
				newOp->SetChild(newOp->op.args.ImageInterpolate.targets[v], bOp);
			}

			newAt = newOp;
			break;
		}

		case OP_TYPE::IM_LAYER:
		{
			Ptr<ASTOpImageLayer> nop = mu::Clone<ASTOpImageLayer>(at);
			nop->base = Visit(nop->base.child(), CurrentSwizzleOp);
			nop->blend = Visit(nop->blend.child(), CurrentSwizzleOp);
			newAt = nop;
			break;
		}

		case OP_TYPE::IM_LAYERCOLOUR:
		{
			Ptr<ASTOpImageLayerColor> nop = mu::Clone<ASTOpImageLayerColor>(at);
			nop->base = Visit(nop->base.child(), CurrentSwizzleOp);

			// We need to swizzle the colour too
			Ptr<ASTOpFixed> NewColorOp = new ASTOpFixed;
			NewColorOp->op.type = OP_TYPE::CO_SWIZZLE;
			for (int i = 0; i < MUTABLE_OP_MAX_SWIZZLE_CHANNELS; ++i)
			{
				NewColorOp->SetChild(NewColorOp->op.args.ColourSwizzle.sources[i], nop->color);
				NewColorOp->op.args.ColourSwizzle.sourceChannels[i] = CurrentSwizzleOp->SourceChannels[i];
			}
			nop->color = NewColorOp;

			newAt = nop;
			break;
		}

		case OP_TYPE::IM_DISPLACE:
		{
			Ptr<ASTOpFixed> NewOp = mu::Clone<ASTOpFixed>(at);

			Ptr<ASTOp> Child = NewOp->children[NewOp->op.args.ImageDisplace.source].child();
			Ptr<ASTOp> NewSource = Visit(Child, CurrentSwizzleOp);
			NewOp->SetChild(NewOp->op.args.ImageDisplace.source, NewSource);

			newAt = NewOp;
			break;
		}

		case OP_TYPE::IM_INVERT:
		{
			Ptr<ASTOpFixed> NewOp = mu::Clone<ASTOpFixed>(at);

			Ptr<ASTOp> Child = NewOp->children[NewOp->op.args.ImageInvert.base].child();
			Ptr<ASTOp> NewSource = Visit(Child, CurrentSwizzleOp);
			NewOp->SetChild(NewOp->op.args.ImageInvert.base, NewSource);

			newAt = NewOp;
			break;
		}

		case OP_TYPE::IM_RASTERMESH:
		{
			Ptr<ASTOpImageRasterMesh> NewOp = mu::Clone<ASTOpImageRasterMesh>(at);

			NewOp->image = Visit(NewOp->image.child(), CurrentSwizzleOp);

			// If we are swapping rgb and alphas, we need to correct some flags
			{
				// We should only find these two cases
				if (CurrentSwizzleOp->SourceChannels[0] == 3 || CurrentSwizzleOp->SourceChannels[1] == 3 || CurrentSwizzleOp->SourceChannels[2] == 3)
				{
					NewOp->bIsRGBFadingEnabled = NewOp->bIsAlphaFadingEnabled;
				}
				else if (CurrentSwizzleOp->Sources[3] && CurrentSwizzleOp->SourceChannels[3] < 3)
				{
					NewOp->bIsAlphaFadingEnabled = NewOp->bIsRGBFadingEnabled;
				}
			}

			newAt = NewOp;
			break;
		}

		case OP_TYPE::IM_TRANSFORM:
		{
			Ptr<ASTOpImageTransform> nop = mu::Clone<ASTOpImageTransform>(at);
			nop->Base = Visit(nop->Base.child(), CurrentSwizzleOp);
			newAt = nop;
			break;
		}

		case OP_TYPE::IM_PIXELFORMAT:
		{
			// If we are not changing channel order, just remove the swizzle and adjust the format.
			bool bSameChannelOrder = true;
			int32 NumChannelsInFormat = GetImageFormatData(CurrentSwizzleOp->Format).Channels;
			for (int32 c = 0; c < NumChannelsInFormat; ++c)
			{
				if (CurrentSwizzleOp->Sources[c] && CurrentSwizzleOp->SourceChannels[c] != c)
				{
					bSameChannelOrder = false;
				}
			}

			if (bSameChannelOrder)
			{
				Ptr<ASTOpImagePixelFormat> NewOp = mu::Clone<ASTOpImagePixelFormat>(at);
				NewOp->Format = CurrentSwizzleOp->Format;
				newAt = NewOp;
			}
			break;
		}

		case OP_TYPE::IM_BLANKLAYOUT:
		{
			Ptr<ASTOpFixed> NewOp = mu::Clone<ASTOpFixed>(at);

			// We can remove the swizzle entirely.
			// It is not 100% equivalent, because blank layouts are initialized with 0,0,0,1 so the result could be
			// different, but those pixels shouldn't be used anyway.

			newAt = NewOp;
			break;
		}

		default:
			break;
		}

		// end on tree branch, replace with format
		if (at == newAt && at != InitialSource)
		{
			mu::Ptr<ASTOpImageSwizzle> NewOp = mu::Clone<ASTOpImageSwizzle>(CurrentSwizzleOp);
			check(NewOp->GetOpType() == OP_TYPE::IM_SWIZZLE);

			ReplaceAllSources(NewOp, at);

			newAt = NewOp;
		}

		OldToNew.Add({ at, CurrentSwizzleOp }, newAt);

		return newAt;
	}


}
