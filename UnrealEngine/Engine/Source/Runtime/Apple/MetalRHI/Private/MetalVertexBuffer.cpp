// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	MetalVertexBuffer.cpp: Metal vertex buffer RHI implementation.
=============================================================================*/

#include "MetalRHIPrivate.h"
#include "MetalProfiler.h"
#include "MetalCommandBuffer.h"
#include "MetalCommandQueue.h"
#include "Containers/ResourceArray.h"
#include "RenderUtils.h"
#include "MetalLLM.h"
#include <objc/runtime.h>

#define METAL_POOL_BUFFER_BACKING 1

#if !METAL_POOL_BUFFER_BACKING
DECLARE_MEMORY_STAT(TEXT("Used Device Buffer Memory"), STAT_MetalDeviceBufferMemory, STATGROUP_MetalRHI);
#endif

#if STATS
#define METAL_INC_DWORD_STAT_BY(Name, Size, Usage) do { if (EnumHasAnyFlags(Usage, BUF_IndexBuffer)) { INC_DWORD_STAT_BY(STAT_MetalIndex##Name, Size); } else { INC_DWORD_STAT_BY(STAT_MetalVertex##Name, Size); } } while (false)
#else
#define METAL_INC_DWORD_STAT_BY(Name, Size, Usage)
#endif

FMetalBufferData::~FMetalBufferData()
{
    if (Data)
    {
        FMemory::Free(Data);
        Data = nullptr;
        Len = 0;
    }
}

void FMetalBufferData::InitWithSize(uint32 Size)
{
    Data = (uint8*)FMemory::Malloc(Size);
    Len = Size;
    check(Data);
}

enum class EMetalBufferUsage
{
	None = 0,
	GPUOnly = 1 << 0,
	LinearTex = 1 << 1,
};
ENUM_CLASS_FLAGS(EMetalBufferUsage);

static EMetalBufferUsage GetMetalBufferUsage(EBufferUsageFlags InUsage)
{
	EMetalBufferUsage Usage = EMetalBufferUsage::None;

	if (EnumHasAnyFlags(InUsage, BUF_VertexBuffer))
	{
		Usage |= EMetalBufferUsage::LinearTex;
	}

	if (EnumHasAnyFlags(InUsage, BUF_IndexBuffer))
	{
		Usage |= (EMetalBufferUsage::GPUOnly | EMetalBufferUsage::LinearTex);
	}

	if (EnumHasAnyFlags(InUsage, BUF_StructuredBuffer))
	{
		Usage |= EMetalBufferUsage::GPUOnly;
	}

	return Usage;
}

static bool CanUsePrivateMemory()
{
	return (FMetalCommandQueue::SupportsFeature(EMetalFeaturesEfficientBufferBlits) || FMetalCommandQueue::SupportsFeature(EMetalFeaturesIABs)) && !FMetalCommandQueue::IsUMASystem();
}

bool FMetalRHIBuffer::UsePrivateMemory() const
{
	return (FMetalCommandQueue::SupportsFeature(EMetalFeaturesEfficientBufferBlits) && EnumHasAnyFlags(GetUsage(), BUF_Dynamic | BUF_Static))
	|| (FMetalCommandQueue::SupportsFeature(EMetalFeaturesIABs) && EnumHasAnyFlags(GetUsage(), BUF_ShaderResource|BUF_UnorderedAccess)) 
	&& !FMetalCommandQueue::IsUMASystem();
}

