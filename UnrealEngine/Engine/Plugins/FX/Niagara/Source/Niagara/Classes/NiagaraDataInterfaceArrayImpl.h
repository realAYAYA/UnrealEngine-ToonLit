// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "NiagaraClearCounts.h"
#include "NiagaraDataInterfaceArray.h"
#include "NiagaraDataInterfaceUtilities.h"
#include "NiagaraGpuComputeDispatchInterface.h"
#include "Niagara/Private/NiagaraGpuReadbackManager.h"
#include "Niagara/Private/NiagaraStats.h"
#include "NiagaraSystemInstance.h"

#include "Async/Async.h"
#include "Containers/ArrayView.h"
#include "ShaderParameterUtils.h"
#include "ShaderCompilerCore.h"

//////////////////////////////////////////////////////////////////////////
// Helpers

#define NDIARRAY_GENERATE_BODY(CLASSNAME, TYPENAME, MEMBERNAME) \
	using FProxyType = FNDIArrayProxyImpl<TYPENAME, CLASSNAME>; \
	virtual void PostInitProperties() override \
	{ \
		Proxy.Reset(new FProxyType(this)); \
		Super::PostInitProperties(); \
	} \
	TArray<TYPENAME>& GetArrayReference() { return MEMBERNAME; }

#if WITH_EDITORONLY_DATA
	#define NDIARRAY_GENERATE_BODY_LWC(CLASSNAME, TYPENAME, MEMBERNAME) \
		using FProxyType = FNDIArrayProxyImpl<TYPENAME, CLASSNAME>; \
		virtual void PostInitProperties() override \
		{ \
			Super::PostInitProperties(); \
			Proxy.Reset(new FProxyType(this)); \
		} \
		virtual void PostLoad() override \
		{ \
			Super::PostLoad(); \
			GetProxyAs<FProxyType>()->template SetArrayData<decltype(MEMBERNAME)::ElementType>(MakeArrayView(MEMBERNAME)); \
		} \
		virtual void PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent) override \
		{ \
			Super::PostEditChangeProperty(PropertyChangedEvent); \
			GetProxyAs<FProxyType>()->template SetArrayData<decltype(MEMBERNAME)::ElementType>(MakeArrayView(MEMBERNAME)); \
		} \
		virtual bool CopyToInternal(UNiagaraDataInterface* Destination) const override \
		{ \
			if ( Super::CopyToInternal(Destination) == false ) \
			{ \
				return false; \
			} \
			CLASSNAME* TypedDestination = Cast<CLASSNAME>(Destination); \
			if ( TypedDestination != nullptr ) \
			{ \
				TypedDestination->MEMBERNAME = MEMBERNAME; \
			} \
			return TypedDestination != nullptr;  \
		} \
		virtual bool Equals(const UNiagaraDataInterface* Other) const override \
		{ \
			const CLASSNAME* TypedOther = Cast<const CLASSNAME>(Other); \
			return \
				Super::Equals(Other) && \
				TypedOther != nullptr && \
				TypedOther->MEMBERNAME == MEMBERNAME; \
		} \
		TArray<TYPENAME>& GetArrayReference() { return Internal##MEMBERNAME; }
#else
	#define NDIARRAY_GENERATE_BODY_LWC(CLASSNAME, TYPENAME, MEMBERNAME) NDIARRAY_GENERATE_BODY(CLASSNAME, TYPENAME, Internal##MEMBERNAME)
#endif

template<typename TArrayType>
struct FNDIArrayImplHelperBase
{
	static constexpr bool bSupportsCPU = true;
	static constexpr bool bSupportsGPU = true;
	static constexpr bool bSupportsAtomicOps = false;

	//-OPT: We can reduce the differences between read and RW if we have typed UAV loads
	//static constexpr TCHAR const* HLSLVariableType		= TEXT("float");
	//static constexpr EPixelFormat ReadPixelFormat		= PF_R32_FLOAT;
	//static constexpr TCHAR const* ReadHLSLBufferType	= TEXT("float");
	//static constexpr TCHAR const* ReadHLSLBufferRead	= TEXT("Value = BUFFER_NAME[Index]");
	//static constexpr EPixelFormat RWPixelFormat			= PF_R32_FLOAT;
	//static constexpr TCHAR const* RWHLSLBufferType		= TEXT("float");
	//static constexpr TCHAR const* RWHLSLBufferRead		= TEXT("Value = BUFFER_NAME[Index]");
	//static constexpr TCHAR const* RWHLSLBufferWrite		= TEXT("BUFFER_NAME[Index] = Value");

	//static const FNiagaraTypeDefinition& GetTypeDefinition() { return FNiagaraTypeDefinition::GetFloatDef(); }
	//static const TArrayType GetDefaultValue();

	static void CopyCpuToCpuMemory(TArrayType* Dest, const TArrayType* Src, int32 NumElements)
	{
		FMemory::Memcpy(Dest, Src, NumElements * sizeof(TArrayType));
	}

	static void CopyCpuToGpuMemory(void* Dest, const TArrayType* Src, int32 NumElements)
	{
		FMemory::Memcpy(Dest, Src, NumElements * sizeof(TArrayType));
	}

	static void CopyGpuToCpuMemory(void* Dest, const void* Src, int32 NumElements)
	{
		FMemory::Memcpy(Dest, Src, NumElements * sizeof(TArrayType));
	}

	//static TArrayType AtomicAdd(TArrayType* Dest, TArrayType Value) { check(false); }
	//static TArrayType AtomicMin(TArrayType* Dest, TArrayType Value) { check(false); }
	//static TArrayType AtomicMax(TArrayType* Dest, TArrayType Value) { check(false); }
};

template<typename TArrayType>
struct FNDIArrayImplHelper : public FNDIArrayImplHelperBase<TArrayType>
{
};

struct FNiagaraDataInterfaceArrayImplHelper
{
	struct FFunctionVersion
	{
		enum Type
		{
			InitialVersion = 0,
			AddOptionalExecuteToSet = 1,

			VersionPlusOne,
			LatestVersion = VersionPlusOne - 1
		};
	};

	static const TCHAR* HLSLReadTemplateFile;
	static const TCHAR* HLSLReadWriteTemplateFile;

	static const FName Function_LengthName;
	static const FName Function_IsValidIndexName;
	static const FName Function_LastIndexName;
	static const FName Function_GetName;

	static const FName Function_ClearName;
	static const FName Function_ResizeName;
	static const FName Function_SetArrayElemName;
	static const FName Function_AddName;
	static const FName Function_RemoveLastElemName;

	static const FName Function_AtomicAddName;
	static const FName Function_AtomicMinName;
	static const FName Function_AtomicMaxName;

#if WITH_EDITORONLY_DATA
	static bool UpgradeFunctionCall(FNiagaraFunctionSignature& FunctionSignature);
#endif
	static const TCHAR* GetHLSLTemplateFile(bool bIsRWArray)
	{
		return bIsRWArray ? FNiagaraDataInterfaceArrayImplHelper::HLSLReadWriteTemplateFile : FNiagaraDataInterfaceArrayImplHelper::HLSLReadTemplateFile;
	}
	static bool IsRWFunction(const FName FunctionName);
};

//////////////////////////////////////////////////////////////////////////
// Instance Data, Proxy with Impl

template<typename TArrayType>
struct FNDIArrayInstanceData_GameThread
{
	bool				bIsModified = false;		// True if the array has ever been modified and we are reading instance data
	bool				bIsRenderDirty = true;		// True if we have made modifications that could be pushed to the render thread
	FRWLock				ArrayRWGuard;
	TArray<TArrayType>	ArrayData;					// Modified array data
};

template<typename TArrayType>
struct FNDIArrayInstanceData_RenderThread
{
	using TVMArrayType = typename FNDIArrayImplHelper<TArrayType>::TVMArrayType;

	~FNDIArrayInstanceData_RenderThread()
	{
		if (CountOffset != INDEX_NONE)
		{
			ComputeInterface->GetGPUInstanceCounterManager().FreeEntry(CountOffset);
			CountOffset = INDEX_NONE;
		}

		ReleaseData();
	}

	bool IsReadOnly() const { return CountOffset == INDEX_NONE; }

	void Initialize(FRHICommandListImmediate& RHICmdList, FNiagaraGpuComputeDispatchInterface* InComputeInterface, int32 InDefaultElements, bool bRWGpuArray)
	{
		ComputeInterface = InComputeInterface;
		DefaultElements = 0;
		NumElements = INDEX_NONE;
		CountOffset = INDEX_NONE;

		if (bRWGpuArray)
		{
			DefaultElements = InDefaultElements;
			CountOffset = ComputeInterface->GetGPUInstanceCounterManager().AcquireOrAllocateEntry(RHICmdList);
		}
	}

	template<typename T = FNDIArrayImplHelper<TArrayType>>
	typename TEnableIf<T::bSupportsGPU>::Type UpdateDataImpl(FRHICommandList& RHICmdList, TArray<TArrayType>& InArrayData)
	{
		const int32 NewNumElements = FMath::Max(DefaultElements, InArrayData.Num());

		// Do we need to update the backing storage for the buffer
		if (NewNumElements != NumElements)
		{
			// Release old data
			ReleaseData();

			// Allocate new data
			NumElements = NewNumElements;
			ArrayNumBytes = (NumElements + 1) * sizeof(TVMArrayType);	// Note +1 because we store the default value at the end of the buffer
			INC_MEMORY_STAT_BY(STAT_NiagaraGPUDataInterfaceMemory, ArrayNumBytes);

			const EPixelFormat PixelFormat = IsReadOnly() ? FNDIArrayImplHelper<TArrayType>::ReadPixelFormat : FNDIArrayImplHelper<TArrayType>::RWPixelFormat;
			const int32 TypeStride = GPixelFormats[PixelFormat].BlockBytes;

			// Create Buffer
			FRHIResourceCreateInfo CreateInfo(TEXT("NiagaraDataInterfaceArray"));
			const EBufferUsageFlags BufferUsage = BUF_Static | BUF_ShaderResource | BUF_VertexBuffer | (IsReadOnly() ? BUF_None : BUF_UnorderedAccess | BUF_SourceCopy);
			const ERHIAccess DefaultAccess = IsReadOnly() ? ERHIAccess::SRVCompute : ERHIAccess::UAVCompute;
			ArrayBuffer = RHICreateBuffer(ArrayNumBytes, BufferUsage, TypeStride, DefaultAccess, CreateInfo);

			ArraySRV = RHICreateShaderResourceView(ArrayBuffer, TypeStride, PixelFormat);
			if ( !IsReadOnly() )
			{
				ArrayUAV = RHICreateUnorderedAccessView(ArrayBuffer, PixelFormat);
			}
		}

		// Copy data in new data over
		{
			uint8* GPUMemory = reinterpret_cast<uint8*>(RHILockBuffer(ArrayBuffer, 0, ArrayNumBytes, RLM_WriteOnly));
			if (InArrayData.Num() > 0)
			{
				T::CopyCpuToGpuMemory(GPUMemory, InArrayData.GetData(), InArrayData.Num());
			}

			const TArrayType DefaultValue = TArrayType(FNDIArrayImplHelper<TArrayType>::GetDefaultValue());
			T::CopyCpuToGpuMemory(GPUMemory + (sizeof(TVMArrayType) * NumElements), &DefaultValue, 1);

			RHIUnlockBuffer(ArrayBuffer);
		}

		// Adjust counter value
		if ( CountOffset != INDEX_NONE )
		{
			//-OPT: We could push this into the count manager and batch set as part of the clear process
			const FNiagaraGPUInstanceCountManager& CounterManager = ComputeInterface->GetGPUInstanceCounterManager();
			const FRWBuffer& CountBuffer = CounterManager.GetInstanceCountBuffer();
			
			const TPair<uint32, int32> DataToClear(CountOffset, InArrayData.Num());
			RHICmdList.Transition(FRHITransitionInfo(CountBuffer.UAV, FNiagaraGPUInstanceCountManager::kCountBufferDefaultState, ERHIAccess::UAVCompute));
			NiagaraClearCounts::ClearCountsInt(RHICmdList, CountBuffer.UAV, MakeArrayView(&DataToClear, 1));
			RHICmdList.Transition(FRHITransitionInfo(CountBuffer.UAV, ERHIAccess::UAVCompute, FNiagaraGPUInstanceCountManager::kCountBufferDefaultState));
		}
	}

	template<typename T = FNDIArrayImplHelper<TArrayType>>
	typename TEnableIf<!T::bSupportsGPU>::Type UpdateDataImpl(FRHICommandList& RHICmdList, TArray<TArrayType>& InArrayData)
	{
	}

	void UpdateData(FRHICommandList& RHICmdList, TArray<TArrayType>& InArrayData)
	{
		UpdateDataImpl(RHICmdList, InArrayData);
	}

	void ReleaseData()
	{
		DEC_MEMORY_STAT_BY(STAT_NiagaraGPUDataInterfaceMemory, ArrayNumBytes);
		ArrayBuffer.SafeRelease();
		ArrayUAV.SafeRelease();
		ArraySRV.SafeRelease();
	}

	FNiagaraGpuComputeDispatchInterface* ComputeInterface = nullptr;

	FBufferRHIRef				ArrayBuffer;
	FUnorderedAccessViewRHIRef	ArrayUAV;
	FShaderResourceViewRHIRef	ArraySRV;
	uint32						ArrayNumBytes = 0;

	int32						DefaultElements = 0;		// The default number of elements in the buffer, can be used to reduce allocations / required for RW buffers
	int32						NumElements = INDEX_NONE;	// Number of elements in the buffer, for RW buffers this is the buffer size since the actual size is in the counter
	uint32						CountOffset = INDEX_NONE;	// Counter offset for RW buffers
};

template<typename TArrayType, class TOwnerType>
struct FNDIArrayProxyImpl : public INDIArrayProxyBase
{
	static constexpr int32 kSafeMaxElements = TNumericLimits<int32>::Max();
	using TVMArrayType = typename FNDIArrayImplHelper<TArrayType>::TVMArrayType;

	struct FReadArrayRef
	{
		FReadArrayRef(TOwnerType* Owner, FNDIArrayInstanceData_GameThread<TArrayType>* InstanceData)
		{
			if ( InstanceData )
			{
				LockObject = &InstanceData->ArrayRWGuard;
				LockObject->ReadLock();
				ArrayData = InstanceData->bIsModified ? &InstanceData->ArrayData : &Owner->GetArrayReference();
			}
			else
			{
				ArrayData = &Owner->GetArrayReference();
			}
		}
		~FReadArrayRef()
		{
			if ( LockObject )
			{
				LockObject->ReadUnlock();
			}
		}

		const TArray<TArrayType>& GetArray() { return *ArrayData; }

	private:
		FRWLock*					LockObject = nullptr;
		const TArray<TArrayType>*	ArrayData = nullptr;
		UE_NONCOPYABLE(FReadArrayRef);
	};

	struct FWriteArrayRef
	{
		FWriteArrayRef(TOwnerType* Owner, FNDIArrayInstanceData_GameThread<TArrayType>* InstanceData, bool bFromBP=false)
		{
			if (InstanceData)
			{
				LockObject = &InstanceData->ArrayRWGuard;
				LockObject->WriteLock();

				if (bFromBP)//Writes from BP are for user parameters and we should discard modifications and write to the actual DI.
				{
					if (InstanceData->bIsModified)
					{
						Owner->GetArrayReference() = InstanceData->ArrayData;
					}
					InstanceData->bIsModified = false;
					InstanceData->ArrayData.Empty();
					ArrayData = &Owner->GetArrayReference();
 				}
 				else
 				{
 					if (InstanceData->bIsModified == false)
 					{
 						InstanceData->bIsModified = true;
 						InstanceData->ArrayData = Owner->GetArrayReference();
 					}
					ArrayData = &InstanceData->ArrayData;
 				}
			}
			else
			{
				ArrayData = &Owner->GetArrayReference();
			}
		}

		~FWriteArrayRef()
		{
			if (LockObject)
			{
				LockObject->WriteUnlock();
			}
		}

		TArray<TArrayType>& GetArray() { return *ArrayData; }

	private:
		FRWLock*			LockObject = nullptr;
		TArray<TArrayType>*	ArrayData = nullptr;
		UE_NONCOPYABLE(FWriteArrayRef);
	};

	struct FGameToRenderInstanceData
	{
		bool				bUpdateData = false;
		TArray<TArrayType>	ArrayData;
	};

	FNDIArrayProxyImpl(TOwnerType* InOwner)
		: Owner(InOwner)
	{
		CachePropertiesFromOwner();
	}

	void CachePropertiesFromOwner()
	{
		bShouldSyncToGpu = FNiagaraUtilities::ShouldSyncCpuToGpu(Owner->GpuSyncMode);
		bShouldSyncToCpu = FNiagaraUtilities::ShouldSyncGpuToCpu(Owner->GpuSyncMode) && Owner->IsUsedByCPUEmitter();
	}

	//////////////////////////////////////////////////////////////////////////
	// FNiagaraDataInterfaceProxyRW
	virtual int32 PerInstanceDataPassedToRenderThreadSize() const override
	{
		return sizeof(FGameToRenderInstanceData);
	}
	
	virtual void ProvidePerInstanceDataForRenderThread(void* InDataForRenderThread, void* PerInstanceData, const FNiagaraSystemInstanceID& InstanceID)
	{
		FGameToRenderInstanceData* GameToRenderInstanceData = new(InDataForRenderThread) FGameToRenderInstanceData();
		FNDIArrayInstanceData_GameThread<TArrayType>* InstanceData_GT = reinterpret_cast<FNDIArrayInstanceData_GameThread<TArrayType>*>(PerInstanceData);
		if (InstanceData_GT->bIsRenderDirty)
		{
			FReadArrayRef ArrayData(Owner, InstanceData_GT);

			GameToRenderInstanceData->bUpdateData	= true;
			GameToRenderInstanceData->ArrayData		= ArrayData.GetArray();

			InstanceData_GT->bIsRenderDirty = false;
		}
	}

	virtual void ConsumePerInstanceDataFromGameThread(void* PerInstanceData, const FNiagaraSystemInstanceID& InstanceID) override
	{
		FGameToRenderInstanceData* GameToRenderInstanceData = reinterpret_cast<FGameToRenderInstanceData*>(PerInstanceData);
		if ( GameToRenderInstanceData->bUpdateData )
		{
			if ( FNDIArrayInstanceData_RenderThread<TArrayType>* InstanceData_RT = PerInstanceData_RenderThread.Find(InstanceID) )
			{
				FRHICommandListImmediate& RHICmdList = FRHICommandListExecutor::GetImmediateCommandList();
				InstanceData_RT->UpdateData(RHICmdList, GameToRenderInstanceData->ArrayData);
			}
		}
		GameToRenderInstanceData->~FGameToRenderInstanceData();
	}

	virtual FIntVector GetElementCount(FNiagaraSystemInstanceID SystemInstanceID) const override
	{
		if (const FNDIArrayInstanceData_RenderThread<TArrayType>* InstanceData_RT = PerInstanceData_RenderThread.Find(SystemInstanceID))
		{
			return FIntVector(InstanceData_RT->NumElements, 1, 1);
		}
		return FIntVector::ZeroValue;
	}

	virtual uint32 GetGPUInstanceCountOffset(FNiagaraSystemInstanceID SystemInstanceID) const override
	{
		if (const FNDIArrayInstanceData_RenderThread<TArrayType>* InstanceData_RT = PerInstanceData_RenderThread.Find(SystemInstanceID))
		{
			return InstanceData_RT->CountOffset;
		}
		return INDEX_NONE;
	}

	virtual void PostSimulate(const FNDIGpuComputePostSimulateContext& Context) override
	{
		if (bShouldSyncToCpu == false)
		{
			return;
		}
		
		const FNDIArrayInstanceData_RenderThread<TArrayType>* InstanceData_RT = PerInstanceData_RenderThread.Find(Context.GetSystemInstanceID());
		if ( !InstanceData_RT || InstanceData_RT->IsReadOnly() || (InstanceData_RT->ArrayNumBytes == 0) )
		{
			return;
		}

		const FNiagaraGPUInstanceCountManager& CountManager = Context.GetComputeDispatchInterface().GetGPUInstanceCounterManager();
		FNiagaraGpuReadbackManager* ReadbackManager = Context.GetComputeDispatchInterface().GetGpuReadbackManager();

		const FNiagaraGpuReadbackManager::FBufferRequest BufferRequests[] =
		{
			{CountManager.GetInstanceCountBuffer().Buffer, uint32(InstanceData_RT->CountOffset * sizeof(uint32)), sizeof(uint32)},
			{InstanceData_RT->ArrayBuffer, 0, InstanceData_RT->ArrayNumBytes},		//-TODO: Technically last element is default for RW buffers
		};

		const FRHITransitionInfo TransitionsBefore[] =
		{
			FRHITransitionInfo(CountManager.GetInstanceCountBuffer().UAV, ERHIAccess::UAVCompute, ERHIAccess::CopySrc),
			FRHITransitionInfo(InstanceData_RT->ArrayBuffer, ERHIAccess::UAVCompute, ERHIAccess::CopySrc),
		};
		const FRHITransitionInfo TransitionsAfter[] =
		{
			FRHITransitionInfo(CountManager.GetInstanceCountBuffer().UAV, ERHIAccess::CopySrc, ERHIAccess::UAVCompute),
			FRHITransitionInfo(InstanceData_RT->ArrayBuffer, ERHIAccess::CopySrc, ERHIAccess::UAVCompute),
		};

		AddPass(
			Context.GetGraphBuilder(),
			RDG_EVENT_NAME("NDIArrayReadback"),
			[BufferRequests, TransitionsBefore, TransitionsAfter, SystemInstanceID=Context.GetSystemInstanceID(), WeakOwner=TWeakObjectPtr(Owner), Proxy=this, ReadbackManager](FRHICommandListImmediate& RHICmdList)
			{
				RHICmdList.Transition(TransitionsBefore);
				ReadbackManager->EnqueueReadbacks(
					RHICmdList,
					BufferRequests,
					[SystemInstanceID, WeakOwner, Proxy](TConstArrayView<TPair<void*, uint32>> ReadbackData)
					{
						const int32 NumElements = *reinterpret_cast<const uint32*>(ReadbackData[0].Key);
						TArray<TArrayType> ArrayData;
						if ( NumElements > 0 )
						{
							ArrayData.AddUninitialized(NumElements);
							FNDIArrayImplHelper<TArrayType>::CopyGpuToCpuMemory(ArrayData.GetData(), reinterpret_cast<const TVMArrayType*>(ReadbackData[1].Key), NumElements);
						}

						AsyncTask(
							ENamedThreads::GameThread,
							[SystemInstanceID, ArrayData=MoveTemp(ArrayData), WeakOwner, Proxy]()
							{
								// If this is nullptr the proxy is no longer valid so discard
								if ( WeakOwner.Get() == nullptr )
								{
									return;
								}

								Proxy->SetInstanceArrayData(SystemInstanceID, ArrayData);
							}
						);
					}
				);
				RHICmdList.Transition(TransitionsAfter);
			}
		);
	}

	//////////////////////////////////////////////////////////////////////////
	// BP user parameter accessors, should remove if we every start to share the object between instances
	template<typename TFromArrayType>
	void SetArrayData(TConstArrayView<TFromArrayType> InArrayData)
	{
		if (PerInstanceData_GameThread.IsEmpty())
		{
			Owner->GetArrayReference().SetNumUninitialized(InArrayData.Num());
			FNDIArrayImplHelper<TArrayType>::CopyCpuToCpuMemory(Owner->GetArrayReference().GetData(), InArrayData.GetData(), InArrayData.Num());
		}
		else
		{
			ensure(PerInstanceData_GameThread.Num() == 1);
			FNDIArrayInstanceData_GameThread<TArrayType>* InstanceData = PerInstanceData_GameThread.CreateConstIterator().Value();
			FWriteScopeLock	ScopeLock(InstanceData->ArrayRWGuard);
			InstanceData->bIsModified = false;
			InstanceData->bIsRenderDirty = bShouldSyncToGpu;
			InstanceData->ArrayData.Empty();
			Owner->GetArrayReference().SetNum(InArrayData.Num());
			FNDIArrayImplHelper<TArrayType>::CopyCpuToCpuMemory(Owner->GetArrayReference().GetData(), InArrayData.GetData(), InArrayData.Num());
		}
	}

	template<typename TToArrayType>
	TArray<TToArrayType> GetArrayDataCopy()
	{
		ensure(PerInstanceData_GameThread.Num() <= 1);
		FNDIArrayInstanceData_GameThread<TArrayType>* InstanceData = PerInstanceData_GameThread.IsEmpty() ? nullptr : PerInstanceData_GameThread.CreateConstIterator().Value();
		FReadArrayRef ArrayRef(Owner, InstanceData);
		TArray<TToArrayType> OutArray;
		OutArray.SetNum(ArrayRef.GetArray().Num());
		FNDIArrayImplHelper<TArrayType>::CopyCpuToCpuMemory(OutArray.GetData(), ArrayRef.GetArray().GetData(), ArrayRef.GetArray().Num());
		return OutArray;
	}

	template<typename TFromArrayType>
	void SetArrayValue(int Index, const TFromArrayType& Value, bool bSizeToFit)
	{
		ensure(PerInstanceData_GameThread.Num() <= 1);
		FNDIArrayInstanceData_GameThread<TArrayType>* InstanceData = PerInstanceData_GameThread.IsEmpty() ? nullptr : PerInstanceData_GameThread.CreateConstIterator().Value();
		FWriteArrayRef ArrayRef(Owner, InstanceData, true);
		if (!ArrayRef.GetArray().IsValidIndex(Index))
		{
			if (!bSizeToFit)
			{
				return;
			}
			ArrayRef.GetArray().AddDefaulted(Index + 1 - ArrayRef.GetArray().Num());
		}

		FNDIArrayImplHelper<TArrayType>::CopyCpuToCpuMemory(ArrayRef.GetArray().GetData() + Index, &Value, 1);
	}

	template<typename TToArrayType>
	TToArrayType GetArrayValue(int Index)
	{
		TArrayType ValueOut = TArrayType(FNDIArrayImplHelper<TArrayType>::GetDefaultValue());

		ensure(PerInstanceData_GameThread.Num() <= 1);
		FNDIArrayInstanceData_GameThread<TArrayType>* InstanceData = PerInstanceData_GameThread.IsEmpty() ? nullptr : PerInstanceData_GameThread.CreateConstIterator().Value();
		FReadArrayRef ArrayRef(Owner, InstanceData);

		if (!ArrayRef.GetArray().IsValidIndex(Index))
		{
			ValueOut = ArrayRef.GetArray()[Index];
		}

		TToArrayType ToValueOut;
		FNDIArrayImplHelper<TArrayType>::CopyCpuToCpuMemory(&ToValueOut, &ValueOut, 1);
		return ToValueOut;
	}

	//////////////////////////////////////////////////////////////////////////
	// VM accessors to ensure we maintain per correctness for shared data interfaces
	void SetInstanceArrayData(FNiagaraSystemInstanceID InstanceID, const TArray<TArrayType>& InArrayData)
	{
		if ( FNDIArrayInstanceData_GameThread<TArrayType>* InstanceData = PerInstanceData_GameThread.FindRef(InstanceID) )
		{
			FWriteArrayRef ArrayData(Owner, InstanceData);
			ArrayData.GetArray() = InArrayData;
			InstanceData->bIsRenderDirty = bShouldSyncToGpu;
		}
	}

	TArray<TArrayType> GetArrayData(FNiagaraSystemInstanceID InstanceID)
	{
		TArray<TArrayType> ArrayDataOut;
		if (FNDIArrayInstanceData_GameThread<TArrayType>* InstanceData = PerInstanceData_GameThread.FindRef(InstanceID))
		{
			FReadArrayRef ArrayData(Owner, InstanceData);
			ArrayDataOut = ArrayData.GetArray();
		}
		return ArrayDataOut;
	}

	void SetArrayValue(FNiagaraSystemInstanceID InstanceID, int Index, const TArrayType& Value, bool bSizeToFit)
	{
		if (FNDIArrayInstanceData_GameThread<TArrayType>* InstanceData = PerInstanceData_GameThread.FindRef(InstanceID))
		{
			FWriteArrayRef ArrayData(Owner, InstanceData);
			if (!ArrayData.GetArray().IsValidIndex(Index))
			{
				if (!bSizeToFit)
				{
					return;
				}
				ArrayData.GetArray().AddDefaulted(Index + 1 - ArrayData.GetArray().Num());
			}

			ArrayData.GetArray()[Index] = Value;
			InstanceData->bIsRenderDirty = bShouldSyncToGpu;
		}
	}

	TArrayType GetArrayValue(FNiagaraSystemInstanceID InstanceID, int Index)
	{
		TArrayType ValueOut = TArrayType(FNDIArrayImplHelper<TArrayType>::GetDefaultValue());

		if (FNDIArrayInstanceData_GameThread<TArrayType>* InstanceData = PerInstanceData_GameThread.FindRef(InstanceID))
		{
			FReadArrayRef ArrayData(Owner, InstanceData);
			if (!ArrayData.GetArray().IsValidIndex(Index))
			{
				ValueOut = ArrayData.GetArray()[Index];
			}
		}

		return ValueOut;
	}

	//////////////////////////////////////////////////////////////////////////
	// INDIArrayProxyBase
	virtual void GetFunctions(TArray<FNiagaraFunctionSignature>& OutFunctions) const override
	{
		OutFunctions.Reserve(OutFunctions.Num() + 9);

		// Immutable functions
		FNiagaraFunctionSignature DefaultImmutableSig;
		DefaultImmutableSig.bMemberFunction = true;
		DefaultImmutableSig.bRequiresContext = false;
		DefaultImmutableSig.bSupportsCPU = FNDIArrayImplHelper<TArrayType>::bSupportsCPU;
		DefaultImmutableSig.bSupportsGPU = FNDIArrayImplHelper<TArrayType>::bSupportsGPU;
		DefaultImmutableSig.Inputs.Emplace(FNiagaraTypeDefinition(TOwnerType::StaticClass()), TEXT("Array interface"));
#if WITH_EDITORONLY_DATA
		DefaultImmutableSig.FunctionVersion = FNiagaraDataInterfaceArrayImplHelper::FFunctionVersion::LatestVersion;
#endif
		{
			FNiagaraFunctionSignature& Sig = OutFunctions.Add_GetRef(DefaultImmutableSig);
			Sig.Name = FNiagaraDataInterfaceArrayImplHelper::Function_LengthName;
			Sig.Outputs.Emplace(FNiagaraTypeDefinition::GetIntDef(), TEXT("Num"));
		#if WITH_EDITORONLY_DATA
			Sig.Description = NSLOCTEXT("Niagara", "Array_LengthDesc", "Gets the number of elements in the array.");
		#endif
		}

		{
			FNiagaraFunctionSignature& Sig = OutFunctions.Add_GetRef(DefaultImmutableSig);
			Sig.Name = FNiagaraDataInterfaceArrayImplHelper::Function_IsValidIndexName;
			Sig.Inputs.Emplace(FNiagaraTypeDefinition::GetIntDef(), TEXT("Index"));
			Sig.Outputs.Emplace(FNiagaraTypeDefinition::GetBoolDef(), TEXT("Valid"));
		#if WITH_EDITORONLY_DATA
			Sig.Description = NSLOCTEXT("Niagara", "Array_IsValidIndexDesc", "Tests to see if the index is valid and exists in the array.");
		#endif
		}

		{
			FNiagaraFunctionSignature& Sig = OutFunctions.Add_GetRef(DefaultImmutableSig);
			Sig.Name = FNiagaraDataInterfaceArrayImplHelper::Function_LastIndexName;
			Sig.Outputs.Emplace(FNiagaraTypeDefinition::GetIntDef(), TEXT("Index"));
		#if WITH_EDITORONLY_DATA
			Sig.Description = NSLOCTEXT("Niagara", "Array_LastIndexDesc", "Returns the last valid index in the array, will be -1 if no elements.");
		#endif
		}

		{
			FNiagaraFunctionSignature& Sig = OutFunctions.Add_GetRef(DefaultImmutableSig);
			Sig.Name = FNiagaraDataInterfaceArrayImplHelper::Function_GetName;
			Sig.Inputs.Emplace(FNiagaraTypeDefinition::GetIntDef(), TEXT("Index"));
			Sig.Outputs.Emplace(FNDIArrayImplHelper<TArrayType>::GetTypeDefinition(), TEXT("Value"));
		#if WITH_EDITORONLY_DATA
			Sig.Description = NSLOCTEXT("Niagara", "Array_GetDesc", "Gets the value from the array at the given zero based index.");
		#endif
		}

		// Mutable functions
		FNiagaraFunctionSignature DefaultMutableSig = DefaultImmutableSig;
		DefaultMutableSig.bSupportsGPU = FNDIArrayImplHelper<TArrayType>::bSupportsGPU;
		DefaultMutableSig.bRequiresExecPin = true;
		{
			FNiagaraFunctionSignature& Sig = OutFunctions.Add_GetRef(DefaultMutableSig);
			Sig.Name = FNiagaraDataInterfaceArrayImplHelper::Function_ClearName;
			Sig.ModuleUsageBitmask = ENiagaraScriptUsageMask::System | ENiagaraScriptUsageMask::Emitter;
		#if WITH_EDITORONLY_DATA
			Sig.Description = NSLOCTEXT("Niagara", "Array_ClearDesc", "Clears the array, removing all elements");
		#endif
		}

		{
			FNiagaraFunctionSignature& Sig = OutFunctions.Add_GetRef(DefaultMutableSig);
			Sig.Name = FNiagaraDataInterfaceArrayImplHelper::Function_ResizeName;
			Sig.ModuleUsageBitmask = ENiagaraScriptUsageMask::System | ENiagaraScriptUsageMask::Emitter;
			Sig.Inputs.Emplace(FNiagaraTypeDefinition::GetIntDef(), TEXT("Num"));
		#if WITH_EDITORONLY_DATA
			Sig.Description = NSLOCTEXT("Niagara", "Array_ResizeDesc", "Resizes the array to the specified size, initializing new elements with the default value.");
		#endif
		}

		{
			FNiagaraFunctionSignature& Sig = OutFunctions.Add_GetRef(DefaultMutableSig);
			Sig.Name = FNiagaraDataInterfaceArrayImplHelper::Function_SetArrayElemName;
			Sig.Inputs.Emplace(FNiagaraTypeDefinition::GetBoolDef(), TEXT("SkipSet"));
			Sig.Inputs.Emplace(FNiagaraTypeDefinition::GetIntDef(), TEXT("Index"));
			Sig.Inputs.Emplace(FNDIArrayImplHelper<TArrayType>::GetTypeDefinition(), TEXT("Value"));
		#if WITH_EDITORONLY_DATA
			Sig.Description = NSLOCTEXT("Niagara", "Array_SetArrayElemDesc", "Sets the value at the given zero based index (i.e the first element is 0).");
			Sig.InputDescriptions.Add(Sig.Inputs[1], NSLOCTEXT("Niagara", "Array_SetArrayElemDesc_SkipSet", "When enabled will not set the array value."));
		#endif
		}

		{
			FNiagaraFunctionSignature& Sig = OutFunctions.Add_GetRef(DefaultMutableSig);
			Sig.Name = FNiagaraDataInterfaceArrayImplHelper::Function_AddName;
			Sig.Inputs.Emplace(FNiagaraTypeDefinition::GetBoolDef(), TEXT("SkipAdd"));
			Sig.Inputs.Emplace(FNDIArrayImplHelper<TArrayType>::GetTypeDefinition(), TEXT("Value"));
		#if WITH_EDITORONLY_DATA
			Sig.Description = NSLOCTEXT("Niagara", "Array_AddDesc", "Optionally add a value onto the end of the array.");
			Sig.InputDescriptions.Add(Sig.Inputs[1], NSLOCTEXT("Niagara", "Array_AddDesc_SkipAdd", "When enabled we will not add an element to the array."));
		#endif
		}

		{
			FNiagaraFunctionSignature& Sig = OutFunctions.Add_GetRef(DefaultMutableSig);
			Sig.Name = FNiagaraDataInterfaceArrayImplHelper::Function_RemoveLastElemName;
			Sig.Inputs.Emplace(FNiagaraTypeDefinition::GetBoolDef(), TEXT("SkipRemove"));
			Sig.Outputs.Emplace(FNDIArrayImplHelper<TArrayType>::GetTypeDefinition(), TEXT("Value"));
			Sig.Outputs.Emplace(FNiagaraTypeDefinition::GetBoolDef(), TEXT("IsValid"));
		#if WITH_EDITORONLY_DATA
			Sig.Description = NSLOCTEXT("Niagara", "Array_RemoveLastElemDesc", "Optionally remove the last element from the array.  Returns the default value if no elements are in the array or you skip the remove.");
			Sig.InputDescriptions.Add(Sig.Inputs[1], NSLOCTEXT("Niagara", "Array_RemoveLastElemDesc_SkipRemove", "When enabled will not remove a value from the array, the return value will therefore be invalid."));
			Sig.OutputDescriptions.Add(Sig.Outputs[1], NSLOCTEXT("Niagara", "Array_RemoveLastElemDesc_IsValid", "True if we removed a value from the array, False if no entries or we skipped the remove."));
		#endif
		}

		if (FNDIArrayImplHelper<TArrayType>::bSupportsAtomicOps)
		{
			{
				FNiagaraFunctionSignature& Sig = OutFunctions.Add_GetRef(DefaultMutableSig);
				Sig.Name = FNiagaraDataInterfaceArrayImplHelper::Function_AtomicAddName;
				Sig.Inputs.Emplace(FNiagaraTypeDefinition::GetBoolDef(), TEXT("SkipAdd"));
				Sig.Inputs.Emplace(FNiagaraTypeDefinition::GetIntDef(), TEXT("Index"));
				Sig.Inputs.Emplace(FNDIArrayImplHelper<TArrayType>::GetTypeDefinition(), TEXT("Value"));
				Sig.Outputs.Emplace(FNDIArrayImplHelper<TArrayType>::GetTypeDefinition(), TEXT("PreviousValue"));
				Sig.Outputs.Emplace(FNDIArrayImplHelper<TArrayType>::GetTypeDefinition(), TEXT("CurrentValue"));
				Sig.SetDescription(NSLOCTEXT("Niagara", "Array_AtomicAddDesc", "Optionally perform an atomic add on the array element."));
				Sig.SetInputDescription(Sig.Inputs[1], NSLOCTEXT("Niagara", "Array_AtomicAdd_SkipAdd", "When enabled will not perform the add operation, the return values will therefore be invalid."));
				Sig.SetOutputDescription(Sig.Inputs[0], NSLOCTEXT("Niagara", "Array_AtomicAdd_PrevValue", "The value before the operation was performed."));
				Sig.SetOutputDescription(Sig.Inputs[1], NSLOCTEXT("Niagara", "Array_AtomicAdd_CurrValue", "The value after the operation was performed."));
			}
			{
				FNiagaraFunctionSignature& Sig = OutFunctions.Add_GetRef(DefaultMutableSig);
				Sig.Name = FNiagaraDataInterfaceArrayImplHelper::Function_AtomicMinName;
				Sig.Inputs.Emplace(FNiagaraTypeDefinition::GetBoolDef(), TEXT("SkipMin"));
				Sig.Inputs.Emplace(FNiagaraTypeDefinition::GetIntDef(), TEXT("Index"));
				Sig.Inputs.Emplace(FNDIArrayImplHelper<TArrayType>::GetTypeDefinition(), TEXT("Value"));
				Sig.Outputs.Emplace(FNDIArrayImplHelper<TArrayType>::GetTypeDefinition(), TEXT("PreviousValue"));
				Sig.Outputs.Emplace(FNDIArrayImplHelper<TArrayType>::GetTypeDefinition(), TEXT("CurrentValue"));
				Sig.SetDescription(NSLOCTEXT("Niagara", "Array_AtomicMinDesc", "Optionally perform an atomic min on the array element."));
				Sig.SetInputDescription(Sig.Inputs[1], NSLOCTEXT("Niagara", "Array_AtomicMin_SkipMin", "When enabled will not perform the min operation, the return values will therefore be invalid."));
				Sig.SetOutputDescription(Sig.Inputs[0], NSLOCTEXT("Niagara", "Array_AtomicMin_PrevValue", "The value before the operation was performed."));
				Sig.SetOutputDescription(Sig.Inputs[1], NSLOCTEXT("Niagara", "Array_AtomicMin_CurrValue", "The value after the operation was performed."));
			}
			{
				FNiagaraFunctionSignature& Sig = OutFunctions.Add_GetRef(DefaultMutableSig);
				Sig.Name = FNiagaraDataInterfaceArrayImplHelper::Function_AtomicMaxName;
				Sig.Inputs.Emplace(FNiagaraTypeDefinition::GetBoolDef(), TEXT("SkipMax"));
				Sig.Inputs.Emplace(FNiagaraTypeDefinition::GetIntDef(), TEXT("Index"));
				Sig.Inputs.Emplace(FNDIArrayImplHelper<TArrayType>::GetTypeDefinition(), TEXT("Value"));
				Sig.Outputs.Emplace(FNDIArrayImplHelper<TArrayType>::GetTypeDefinition(), TEXT("PreviousValue"));
				Sig.Outputs.Emplace(FNDIArrayImplHelper<TArrayType>::GetTypeDefinition(), TEXT("CurrentValue"));
				Sig.SetDescription(NSLOCTEXT("Niagara", "Array_AtomicMaxDesc", "Optionally perform an atomic max on the array element."));
				Sig.SetInputDescription(Sig.Inputs[1], NSLOCTEXT("Niagara", "Array_AtomicMax_SkipMax", "When enabled will not perform the max operation, the return values will therefore be invalid."));
				Sig.SetOutputDescription(Sig.Inputs[0], NSLOCTEXT("Niagara", "Array_AtomicMax_PrevValue", "The value before the operation was performed."));
				Sig.SetOutputDescription(Sig.Inputs[1], NSLOCTEXT("Niagara", "Array_AtomicMax_CurrValue", "The value after the operation was performed."));
			}
		}
	}

	template<typename T = FNDIArrayImplHelper<TArrayType>>
	typename TEnableIf<T::bSupportsCPU>::Type GetVMExternalFunction_Internal(const FVMExternalFunctionBindingInfo& BindingInfo, void* InstanceData, FVMExternalFunction& OutFunc)
	{
		// Immutable functions
		if (BindingInfo.Name == FNiagaraDataInterfaceArrayImplHelper::Function_LengthName)
		{
			check(BindingInfo.GetNumInputs() == 1 && BindingInfo.GetNumOutputs() == 1);
			OutFunc = FVMExternalFunction::CreateLambda([this](FVectorVMExternalFunctionContext& Context) { this->VMGetLength(Context); });
		}
		else if (BindingInfo.Name == FNiagaraDataInterfaceArrayImplHelper::Function_IsValidIndexName)
		{
			check(BindingInfo.GetNumInputs() == 2 && BindingInfo.GetNumOutputs() == 1);
			OutFunc = FVMExternalFunction::CreateLambda([this](FVectorVMExternalFunctionContext& Context) { this->VMIsValidIndex(Context); });
		}
		else if (BindingInfo.Name == FNiagaraDataInterfaceArrayImplHelper::Function_LastIndexName)
		{
			check(BindingInfo.GetNumInputs() == 1 && BindingInfo.GetNumOutputs() == 1);
			OutFunc = FVMExternalFunction::CreateLambda([this](FVectorVMExternalFunctionContext& Context) { this->VMGetLastIndex(Context); });
		}
		else if (BindingInfo.Name == FNiagaraDataInterfaceArrayImplHelper::Function_GetName)
		{
			// Note: Outputs is variable based upon type
			//check(BindingInfo.GetNumInputs() == 2 && BindingInfo.GetNumOutputs() == 1);
			OutFunc = FVMExternalFunction::CreateLambda([this](FVectorVMExternalFunctionContext& Context) { this->VMGetValue(Context); });
		}
		// Mutable functions
		else if (BindingInfo.Name == FNiagaraDataInterfaceArrayImplHelper::Function_ClearName)
		{
			check(BindingInfo.GetNumInputs() == 1 && BindingInfo.GetNumOutputs() == 0);
			OutFunc = FVMExternalFunction::CreateLambda([this](FVectorVMExternalFunctionContext& Context) { this->VMClear(Context); });
		}
		else if (BindingInfo.Name == FNiagaraDataInterfaceArrayImplHelper::Function_ResizeName)
		{
			check(BindingInfo.GetNumInputs() == 2 && BindingInfo.GetNumOutputs() == 0);
			OutFunc = FVMExternalFunction::CreateLambda([this](FVectorVMExternalFunctionContext& Context) { this->VMResize(Context); });
		}
		else if (BindingInfo.Name == FNiagaraDataInterfaceArrayImplHelper::Function_SetArrayElemName)
		{
			OutFunc = FVMExternalFunction::CreateLambda([this](FVectorVMExternalFunctionContext& Context) { this->VMSetValue(Context); });
		}
		else if (BindingInfo.Name == FNiagaraDataInterfaceArrayImplHelper::Function_AddName)
		{
			// Note: Inputs is variable based upon type
			OutFunc = FVMExternalFunction::CreateLambda([this](FVectorVMExternalFunctionContext& Context) { this->VMPushValue(Context); });
		}
		else if (BindingInfo.Name == FNiagaraDataInterfaceArrayImplHelper::Function_RemoveLastElemName)
		{
			// Note: Outputs is variable based upon type
			OutFunc = FVMExternalFunction::CreateLambda([this](FVectorVMExternalFunctionContext& Context) { this->VMPopValue(Context); });
		}
	}

	template<typename T = FNDIArrayImplHelper<TArrayType>>
	typename TEnableIf<!T::bSupportsCPU>::Type GetVMExternalFunction_Internal(const FVMExternalFunctionBindingInfo& BindingInfo, void* InstanceData, FVMExternalFunction& OutFunc)
	{
	}

	template<typename T = FNDIArrayImplHelper<TArrayType>>
	typename TEnableIf<T::bSupportsAtomicOps>::Type GetVMExternalFunction_AtomicInternal(const FVMExternalFunctionBindingInfo& BindingInfo, void* InstanceData, FVMExternalFunction& OutFunc)
	{
		if (BindingInfo.Name == FNiagaraDataInterfaceArrayImplHelper::Function_AtomicAddName)
		{
			OutFunc = FVMExternalFunction::CreateLambda([this](FVectorVMExternalFunctionContext& Context) { this->VMAtomicAdd(Context); });
		}
		else if (BindingInfo.Name == FNiagaraDataInterfaceArrayImplHelper::Function_AtomicMinName)
		{
			OutFunc = FVMExternalFunction::CreateLambda([this](FVectorVMExternalFunctionContext& Context) { this->VMAtomicMin(Context); });
		}
		else if (BindingInfo.Name == FNiagaraDataInterfaceArrayImplHelper::Function_AtomicMaxName)
		{
			OutFunc = FVMExternalFunction::CreateLambda([this](FVectorVMExternalFunctionContext& Context) { this->VMAtomicMax(Context); });
		}
	}

	template<typename T = FNDIArrayImplHelper<TArrayType>>
	typename TEnableIf<!T::bSupportsAtomicOps>::Type GetVMExternalFunction_AtomicInternal(const FVMExternalFunctionBindingInfo& BindingInfo, void* InstanceData, FVMExternalFunction& OutFunc)
	{
	}

	virtual void GetVMExternalFunction(const FVMExternalFunctionBindingInfo& BindingInfo, void* InstanceData, FVMExternalFunction& OutFunc) override
	{
		GetVMExternalFunction_Internal(BindingInfo, InstanceData, OutFunc);
		if (OutFunc.IsBound() == false)
		{
			GetVMExternalFunction_AtomicInternal(BindingInfo, InstanceData, OutFunc);
		}
	}

#if WITH_EDITORONLY_DATA
	bool IsRWGpuArray(const FNiagaraDataInterfaceGPUParamInfo& ParamInfo) const
	{
		return ParamInfo.GeneratedFunctions.ContainsByPredicate(
			[&](const FNiagaraDataInterfaceGeneratedFunction& Function)
			{
				return FNiagaraDataInterfaceArrayImplHelper::IsRWFunction(Function.DefinitionName);
			}
		);
	}

	template<typename T = FNDIArrayImplHelper<TArrayType>>
	typename TEnableIf<T::bSupportsGPU>::Type GetParameterDefinitionHLSL_Internal(const FNiagaraDataInterfaceGPUParamInfo& ParamInfo, FString& OutHLSL) const
	{
		TMap<FString, FStringFormatArg> TemplateArgs =
		{
			{TEXT("ParameterName"),		ParamInfo.DataInterfaceHLSLSymbol},
			{TEXT("VariableType"),		FNDIArrayImplHelper<TArrayType>::HLSLVariableType},
			{TEXT("ReadBufferType"),	FNDIArrayImplHelper<TArrayType>::ReadHLSLBufferType},
			{TEXT("ReadBufferRead"),	FNDIArrayImplHelper<TArrayType>::ReadHLSLBufferRead},
			{TEXT("RWBufferType"),		FNDIArrayImplHelper<TArrayType>::RWHLSLBufferType},
			{TEXT("RWBufferRead"),		FNDIArrayImplHelper<TArrayType>::RWHLSLBufferRead},
			{TEXT("RWBufferWrite"),		FNDIArrayImplHelper<TArrayType>::RWHLSLBufferWrite},
			{TEXT("bSupportsAtomicOps"),	FNDIArrayImplHelper<TArrayType>::bSupportsAtomicOps ? 1 : 0},
		};

		FString TemplateFile;
		LoadShaderSourceFile(FNiagaraDataInterfaceArrayImplHelper::GetHLSLTemplateFile(IsRWGpuArray(ParamInfo)), EShaderPlatform::SP_PCD3D_SM5, &TemplateFile, nullptr);
		OutHLSL += FString::Format(*TemplateFile, TemplateArgs);
	}

	template<typename T = FNDIArrayImplHelper<TArrayType>>
	typename TEnableIf<!T::bSupportsGPU>::Type GetParameterDefinitionHLSL_Internal(const FNiagaraDataInterfaceGPUParamInfo& ParamInfo, FString& OutHLSL) const
	{
	}

	virtual void GetParameterDefinitionHLSL(const FNiagaraDataInterfaceGPUParamInfo& ParamInfo, FString& OutHLSL) const override
	{
		GetParameterDefinitionHLSL_Internal(ParamInfo, OutHLSL);
	}

	virtual bool GetFunctionHLSL(const FNiagaraDataInterfaceGPUParamInfo& ParamInfo, const FNiagaraDataInterfaceGeneratedFunction& FunctionInfo, int FunctionInstanceIndex, FString& OutHLSL) const override
	{
		if (FNDIArrayImplHelper<TArrayType>::bSupportsGPU)
		{
			if ((FunctionInfo.DefinitionName == FNiagaraDataInterfaceArrayImplHelper::Function_LengthName) ||
				(FunctionInfo.DefinitionName == FNiagaraDataInterfaceArrayImplHelper::Function_IsValidIndexName) ||
				(FunctionInfo.DefinitionName == FNiagaraDataInterfaceArrayImplHelper::Function_LastIndexName) ||
				(FunctionInfo.DefinitionName == FNiagaraDataInterfaceArrayImplHelper::Function_GetName))
			{
				return true;
			}

			if ((FunctionInfo.DefinitionName == FNiagaraDataInterfaceArrayImplHelper::Function_ClearName) ||
				(FunctionInfo.DefinitionName == FNiagaraDataInterfaceArrayImplHelper::Function_ResizeName) ||
				(FunctionInfo.DefinitionName == FNiagaraDataInterfaceArrayImplHelper::Function_SetArrayElemName) ||
				(FunctionInfo.DefinitionName == FNiagaraDataInterfaceArrayImplHelper::Function_AddName) ||
				(FunctionInfo.DefinitionName == FNiagaraDataInterfaceArrayImplHelper::Function_RemoveLastElemName))
			{
				return true;
			}

			if ( FNDIArrayImplHelper<TArrayType>::bSupportsAtomicOps )
			{
				if ((FunctionInfo.DefinitionName == FNiagaraDataInterfaceArrayImplHelper::Function_AtomicAddName) ||
					(FunctionInfo.DefinitionName == FNiagaraDataInterfaceArrayImplHelper::Function_AtomicMinName) ||
					(FunctionInfo.DefinitionName == FNiagaraDataInterfaceArrayImplHelper::Function_AtomicMaxName) )
				{
					return true;
				}
			}
		}
		return false;
	}

	virtual bool AppendCompileHash(FNiagaraCompileHashVisitor* InVisitor) const override
	{
		if (FNDIArrayImplHelper<TArrayType>::bSupportsGPU)
		{
			FSHAHash Hash = GetShaderFileHash(FNiagaraDataInterfaceArrayImplHelper::GetHLSLTemplateFile(false), EShaderPlatform::SP_PCD3D_SM5);
			InVisitor->UpdateString(TEXT("NiagaraDataInterfaceArrayTemplateHLSLSource"), Hash.ToString());
			Hash = GetShaderFileHash(FNiagaraDataInterfaceArrayImplHelper::GetHLSLTemplateFile(true), EShaderPlatform::SP_PCD3D_SM5);
			InVisitor->UpdateString(TEXT("NiagaraDataInterfaceArrayTemplateHLSLSource"), Hash.ToString());
		}
		return true;
	}

	virtual bool UpgradeFunctionCall(FNiagaraFunctionSignature& FunctionSignature) const override
	{
		return FNiagaraDataInterfaceArrayImplHelper::UpgradeFunctionCall(FunctionSignature);
	}
#endif
#if WITH_NIAGARA_DEBUGGER
	virtual void DrawDebugHud(UCanvas* Canvas, FNiagaraSystemInstance* SystemInstance, FString& VariableDataString, bool bVerbose) const override
	{
		FNDIArrayInstanceData_GameThread<TArrayType>* InstanceData = PerInstanceData_GameThread.FindRef(SystemInstance->GetId());
		if (InstanceData == nullptr )
		{
			return;
		}
		FReadArrayRef ArrayData(Owner, InstanceData);
		VariableDataString = FString::Printf(TEXT("ArrayType(%s) CpuLength(%d)"), *FNDIArrayImplHelper<TArrayType>::GetTypeDefinition().GetName(), ArrayData.GetArray().Num());
	}
#endif

	virtual bool CopyToInternal(INDIArrayProxyBase* InDestination) const override
	{
		auto Destination = static_cast<FNDIArrayProxyImpl<TArrayType, TOwnerType>*>(InDestination);
		Destination->Owner->GetArrayReference() = Owner->GetArrayReference();
		return true;
	}

	virtual bool Equals(const INDIArrayProxyBase* InOther) const override
	{
		auto Other = static_cast<const FNDIArrayProxyImpl<TArrayType, TOwnerType>*>(InOther);
		return Other->Owner->GetArrayReference() == Owner->GetArrayReference();
	}

	virtual int32 PerInstanceDataSize() const override
	{
		return sizeof(FNDIArrayInstanceData_GameThread<TArrayType>);
	}

	virtual bool InitPerInstanceData(UNiagaraDataInterface* DataInterface, void* InPerInstanceData, FNiagaraSystemInstance* SystemInstance) override
	{
		// Ensure we have the latest sync mode settings
		CachePropertiesFromOwner();

		auto* InstanceData_GT = new(InPerInstanceData) FNDIArrayInstanceData_GameThread<TArrayType>();
		InstanceData_GT->bIsRenderDirty = true;

		PerInstanceData_GameThread.Emplace(SystemInstance->GetId(), InstanceData_GT);

		if ( FNDIArrayImplHelper<TArrayType>::bSupportsGPU && Owner->IsUsedWithGPUEmitter() )
		{
			bool bRWGpuArray = false;
			FNiagaraDataInterfaceUtilities::ForEachGpuFunction(
				DataInterface, SystemInstance,
				[&](const FNiagaraDataInterfaceGeneratedFunction& Function) -> bool
				{
					bRWGpuArray = FNiagaraDataInterfaceArrayImplHelper::IsRWFunction(Function.DefinitionName);
					return !bRWGpuArray;
				}
			);

			ENQUEUE_RENDER_COMMAND(FNDIArrayProxyImpl_AddProxy)
			(
				[Proxy_RT=this, InstanceID_RT=SystemInstance->GetId(), ComputeInterface_RT=SystemInstance->GetComputeDispatchInterface(), MaxElements_RT=Owner->MaxElements, bRWGpuArray_RT = bRWGpuArray](FRHICommandListImmediate& RHICmdList)
				{
					FNDIArrayInstanceData_RenderThread<TArrayType>* InstanceData_RT = &Proxy_RT->PerInstanceData_RenderThread.Add(InstanceID_RT);
					InstanceData_RT->Initialize(RHICmdList, ComputeInterface_RT, MaxElements_RT, bRWGpuArray_RT);
				}
			);
		}

		return true;
	}

	virtual void DestroyPerInstanceData(void* InPerInstanceData, FNiagaraSystemInstance* SystemInstance) override
	{
		auto* InstanceData_GT = reinterpret_cast<FNDIArrayInstanceData_GameThread<TArrayType>*>(InPerInstanceData);

		if ( FNDIArrayImplHelper<TArrayType>::bSupportsGPU && Owner->IsUsedWithGPUEmitter() )
		{
			ENQUEUE_RENDER_COMMAND(FNDIArrayProxyImpl_RemoveProxy)
			(
				[Proxy_RT=this, InstanceID_RT=SystemInstance->GetId()](FRHICommandListImmediate& RHICmdList)
				{
					Proxy_RT->PerInstanceData_RenderThread.Remove(InstanceID_RT);
				}
			);
		}
		PerInstanceData_GameThread.Remove(SystemInstance->GetId());
		InstanceData_GT->~FNDIArrayInstanceData_GameThread<TArrayType>();
	}

	virtual void SetShaderParameters(FShaderParameters* ShaderParameters, FNiagaraSystemInstanceID SystemInstanceID) const override
	{
		const auto* InstanceData_RT = &PerInstanceData_RenderThread.FindChecked(SystemInstanceID);
		if ( InstanceData_RT->IsReadOnly() )
		{
			ShaderParameters->ArrayBufferParams.X	= InstanceData_RT->NumElements;
			ShaderParameters->ArrayBufferParams.Y	= FMath::Max(0, InstanceData_RT->NumElements - 1);
			ShaderParameters->ArrayReadBuffer		= InstanceData_RT->ArraySRV;
		}
		else
		{
			ShaderParameters->ArrayBufferParams.X	= (int32)InstanceData_RT->CountOffset;
			ShaderParameters->ArrayBufferParams.Y	= InstanceData_RT->NumElements;
			ShaderParameters->ArrayRWBuffer			= InstanceData_RT->ArrayUAV;
		}
	}

	//////////////////////////////////////////////////////////////////////////
	void VMGetLength(FVectorVMExternalFunctionContext& Context)
	{
		VectorVM::FUserPtrHandler<FNDIArrayInstanceData_GameThread<TArrayType>> InstanceData(Context);
		FNDIOutputParam<int32> OutValue(Context);

		FReadArrayRef ArrayData(Owner, InstanceData);
		const int32 Num = ArrayData.GetArray().Num();
		for (int32 i = 0; i < Context.GetNumInstances(); ++i)
		{
			OutValue.SetAndAdvance(Num);
		}
	}

	void VMIsValidIndex(FVectorVMExternalFunctionContext& Context)
	{
		VectorVM::FUserPtrHandler<FNDIArrayInstanceData_GameThread<TArrayType>> InstanceData(Context);
		FNDIInputParam<int32> IndexParam(Context);
		FNDIOutputParam<FNiagaraBool> OutValue(Context);

		FReadArrayRef ArrayData(Owner, InstanceData);
		const int32 Num = ArrayData.GetArray().Num();
		for (int32 i = 0; i < Context.GetNumInstances(); ++i)
		{
			const int32 Index = IndexParam.GetAndAdvance();
			OutValue.SetAndAdvance((Index >= 0) && (Index < Num));
		}
	}

	void VMGetLastIndex(FVectorVMExternalFunctionContext& Context)
	{
		VectorVM::FUserPtrHandler<FNDIArrayInstanceData_GameThread<TArrayType>> InstanceData(Context);
		FNDIOutputParam<int32> OutValue(Context);

		FReadArrayRef ArrayData(Owner, InstanceData);
		const int32 Num = ArrayData.GetArray().Num() - 1;
		for (int32 i = 0; i < Context.GetNumInstances(); ++i)
		{
			OutValue.SetAndAdvance(Num);
		}
	}

	void VMGetValue(FVectorVMExternalFunctionContext& Context)
	{
		VectorVM::FUserPtrHandler<FNDIArrayInstanceData_GameThread<TArrayType>> InstanceData(Context);
		FNDIInputParam<int32> IndexParam(Context);
		FNDIOutputParam<TVMArrayType> OutValue(Context);

		FReadArrayRef ArrayData(Owner, InstanceData);
		const int32 Num = ArrayData.GetArray().Num() - 1;
		if (Num >= 0)
		{
			for (int32 i = 0; i < Context.GetNumInstances(); ++i)
			{
				const int32 Index = FMath::Clamp(IndexParam.GetAndAdvance(), 0, Num);
				OutValue.SetAndAdvance(TVMArrayType(ArrayData.GetArray()[Index]));
			}
		}
		else
		{
			const TVMArrayType DefaultValue = FNDIArrayImplHelper<TArrayType>::GetDefaultValue();
			for (int32 i = 0; i < Context.GetNumInstances(); ++i)
			{
				OutValue.SetAndAdvance(DefaultValue);
			}
		}
	}

	void VMClear(FVectorVMExternalFunctionContext& Context)
	{
		ensureMsgf(Context.GetNumInstances() == 1, TEXT("Setting the number of values in an array with more than one instance, which doesn't make sense"));
		VectorVM::FUserPtrHandler<FNDIArrayInstanceData_GameThread<TArrayType>> InstanceData(Context);

		FWriteArrayRef ArrayData(Owner, InstanceData);
		ArrayData.GetArray().Reset();

		InstanceData->bIsRenderDirty = bShouldSyncToGpu;
	}

	void VMResize(FVectorVMExternalFunctionContext& Context)
	{
		ensureMsgf(Context.GetNumInstances() == 1, TEXT("Setting the number of values in an array with more than one instance, which doesn't make sense"));
		VectorVM::FUserPtrHandler<FNDIArrayInstanceData_GameThread<TArrayType>> InstanceData(Context);
		FNDIInputParam<int32> NewNumParam(Context);

		FWriteArrayRef ArrayData(Owner, InstanceData);

		const int32 OldNum = ArrayData.GetArray().Num();
		const int32 NewNum = FMath::Min(NewNumParam.GetAndAdvance(), kSafeMaxElements);
		ArrayData.GetArray().SetNumUninitialized(NewNum);

		if (NewNum > OldNum)
		{
			const TArrayType DefaultValue = TArrayType(FNDIArrayImplHelper<TArrayType>::GetDefaultValue());
			for (int32 i = OldNum; i < NewNum; ++i)
			{
				ArrayData.GetArray()[i] = DefaultValue;
			}
		}

		InstanceData->bIsRenderDirty = bShouldSyncToGpu;
	}

	void VMSetValue(FVectorVMExternalFunctionContext& Context)
	{
		VectorVM::FUserPtrHandler<FNDIArrayInstanceData_GameThread<TArrayType>> InstanceData(Context);
		FNDIInputParam<FNiagaraBool> InSkipSet(Context);
		FNDIInputParam<int32> IndexParam(Context);
		FNDIInputParam<TVMArrayType> InValue(Context);

		FWriteArrayRef ArrayData(Owner, InstanceData);
		for (int32 i = 0; i < Context.GetNumInstances(); ++i)
		{
			const int32 Index = IndexParam.GetAndAdvance();
			const TArrayType Value = (TArrayType)InValue.GetAndAdvance();
			const bool bSkipSet = InSkipSet.GetAndAdvance();

			if (!bSkipSet && ArrayData.GetArray().IsValidIndex(Index))
			{
				ArrayData.GetArray()[Index] = Value;
			}
		}

		InstanceData->bIsRenderDirty = bShouldSyncToGpu;
	}

	void VMPushValue(FVectorVMExternalFunctionContext& Context)
	{
		VectorVM::FUserPtrHandler<FNDIArrayInstanceData_GameThread<TArrayType>> InstanceData(Context);
		FNDIInputParam<FNiagaraBool> InSkipExecute(Context);
		FNDIInputParam<TVMArrayType> InValue(Context);

		const int32 MaxElements = Owner->MaxElements > 0 ? Owner->MaxElements : kSafeMaxElements;

		FWriteArrayRef ArrayData(Owner, InstanceData);
		for (int32 i = 0; i < Context.GetNumInstances(); ++i)
		{
			const bool bSkipExecute = InSkipExecute.GetAndAdvance();
			const TArrayType Value = (TArrayType)InValue.GetAndAdvance();
			if (!bSkipExecute && (ArrayData.GetArray().Num() < MaxElements))
			{
				ArrayData.GetArray().Emplace(Value);
			}
		}

		InstanceData->bIsRenderDirty = bShouldSyncToGpu;
	}

	void VMPopValue(FVectorVMExternalFunctionContext& Context)
	{
		VectorVM::FUserPtrHandler<FNDIArrayInstanceData_GameThread<TArrayType>> InstanceData(Context);
		FNDIInputParam<FNiagaraBool> InSkipExecute(Context);
		FNDIOutputParam<TVMArrayType> OutValue(Context);
		FNDIOutputParam<FNiagaraBool> OutIsValid(Context);
		const TVMArrayType DefaultValue = FNDIArrayImplHelper<TArrayType>::GetDefaultValue();

		FWriteArrayRef ArrayData(Owner, InstanceData);
		for (int32 i=0; i < Context.GetNumInstances(); ++i)
		{
			const bool bSkipExecute = InSkipExecute.GetAndAdvance();
			if (bSkipExecute || (ArrayData.GetArray().Num() == 0))
			{
				OutValue.SetAndAdvance(DefaultValue);
				OutIsValid.SetAndAdvance(false);
			}
			else
			{
				OutValue.SetAndAdvance(TVMArrayType(ArrayData.GetArray().Pop()));
				OutIsValid.SetAndAdvance(true);
			}
		}

		InstanceData->bIsRenderDirty = bShouldSyncToGpu;
	}

	template<typename T = FNDIArrayImplHelper<TArrayType>>
	typename TEnableIf<!T::bSupportsAtomicOps>::Type VMAtomicAdd(FVectorVMExternalFunctionContext& Context) { check(false); }
	template<typename T = FNDIArrayImplHelper<TArrayType>>
	typename TEnableIf<!T::bSupportsAtomicOps>::Type VMAtomicMin(FVectorVMExternalFunctionContext& Context) { check(false); }
	template<typename T = FNDIArrayImplHelper<TArrayType>>
	typename TEnableIf<!T::bSupportsAtomicOps>::Type VMAtomicMax(FVectorVMExternalFunctionContext& Context) { check(false); }

	template<typename T = FNDIArrayImplHelper<TArrayType>>
	typename TEnableIf<T::bSupportsAtomicOps>::Type VMAtomicAdd(FVectorVMExternalFunctionContext& Context)
	{
		VectorVM::FUserPtrHandler<FNDIArrayInstanceData_GameThread<TArrayType>> InstanceData(Context);
		FNDIInputParam<FNiagaraBool>	InSkipOp(Context);
		FNDIInputParam<int32>			InIndex(Context);
		FNDIInputParam<TVMArrayType>	InValue(Context);
		FNDIOutputParam<TVMArrayType>	OutPrevValue(Context);
		FNDIOutputParam<TVMArrayType>	OutCurrValue(Context);
		
		const TVMArrayType DefaultValue = FNDIArrayImplHelper<TArrayType>::GetDefaultValue();

		//-OPT: Can do a in batches or single atomic op rather than one per instance
		FWriteArrayRef ArrayData(Owner, InstanceData);
		for (int32 i = 0; i < Context.GetNumInstances(); ++i)
		{
			const bool bSkipExecute = InSkipOp.GetAndAdvance();
			const int32 Index = InIndex.GetAndAdvance();
			const TVMArrayType Value = InValue.GetAndAdvance();
			if (!bSkipExecute && ArrayData.GetArray().IsValidIndex(Index))
			{
				TVMArrayType PreviousValue = FNDIArrayImplHelper<TArrayType>::AtomicAdd(&ArrayData.GetArray().GetData()[Index], Value);
				OutPrevValue.SetAndAdvance(PreviousValue);
				OutCurrValue.SetAndAdvance(PreviousValue + Value);
			}
			else
			{
				OutPrevValue.SetAndAdvance(DefaultValue);
				OutCurrValue.SetAndAdvance(DefaultValue);
			}
		}

		InstanceData->bIsRenderDirty = bShouldSyncToGpu;
	}

	template<typename T = FNDIArrayImplHelper<TArrayType>>
	typename TEnableIf<T::bSupportsAtomicOps>::Type VMAtomicMin(FVectorVMExternalFunctionContext& Context)
	{
		VectorVM::FUserPtrHandler<FNDIArrayInstanceData_GameThread<TArrayType>> InstanceData(Context);
		FNDIInputParam<FNiagaraBool>	InSkipOp(Context);
		FNDIInputParam<int32>			InIndex(Context);
		FNDIInputParam<TVMArrayType>	InValue(Context);
		FNDIOutputParam<TVMArrayType>	OutPrevValue(Context);
		FNDIOutputParam<TVMArrayType>	OutCurrValue(Context);

		const TVMArrayType DefaultValue = FNDIArrayImplHelper<TArrayType>::GetDefaultValue();

		//-OPT: Can do a in batches or single atomic op rather than one per instance
		FWriteArrayRef ArrayData(Owner, InstanceData);
		for (int32 i = 0; i < Context.GetNumInstances(); ++i)
		{
			const bool bSkipExecute = InSkipOp.GetAndAdvance();
			const int32 Index = InIndex.GetAndAdvance();
			const TVMArrayType Value = InValue.GetAndAdvance();
			if (!bSkipExecute && ArrayData.GetArray().IsValidIndex(Index))
			{
				TVMArrayType PreviousValue = FNDIArrayImplHelper<TArrayType>::AtomicMin(&ArrayData.GetArray().GetData()[Index], Value);
				OutPrevValue.SetAndAdvance(PreviousValue);
				OutCurrValue.SetAndAdvance(PreviousValue + Value);
			}
			else
			{
				OutPrevValue.SetAndAdvance(DefaultValue);
				OutCurrValue.SetAndAdvance(DefaultValue);
			}
		}

		InstanceData->bIsRenderDirty = bShouldSyncToGpu;
	}

	template<typename T = FNDIArrayImplHelper<TArrayType>>
	typename TEnableIf<T::bSupportsAtomicOps>::Type VMAtomicMax(FVectorVMExternalFunctionContext& Context)
	{
		VectorVM::FUserPtrHandler<FNDIArrayInstanceData_GameThread<TArrayType>> InstanceData(Context);
		FNDIInputParam<FNiagaraBool>	InSkipOp(Context);
		FNDIInputParam<int32>			InIndex(Context);
		FNDIInputParam<TVMArrayType>	InValue(Context);
		FNDIOutputParam<TVMArrayType>	OutPrevValue(Context);
		FNDIOutputParam<TVMArrayType>	OutCurrValue(Context);

		const TVMArrayType DefaultValue = FNDIArrayImplHelper<TArrayType>::GetDefaultValue();

		//-OPT: Can do a in batches or single atomic op rather than one per instance
		FWriteArrayRef ArrayData(Owner, InstanceData);
		for (int32 i = 0; i < Context.GetNumInstances(); ++i)
		{
			const bool bSkipExecute = InSkipOp.GetAndAdvance();
			const int32 Index = InIndex.GetAndAdvance();
			const TVMArrayType Value = InValue.GetAndAdvance();
			if (!bSkipExecute && ArrayData.GetArray().IsValidIndex(Index))
			{
				TVMArrayType PreviousValue = FNDIArrayImplHelper<TArrayType>::AtomicMax(&ArrayData.GetArray().GetData()[Index], Value);
				OutPrevValue.SetAndAdvance(PreviousValue);
				OutCurrValue.SetAndAdvance(PreviousValue + Value);
			}
			else
			{
				OutPrevValue.SetAndAdvance(DefaultValue);
				OutCurrValue.SetAndAdvance(DefaultValue);
			}
		}

		InstanceData->bIsRenderDirty = bShouldSyncToGpu;
	}

private:
	TOwnerType*	Owner = nullptr;
	bool bShouldSyncToGpu = false;
	bool bShouldSyncToCpu = false;

	TMap<FNiagaraSystemInstanceID, FNDIArrayInstanceData_GameThread<TArrayType>*>	PerInstanceData_GameThread;
	TMap<FNiagaraSystemInstanceID, FNDIArrayInstanceData_RenderThread<TArrayType>>	PerInstanceData_RenderThread;
};
