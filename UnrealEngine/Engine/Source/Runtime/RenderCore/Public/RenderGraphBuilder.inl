// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

inline FRDGTexture* FRDGBuilder::FindExternalTexture(FRHITexture* ExternalTexture) const
{
	if (FRDGTexture* const* FoundTexturePtr = ExternalTextures.Find(ExternalTexture))
	{
		return *FoundTexturePtr;
	}
	return nullptr;
}

inline FRDGTexture* FRDGBuilder::FindExternalTexture(IPooledRenderTarget* ExternalTexture) const
{
	if (ExternalTexture)
	{
		return FindExternalTexture(ExternalTexture->GetRHI());
	}
	return nullptr;
}

inline FRDGBuffer* FRDGBuilder::FindExternalBuffer(FRHIBuffer* ExternalBuffer) const
{
	if (FRDGBuffer* const* FoundBufferPtr = ExternalBuffers.Find(ExternalBuffer))
	{
		return *FoundBufferPtr;
	}
	return nullptr;
}

inline FRDGBuffer* FRDGBuilder::FindExternalBuffer(FRDGPooledBuffer* ExternalBuffer) const
{
	if (ExternalBuffer)
	{
		return FindExternalBuffer(ExternalBuffer->GetRHI());
	}
	return nullptr;
}

inline FRDGTextureRef FRDGBuilder::CreateTexture(
	const FRDGTextureDesc& Desc,
	const TCHAR* Name,
	ERDGTextureFlags Flags)
{
	// RDG no longer supports the legacy transient resource API.
	FRDGTextureDesc OverrideDesc = Desc;

#if !UE_BUILD_SHIPPING
	ensureMsgf(OverrideDesc.Extent.X >= 1, TEXT("CreateTexture %s X size too small: %i, Min: %i, clamping"), Name ? Name : TEXT(""), OverrideDesc.Extent.X, 1);
	ensureMsgf(OverrideDesc.Extent.Y >= 1, TEXT("CreateTexture %s Y size too small: %i, Min: %i, clamping"), Name ? Name : TEXT(""), OverrideDesc.Extent.Y, 1);
	ensureMsgf(((uint32)OverrideDesc.Extent.X) <= GetMax2DTextureDimension(), TEXT("CreateTexture %s X size too large: %i, Max: %i, clamping"), Name ? Name : TEXT(""), OverrideDesc.Extent.X, GetMax2DTextureDimension());
	ensureMsgf(((uint32)OverrideDesc.Extent.Y) <= GetMax2DTextureDimension(), TEXT("CreateTexture %s Y size too large: %i, Max: %i, clamping"), Name ? Name : TEXT(""), OverrideDesc.Extent.Y, GetMax2DTextureDimension());
#endif
	// Clamp the texture size to that which is permissible, otherwise it's a guaranteed crash.
	OverrideDesc.Extent.X = FMath::Clamp<int32>(OverrideDesc.Extent.X, 1, GetMax2DTextureDimension());
	OverrideDesc.Extent.Y = FMath::Clamp<int32>(OverrideDesc.Extent.Y, 1, GetMax2DTextureDimension());

	IF_RDG_ENABLE_DEBUG(UserValidation.ValidateCreateTexture(OverrideDesc, Name, Flags));
	FRDGTextureRef Texture = Textures.Allocate(Allocator, Name, OverrideDesc, Flags);
	IF_RDG_ENABLE_DEBUG(UserValidation.ValidateCreateTexture(Texture));
	IF_RDG_ENABLE_TRACE(Trace.AddResource(Texture));
	return Texture;
}

inline FRDGBufferRef FRDGBuilder::CreateBuffer(
	const FRDGBufferDesc& Desc,
	const TCHAR* Name,
	ERDGBufferFlags Flags)
{
	// RDG no longer supports the legacy transient resource API.
	FRDGBufferDesc OverrideDesc = Desc;

	IF_RDG_ENABLE_DEBUG(UserValidation.ValidateCreateBuffer(Desc, Name, Flags));
	FRDGBufferRef Buffer = Buffers.Allocate(Allocator, Name, OverrideDesc, Flags);
	IF_RDG_ENABLE_DEBUG(UserValidation.ValidateCreateBuffer(Buffer));
	IF_RDG_ENABLE_TRACE(Trace.AddResource(Buffer));
	return Buffer;
}