FMetalRHIBuffer::FMetalRHIBuffer(FRHICommandListBase& RHICmdList, FRHIBufferDesc const& InBufferDesc, FRHIResourceCreateInfo& CreateInfo)
	: FRHIBuffer(InBufferDesc)
	, Size(InBufferDesc.Size)
	, Mode(BUFFER_STORAGE_MODE)
{
#if METAL_RHI_RAYTRACING
	if (EnumHasAnyFlags(InBufferDesc.Usage, BUF_AccelerationStructure))
	{
		AccelerationStructureHandle = GetMetalDeviceContext().GetDevice().NewAccelerationStructureWithSize(Size);
		return;
	}
#endif

	const EMetalBufferUsage MetalUsage = GetMetalBufferUsage(InBufferDesc.Usage);
	
	const bool bIsStatic  	= EnumHasAnyFlags(InBufferDesc.Usage, BUF_Static);
	const bool bIsDynamic 	= EnumHasAnyFlags(InBufferDesc.Usage, BUF_Dynamic);
	const bool bIsVolatile	= EnumHasAnyFlags(InBufferDesc.Usage, BUF_Volatile);
	const bool bIsNull		= EnumHasAnyFlags(InBufferDesc.Usage, BUF_NullResource);
	const bool bWantsView 	= EnumHasAnyFlags(InBufferDesc.Usage, BUF_ShaderResource | BUF_UnorderedAccess);
	
	uint32_t ValidateTypeCount = (uint32_t)bIsStatic + (uint32_t)bIsDynamic +
								(uint32_t)bIsVolatile + (uint32_t)bIsNull;
	
	check(ValidateTypeCount == 1);

	Mode = UsePrivateMemory() ? MTL::StorageModePrivate : BUFFER_STORAGE_MODE;
	Mode = CanUsePrivateMemory() ? MTL::StorageModePrivate : Mode;
	
	if (InBufferDesc.Size)
	{
		checkf(InBufferDesc.Size <= GetMetalDeviceContext().GetDevice()->maxBufferLength(), TEXT("Requested buffer size larger than supported by device."));
		
		// Temporary buffers less than the buffer page size - currently 4Kb - is better off going through the set*Bytes API if available.
		// These can't be used for shader resources or UAVs if we want to use the 'Linear Texture' code path
		if (!bWantsView
			&& bIsVolatile
			&& !EnumHasAnyFlags(MetalUsage, EMetalBufferUsage::GPUOnly)
			&& InBufferDesc.Size < MetalBufferPageSize
			&& InBufferDesc.Size < MetalBufferBytesSize)
		{
            Data = new FMetalBufferData();
            Data->InitWithSize(InBufferDesc.Size);
			METAL_INC_DWORD_STAT_BY(MemAlloc, InBufferDesc.Size, InBufferDesc.Usage);
		}
		else
		{
#if PLATFORM_MAC
			// Buffer can be blit encoder copied on lock/unlock, we need to know that the buffer size is large enough for copy operations that are in multiples of
			// 4 bytes on macOS, iOS can be 1 byte.  Update size to know we have at least this much buffer memory, it will be larger in the end.
			Size = Align(InBufferDesc.Size, 4);
#endif
			uint32 AllocSize = Size;
			
			if (EnumHasAnyFlags(MetalUsage, EMetalBufferUsage::LinearTex) && !FMetalCommandQueue::SupportsFeature(EMetalFeaturesTextureBuffers))
			{
				if (EnumHasAnyFlags(InBufferDesc.Usage, BUF_UnorderedAccess))
				{
					// Padding for write flushing when not using linear texture bindings for buffers
					AllocSize = Align(AllocSize + 512, 1024);
				}
				
				if (bWantsView)
				{
					uint32 NumElements = AllocSize;
					uint32 SizeX = NumElements;
					uint32 SizeY = 1;
					uint32 Dimension = GMaxTextureDimensions;
					while (SizeX > GMaxTextureDimensions)
					{
						while((NumElements % Dimension) != 0)
						{
							check(Dimension >= 1);
							Dimension = (Dimension >> 1);
						}
						SizeX = Dimension;
						SizeY = NumElements / Dimension;
						if(SizeY > GMaxTextureDimensions)
						{
							Dimension <<= 1;
							checkf(SizeX <= GMaxTextureDimensions, TEXT("Calculated width %u is greater than maximum permitted %d when converting buffer of size %u to a 2D texture."), Dimension, (int32)GMaxTextureDimensions, AllocSize);
							if(Dimension <= GMaxTextureDimensions)
							{
								AllocSize = Align(Size, Dimension);
								NumElements = AllocSize;
								SizeX = NumElements;
							}
							else
							{
								// We don't know the Pixel Format and so the bytes per element for the potential linear texture
								// Use max texture dimension as the align to be a worst case rather than crashing
								AllocSize = Align(Size, GMaxTextureDimensions);
								break;
							}
						}
					}
					
					AllocSize = Align(AllocSize, 1024);
				}
			}
			
			// Static buffers will never be discarded. You can update them directly.
			if(bIsStatic)
			{
				NumberOfBuffers = 1;
			}
			else
			{
				check(bIsDynamic || bIsVolatile);
				NumberOfBuffers = 3;
			}
			
			check(NumberOfBuffers > 0);
			
			BufferPool.SetNum(NumberOfBuffers);
			
			// These allocations will not go into the pool.
			uint32 RequestedBufferOffsetAlignment = BufferOffsetAlignment;
			if(bWantsView)
			{
				// Buffer backed linear textures have specific align requirements
				// We don't know upfront the pixel format that may be requested for an SRV so we can't use minimumLinearTextureAlignmentForPixelFormat:
				RequestedBufferOffsetAlignment = BufferBackedLinearTextureOffsetAlignment;
			}
			
			AllocSize = Align(AllocSize, RequestedBufferOffsetAlignment);
			for(uint32 i = 0; i < NumberOfBuffers; i++)
			{
				FMetalBufferPtr Buffer = nullptr;

#if METAL_POOL_BUFFER_BACKING
				FMetalPooledBufferArgs ArgsCPU(GetMetalDeviceContext().GetDevice(), AllocSize, InBufferDesc.Usage, Mode);
				Buffer = GetMetalDeviceContext().CreatePooledBuffer(ArgsCPU);
				Buffer->SetOwner(nullptr, false);
#else
				NS::UInteger Options = (((NS::UInteger) Mode) << MTL::ResourceStorageModeShift);
				
				METAL_GPUPROFILE(FScopedMetalCPUStats CPUStat(FString::Printf(TEXT("AllocBuffer: %llu, %llu"), AllocSize, Options)));
				// Allocate one.
				//Buffer = FMetalBuffer(MTLPP_VALIDATE(MTL::Device, GetMetalDeviceContext().GetDevice(), SafeGetRuntimeDebuggingLevel() >= EMetalDebugLevelValidation, NewBuffer(AllocSize, (mtlpp::ResourceOptions) Options)), false);
                MTLBufferPtr BufferPtr = NS::TransferPtr(GetMetalDeviceContext().GetDevice()->newBuffer(AllocSize, (MTL::ResourceOptions) Options));
                Buffer = FMetalBufferPtr(new FMetalBuffer(BufferPtr));
                
				#if STATS || ENABLE_LOW_LEVEL_MEM_TRACKER
					MetalLLM::LogAllocBuffer(GetMetalDeviceContext().GetDevice(), Buffer);
				#endif
				INC_MEMORY_STAT_BY(STAT_MetalDeviceBufferMemory, Buffer->GetLength());
				
				if (GMetalBufferZeroFill && Mode != MTL::StorageModePrivate)
				{
					FMemory::Memset(((uint8*)Buffer->Contents()), 0, Buffer->GetLength());
				}
				
				METAL_DEBUG_OPTION(GetMetalDeviceContext().ValidateIsInactiveBuffer(Buffer->GetMTLBuffer().get(), Buffer->GetRange()));
				METAL_FATAL_ASSERT(Buffer, TEXT("Failed to create buffer of size %u and resource options %u"), Size, (uint32)Options);
				
				if(bIsStatic)
				{
                    FString Label = FString::Printf(TEXT("Static on frame %u"), GetMetalDeviceContext().GetFrameNumberRHIThread());
                    BufferPtr->setLabel(FStringToNSString(Label));
				}
				else
				{
                    FString Label = FString::Printf(TEXT("Buffer on frame %u"), GetMetalDeviceContext().GetFrameNumberRHIThread());
                    BufferPtr->setLabel(FStringToNSString(Label));
				}
#endif
                BufferPool[i] = Buffer;
                
                check(Buffer);
                check(AllocSize <= Buffer->GetLength());
                check(Buffer->GetMTLBuffer()->storageMode() == Mode);
			}
		}
	}

	if (CreateInfo.ResourceArray && InBufferDesc.Size > 0)
	{
		check(InBufferDesc.Size == CreateInfo.ResourceArray->GetResourceDataSize());

		if (Data)
		{
			FMemory::Memcpy(Data->Data, CreateInfo.ResourceArray->GetResourceData(), InBufferDesc.Size);
		}
		else
		{
			if (Mode == MTL::StorageModePrivate)
			{
				if (RHICmdList.IsBottomOfPipe())
				{
					void* Backing = this->Lock(true, RLM_WriteOnly, 0, InBufferDesc.Size);
					FMemory::Memcpy(Backing, CreateInfo.ResourceArray->GetResourceData(), InBufferDesc.Size);
					this->Unlock();
				}
				else
				{
					void* Result = FMemory::Malloc(InBufferDesc.Size, 16);
					FMemory::Memcpy(Result, CreateInfo.ResourceArray->GetResourceData(), InBufferDesc.Size);

					RHICmdList.EnqueueLambda(
						[this, Result, InSize = InBufferDesc.Size](FRHICommandListBase& RHICmdList)
						{
							void* Backing = this->Lock(true, RLM_WriteOnly, 0, InSize);
							FMemory::Memcpy(Backing, Result, InSize);
							this->Unlock();
							FMemory::Free(Result);
						});
				}
			}
			else
			{
				FMetalBufferPtr TheBuffer = GetCurrentBuffer();
                MTLBufferPtr MTLBuffer = TheBuffer->GetMTLBuffer();
				FMemory::Memcpy(TheBuffer->Contents(), CreateInfo.ResourceArray->GetResourceData(), InBufferDesc.Size);
#if PLATFORM_MAC
				if (Mode == MTL::StorageModeManaged)
				{
					//MTLPP_VALIDATE(mtlpp::Buffer, TheBuffer, SafeGetRuntimeDebuggingLevel() >= EMetalDebugLevelValidation, DidModify(ns::Range(0, GMetalBufferZeroFill ? TheBuffer.GetLength() : InBufferDesc.Size)));
                    
                    NS::Range ModifyRange = NS::Range(TheBuffer->GetOffset(), TheBuffer->GetLength());
                    MTLBuffer->didModifyRange(ModifyRange);
				}
#endif
			}
		}

		// Discard the resource array's contents.
		CreateInfo.ResourceArray->Discard();
	}
}

