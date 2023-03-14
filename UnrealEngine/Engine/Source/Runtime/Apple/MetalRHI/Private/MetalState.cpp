// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	MetalState.cpp: Metal state implementation.
=============================================================================*/

#include "MetalRHIPrivate.h"
#include "MetalProfiler.h"

static uint32 GetMetalMaxAnisotropy(ESamplerFilter Filter, uint32 MaxAniso)
{
	switch (Filter)
	{
		case SF_AnisotropicPoint:
		case SF_AnisotropicLinear:	return ComputeAnisotropyRT(MaxAniso);
		default:					return 1;
	}
}

static mtlpp::SamplerMinMagFilter TranslateZFilterMode(ESamplerFilter Filter)
{	switch (Filter)
	{
		case SF_Point:				return mtlpp::SamplerMinMagFilter::Nearest;
		case SF_AnisotropicPoint:	return mtlpp::SamplerMinMagFilter::Nearest;
		case SF_AnisotropicLinear:	return mtlpp::SamplerMinMagFilter::Linear;
		default:					return mtlpp::SamplerMinMagFilter::Linear;
	}
}

static mtlpp::SamplerAddressMode TranslateWrapMode(ESamplerAddressMode AddressMode)
{
	switch (AddressMode)
	{
		case AM_Clamp:	return mtlpp::SamplerAddressMode::ClampToEdge;
		case AM_Mirror: return mtlpp::SamplerAddressMode::MirrorRepeat;
		case AM_Border: return mtlpp::SamplerAddressMode::ClampToEdge;
		default:		return mtlpp::SamplerAddressMode::Repeat;
	}
}

static mtlpp::CompareFunction TranslateCompareFunction(ECompareFunction CompareFunction)
{
	switch(CompareFunction)
	{
		case CF_Less:			return mtlpp::CompareFunction::Less;
		case CF_LessEqual:		return mtlpp::CompareFunction::LessEqual;
		case CF_Greater:		return mtlpp::CompareFunction::Greater;
		case CF_GreaterEqual:	return mtlpp::CompareFunction::GreaterEqual;
		case CF_Equal:			return mtlpp::CompareFunction::Equal;
		case CF_NotEqual:		return mtlpp::CompareFunction::NotEqual;
		case CF_Never:			return mtlpp::CompareFunction::Never;
		default:				return mtlpp::CompareFunction::Always;
	};
}

static mtlpp::CompareFunction TranslateSamplerCompareFunction(ESamplerCompareFunction SamplerComparisonFunction)
{
	switch(SamplerComparisonFunction)
	{
		case SCF_Less:		return mtlpp::CompareFunction::Less;
		case SCF_Never:		return mtlpp::CompareFunction::Never;
		default:			return mtlpp::CompareFunction::Never;
	};
}

static mtlpp::StencilOperation TranslateStencilOp(EStencilOp StencilOp)
{
	switch(StencilOp)
	{
		case SO_Zero:				return mtlpp::StencilOperation::Zero;
		case SO_Replace:			return mtlpp::StencilOperation::Replace;
		case SO_SaturatedIncrement:	return mtlpp::StencilOperation::IncrementClamp;
		case SO_SaturatedDecrement:	return mtlpp::StencilOperation::DecrementClamp;
		case SO_Invert:				return mtlpp::StencilOperation::Invert;
		case SO_Increment:			return mtlpp::StencilOperation::IncrementWrap;
		case SO_Decrement:			return mtlpp::StencilOperation::DecrementWrap;
		default:					return mtlpp::StencilOperation::Keep;
	};
}

static mtlpp::BlendOperation TranslateBlendOp(EBlendOperation BlendOp)
{
	switch(BlendOp)
	{
		case BO_Subtract:	return mtlpp::BlendOperation::Subtract;
		case BO_Min:		return mtlpp::BlendOperation::Min;
		case BO_Max:		return mtlpp::BlendOperation::Max;
		default:			return mtlpp::BlendOperation::Add;
	};
}


