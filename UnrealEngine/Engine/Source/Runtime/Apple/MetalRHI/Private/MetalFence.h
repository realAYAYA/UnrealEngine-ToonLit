// Copyright Epic Games, Inc. All Rights Reserved.


#pragma once

#import <Metal/Metal.h>

#pragma clang diagnostic ignored "-Wnullability-completeness"

@class FMetalDebugCommandEncoder;

@interface FMetalDebugFence : FApplePlatformObject<MTLFence>
{
	TLockFreePointerListLIFO<FMetalDebugCommandEncoder> UpdatingEncoders;
	TLockFreePointerListLIFO<FMetalDebugCommandEncoder> WaitingEncoders;
	NSString* Label;
}
@property (retain) id<MTLFence> Inner;
-(void)updatingEncoder:(FMetalDebugCommandEncoder*)Encoder;
-(void)waitingEncoder:(FMetalDebugCommandEncoder*)Encoder;
-(TLockFreePointerListLIFO<FMetalDebugCommandEncoder>*)updatingEncoders;
-(TLockFreePointerListLIFO<FMetalDebugCommandEncoder>*)waitingEncoders;
-(void)validate;
@end

@protocol MTLDeviceExtensions <MTLDevice>
/*!
 @method newFence
 @abstract Create a new MTLFence object
 */
- (id <MTLFence>)newFence;
@end

@protocol MTLBlitCommandEncoderExtensions <MTLBlitCommandEncoder>
/*!
 @abstract Update the event to capture all GPU work so far enqueued by this encoder. */
-(void) updateFence:(id <MTLFence>)fence;
/*!
 @abstract Prevent further GPU work until the event is reached. */
-(void) waitForFence:(id <MTLFence>)fence;
@end
@protocol MTLComputeCommandEncoderExtensions <MTLComputeCommandEncoder>
/*!
 @abstract Update the event to capture all GPU work so far enqueued by this encoder. */
-(void) updateFence:(id <MTLFence>)fence;
/*!
 @abstract Prevent further GPU work until the event is reached. */
-(void) waitForFence:(id <MTLFence>)fence;
@end
@protocol MTLRenderCommandEncoderExtensions <MTLRenderCommandEncoder>
/*!
 @abstract Update the event to capture all GPU work so far enqueued by this encoder for the given
 stages.
 @discussion Unlike <st>updateFence:</st>, this method will update the event when the given stage(s) complete, allowing for commands to overlap in execution.
 */
-(void) updateFence:(id <MTLFence>)fence afterStages:(MTLRenderStages)stages;
/*!
 @abstract Prevent further GPU work until the event is reached for the given stages.
 @discussion Unlike <st>waitForFence:</st>, this method will only block commands assoicated with the given stage(s), allowing for commands to overlap in execution.
 */
-(void) waitForFence:(id <MTLFence>)fence beforeStages:(MTLRenderStages)stages;
@end

class FMetalFence
{
	enum
	{
		NumFenceStages = 2
	};
public:
	FMetalFence()
	: NumRefs(0)
	{
		for (uint32 i = 0; i < NumFenceStages; i++)
		{
			Writes[i] = 0;
			Waits[i] = 0;
		}
	}
	
	explicit FMetalFence(FMetalFence const& Other)
	: NumRefs(0)
	{
		operator=(Other);
	}
	
	~FMetalFence()
	{
		check(!NumRefs);
	}
	
	uint32 AddRef() const
	{
		return uint32(FPlatformAtomics::InterlockedIncrement(&NumRefs));
	}
	uint32 Release() const;
	uint32 GetRefCount() const
	{
		return uint32(FPlatformAtomics::AtomicRead(&NumRefs));
	}
	
	FMetalFence& operator=(FMetalFence const& Other)
	{
		if (&Other != this)
		{
			for (uint32 i = 0; i < NumFenceStages; i++)
			{
				Fences[i] = Other.Fences[i];
			}
		}
		return *this;
	}
	
#if METAL_DEBUG_OPTIONS
	void Validate(void) const;
#endif
	
	void Reset(void)
	{
		for (uint32 i = 0; i < NumFenceStages; i++)
		{
			Writes[i] = 0;
			Waits[i] = 0;
		}
	}
	
	void Write(mtlpp::RenderStages Stage)
	{
		Writes[(uint32)Stage - 1u]++;
	}
	
	void Wait(mtlpp::RenderStages Stage)
	{
		Waits[(uint32)Stage - 1u]++;
	}
	
	int8 NumWrites(mtlpp::RenderStages Stage) const
	{
		return Writes[(uint32)Stage - 1u];
	}
	
	int8 NumWaits(mtlpp::RenderStages Stage) const
	{
		return Waits[(uint32)Stage - 1u];
	}
	
	bool NeedsWrite(mtlpp::RenderStages Stage) const
	{
		return Writes[(uint32)Stage - 1u] == 0 || (Waits[(uint32)Stage - 1u] > Writes[(uint32)Stage - 1u]);
	}
	
	bool NeedsWait(mtlpp::RenderStages Stage) const
	{
		return Waits[(uint32)Stage - 1u] == 0 || (Writes[(uint32)Stage - 1u] > Waits[(uint32)Stage - 1u]);
	}
	
	mtlpp::Fence Get(mtlpp::RenderStages Stage) const
	{
		return Fences[(uint32)Stage - 1u];
	}
	
	void Set(mtlpp::RenderStages Stage, mtlpp::Fence Fence)
	{
		Fences[(uint32)Stage - 1u] = Fence;
	}
	
	static void ValidateUsage(FMetalFence* Fence);
	
private:
	mtlpp::Fence Fences[NumFenceStages];
	int8 Writes[NumFenceStages];
	int8 Waits[NumFenceStages];
	mutable int32 NumRefs;	
};

class FMetalFencePool
{
	enum
	{
		NumFences = 2048
	};
public:
	FMetalFencePool() {}
	
	static FMetalFencePool& Get()
	{
		static FMetalFencePool sSelf;
		return sSelf;
	}
	
	void Initialise(mtlpp::Device const& InDevice);
	
	FMetalFence* AllocateFence();
	void ReleaseFence(FMetalFence* const InFence);
	
	int32 Max() const { return Count; }
	int32 Num() const { return Allocated; }
	
private:
	int32 Count;
	int32 Allocated;
	mtlpp::Device Device;
#if METAL_DEBUG_OPTIONS
	TSet<FMetalFence*> Fences;
	FCriticalSection Mutex;
#endif
	TLockFreePointerListFIFO<FMetalFence, PLATFORM_CACHE_LINE_SIZE> Lifo;
};