FMetalRHIBuffer::~FMetalRHIBuffer()
{
    ReleaseOwnership();
}

void FMetalRHIBuffer::AllocTransferBuffer(bool bOnRHIThread, uint32 InSize, EResourceLockMode LockMode)
{
	check(!TransferBuffer);
	FMetalPooledBufferArgs ArgsCPU(GetMetalDeviceContext().GetDevice(), InSize, BUF_Dynamic, MTL::StorageModeShared);
	TransferBuffer = GetMetalDeviceContext().CreatePooledBuffer(ArgsCPU);
	TransferBuffer->SetOwner(nullptr, false);
	check(TransferBuffer);
	METAL_INC_DWORD_STAT_BY(MemAlloc, InSize, GetUsage());
	METAL_FATAL_ASSERT(TransferBuffer, TEXT("Failed to create buffer of size %u and storage mode %u"), InSize, (uint32)MTL::StorageModeShared);
}

void* FMetalRHIBuffer::Lock(bool bIsOnRHIThread, EResourceLockMode InLockMode, uint32 Offset, uint32 InSize)
{
	check(CurrentLockMode == RLM_Num);
	check(LockSize == 0 && LockOffset == 0);
	check(MetalIsSafeToUseRHIThreadResources());
	check(!TransferBuffer);
	
	if (Data)
	{
		check(Data->Data);
		return ((uint8*)Data->Data) + Offset;
	}
	
#if PLATFORM_MAC
	// Blit encoder validation error, lock size and subsequent blit copy unlock operations need to be in 4 byte multiples on macOS
	InSize = FMath::Min(Align(InSize, 4), Size - Offset);
#endif
	
	const bool bWriteLock = InLockMode == RLM_WriteOnly;
	const bool bIsStatic = EnumHasAnyFlags(GetUsage(), BUF_Static);
	const bool bIsDynamic = EnumHasAnyFlags(GetUsage(), BUF_Dynamic);
	const bool bIsVolatile = EnumHasAnyFlags(GetUsage(), BUF_Volatile);
	
	void* ReturnPointer = nullptr;
	
	uint32 Len = GetCurrentBuffer()->GetLength(); // all buffers should have the same length or we are in trouble.
	check(Len >= InSize);
	
	if(bWriteLock)
	{
		if(bIsStatic)
		{
			// Static buffers do not discard. They just return the buffer or a transfer buffer.
			// You are not supposed to lock more than once a frame.
		}
		else
		{
			check(bIsDynamic || bIsVolatile);
			// cycle to next allocation
			AdvanceBackingIndex();
		}

		// Use transfer buffer for writing into 'Static' buffers as they could be in use by GPU atm
		// Initialization of 'Static' buffers still uses direct copy when possible
		const bool bUseTransferBuffer = (Mode == MTL::StorageModePrivate || (Mode == MTL::StorageModeShared && bIsStatic));
		if(bUseTransferBuffer)
		{
			FMetalFrameAllocator::AllocationEntry TempBacking = GetMetalDeviceContext().GetTransferAllocator()->AcquireSpace(Len);
			GetMetalDeviceContext().NewLock(this, TempBacking);
			check(TempBacking.Backing);
			ReturnPointer = (uint8*) TempBacking.Backing->contents() + TempBacking.Offset;
		}
		else
		{
			check(GetCurrentBuffer());
			ReturnPointer = GetCurrentBuffer()->Contents();
		}
		check(ReturnPointer != nullptr);
	}
	else
	{
		check(InLockMode == EResourceLockMode::RLM_ReadOnly);
		// assumes offset is 0 for reads.
		check(Offset == 0);
		
		if(Mode == MTL::StorageModePrivate)
		{
			SCOPE_CYCLE_COUNTER(STAT_MetalBufferPageOffTime);
			AllocTransferBuffer(true, Len, RLM_WriteOnly);
			check(TransferBuffer->GetLength() >= InSize);
			
			// Synchronise the buffer with the CPU
			GetMetalDeviceContext().CopyFromBufferToBuffer(GetCurrentBuffer(), 0, TransferBuffer, 0, GetCurrentBuffer()->GetLength());
			
			//kick the current command buffer.
			GetMetalDeviceContext().SubmitCommandBufferAndWait();
			
			ReturnPointer = TransferBuffer->Contents();
		}
		#if PLATFORM_MAC
		else if(Mode == MTL::StorageModeManaged)
		{
			SCOPE_CYCLE_COUNTER(STAT_MetalBufferPageOffTime);
			
			// Synchronise the buffer with the CPU
			GetMetalDeviceContext().SynchroniseResource(GetCurrentBuffer()->GetMTLBuffer().get());
			
			//kick the current command buffer.
			GetMetalDeviceContext().SubmitCommandBufferAndWait();
			
			ReturnPointer = GetCurrentBuffer()->Contents();
		}
		#endif
		else
		{
			// Shared
            ReturnPointer = GetCurrentBuffer()->Contents();
		}
	} // Read Path
	
	
	
	check(GetCurrentBuffer());
	check((!GetCurrentBuffer()->GetMTLBuffer()->heap() && !GetCurrentBuffer()->GetMTLBuffer()->isAliasable()) || GetCurrentBuffer()->GetMTLBuffer()->heap() != nullptr);
	
	check(ReturnPointer);
	LockOffset = Offset;
	LockSize = InSize;
	CurrentLockMode = InLockMode;
	
	if(InSize == 0)
	{
		LockSize = Len;
	}
	
	ReturnPointer = ((uint8*) (ReturnPointer)) + Offset;
	return ReturnPointer;
}