static mtlpp::BlendFactor TranslateBlendFactor(EBlendFactor BlendFactor)
{
	switch(BlendFactor)
	{
		case BF_One:					return mtlpp::BlendFactor::One;
		case BF_SourceColor:			return mtlpp::BlendFactor::SourceColor;
		case BF_InverseSourceColor:		return mtlpp::BlendFactor::OneMinusSourceColor;
		case BF_SourceAlpha:			return mtlpp::BlendFactor::SourceAlpha;
		case BF_InverseSourceAlpha:		return mtlpp::BlendFactor::OneMinusSourceAlpha;
		case BF_DestAlpha:				return mtlpp::BlendFactor::DestinationAlpha;
		case BF_InverseDestAlpha:		return mtlpp::BlendFactor::OneMinusDestinationAlpha;
		case BF_DestColor:				return mtlpp::BlendFactor::DestinationColor;
		case BF_InverseDestColor:		return mtlpp::BlendFactor::OneMinusDestinationColor;
		case BF_Source1Color:			return mtlpp::BlendFactor::Source1Color;
		case BF_InverseSource1Color:	return mtlpp::BlendFactor::OneMinusSource1Color;
		case BF_Source1Alpha:			return mtlpp::BlendFactor::Source1Alpha;
		case BF_InverseSource1Alpha:	return mtlpp::BlendFactor::OneMinusSource1Alpha;
		default:						return mtlpp::BlendFactor::Zero;
	};
}

static mtlpp::ColorWriteMask TranslateWriteMask(EColorWriteMask WriteMask)
{
	uint32 Result = 0;
	Result |= (WriteMask & CW_RED) ? (mtlpp::ColorWriteMask::Red) : 0;
	Result |= (WriteMask & CW_GREEN) ? (mtlpp::ColorWriteMask::Green) : 0;
	Result |= (WriteMask & CW_BLUE) ? (mtlpp::ColorWriteMask::Blue) : 0;
	Result |= (WriteMask & CW_ALPHA) ? (mtlpp::ColorWriteMask::Alpha) : 0;
	
	return (mtlpp::ColorWriteMask)Result;
}

static EBlendOperation TranslateBlendOp(MTLBlendOperation BlendOp)
{
	switch(BlendOp)
	{
		case MTLBlendOperationSubtract:		return BO_Subtract;
		case MTLBlendOperationMin:			return BO_Min;
		case MTLBlendOperationMax:			return BO_Max;
		case MTLBlendOperationAdd: default:	return BO_Add;
	};
}


static EBlendFactor TranslateBlendFactor(MTLBlendFactor BlendFactor)
{
	switch(BlendFactor)
	{
		case MTLBlendFactorOne:							return BF_One;
		case MTLBlendFactorSourceColor:					return BF_SourceColor;
		case MTLBlendFactorOneMinusSourceColor:			return BF_InverseSourceColor;
		case MTLBlendFactorSourceAlpha:					return BF_SourceAlpha;
		case MTLBlendFactorOneMinusSourceAlpha:			return BF_InverseSourceAlpha;
		case MTLBlendFactorDestinationAlpha:			return BF_DestAlpha;
		case MTLBlendFactorOneMinusDestinationAlpha:	return BF_InverseDestAlpha;
		case MTLBlendFactorDestinationColor:			return BF_DestColor;
		case MTLBlendFactorOneMinusDestinationColor:	return BF_InverseDestColor;
		case MTLBlendFactorSource1Color:				return BF_Source1Color;
		case MTLBlendFactorOneMinusSource1Color:		return BF_InverseSource1Color;
		case MTLBlendFactorSource1Alpha:				return BF_Source1Alpha;
		case MTLBlendFactorOneMinusSource1Alpha:		return BF_InverseSource1Alpha;
		case MTLBlendFactorZero: default:				return BF_Zero;
	};
}

static EColorWriteMask TranslateWriteMask(MTLColorWriteMask WriteMask)
{
	uint32 Result = 0;
	Result |= (WriteMask & MTLColorWriteMaskRed) ? (CW_RED) : 0;
	Result |= (WriteMask & MTLColorWriteMaskGreen) ? (CW_GREEN) : 0;
	Result |= (WriteMask & MTLColorWriteMaskBlue) ? (CW_BLUE) : 0;
	Result |= (WriteMask & MTLColorWriteMaskAlpha) ? (CW_ALPHA) : 0;
	
	return (EColorWriteMask)Result;
}

