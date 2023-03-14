// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/Array.h"
#include "Containers/Map.h"
#include "Containers/Set.h"
#include "Containers/SparseArray.h"
#include "HAL/Platform.h"
#include "RHI.h"
#include "RenderGraphAllocator.h"
#include "RenderGraphDefinitions.h"
#include "RenderGraphPass.h"
#include "RenderGraphResources.h"

class FRDGBarrierBatchBegin;
class FRDGBarrierBatchEnd;
class FRDGEventName;
class FRDGPass;
class FShaderParametersMetadata;
struct IPooledRenderTarget;
template <typename ReferencedType> class TRefCountPtr;

#if RDG_ENABLE_DEBUG

/** Used by the render graph builder to validate correct usage of the graph API from setup to execution.
 *  Validation is compiled out in shipping builds. This class tracks resources and passes as they are
 *  added to the graph. It will then validate execution of the graph, including whether resources are
 *  used during execution, and that they are properly produced before being consumed. All found issues
 *  must be clear enough to help the user identify the problem in client code. Validation should occur
 *  as soon as possible in the graph lifecycle. It's much easier to catch an issue at the setup location
 *  rather than during deferred execution.
 *
 *  Finally, this class is designed for user validation, not for internal graph validation. In other words,
 *  if the user can break the graph externally via the client-facing API, this validation layer should catch it.
 *  Any internal validation of the graph state should be kept out of this class in order to provide a clear
 *  and modular location to extend the validation layer as well as clearly separate the graph implementation
 *  details from events in the graph.
 */
class RENDERCORE_API FRDGUserValidation final
{
public:
	FRDGUserValidation(FRDGAllocator& Allocator, bool bParallelExecuteEnabled);
	FRDGUserValidation(const FRDGUserValidation&) = delete;
	~FRDGUserValidation();

	/** Tracks and validates inputs into resource creation functions. */
	void ValidateCreateTexture(const FRDGTextureDesc& Desc, const TCHAR* Name, ERDGTextureFlags Flags);
	void ValidateCreateBuffer(const FRDGBufferDesc& Desc, const TCHAR* Name, ERDGBufferFlags Flags);
	void ValidateCreateSRV(const FRDGTextureSRVDesc& Desc);
	void ValidateCreateSRV(const FRDGBufferSRVDesc& Desc);
	void ValidateCreateUAV(const FRDGTextureUAVDesc& Desc);
	void ValidateCreateUAV(const FRDGBufferUAVDesc& Desc);
	void ValidateCreateUniformBuffer(const void* ParameterStruct, const FShaderParametersMetadata* Metadata);

	/** Tracks and validates the creation of a new externally created / registered resource instances. */
	void ValidateCreateTexture(FRDGTextureRef Texture);
	void ValidateCreateBuffer(FRDGBufferRef Buffer);
	void ValidateCreateSRV(FRDGTextureSRVRef SRV);
	void ValidateCreateSRV(FRDGBufferSRVRef SRV);
	void ValidateCreateUAV(FRDGTextureUAVRef UAV);
	void ValidateCreateUAV(FRDGBufferUAVRef UAV);
	void ValidateCreateUniformBuffer(FRDGUniformBufferRef UniformBuffer);

	void ValidateRegisterExternalTexture(
		const TRefCountPtr<IPooledRenderTarget>& ExternalPooledTexture,
		const TCHAR* Name,
		ERDGTextureFlags Flags);

	void ValidateRegisterExternalBuffer(
		const TRefCountPtr<FRDGPooledBuffer>& ExternalPooledBuffer,
		const TCHAR* Name,
		ERDGBufferFlags Flags);

	void ValidateRegisterExternalTexture(FRDGTextureRef Texture);
	void ValidateRegisterExternalBuffer(FRDGBufferRef Buffer);

	void ValidateUploadBuffer(FRDGBufferRef Buffer, const void* InitialData, uint64 InitialDataSize);
	void ValidateUploadBuffer(FRDGBufferRef Buffer, const void* InitialData, uint64 InitialDataSize, const FRDGBufferInitialDataFreeCallback& InitialDataFreeCallback);
	void ValidateUploadBuffer(FRDGBufferRef Buffer, const FRDGBufferInitialDataCallback& InitialDataCallback, const FRDGBufferInitialDataSizeCallback& InitialDataSizeCallback);
	void ValidateUploadBuffer(FRDGBufferRef Buffer, const FRDGBufferInitialDataCallback& InitialDataCallback, const FRDGBufferInitialDataSizeCallback& InitialDataSizeCallback, const FRDGBufferInitialDataFreeCallback& InitialDataFreeCallback);