inline FRDGBufferRef FRDGBuilder::CreateBuffer(
	const FRDGBufferDesc& Desc,
	const TCHAR* Name,
	FRDGBufferNumElementsCallback&& NumElementsCallback,
	ERDGBufferFlags Flags)
{
	// RDG no longer supports the legacy transient resource API.
	FRDGBufferDesc OverrideDesc = Desc;

	IF_RDG_ENABLE_DEBUG(UserValidation.ValidateCreateBuffer(Desc, Name, Flags));
	FRDGBufferRef Buffer = Buffers.Allocate(Allocator, Name, OverrideDesc, Flags, MoveTemp(NumElementsCallback));
	IF_RDG_ENABLE_DEBUG(UserValidation.ValidateCreateBuffer(Buffer));
	IF_RDG_ENABLE_TRACE(Trace.AddResource(Buffer));
	return Buffer;
}

inline FRDGTextureSRVRef FRDGBuilder::CreateSRV(const FRDGTextureSRVDesc& Desc)
{
	IF_RDG_ENABLE_DEBUG(UserValidation.ValidateCreateSRV(Desc));
	FRDGTextureSRVRef SRV = Views.Allocate<FRDGTextureSRV>(Allocator, Desc.Texture->Name, Desc);
	IF_RDG_ENABLE_DEBUG(UserValidation.ValidateCreateSRV(SRV));
	return SRV;
}

inline FRDGBufferSRVRef FRDGBuilder::CreateSRV(const FRDGBufferSRVDesc& Desc)
{
	IF_RDG_ENABLE_DEBUG(UserValidation.ValidateCreateSRV(Desc));
	FRDGBufferSRVRef SRV = Views.Allocate<FRDGBufferSRV>(Allocator, Desc.Buffer->Name, Desc);
	IF_RDG_ENABLE_DEBUG(UserValidation.ValidateCreateSRV(SRV));
	return SRV;
}

inline FRDGTextureUAVRef FRDGBuilder::CreateUAV(const FRDGTextureUAVDesc& Desc, ERDGUnorderedAccessViewFlags InFlags)
{
	IF_RDG_ENABLE_DEBUG(UserValidation.ValidateCreateUAV(Desc));
	FRDGTextureUAVRef UAV = Views.Allocate<FRDGTextureUAV>(Allocator, Desc.Texture->Name, Desc, InFlags);
	IF_RDG_ENABLE_DEBUG(UserValidation.ValidateCreateUAV(UAV));
	return UAV;
}

inline FRDGBufferUAVRef FRDGBuilder::CreateUAV(const FRDGBufferUAVDesc& Desc, ERDGUnorderedAccessViewFlags InFlags)
{
	IF_RDG_ENABLE_DEBUG(UserValidation.ValidateCreateUAV(Desc));
	FRDGBufferUAVRef UAV = Views.Allocate<FRDGBufferUAV>(Allocator, Desc.Buffer->Name, Desc, InFlags);
	IF_RDG_ENABLE_DEBUG(UserValidation.ValidateCreateUAV(UAV));
	return UAV;
}

FORCEINLINE void* FRDGBuilder::Alloc(uint64 SizeInBytes, uint32 AlignInBytes)
{
	return Allocator.Alloc(SizeInBytes, AlignInBytes);
}

template <typename PODType>
FORCEINLINE PODType* FRDGBuilder::AllocPOD()
{
	return Allocator.AllocUninitialized<PODType>();
}

template <typename PODType>
FORCEINLINE PODType* FRDGBuilder::AllocPODArray(uint32 Count)
{
	return Allocator.AllocUninitialized<PODType>(Count);
}