void FMetalRHIBuffer::Unlock()
{
	check(MetalIsSafeToUseRHIThreadResources());

	if (!Data)
	{
		FMetalBufferPtr CurrentBuffer = GetCurrentBuffer();
		
		check(CurrentBuffer);
		check(LockSize > 0);
		const bool bWriteLock = CurrentLockMode == RLM_WriteOnly;
		const bool bIsStatic = EnumHasAnyFlags(GetUsage(), BUF_Static);
		
		if (bWriteLock)
		{
			check(!TransferBuffer);
			check(LockOffset == 0);
            check(LockSize <= CurrentBuffer->GetLength());

			// Use transfer buffer for writing into 'Static' buffers as they could be in use by GPU atm
			// Initialization of 'Static' buffers still uses direct copy when possible
			const bool bUseTransferBuffer = (Mode == MTL::StorageModePrivate || (Mode == MTL::StorageModeShared && bIsStatic));
			if (bUseTransferBuffer)
			{
				FMetalFrameAllocator::AllocationEntry Entry = GetMetalDeviceContext().FetchAndRemoveLock(this);
				
                MTLBufferPtr Transfer = Entry.Backing;
                FMetalBufferPtr TransferProxy = FMetalBufferPtr(new FMetalBuffer(Transfer, NS::Range(0, Transfer->length()), false));
                                            
				GetMetalDeviceContext().AsyncCopyFromBufferToBuffer(TransferProxy, Entry.Offset, CurrentBuffer, 0, LockSize);
			}
#if PLATFORM_MAC
			else if (Mode == MTL::StorageModeManaged)
			{
				if (GMetalBufferZeroFill)
                {
                    //MTLPP_VALIDATE(MTL::Buffer, CurrentBuffer, SafeGetRuntimeDebuggingLevel() >= EMetalDebugLevelValidation, DidModify(ns::Range(0, CurrentBuffer.GetLength())));
                    CurrentBuffer->GetMTLBuffer()->didModifyRange(NS::Range(CurrentBuffer->GetOffset(), CurrentBuffer->GetLength()));
                }
				else
                {
                    //MTLPP_VALIDATE(MTL::Buffer, CurrentBuffer, SafeGetRuntimeDebuggingLevel() >= EMetalDebugLevelValidation, DidModify(ns::Range(LockOffset, LockSize)));
                    CurrentBuffer->GetMTLBuffer()->didModifyRange(NS::Range(LockOffset + CurrentBuffer->GetOffset(), LockSize));
                }
			}
#endif //PLATFORM_MAC
			else
			{
				// shared buffers are always mapped so nothing happens
				check(Mode == MTL::StorageModeShared);
			}

			UpdateLinkedViews();
		}
		else
		{
			check(CurrentLockMode == RLM_ReadOnly);
			if(TransferBuffer)
			{
				check(Mode == MTL::StorageModePrivate);
				SafeReleaseMetalBuffer(TransferBuffer);
				TransferBuffer = nullptr;
			}
		}
	}
	
	check(!TransferBuffer);
	CurrentLockMode = RLM_Num;
	LockSize = 0;
	LockOffset = 0;
}