template <typename InitializerType, typename StateType>
class FMetalStateObjectCache
{
public:
	FMetalStateObjectCache()
	{
		
	}
	
	~FMetalStateObjectCache()
	{
		
	}
	
	StateType Find(InitializerType Init)
	{
		if(IsRunningRHIInSeparateThread())
		{
			Mutex.ReadLock();
		}
		
		StateType* State = Cache.Find(Init);
		
		if(IsRunningRHIInSeparateThread())
		{
			Mutex.ReadUnlock();
		}
		
		return State ? *State : StateType(nullptr);
	}
	
	void Add(InitializerType Init, StateType const& State)
	{
		if(IsRunningRHIInSeparateThread())
		{
			Mutex.WriteLock();
		}
		
		Cache.Add(Init, State);
		
		if(IsRunningRHIInSeparateThread())
		{
			Mutex.WriteUnlock();
		}
	}
	
private:
	TMap<InitializerType, StateType> Cache;
	FRWLock Mutex;
};

static FMetalStateObjectCache<FSamplerStateInitializerRHI, FMetalSampler> Samplers;

static FMetalSampler FindOrCreateSamplerState(mtlpp::Device Device, const FSamplerStateInitializerRHI& Initializer)
{
	FMetalSampler State = Samplers.Find(Initializer);
	if (!State.GetPtr())
	{
		mtlpp::SamplerDescriptor Desc;
		switch(Initializer.Filter)
		{
			case SF_AnisotropicLinear:
			case SF_AnisotropicPoint:
				Desc.SetMinFilter(mtlpp::SamplerMinMagFilter::Linear);
				Desc.SetMagFilter(mtlpp::SamplerMinMagFilter::Linear);
				Desc.SetMipFilter(mtlpp::SamplerMipFilter::Linear);
				break;
			case SF_Trilinear:
				Desc.SetMinFilter(mtlpp::SamplerMinMagFilter::Linear);
				Desc.SetMagFilter(mtlpp::SamplerMinMagFilter::Linear);
				Desc.SetMipFilter(mtlpp::SamplerMipFilter::Linear);
				break;
			case SF_Bilinear:
				Desc.SetMinFilter(mtlpp::SamplerMinMagFilter::Linear);
				Desc.SetMagFilter(mtlpp::SamplerMinMagFilter::Linear);
				Desc.SetMipFilter(mtlpp::SamplerMipFilter::Nearest);
				break;
			case SF_Point:
				Desc.SetMinFilter(mtlpp::SamplerMinMagFilter::Nearest);
				Desc.SetMagFilter(mtlpp::SamplerMinMagFilter::Nearest);
				Desc.SetMipFilter(mtlpp::SamplerMipFilter::Nearest);
				break;
		}
		Desc.SetMaxAnisotropy(GetMetalMaxAnisotropy(Initializer.Filter, Initializer.MaxAnisotropy));
		Desc.SetSAddressMode(TranslateWrapMode(Initializer.AddressU));
		Desc.SetTAddressMode(TranslateWrapMode(Initializer.AddressV));
		Desc.SetRAddressMode(TranslateWrapMode(Initializer.AddressW));
		Desc.SetLodMinClamp(Initializer.MinMipLevel);
		Desc.SetLodMaxClamp(Initializer.MaxMipLevel);
#if PLATFORM_TVOS
		Desc.SetCompareFunction(mtlpp::CompareFunction::Never);	
#elif PLATFORM_IOS
		Desc.SetCompareFunction(Device.SupportsFeatureSet(mtlpp::FeatureSet::iOS_GPUFamily3_v1) ? TranslateSamplerCompareFunction(Initializer.SamplerComparisonFunction) : mtlpp::CompareFunction::Never);
#else
		Desc.SetCompareFunction(TranslateSamplerCompareFunction(Initializer.SamplerComparisonFunction));
#endif
#if PLATFORM_MAC
		Desc.SetBorderColor(Initializer.BorderColor == 0 ? mtlpp::SamplerBorderColor::TransparentBlack : mtlpp::SamplerBorderColor::OpaqueWhite);
#endif
		if (FMetalCommandQueue::SupportsFeature(EMetalFeaturesIABs))
		{
			Desc.SetSupportArgumentBuffers(true);
		}
		
		State = Device.NewSamplerState(Desc);
		
		Samplers.Add(Initializer, State);
	}
	return State;
}

