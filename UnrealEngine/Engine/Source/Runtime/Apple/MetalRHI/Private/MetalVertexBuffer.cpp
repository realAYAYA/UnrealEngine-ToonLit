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

@implementation FMetalBufferData

APPLE_PLATFORM_OBJECT_ALLOC_OVERRIDES(FMetalBufferData)

-(instancetype)init
{
	id Self = [super init];
	if (Self)
	{
		self->Data = nullptr;
		self->Len = 0;
	}
	return Self;
}
-(instancetype)initWithSize:(uint32)InSize
{
	id Self = [super init];
	if (Self)
	{
		self->Data = (uint8*)FMemory::Malloc(InSize);
		self->Len = InSize;
		check(self->Data);
	}
	return Self;
}
-(instancetype)initWithBytes:(void const*)InData length:(uint32)InSize
{
	id Self = [super init];
	if (Self)
	{
		self->Data = (uint8*)FMemory::Malloc(InSize);
		self->Len = InSize;
		check(self->Data);
		FMemory::Memcpy(self->Data, InData, InSize);
	}
	return Self;
}
-(void)dealloc
{
	if (self->Data)
	{
		FMemory::Free(self->Data);
		self->Data = nullptr;
		self->Len = 0;
	}
	[super dealloc];
}
@end

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
	
	const bool bIsStatic   = EnumHasAnyFlags(InBufferDesc.Usage, BUF_Static);
	const bool bIsDynamic  = EnumHasAnyFlags(InBufferDesc.Usage, BUF_Dynamic);
	const bool bIsVolatile = EnumHasAnyFlags(InBufferDesc.Usage, BUF_Volatile);
	const bool bWantsView  = EnumHasAnyFlags(InBufferDesc.Usage, BUF_ShaderResource | BUF_UnorderedAccess);
	
	check(bIsStatic ^ bIsDynamic ^ bIsVolatile);

	Mode = UsePrivateMemory() ? mtlpp::StorageMode::Private : BUFFER_STORAGE_MODE;
	Mode = CanUsePrivateMemory() ? mtlpp::StorageMode::Private : Mode;
	
	if (InBufferDesc.Size)
	{
		checkf(InBufferDesc.Size <= [GetMetalDeviceContext().GetDevice().GetPtr() maxBufferLength], TEXT("Requested buffer size larger than supported by device."));
		
		// Temporary buffers less than the buffer page size - currently 4Kb - is better off going through the set*Bytes API if available.
		// These can't be used for shader resources or UAVs if we want to use the 'Linear Texture' code path
		if (!bWantsView
			&& bIsVolatile
			&& !EnumHasAnyFlags(MetalUsage, EMetalBufferUsage::GPUOnly)
			&& InBufferDesc.Size < MetalBufferPageSize
			&& InBufferDesc.Size < MetalBufferBytesSize)
		{
			Data = [[FMetalBufferData alloc] initWithSize:InBufferDesc.Size];
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
				FMetalBuffer& Buffer = BufferPool[i];

#if METAL_POOL_BUFFER_BACKING
				FMetalPooledBufferArgs ArgsCPU(GetMetalDeviceContext().GetDevice(), AllocSize, InBufferDesc.Usage, Mode);
				Buffer = GetMetalDeviceContext().CreatePooledBuffer(ArgsCPU);
				Buffer.SetOwner(nullptr, false);
#else
				
				NSUInteger Options = (((NSUInteger) Mode) << mtlpp::ResourceStorageModeShift);
				
				METAL_GPUPROFILE(FScopedMetalCPUStats CPUStat(FString::Printf(TEXT("AllocBuffer: %llu, %llu"), AllocSize, Options)));
				// Allocate one.
				Buffer = FMetalBuffer(MTLPP_VALIDATE(mtlpp::Device, GetMetalDeviceContext().GetDevice(), SafeGetRuntimeDebuggingLevel() >= EMetalDebugLevelValidation, NewBuffer(AllocSize, (mtlpp::ResourceOptions) Options)), false);
				
				#if STATS || ENABLE_LOW_LEVEL_MEM_TRACKER
					MetalLLM::LogAllocBuffer(GetMetalDeviceContext().GetDevice(), Buffer);
				#endif
				INC_MEMORY_STAT_BY(STAT_MetalDeviceBufferMemory, Buffer.GetLength());
				
				if (GMetalBufferZeroFill && Mode != mtlpp::StorageMode::Private)
				{
					FMemory::Memset(((uint8*)Buffer.GetContents()), 0, Buffer.GetLength());
				}
				
				METAL_DEBUG_OPTION(GetMetalDeviceContext().ValidateIsInactiveBuffer(Buffer));
				METAL_FATAL_ASSERT(Buffer, TEXT("Failed to create buffer of size %u and resource options %u"), Size, (uint32)Options);
				
				if(bIsStatic)
				{
					[Buffer.GetPtr() setLabel:[NSString stringWithFormat:@"Static on frame %u", GetMetalDeviceContext().GetFrameNumberRHIThread()]];
				}
				else
				{
					[Buffer.GetPtr() setLabel:[NSString stringWithFormat:@"buffer on frame %u", GetMetalDeviceContext().GetFrameNumberRHIThread()]];
				}
				
#endif
			}
			
			for (FMetalBuffer& Buffer : BufferPool)
			{
				check(Buffer);
				check(AllocSize <= Buffer.GetLength());
				check(Buffer.GetStorageMode() == Mode);
			}
		}
	}

	if (CreateInfo.ResourceArray)
	{
		check(InBufferDesc.Size == CreateInfo.ResourceArray->GetResourceDataSize());

		if (Data)
		{
			FMemory::Memcpy(Data->Data, CreateInfo.ResourceArray->GetResourceData(), InBufferDesc.Size);
		}
		else
		{
			if (Mode == mtlpp::StorageMode::Private)
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
				FMetalBuffer& TheBuffer = GetCurrentBuffer();
				FMemory::Memcpy(TheBuffer.GetContents(), CreateInfo.ResourceArray->GetResourceData(), InBufferDesc.Size);
#if PLATFORM_MAC
				if (Mode == mtlpp::StorageMode::Managed)
				{
					MTLPP_VALIDATE(mtlpp::Buffer, TheBuffer, SafeGetRuntimeDebuggingLevel() >= EMetalDebugLevelValidation, DidModify(ns::Range(0, GMetalBufferZeroFill ? TheBuffer.GetLength() : InBufferDesc.Size)));
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
	FMetalPooledBufferArgs ArgsCPU(GetMetalDeviceContext().GetDevice(), InSize, BUF_Dynamic, mtlpp::StorageMode::Shared);
	TransferBuffer = GetMetalDeviceContext().CreatePooledBuffer(ArgsCPU);
	TransferBuffer.SetOwner(nullptr, false);
	check(TransferBuffer && TransferBuffer.GetPtr());
	METAL_INC_DWORD_STAT_BY(MemAlloc, InSize, GetUsage());
	METAL_FATAL_ASSERT(TransferBuffer, TEXT("Failed to create buffer of size %u and storage mode %u"), InSize, (uint32)mtlpp::StorageMode::Shared);
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
	
	uint32 Len = GetCurrentBuffer().GetLength(); // all buffers should have the same length or we are in trouble.
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
		const bool bUseTransferBuffer = (Mode == mtlpp::StorageMode::Private || (Mode == mtlpp::StorageMode::Shared && bIsStatic));
		if(bUseTransferBuffer)
		{
			FMetalFrameAllocator::AllocationEntry TempBacking = GetMetalDeviceContext().GetTransferAllocator()->AcquireSpace(Len);
			GetMetalDeviceContext().NewLock(this, TempBacking);
			check(TempBacking.Backing);
			ReturnPointer = (uint8*) [TempBacking.Backing contents] + TempBacking.Offset;
		}
		else
		{
			check(GetCurrentBuffer());
			ReturnPointer = GetCurrentBuffer().GetContents();
		}
		check(ReturnPointer != nullptr);
	}
	else
	{
		check(InLockMode == EResourceLockMode::RLM_ReadOnly);
		// assumes offset is 0 for reads.
		check(Offset == 0);
		
		if(Mode == mtlpp::StorageMode::Private)
		{
			SCOPE_CYCLE_COUNTER(STAT_MetalBufferPageOffTime);
			AllocTransferBuffer(true, Len, RLM_WriteOnly);
			check(TransferBuffer.GetLength() >= InSize);
			
			// Synchronise the buffer with the CPU
			GetMetalDeviceContext().CopyFromBufferToBuffer(GetCurrentBuffer(), 0, TransferBuffer, 0, GetCurrentBuffer().GetLength());
			
			//kick the current command buffer.
			GetMetalDeviceContext().SubmitCommandBufferAndWait();
			
			ReturnPointer = TransferBuffer.GetContents();
		}
		#if PLATFORM_MAC
		else if(Mode == mtlpp::StorageMode::Managed)
		{
			SCOPE_CYCLE_COUNTER(STAT_MetalBufferPageOffTime);
			
			// Synchronise the buffer with the CPU
			GetMetalDeviceContext().SynchroniseResource(GetCurrentBuffer());
			
			//kick the current command buffer.
			GetMetalDeviceContext().SubmitCommandBufferAndWait();
			
			ReturnPointer = GetCurrentBuffer().GetContents();
		}
		#endif
		else
		{
			// Shared
			ReturnPointer = GetCurrentBuffer().GetContents();
		}
	} // Read Path
	
	
	
	check(GetCurrentBuffer());
	check(!GetCurrentBuffer().IsAliasable());
	
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
		FMetalBuffer& CurrentBuffer = GetCurrentBuffer();
		
		check(CurrentBuffer);
		check(LockSize > 0);
		const bool bWriteLock = CurrentLockMode == RLM_WriteOnly;
		const bool bIsStatic = EnumHasAnyFlags(GetUsage(), BUF_Static);
		
		if (bWriteLock)
		{
			check(!TransferBuffer);
			check(LockOffset == 0);
			check(LockSize <= CurrentBuffer.GetLength());

			// Use transfer buffer for writing into 'Static' buffers as they could be in use by GPU atm
			// Initialization of 'Static' buffers still uses direct copy when possible
			const bool bUseTransferBuffer = (Mode == mtlpp::StorageMode::Private || (Mode == mtlpp::StorageMode::Shared && bIsStatic));
			if (bUseTransferBuffer)
			{
				FMetalFrameAllocator::AllocationEntry Entry = GetMetalDeviceContext().FetchAndRemoveLock(this);
				FMetalBuffer Transfer = Entry.Backing;
				GetMetalDeviceContext().AsyncCopyFromBufferToBuffer(Transfer, Entry.Offset, CurrentBuffer, 0, LockSize);
			}
#if PLATFORM_MAC
			else if (Mode == mtlpp::StorageMode::Managed)
			{
				if (GMetalBufferZeroFill)
					MTLPP_VALIDATE(mtlpp::Buffer, CurrentBuffer, SafeGetRuntimeDebuggingLevel() >= EMetalDebugLevelValidation, DidModify(ns::Range(0, CurrentBuffer.GetLength())));
				else
					MTLPP_VALIDATE(mtlpp::Buffer, CurrentBuffer, SafeGetRuntimeDebuggingLevel() >= EMetalDebugLevelValidation, DidModify(ns::Range(LockOffset, LockSize)));
			}
#endif //PLATFORM_MAC
			else
			{
				// shared buffers are always mapped so nothing happens
				check(Mode == mtlpp::StorageMode::Shared);
			}

			UpdateLinkedViews();
		}
		else
		{
			check(CurrentLockMode == RLM_ReadOnly);
			if(TransferBuffer)
			{
				check(Mode == mtlpp::StorageMode::Private);
				SafeReleaseMetalBuffer(TransferBuffer);
				TransferBuffer = nil;
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
    
    Other.TransferBuffer = nil;
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
        METAL_INC_DWORD_STAT_BY(MemFreed, TransferBuffer.GetLength(), GetUsage());
        SafeReleaseMetalBuffer(TransferBuffer);
    }
    
    for (FMetalBuffer& Buffer : BufferPool)
    {
        check(Buffer);
        
        METAL_INC_DWORD_STAT_BY(MemFreed, Buffer.GetLength(), GetUsage());
        SafeReleaseMetalBuffer(Buffer);
    }

    if (Data)
    {
        METAL_INC_DWORD_STAT_BY(MemFreed, Size, GetUsage());
        SafeReleaseMetalObject(Data);
    }

#if METAL_RHI_RAYTRACING
    if (EnumHasAnyFlags(GetUsage(), BUF_AccelerationStructure))
    {
        SafeReleaseMetalObject(AccelerationStructureHandle);
        AccelerationStructureHandle = nil;
    }
#endif // METAL_RHI_RAYTRACING
}

FBufferRHIRef FMetalDynamicRHI::RHICreateBuffer(FRHICommandListBase& RHICmdList, FRHIBufferDesc const& Desc, ERHIAccess /*ResourceState*/, FRHIResourceCreateInfo& CreateInfo)
{
	@autoreleasepool{

		// No life-time usage information? Enforce Dynamic.
		if (!EnumHasAnyFlags(Desc.Usage, BUF_Static | BUF_Dynamic | BUF_Volatile))
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
}

void* FMetalDynamicRHI::LockBuffer_BottomOfPipe(FRHICommandListBase& RHICmdList, FRHIBuffer* BufferRHI, uint32 Offset, uint32 Size, EResourceLockMode LockMode)
{
	@autoreleasepool {
	FMetalRHIBuffer* Buffer = ResourceCast(BufferRHI);

	// default to buffer memory
	return (uint8*)Buffer->Lock(true, LockMode, Offset, Size);
	}
}

void FMetalDynamicRHI::UnlockBuffer_BottomOfPipe(FRHICommandListBase& RHICmdList, FRHIBuffer* BufferRHI)
{
	@autoreleasepool {
	FMetalRHIBuffer* Buffer = ResourceCast(BufferRHI);

	Buffer->Unlock();
	}
}

void FMetalDynamicRHI::RHICopyBuffer(FRHIBuffer* SourceBufferRHI, FRHIBuffer* DestBufferRHI)
{
	@autoreleasepool {
		FMetalRHIBuffer* SrcBuffer = ResourceCast(SourceBufferRHI);
		FMetalRHIBuffer* DstBuffer = ResourceCast(DestBufferRHI);
		
		const FMetalBuffer& TheSrcBuffer = SrcBuffer->GetCurrentBuffer();
		const FMetalBuffer& TheDstBuffer = DstBuffer->GetCurrentBuffer();
	
		if (TheSrcBuffer && TheDstBuffer)
		{
			GetMetalDeviceContext().CopyFromBufferToBuffer(TheSrcBuffer, 0, TheDstBuffer, 0, FMath::Min(SrcBuffer->GetSize(), DstBuffer->GetSize()));
		}
		else if (TheDstBuffer)
		{
			FMetalPooledBufferArgs ArgsCPU(GetMetalDeviceContext().GetDevice(), SrcBuffer->GetSize(), BUF_Dynamic, mtlpp::StorageMode::Shared);
			FMetalBuffer TempBuffer = GetMetalDeviceContext().CreatePooledBuffer(ArgsCPU);
			FMemory::Memcpy(TempBuffer.GetContents(), SrcBuffer->Data->Data, SrcBuffer->GetSize());
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
}

void FMetalDynamicRHI::RHITransferBufferUnderlyingResource(FRHIBuffer* DestBuffer, FRHIBuffer* SrcBuffer)
{
	@autoreleasepool {
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
}
