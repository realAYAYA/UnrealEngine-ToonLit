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
public:
	FMetalFence()
	: NumRefs(0)
	{
		Reset();
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
			Fence = Other.Fence;
		}
		return *this;
	}
	
	void Reset(void)
	{
		WriteNum = 0;
		WaitNum = 0;
	}
	
	void Write()
	{
		WriteNum++;
	}
	
	void Wait()
	{
		WaitNum++;
	}
	
	int8 NumWrite() const
	{
		return WriteNum;
	}
	
	int8 NumWait() const
	{
		return WaitNum;
	}
	
	mtlpp::Fence Get() const
	{
		return Fence;
	}
	
	void Set(mtlpp::Fence InFence)
	{
		Fence = InFence;
	}
	
private:
	mtlpp::Fence Fence;
	int8 WriteNum;
	int8 WaitNum;
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
