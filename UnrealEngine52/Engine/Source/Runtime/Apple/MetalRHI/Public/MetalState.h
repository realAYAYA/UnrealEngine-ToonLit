// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	MetalState.h: Metal state definitions.
=============================================================================*/

#pragma once

#include "CoreMinimal.h"
#include "RHI.h"
THIRD_PARTY_INCLUDES_START
#include "mtlpp.hpp"
THIRD_PARTY_INCLUDES_END

class FMetalSampler : public mtlpp::SamplerState
{
public:
	FMetalSampler(ns::Ownership retain = ns::Ownership::Retain) : mtlpp::SamplerState(nullptr, nullptr, retain) { }
	FMetalSampler(ns::Protocol<id<MTLSamplerState>>::type handle, ns::Ownership retain = ns::Ownership::Retain)
	: mtlpp::SamplerState(handle, nullptr, retain) {}
	
	FMetalSampler(mtlpp::SamplerState&& rhs)
	: mtlpp::SamplerState((mtlpp::SamplerState&&)rhs)
	{
		
	}
	
	FMetalSampler(const FMetalSampler& rhs)
	: mtlpp::SamplerState(rhs)
	{
		
	}
	
	FMetalSampler(const SamplerState& rhs)
	: mtlpp::SamplerState(rhs)
	{
		
	}
	
	FMetalSampler(FMetalSampler&& rhs)
	: mtlpp::SamplerState((mtlpp::SamplerState&&)rhs)
	{
		
	}
	
	FMetalSampler& operator=(const FMetalSampler& rhs)
	{
		if (this != &rhs)
		{
			mtlpp::SamplerState::operator=(rhs);
		}
		return *this;
	}
	
	FMetalSampler& operator=(FMetalSampler&& rhs)
	{
		mtlpp::SamplerState::operator=((mtlpp::SamplerState&&)rhs);
		return *this;
	}
	
	inline bool operator==(FMetalSampler const& rhs) const
	{
		return GetPtr() == rhs.GetPtr();
	}
	
	inline bool operator!=(FMetalSampler const& rhs) const
	{
		return GetPtr() != rhs.GetPtr();
	}
	
	friend uint32 GetTypeHash(FMetalSampler const& Hash)
	{
		return GetTypeHash(Hash.GetPtr());
	}
};

class FMetalSamplerState : public FRHISamplerState
{
public:
	
	/** 
	 * Constructor/destructor
	 */
	FMetalSamplerState(mtlpp::Device Device, const FSamplerStateInitializerRHI& Initializer);
	~FMetalSamplerState();

	FMetalSampler State;
#if !PLATFORM_MAC
	FMetalSampler NoAnisoState;
#endif
};

class FMetalRasterizerState : public FRHIRasterizerState
{
public:

	/**
	 * Constructor/destructor
	 */
	FMetalRasterizerState(const FRasterizerStateInitializerRHI& Initializer);
	~FMetalRasterizerState();
	
	virtual bool GetInitializer(FRasterizerStateInitializerRHI& Init) override final;
	
	FRasterizerStateInitializerRHI State;
};

class FMetalDepthStencilState : public FRHIDepthStencilState
{
public:

	/**
	 * Constructor/destructor
	 */
	FMetalDepthStencilState(mtlpp::Device Device, const FDepthStencilStateInitializerRHI& Initializer);
	~FMetalDepthStencilState();
	
	virtual bool GetInitializer(FDepthStencilStateInitializerRHI& Init) override final;
	
	FDepthStencilStateInitializerRHI Initializer;
	mtlpp::DepthStencilState State;
	bool bIsDepthWriteEnabled;
	bool bIsStencilWriteEnabled;
};

class FMetalBlendState : public FRHIBlendState
{
public:

	/**
	 * Constructor/destructor
	 */
	FMetalBlendState(const FBlendStateInitializerRHI& Initializer);
	~FMetalBlendState();
	
	virtual bool GetInitializer(FBlendStateInitializerRHI& Init) override final;

	struct FBlendPerMRT
	{
		mtlpp::RenderPipelineColorAttachmentDescriptor BlendState;
		uint8 BlendStateKey;
	};
	FBlendPerMRT RenderTargetStates[MaxSimultaneousRenderTargets];
	bool bUseIndependentRenderTargetBlendStates;
	bool bUseAlphaToCoverage;

private:
	// this tracks blend settings (in a bit flag) into a unique key that uses few bits, for PipelineState MRT setup
	static TMap<uint32, uint8> BlendSettingsToUniqueKeyMap;
	static uint8 NextKey;
	static FCriticalSection Mutex;
};