void FMetalRHIBuffer::TakeOwnership(FMetalRHIBuffer& Other)
{
    check(Other.CurrentLockMode == RLM_Num);

    // Clean up any resource this buffer already owns
    ReleaseOwnership();

    // Transfer ownership of Other's resources to this instance
    FRHIBuffer::TakeOwnership(Other);

    TransferBuffer = Other.TransferBuffer;
    BufferPool = Other.BufferPool;
    Data = Other.Data;
    CurrentIndex = Other.CurrentIndex;
    NumberOfBuffers = Other.NumberOfBuffers;
    CurrentLockMode = Other.CurrentLockMode;
    LockOffset = Other.LockOffset;
    LockSize = Other.LockSize;
    Size = Other.Size;
    Mode = Other.Mode;
    
    Other.TransferBuffer = nullptr;
    Other.BufferPool.SetNum(0);
    Other.Data = nullptr;
    Other.CurrentIndex = 0;
    Other.NumberOfBuffers = 0;
    Other.CurrentLockMode = RLM_Num;
    Other.LockOffset = 0;
    Other.LockSize = 0;
    Other.Size = 0;
}

void FMetalRHIBuffer::ReleaseOwnership()
{
    FRHIBuffer::ReleaseOwnership();
    
    if(TransferBuffer)
    {
        METAL_INC_DWORD_STAT_BY(MemFreed, TransferBuffer->GetLength(), GetUsage());
        SafeReleaseMetalBuffer(TransferBuffer);
    }
    
    for (FMetalBufferPtr Buffer : BufferPool)
    {
        check(Buffer);
        
        METAL_INC_DWORD_STAT_BY(MemFreed, Buffer->GetLength(), GetUsage());
        SafeReleaseMetalBuffer(Buffer);
    }

    if (Data)
    {
        METAL_INC_DWORD_STAT_BY(MemFreed, Size, GetUsage());
        auto ReleaseFunction = [ReleaseData=Data](){
            delete ReleaseData;
        };
        SafeReleaseFunction(ReleaseFunction);
    }

#if METAL_RHI_RAYTRACING
    if (EnumHasAnyFlags(GetUsage(), BUF_AccelerationStructure))
    {
        SafeReleaseMetalObject(AccelerationStructureHandle);
        AccelerationStructureHandle = nullptr;
    }
#endif // METAL_RHI_RAYTRACING
}