template <typename ObjectType, typename... TArgs>
FORCEINLINE ObjectType* FRDGBuilder::AllocObject(TArgs&&... Args)
{
	return Allocator.Alloc<ObjectType>(Forward<TArgs&&>(Args)...);
}

template <typename ObjectType>
FORCEINLINE TArray<ObjectType, FRDGArrayAllocator>& FRDGBuilder::AllocArray()
{
	return *Allocator.Alloc<TArray<ObjectType, FRDGArrayAllocator>>();
}

template <typename ParameterStructType>
FORCEINLINE ParameterStructType* FRDGBuilder::AllocParameters()
{
	return Allocator.Alloc<ParameterStructType>();
}

template <typename ParameterStructType>
FORCEINLINE ParameterStructType* FRDGBuilder::AllocParameters(ParameterStructType* StructToCopy)
{
	ParameterStructType* Struct = Allocator.Alloc<ParameterStructType>();
	*Struct = *StructToCopy;
	return Struct;
}

template <typename BaseParameterStructType>
BaseParameterStructType* FRDGBuilder::AllocParameters(const FShaderParametersMetadata* ParametersMetadata)
{
	return &AllocParameters<BaseParameterStructType>(ParametersMetadata, 1)[0];
}

template <typename BaseParameterStructType>
TStridedView<BaseParameterStructType> FRDGBuilder::AllocParameters(const FShaderParametersMetadata* ParametersMetadata, uint32 NumStructs)
{
	// NOTE: Contents are always zero! This might differ if shader parameters have a non-zero default initializer.
	const int32 Stride = ParametersMetadata->GetSize();
	BaseParameterStructType* Contents = reinterpret_cast<BaseParameterStructType*>(Allocator.Alloc(Stride * NumStructs, SHADER_PARAMETER_STRUCT_ALIGNMENT));
	FMemory::Memset(Contents, 0, Stride * NumStructs);
	TStridedView<BaseParameterStructType> ParameterArray(Stride, Contents, NumStructs);

	struct FClearUniformBuffers
	{
	public:
		FClearUniformBuffers(TStridedView<BaseParameterStructType> InParameterArray, const FRHIUniformBufferLayout& InLayout)
			: ParameterArray(InParameterArray)
			, Layout(&InLayout)
		{}

		~FClearUniformBuffers()
		{
			for (BaseParameterStructType& ParameterStruct : ParameterArray)
			{
				FRDGParameterStruct::ClearUniformBuffers(&ParameterStruct, Layout);
			}
		}

	private:
		TStridedView<BaseParameterStructType> ParameterArray;
		const FRHIUniformBufferLayout* Layout;
	};

	AllocObject<FClearUniformBuffers>(ParameterArray, ParametersMetadata->GetLayout());
	return ParameterArray;
}

FORCEINLINE FRDGSubresourceState* FRDGBuilder::AllocSubresource(const FRDGSubresourceState& Other)
{
	return Allocator.AllocNoDestruct<FRDGSubresourceState>(Other);
}

template <typename ParameterStructType>
TRDGUniformBufferRef<ParameterStructType> FRDGBuilder::CreateUniformBuffer(const ParameterStructType* ParameterStruct)
{
	IF_RDG_ENABLE_DEBUG(UserValidation.ValidateCreateUniformBuffer(ParameterStruct, &ParameterStructType::StaticStructMetadata));
	auto* UniformBuffer = UniformBuffers.Allocate<TRDGUniformBuffer<ParameterStructType>>(Allocator, ParameterStruct, ParameterStructType::StaticStructMetadata.GetShaderVariableName());
	IF_RDG_ENABLE_DEBUG(UserValidation.ValidateCreateUniformBuffer(UniformBuffer));
	return UniformBuffer;
}

