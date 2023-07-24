// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	MetalGraphicsPipelineState.cpp: Metal RHI graphics pipeline state class.
=============================================================================*/


#include "MetalRHIPrivate.h"
#include "MetalVertexDeclaration.h"
#include "MetalShaderTypes.h"
#include "MetalGraphicsPipelineState.h"


//------------------------------------------------------------------------------

#pragma mark - Metal Graphics Pipeline State Support Routines


// From MetalPipeline.cpp:
extern FMetalShaderPipeline* GetMTLRenderPipeline(bool const bSync, FMetalGraphicsPipelineState const* State, const FGraphicsPipelineStateInitializer& Init);
extern void ReleaseMTLRenderPipeline(FMetalShaderPipeline* Pipeline);


//------------------------------------------------------------------------------

#pragma mark - Metal Graphics Pipeline State Class Implementation


FMetalGraphicsPipelineState::FMetalGraphicsPipelineState(const FGraphicsPipelineStateInitializer& Init)
	: Initializer(Init)
	, PipelineState(nil)
{
	// void
}

bool FMetalGraphicsPipelineState::Compile()
{
	check(PipelineState == nil);
	PipelineState = [GetMTLRenderPipeline(true, this, Initializer) retain];
	return (PipelineState != nil);
}

FMetalGraphicsPipelineState::~FMetalGraphicsPipelineState()
{
	if (PipelineState != nil)
	{
		ReleaseMTLRenderPipeline(PipelineState);
		PipelineState = nil;
	}
}

FMetalShaderPipeline* FMetalGraphicsPipelineState::GetPipeline()
{
	if (PipelineState == nil)
	{
		PipelineState = [GetMTLRenderPipeline(true, this, Initializer) retain];
		check(PipelineState != nil);
	}

    return PipelineState;
}