FBufferRHIRef FMetalDynamicRHI::RHICreateBuffer(FRHICommandListBase& RHICmdList, FRHIBufferDesc const& Desc, ERHIAccess /*ResourceState*/, FRHIResourceCreateInfo& CreateInfo)
{
    MTL_SCOPED_AUTORELEASE_POOL;

    // No life-time usage information? Enforce Dynamic.
    if (!EnumHasAnyFlags(Desc.Usage, BUF_Static | BUF_Dynamic | BUF_Volatile | BUF_NullResource))
    {
        FRHIBufferDesc Copy = Desc;
        Copy.Usage |= BUF_Dynamic;

        return new FMetalRHIBuffer(RHICmdList, Copy, CreateInfo);
    }
    else
    {
        return new FMetalRHIBuffer(RHICmdList, Desc, CreateInfo);
    }
}

void* FMetalDynamicRHI::LockBuffer_BottomOfPipe(FRHICommandListBase& RHICmdList, FRHIBuffer* BufferRHI, uint32 Offset, uint32 Size, EResourceLockMode LockMode)
{
    MTL_SCOPED_AUTORELEASE_POOL;
    
	FMetalRHIBuffer* Buffer = ResourceCast(BufferRHI);

	// default to buffer memory
	return (uint8*)Buffer->Lock(true, LockMode, Offset, Size);
}