FMetalSamplerState::FMetalSamplerState(mtlpp::Device Device, const FSamplerStateInitializerRHI& Initializer)
{
	State = FindOrCreateSamplerState(Device, Initializer);
#if !PLATFORM_MAC
	if (GetMetalMaxAnisotropy(Initializer.Filter, Initializer.MaxAnisotropy))
	{
		FSamplerStateInitializerRHI Init = Initializer;
		Init.MaxAnisotropy = 1;
		NoAnisoState = FindOrCreateSamplerState(Device, Init);
	}
#endif
}

FMetalSamplerState::~FMetalSamplerState()
{
}

FMetalRasterizerState::FMetalRasterizerState(const FRasterizerStateInitializerRHI& Initializer)
{
	State = Initializer;
}

FMetalRasterizerState::~FMetalRasterizerState()
{
	
}

bool FMetalRasterizerState::GetInitializer(FRasterizerStateInitializerRHI& Init)
{
	Init = State;
	return true;
}

static FMetalStateObjectCache<FDepthStencilStateInitializerRHI, mtlpp::DepthStencilState> DepthStencilStates;

FMetalDepthStencilState::FMetalDepthStencilState(mtlpp::Device Device, const FDepthStencilStateInitializerRHI& InInitializer)
{
	Initializer = InInitializer;

	State = DepthStencilStates.Find(Initializer);
	if (!State.GetPtr())
	{
		mtlpp::DepthStencilDescriptor Desc;
		
		Desc.SetDepthCompareFunction(TranslateCompareFunction(Initializer.DepthTest));
		Desc.SetDepthWriteEnabled(Initializer.bEnableDepthWrite);
		
		if (Initializer.bEnableFrontFaceStencil)
		{
			// set up front face stencil operations
			mtlpp::StencilDescriptor Stencil;
			Stencil.SetStencilCompareFunction(TranslateCompareFunction(Initializer.FrontFaceStencilTest));
			Stencil.SetStencilFailureOperation(TranslateStencilOp(Initializer.FrontFaceStencilFailStencilOp));
			Stencil.SetDepthFailureOperation(TranslateStencilOp(Initializer.FrontFaceDepthFailStencilOp));
			Stencil.SetDepthStencilPassOperation(TranslateStencilOp(Initializer.FrontFacePassStencilOp));
			Stencil.SetReadMask(Initializer.StencilReadMask);
			Stencil.SetWriteMask(Initializer.StencilWriteMask);
			Desc.SetFrontFaceStencil(Stencil);
		}
		
		if (Initializer.bEnableBackFaceStencil)
		{
			// set up back face stencil operations
			mtlpp::StencilDescriptor Stencil;
			Stencil.SetStencilCompareFunction(TranslateCompareFunction(Initializer.BackFaceStencilTest));
			Stencil.SetStencilFailureOperation(TranslateStencilOp(Initializer.BackFaceStencilFailStencilOp));
			Stencil.SetDepthFailureOperation(TranslateStencilOp(Initializer.BackFaceDepthFailStencilOp));
			Stencil.SetDepthStencilPassOperation(TranslateStencilOp(Initializer.BackFacePassStencilOp));
			Stencil.SetReadMask(Initializer.StencilReadMask);
			Stencil.SetWriteMask(Initializer.StencilWriteMask);
			Desc.SetBackFaceStencil(Stencil);
		}
		else if(Initializer.bEnableFrontFaceStencil)
		{
			// set up back face stencil operations to front face in single-face mode
			mtlpp::StencilDescriptor Stencil;
			Stencil.SetStencilCompareFunction(TranslateCompareFunction(Initializer.FrontFaceStencilTest));
			Stencil.SetStencilFailureOperation(TranslateStencilOp(Initializer.FrontFaceStencilFailStencilOp));
			Stencil.SetDepthFailureOperation(TranslateStencilOp(Initializer.FrontFaceDepthFailStencilOp));
			Stencil.SetDepthStencilPassOperation(TranslateStencilOp(Initializer.FrontFacePassStencilOp));
			Stencil.SetReadMask(Initializer.StencilReadMask);
			Stencil.SetWriteMask(Initializer.StencilWriteMask);
			Desc.SetBackFaceStencil(Stencil);
		}
		
		// bake out the descriptor
		State = Device.NewDepthStencilState(Desc);
		
		DepthStencilStates.Add(Initializer, State);
	}
	
	// cache some pipeline state info
	bIsDepthWriteEnabled = Initializer.bEnableDepthWrite;
	bIsStencilWriteEnabled = Initializer.bEnableFrontFaceStencil || Initializer.bEnableBackFaceStencil;
}

