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

class RENDERCORE_API FRDGTrace
{
public:
	FRDGTrace();

	void OutputGraphBegin();
	void OutputGraphEnd(const FRDGBuilder& GraphBuilder);

	void AddResource(FRDGViewableResource* Resource);
	void AddTexturePassDependency(FRDGTexture* Texture, FRDGPass* Pass);
	void AddBufferPassDependency(FRDGBuffer* Buffer, FRDGPass* Pass);

	FRHITransientAllocationStats TransientAllocationStats;

	bool IsEnabled() const;

private:
	uint64 GraphStartCycles{};
	uint32 ResourceOrder{};
	bool bEnabled;
};

#endif