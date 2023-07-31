// Copyright Epic Games, Inc. All Rights Reserved.

/*==============================================================================
	MetalDynamicRHIModule.cpp: Metal Dynamic RHI Module Class Implementation.
==============================================================================*/


#include "MetalLLM.h"
#include "DynamicRHI.h"
#include "MetalDynamicRHIModule.h"
#include "Modules/ModuleManager.h"


//------------------------------------------------------------------------------

#pragma mark - Metal Dynamic RHI Module Implementation


IMPLEMENT_MODULE(FMetalDynamicRHIModule, MetalRHI);


//------------------------------------------------------------------------------

#pragma mark - Metal Dynamic RHI Module Class Methods


bool FMetalDynamicRHIModule::IsSupported()
{
	return true;
}

FDynamicRHI* FMetalDynamicRHIModule::CreateRHI(ERHIFeatureLevel::Type RequestedFeatureLevel)
{
	LLM(MetalLLM::Initialise());
	return new FMetalDynamicRHI(RequestedFeatureLevel);
}