template <typename ExecuteLambdaType>
FRDGPassRef FRDGBuilder::AddPass(
	FRDGEventName&& Name,
	ERDGPassFlags Flags,
	ExecuteLambdaType&& ExecuteLambda)
{
	using LambdaPassType = TRDGEmptyLambdaPass<ExecuteLambdaType>;

	IF_RDG_ENABLE_DEBUG(UserValidation.ValidateAddPass(Name, Flags));

	Flags |= ERDGPassFlags::NeverCull;

	FlushAccessModeQueue();

	LambdaPassType* Pass = Passes.Allocate<LambdaPassType>(Allocator, MoveTemp(Name), Flags, MoveTemp(ExecuteLambda));
	SetupEmptyPass(Pass);
	return Pass;
}

template <typename ParameterStructType, typename ExecuteLambdaType>
FRDGPassRef FRDGBuilder::AddPassInternal(
	FRDGEventName&& Name,
	const FShaderParametersMetadata* ParametersMetadata,
	const ParameterStructType* ParameterStruct,
	ERDGPassFlags Flags,
	ExecuteLambdaType&& ExecuteLambda)
{
	using LambdaPassType = TRDGLambdaPass<ParameterStructType, ExecuteLambdaType>;

	IF_RDG_ENABLE_DEBUG(UserValidation.ValidateAddPass(ParameterStruct, ParametersMetadata, Name, Flags));

	FlushAccessModeQueue();

	FRDGPass* Pass = Allocator.AllocNoDestruct<LambdaPassType>(
		MoveTemp(Name),
		ParametersMetadata,
		ParameterStruct,
		OverridePassFlags(Name.GetTCHAR(), Flags, LambdaPassType::kSupportsAsyncCompute),
		MoveTemp(ExecuteLambda));

	IF_RDG_ENABLE_DEBUG(ClobberPassOutputs(Pass));
	Passes.Insert(Pass);
	SetupParameterPass(Pass);
	return Pass;
}

template <typename ExecuteLambdaType>
FRDGPassRef FRDGBuilder::AddPass(
	FRDGEventName&& Name,
	const FShaderParametersMetadata* ParametersMetadata,
	const void* ParameterStruct,
	ERDGPassFlags Flags,
	ExecuteLambdaType&& ExecuteLambda)
{
	return AddPassInternal(Forward<FRDGEventName>(Name), ParametersMetadata, ParameterStruct, Flags, Forward<ExecuteLambdaType>(ExecuteLambda));
}

template <typename ParameterStructType, typename ExecuteLambdaType>
FRDGPassRef FRDGBuilder::AddPass(
	FRDGEventName&& Name,
	const ParameterStructType* ParameterStruct,
	ERDGPassFlags Flags,
	ExecuteLambdaType&& ExecuteLambda)
{
	return AddPassInternal(Forward<FRDGEventName>(Name), ParameterStructType::FTypeInfo::GetStructMetadata(), ParameterStruct, Flags, Forward<ExecuteLambdaType>(ExecuteLambda));
}

inline void FRDGBuilder::QueueBufferUpload(FRDGBufferRef Buffer, const void* InitialData, uint64 InitialDataSize, ERDGInitialDataFlags InitialDataFlags)
{
	IF_RDG_ENABLE_DEBUG(UserValidation.ValidateUploadBuffer(Buffer, InitialData, InitialDataSize));

	if (InitialDataSize > 0 && !EnumHasAnyFlags(InitialDataFlags, ERDGInitialDataFlags::NoCopy))
	{
		void* InitialDataCopy = Alloc(InitialDataSize, 16);
		FMemory::Memcpy(InitialDataCopy, InitialData, InitialDataSize);
		InitialData = InitialDataCopy;
	}

	UploadedBuffers.Emplace(Buffer, InitialData, InitialDataSize);
	Buffer->bQueuedForUpload = 1;
	Buffer->bForceNonTransient = 1;
}

