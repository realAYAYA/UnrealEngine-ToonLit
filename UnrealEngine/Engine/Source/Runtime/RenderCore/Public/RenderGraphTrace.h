// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "HAL/Platform.h"
#include "RHITransientResourceAllocator.h"
#include "RenderGraphDefinitions.h"
#include "Trace/Trace.h"

class FRDGBuffer;
class FRDGBuilder;
class FRDGPass;
class FRDGTexture;
class FRDGViewableResource;
namespace UE { namespace Trace { class FChannel; } }

#if RDG_ENABLE_TRACE

UE_TRACE_CHANNEL_EXTERN(RDGChannel, RENDERCORE_API);

class FRDGTrace
{
public:
	RENDERCORE_API FRDGTrace();

	RENDERCORE_API void OutputGraphBegin();
	RENDERCORE_API void OutputGraphEnd(const FRDGBuilder& GraphBuilder);

	RENDERCORE_API void AddResource(FRDGViewableResource* Resource);
	RENDERCORE_API void AddTexturePassDependency(FRDGTexture* Texture, FRDGPass* Pass);
	RENDERCORE_API void AddBufferPassDependency(FRDGBuffer* Buffer, FRDGPass* Pass);

	FRHITransientAllocationStats TransientAllocationStats;

	RENDERCORE_API bool IsEnabled() const;

private:
	uint64 GraphStartCycles{};
	uint32 ResourceOrder{};
	bool bEnabled;
};

#endif