FMetalDepthStencilState::~FMetalDepthStencilState()
{
}

bool FMetalDepthStencilState::GetInitializer(FDepthStencilStateInitializerRHI& Init)
{
	Init = Initializer;
	return true;
}



// statics
static FMetalStateObjectCache<FBlendStateInitializerRHI::FRenderTarget, mtlpp::RenderPipelineColorAttachmentDescriptor> BlendStates;
TMap<uint32, uint8> FMetalBlendState::BlendSettingsToUniqueKeyMap;
uint8 FMetalBlendState::NextKey = 0;
FCriticalSection FMetalBlendState::Mutex;

FMetalBlendState::FMetalBlendState(const FBlendStateInitializerRHI& Initializer)
{
	bUseIndependentRenderTargetBlendStates = Initializer.bUseIndependentRenderTargetBlendStates;
	bUseAlphaToCoverage = Initializer.bUseAlphaToCoverage;
	for(uint32 RenderTargetIndex = 0;RenderTargetIndex < MaxSimultaneousRenderTargets; ++RenderTargetIndex)
	{
		// which initializer to use
		const FBlendStateInitializerRHI::FRenderTarget& Init =
			Initializer.bUseIndependentRenderTargetBlendStates
				? Initializer.RenderTargets[RenderTargetIndex]
				: Initializer.RenderTargets[0];

		// make a new blend state
		mtlpp::RenderPipelineColorAttachmentDescriptor BlendState = BlendStates.Find(Init);

		if (!BlendState.GetPtr())
		{
			BlendState = mtlpp::RenderPipelineColorAttachmentDescriptor();
			
			// set values
			BlendState.SetBlendingEnabled(
				Init.ColorBlendOp != BO_Add || Init.ColorDestBlend != BF_Zero || Init.ColorSrcBlend != BF_One ||
				Init.AlphaBlendOp != BO_Add || Init.AlphaDestBlend != BF_Zero || Init.AlphaSrcBlend != BF_One);
			BlendState.SetSourceRgbBlendFactor(TranslateBlendFactor(Init.ColorSrcBlend));
			BlendState.SetDestinationRgbBlendFactor(TranslateBlendFactor(Init.ColorDestBlend));
			BlendState.SetRgbBlendOperation(TranslateBlendOp(Init.ColorBlendOp));
			BlendState.SetSourceAlphaBlendFactor(TranslateBlendFactor(Init.AlphaSrcBlend));
			BlendState.SetDestinationAlphaBlendFactor(TranslateBlendFactor(Init.AlphaDestBlend));
			BlendState.SetAlphaBlendOperation(TranslateBlendOp(Init.AlphaBlendOp));
			BlendState.SetWriteMask(TranslateWriteMask(Init.ColorWriteMask));
			
			BlendStates.Add(Init, BlendState);
		}
		
		RenderTargetStates[RenderTargetIndex].BlendState = BlendState;

		// get the unique key
		uint32 BlendBitMask =
			((uint32)BlendState.GetSourceRgbBlendFactor() << 0) | ((uint32)BlendState.GetDestinationRgbBlendFactor() << 4) | ((uint32)BlendState.GetRgbBlendOperation() << 8) |
			((uint32)BlendState.GetSourceAlphaBlendFactor() << 11) | ((uint32)BlendState.GetDestinationAlphaBlendFactor() << 15) | ((uint32)BlendState.GetAlphaBlendOperation() << 19) |
			((uint32)BlendState.GetWriteMask() << 22);
		
		
		if(IsRunningRHIInSeparateThread())
		{
			Mutex.Lock();
		}
		uint8* Key = BlendSettingsToUniqueKeyMap.Find(BlendBitMask);
		if (Key == NULL)
		{
			Key = &BlendSettingsToUniqueKeyMap.Add(BlendBitMask, NextKey++);

			// only giving limited bits to the key, since we need to pack 8 of them into a key
			checkf(NextKey < (1 << NumBits_BlendState), TEXT("Too many unique blend states to fit into the PipelineStateHash [%d allowed]"), 1 << NumBits_BlendState);
		}
		// set the key
		RenderTargetStates[RenderTargetIndex].BlendStateKey = *Key;
		if(IsRunningRHIInSeparateThread())
		{
			Mutex.Unlock();
		}
	}
}