inline void FRDGBuilder::QueueBufferUpload(FRDGBufferRef Buffer, const void* InitialData, uint64 InitialDataSize, FRDGBufferInitialDataFreeCallback&& InitialDataFreeCallback)
{
	IF_RDG_ENABLE_DEBUG(UserValidation.ValidateUploadBuffer(Buffer, InitialData, InitialDataSize));

	if (InitialDataSize == 0)
	{
		return;
	}

	UploadedBuffers.Emplace(Buffer, InitialData, InitialDataSize, MoveTemp(InitialDataFreeCallback));
	Buffer->bQueuedForUpload = 1;
	Buffer->bForceNonTransient = 1;
}

inline void FRDGBuilder::QueueBufferUpload(FRDGBufferRef Buffer, FRDGBufferInitialDataCallback&& InitialDataCallback, FRDGBufferInitialDataSizeCallback&& InitialDataSizeCallback)
{
	IF_RDG_ENABLE_DEBUG(UserValidation.ValidateUploadBuffer(Buffer, InitialDataCallback, InitialDataSizeCallback));

	UploadedBuffers.Emplace(Buffer, MoveTemp(InitialDataCallback), MoveTemp(InitialDataSizeCallback));
	Buffer->bQueuedForUpload = 1;
	Buffer->bForceNonTransient = 1;
}

inline void FRDGBuilder::QueueBufferUpload(FRDGBufferRef Buffer, FRDGBufferInitialDataCallback&& InitialDataCallback, FRDGBufferInitialDataSizeCallback&& InitialDataSizeCallback, FRDGBufferInitialDataFreeCallback&& InitialDataFreeCallback)
{
	IF_RDG_ENABLE_DEBUG(UserValidation.ValidateUploadBuffer(Buffer, InitialDataCallback, InitialDataSizeCallback, InitialDataFreeCallback));

	UploadedBuffers.Emplace(Buffer, MoveTemp(InitialDataCallback), MoveTemp(InitialDataSizeCallback), MoveTemp(InitialDataFreeCallback));
	Buffer->bQueuedForUpload = 1;
	Buffer->bForceNonTransient = 1;
}

inline void FRDGBuilder::QueueTextureExtraction(FRDGTextureRef Texture, TRefCountPtr<IPooledRenderTarget>* OutTexturePtr, ERHIAccess AccessFinal, ERDGResourceExtractionFlags Flags)
{
	QueueTextureExtraction(Texture, OutTexturePtr, Flags);
	SetTextureAccessFinal(Texture, AccessFinal);
}

inline void FRDGBuilder::QueueTextureExtraction(FRDGTextureRef Texture, TRefCountPtr<IPooledRenderTarget>* OutTexturePtr, ERDGResourceExtractionFlags Flags)
{
	IF_RDG_ENABLE_DEBUG(UserValidation.ValidateExtractTexture(Texture, OutTexturePtr));

	*OutTexturePtr = nullptr;

	Texture->bExtracted = true;

	if (EnumHasAnyFlags(Flags, ERDGResourceExtractionFlags::AllowTransient))
	{
		if (Texture->TransientExtractionHint != FRDGTexture::ETransientExtractionHint::Disable)
		{
			Texture->TransientExtractionHint = FRDGTexture::ETransientExtractionHint::Enable;
		}
	}
	else
	{
		Texture->TransientExtractionHint = FRDGTexture::ETransientExtractionHint::Disable;
	}

	ExtractedTextures.Emplace(Texture, OutTexturePtr);
}

inline void FRDGBuilder::QueueBufferExtraction(FRDGBufferRef Buffer, TRefCountPtr<FRDGPooledBuffer>* OutBufferPtr)
{
	IF_RDG_ENABLE_DEBUG(UserValidation.ValidateExtractBuffer(Buffer, OutBufferPtr));

	*OutBufferPtr = nullptr;

	Buffer->bExtracted = true;
	Buffer->bForceNonTransient = true;
	ExtractedBuffers.Emplace(Buffer, OutBufferPtr);
}

inline void FRDGBuilder::QueueBufferExtraction(FRDGBufferRef Buffer, TRefCountPtr<FRDGPooledBuffer>* OutBufferPtr, ERHIAccess AccessFinal)
{
	QueueBufferExtraction(Buffer, OutBufferPtr);
	SetBufferAccessFinal(Buffer, AccessFinal);
}

