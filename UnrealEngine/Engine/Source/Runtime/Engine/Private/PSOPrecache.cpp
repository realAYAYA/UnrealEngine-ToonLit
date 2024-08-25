// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	PSOPrecache.cpp: 
=============================================================================*/

#include "PSOPrecache.h"
#include "Misc/App.h"
#include "HAL/IConsoleManager.h"

static TAutoConsoleVariable<int32> CVarPrecacheGlobalComputeShaders(
	TEXT("r.PSOPrecache.GlobalComputeShaders"),
	0,
	TEXT("Precache all global compute shaders during startup (default 0)."),
	ECVF_ReadOnly
);

int32 GPSOPrecacheComponents = 1;
static FAutoConsoleVariableRef CVarPSOPrecacheComponents(
	TEXT("r.PSOPrecache.Components"),
	GPSOPrecacheComponents,
	TEXT("Precache all possible used PSOs by components during Postload (default 1 if PSOPrecaching is enabled)."),
	ECVF_ReadOnly
);

int32 GPSOPrecacheResources = 0;
static FAutoConsoleVariableRef CVarPSOPrecacheResources(
	TEXT("r.PSOPrecache.Resources"),
	GPSOPrecacheResources,
	TEXT("Precache all possible used PSOs by resources during Postload (default 0 if PSOPrecaching is enabled)."),
	ECVF_ReadOnly
);

int32 GPSOProxyCreationWhenPSOReady = 1;
static FAutoConsoleVariableRef CVarPSOProxyCreationWhenPSOReady(
	TEXT("r.PSOPrecache.ProxyCreationWhenPSOReady"),
	GPSOProxyCreationWhenPSOReady,
	TEXT("Delay the component proxy creation when the requested PSOs for precaching are still compiling.\n")
	TEXT(" 0: always create regardless of PSOs status (default)\n")
	TEXT(" 1: delay the creation of the render proxy depending on the specific strategy controlled by r.PSOPrecache.ProxyCreationDelayStrategy\n"),
	ECVF_ReadOnly
);

int32 GPSOProxyCreationDelayStrategy = 0;
static FAutoConsoleVariableRef CVarPSOProxyCreationDelayStrategy(
	TEXT("r.PSOPrecache.ProxyCreationDelayStrategy"),
	GPSOProxyCreationDelayStrategy,
	TEXT("Control the component proxy creation strategy when the requested PSOs for precaching are still compiling. Ignored if r.PSOPrecache.ProxyCreationWhenPSOReady = 0.\n")
	TEXT(" 0: delay creation until PSOs are ready (default)\n")
	TEXT(" 1: create a proxy using the default material until PSOs are ready. Currently implemented for static and skinned meshes - Niagara components will delay creation instead"),
	ECVF_ReadOnly
);

bool IsComponentPSOPrecachingEnabled()
{
	return FApp::CanEverRender() && PipelineStateCache::IsPSOPrecachingEnabled() && GPSOPrecacheComponents && !GIsEditor;
}

bool IsResourcePSOPrecachingEnabled()
{
	return FApp::CanEverRender() && PipelineStateCache::IsPSOPrecachingEnabled() && GPSOPrecacheResources && !GIsEditor;
}

EPSOPrecacheProxyCreationStrategy GetPSOPrecacheProxyCreationStrategy()
{
	if (GPSOProxyCreationWhenPSOReady != 1)
	{
		return EPSOPrecacheProxyCreationStrategy::AlwaysCreate;
	}

	switch (GPSOProxyCreationDelayStrategy)
	{
	case 1:
		return EPSOPrecacheProxyCreationStrategy::UseDefaultMaterialUntilPSOPrecached;
	case 0:
		[[fallthrough]];
	default:
		return EPSOPrecacheProxyCreationStrategy::DelayUntilPSOPrecached;
	}
}

bool ProxyCreationWhenPSOReady()
{
	return FApp::CanEverRender() && PipelineStateCache::IsPSOPrecachingEnabled() && GPSOProxyCreationWhenPSOReady && !GIsEditor;
}

FPSOPrecacheVertexFactoryData::FPSOPrecacheVertexFactoryData(
	const FVertexFactoryType* InVertexFactoryType, const FVertexDeclarationElementList& ElementList) 
	: VertexFactoryType(InVertexFactoryType)
{
	CustomDefaultVertexDeclaration = PipelineStateCache::GetOrCreateVertexDeclaration(ElementList);
}

void AddMaterialInterfacePSOPrecacheParamsToList(const FMaterialInterfacePSOPrecacheParams& EntryToAdd, FMaterialInterfacePSOPrecacheParamsList& List)
{
	FMaterialInterfacePSOPrecacheParams* CurrentEntry = List.FindByPredicate([EntryToAdd](const FMaterialInterfacePSOPrecacheParams& Other)
		{
			return (Other.Priority			== EntryToAdd.Priority &&
					Other.MaterialInterface == EntryToAdd.MaterialInterface &&
					Other.PSOPrecacheParams == EntryToAdd.PSOPrecacheParams);
		});
	if (CurrentEntry)
	{
		for (const FPSOPrecacheVertexFactoryData& VFData : EntryToAdd.VertexFactoryDataList)
		{
			CurrentEntry->VertexFactoryDataList.AddUnique(VFData);
		}
	}
	else
	{
		List.Add(EntryToAdd);
	}
}