	/** Validates a resource extraction operation. */
	void ValidateExtractTexture(FRDGTextureRef Texture, TRefCountPtr<IPooledRenderTarget>* OutTexturePtr);
	void ValidateExtractBuffer(FRDGBufferRef Buffer, TRefCountPtr<FRDGPooledBuffer>* OutBufferPtr);

	void ValidateConvertToExternalResource(FRDGViewableResource* Resource);

	/** Tracks and validates the addition of a new pass to the graph.
	 *  @param bSkipPassAccessMarking Skips marking the pass as a producer or incrementing the pass access. Useful when
	 *      the builder needs to inject a pass for debugging while preserving error messages and warnings for the original
	 *      graph structure.
	 */
	void ValidateAddPass(const void* ParameterStruct, const FShaderParametersMetadata* Metadata, const FRDGEventName& Name, ERDGPassFlags Flags);
	void ValidateAddPass(const FRDGEventName& Name, ERDGPassFlags Flags);
	void ValidateAddPass(const FRDGPass* Pass, bool bSkipPassAccessMarking);

	/** Validate pass state before and after execution. */
	void ValidateExecutePassBegin(const FRDGPass* Pass);
	void ValidateExecutePassEnd(const FRDGPass* Pass);

	/** Validate graph state before and after execution. */
	void ValidateExecuteBegin();
	void ValidateExecuteEnd();

	/** Removes the 'produced but not used' warning from the requested resource. */
	void RemoveUnusedWarning(FRDGViewableResource* Resource);

	/** Attempts to mark a resource for clobbering. If already marked, returns false.  */
	bool TryMarkForClobber(FRDGViewableResource* Resource) const;

	void ValidateGetPooledTexture(FRDGTextureRef Texture) const;
	void ValidateGetPooledBuffer(FRDGBufferRef Buffer) const;

	void ValidateSetAccessFinal(FRDGViewableResource* Resource, ERHIAccess AccessFinal);

	void ValidateAddSubresourceAccess(FRDGViewableResource* Resource, const FRDGSubresourceState& Subresource, ERHIAccess Access);

	void ValidateUseExternalAccessMode(FRDGViewableResource* Resource, ERHIAccess ReadOnlyAccess, ERHIPipeline Pipelines);
	void ValidateUseInternalAccessMode(FRDGViewableResource* Resaource);

	void ValidateExternalAccess(FRDGViewableResource* Resource, ERHIAccess Access, const FRDGPass* Pass);

	/** Traverses all resources in the pass and marks whether they are externally accessible by user pass implementations. */
	static void SetAllowRHIAccess(const FRDGPass* Pass, bool bAllowAccess);

private:
	void ValidateCreateViewableResource(FRDGViewableResource* Resource);
	void ValidateCreateResource(FRDGResourceRef Resource);
	void ValidateExtractResource(FRDGViewableResource* Resource);

	FRDGAllocator& Allocator;

	/** List of tracked resources for validation prior to shutdown. */
	TArray<FRDGTextureRef, FRDGArrayAllocator> TrackedTextures;
	TArray<FRDGBufferRef, FRDGArrayAllocator> TrackedBuffers;

	/** Map tracking all active resources in the graph. */
	TSet<FRDGResourceRef, DefaultKeyFuncs<FRDGResourceRef>, FRDGSetAllocator> ResourceMap;

	/** Whether the Execute() has already been called. */
	bool bHasExecuted = false;
	bool bHasExecuteBegun = false;

	bool bParallelExecuteEnabled;

	void ExecuteGuard(const TCHAR* Operation, const TCHAR* ResourceName);
};

/** This class validates and logs barriers submitted by the graph. */
class FRDGBarrierValidation
{
public:
	FRDGBarrierValidation(const FRDGPassRegistry* InPasses, const FRDGEventName& InGraphName);
	FRDGBarrierValidation(const FRDGBarrierValidation&) = delete;

	/** Validates a begin barrier batch just prior to submission to the command list. */
	void ValidateBarrierBatchBegin(const FRDGPass* Pass, const FRDGBarrierBatchBegin& Batch);

	/** Validates an end barrier batch just prior to submission to the command list. */
	void ValidateBarrierBatchEnd(const FRDGPass* Pass, const FRDGBarrierBatchEnd& Batch);

private:
	struct FResourceMap
	{
		TMap<FRDGTextureRef, TArray<FRHITransitionInfo>> Textures;
		TMap<FRDGBufferRef, FRHITransitionInfo> Buffers;
		TMap<FRDGViewableResource*, FRHITransientAliasingInfo> Aliases;
	};

	using FBarrierBatchMap = TMap<const FRDGBarrierBatchBegin*, FResourceMap>;

	FBarrierBatchMap BatchMap;

	const FRDGPassRegistry* Passes = nullptr;
	const TCHAR* GraphName = nullptr;
};

#endif