inline void FRDGBuilder::SetCommandListStat(TStatId StatId)
{
#if RDG_CMDLIST_STATS
	CommandListStatScope = StatId;
	RHICmdList.SetCurrentStat(StatId);
#endif
}

inline void FRDGBuilder::AddDispatchHint()
{
	if (Passes.Num() > 0)
	{
		Passes[Passes.Last()]->bDispatchAfterExecute = 1;
	}
}

template <typename TaskLambda>
void FRDGBuilder::AddSetupTask(TaskLambda&& Task)
{
	if (bParallelExecuteEnabled)
	{
		ParallelSetupEvents.Emplace(UE::Tasks::Launch(TEXT("FRDGBuilder::AddSetupTask"), [Task = MoveTemp(Task)]
		{
			FTaskTagScope Scope(ETaskTag::EParallelRenderingThread);
			Task();
		}));
	}
	else
	{
		Task();
	}
}

template <typename TaskLambda>
void FRDGBuilder::AddCommandListSetupTask(TaskLambda&& Task)
{
	if (bParallelExecuteEnabled)
	{
		FRHICommandList* RHICmdListTask = new FRHICommandList(FRHIGPUMask::All());

		ParallelSetupEvents.Emplace(UE::Tasks::Launch(TEXT("FRDGBuilder::AddCommandListSetupTask"), [Task = MoveTemp(Task), RHICmdListTask]
		{
			FTaskTagScope Scope(ETaskTag::EParallelRenderingThread);
			Task(*RHICmdListTask);

			RHICmdListTask->FinishRecording();

		}));

		RHICmdList.QueueAsyncCommandListSubmit(RHICmdListTask);
	}
	else
	{
		Task(RHICmdList);
	}
}

inline const TRefCountPtr<IPooledRenderTarget>& FRDGBuilder::GetPooledTexture(FRDGTextureRef Texture) const
{
	IF_RDG_ENABLE_DEBUG(UserValidation.ValidateGetPooledTexture(Texture));
	return Texture->Allocation;
}

inline const TRefCountPtr<FRDGPooledBuffer>& FRDGBuilder::GetPooledBuffer(FRDGBufferRef Buffer) const
{
	IF_RDG_ENABLE_DEBUG(UserValidation.ValidateGetPooledBuffer(Buffer));
	return Buffer->Allocation;
}

inline void FRDGBuilder::SetTextureAccessFinal(FRDGTextureRef Texture, ERHIAccess AccessFinal)
{
	IF_RDG_ENABLE_DEBUG(UserValidation.ValidateSetAccessFinal(Texture, AccessFinal));
	Texture->EpilogueAccess = AccessFinal;
}

inline void FRDGBuilder::SetBufferAccessFinal(FRDGBufferRef Buffer, ERHIAccess AccessFinal)
{
	IF_RDG_ENABLE_DEBUG(UserValidation.ValidateSetAccessFinal(Buffer, AccessFinal));
	Buffer->EpilogueAccess = AccessFinal;
}

inline void FRDGBuilder::RemoveUnusedTextureWarning(FRDGTextureRef Texture)
{
	IF_RDG_ENABLE_DEBUG(UserValidation.RemoveUnusedWarning(Texture));
}

inline void FRDGBuilder::RemoveUnusedBufferWarning(FRDGBufferRef Buffer)
{
	IF_RDG_ENABLE_DEBUG(UserValidation.RemoveUnusedWarning(Buffer));
}

inline void FRDGBuilder::BeginEventScope(FRDGEventName&& ScopeName)
{
#if RDG_GPU_DEBUG_SCOPES
	GPUScopeStacks.BeginEventScope(MoveTemp(ScopeName), RHICmdList.GetGPUMask());
#endif
}

inline void FRDGBuilder::EndEventScope()
{
#if RDG_GPU_DEBUG_SCOPES
	GPUScopeStacks.EndEventScope();
#endif
}