FMetalBlendState::~FMetalBlendState()
{
}

bool FMetalBlendState::GetInitializer(FBlendStateInitializerRHI& Initializer)
{
	Initializer.bUseIndependentRenderTargetBlendStates = bUseIndependentRenderTargetBlendStates;
	Initializer.bUseAlphaToCoverage = bUseAlphaToCoverage;
	for(uint32 RenderTargetIndex = 0;RenderTargetIndex < MaxSimultaneousRenderTargets; ++RenderTargetIndex)
	{
		// which initializer to use
		FBlendStateInitializerRHI::FRenderTarget& Init = Initializer.RenderTargets[RenderTargetIndex];
		MTLRenderPipelineColorAttachmentDescriptor* CurrentState = RenderTargetStates[RenderTargetIndex].BlendState;
		
		if (CurrentState)
		{
			Init.ColorSrcBlend = TranslateBlendFactor(CurrentState.sourceRGBBlendFactor);
			Init.ColorDestBlend = TranslateBlendFactor(CurrentState.destinationRGBBlendFactor);
			Init.ColorBlendOp = TranslateBlendOp(CurrentState.rgbBlendOperation);
			Init.AlphaSrcBlend = TranslateBlendFactor(CurrentState.sourceAlphaBlendFactor);
			Init.AlphaDestBlend = TranslateBlendFactor(CurrentState.destinationAlphaBlendFactor);
			Init.AlphaBlendOp = TranslateBlendOp(CurrentState.alphaBlendOperation);
			Init.ColorWriteMask = TranslateWriteMask(CurrentState.writeMask);
		}
		
		if (!bUseIndependentRenderTargetBlendStates)
		{
			break;
		}
	}
	
	return true;
}





FSamplerStateRHIRef FMetalDynamicRHI::RHICreateSamplerState(const FSamplerStateInitializerRHI& Initializer)
{
    @autoreleasepool {
	return new FMetalSamplerState(ImmediateContext.Context->GetDevice(), Initializer);
	}
}

FRasterizerStateRHIRef FMetalDynamicRHI::RHICreateRasterizerState(const FRasterizerStateInitializerRHI& Initializer)
{
	@autoreleasepool {
    return new FMetalRasterizerState(Initializer);
	}
}

FDepthStencilStateRHIRef FMetalDynamicRHI::RHICreateDepthStencilState(const FDepthStencilStateInitializerRHI& Initializer)
{
	@autoreleasepool {
	return new FMetalDepthStencilState(ImmediateContext.Context->GetDevice(), Initializer);
	}
}


FBlendStateRHIRef FMetalDynamicRHI::RHICreateBlendState(const FBlendStateInitializerRHI& Initializer)
{
	@autoreleasepool {
	return new FMetalBlendState(Initializer);
	}
}