void FMetalDynamicRHI::UnlockBuffer_BottomOfPipe(FRHICommandListBase& RHICmdList, FRHIBuffer* BufferRHI)
{
    MTL_SCOPED_AUTORELEASE_POOL;
    
    FMetalRHIBuffer* Buffer = ResourceCast(BufferRHI);
	Buffer->Unlock();
}

void FMetalDynamicRHI::RHICopyBuffer(FRHIBuffer* SourceBufferRHI, FRHIBuffer* DestBufferRHI)
{
    MTL_SCOPED_AUTORELEASE_POOL;
    
    FMetalRHIBuffer* SrcBuffer = ResourceCast(SourceBufferRHI);
    FMetalRHIBuffer* DstBuffer = ResourceCast(DestBufferRHI);
    
    FMetalBufferPtr TheSrcBuffer = SrcBuffer->GetCurrentBuffer();
    FMetalBufferPtr TheDstBuffer = DstBuffer->GetCurrentBuffer();

    if (TheSrcBuffer && TheDstBuffer)
    {
        GetMetalDeviceContext().CopyFromBufferToBuffer(TheSrcBuffer, 0, TheDstBuffer, 0, FMath::Min(SrcBuffer->GetSize(), DstBuffer->GetSize()));
    }
    else if (TheDstBuffer)
    {
        FMetalPooledBufferArgs ArgsCPU(GetMetalDeviceContext().GetDevice(), SrcBuffer->GetSize(), BUF_Dynamic, MTL::StorageModeShared);
        FMetalBufferPtr TempBuffer = GetMetalDeviceContext().CreatePooledBuffer(ArgsCPU);
        FMemory::Memcpy(TempBuffer->Contents(), SrcBuffer->Data->Data, SrcBuffer->GetSize());
        GetMetalDeviceContext().CopyFromBufferToBuffer(TempBuffer, 0, TheDstBuffer, 0, FMath::Min(SrcBuffer->GetSize(), DstBuffer->GetSize()));
        SafeReleaseMetalBuffer(TempBuffer);
    }
    else
    {
        void const* SrcData = SrcBuffer->Lock(true, RLM_ReadOnly, 0);
        void* DstData = DstBuffer->Lock(true, RLM_WriteOnly, 0);
        FMemory::Memcpy(DstData, SrcData, FMath::Min(SrcBuffer->GetSize(), DstBuffer->GetSize()));
        SrcBuffer->Unlock();
        DstBuffer->Unlock();
    }
}

void FMetalDynamicRHI::RHITransferBufferUnderlyingResource(FRHICommandListBase& RHICmdList, FRHIBuffer* DestBuffer, FRHIBuffer* SrcBuffer)
{
    MTL_SCOPED_AUTORELEASE_POOL;
    FMetalRHIBuffer* Dst = ResourceCast(DestBuffer);
    FMetalRHIBuffer* Src = ResourceCast(SrcBuffer);

    if (Src)
    {
        // The source buffer should not have any associated views.
        check(!Src->HasLinkedViews());

        Dst->TakeOwnership(*Src);
    }
    else
    {
        Dst->ReleaseOwnership();
    }

    Dst->UpdateLinkedViews();
}
