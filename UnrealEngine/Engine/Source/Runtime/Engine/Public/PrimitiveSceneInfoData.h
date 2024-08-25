// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once 

#include "RendererInterface.h"
#include "HAL/ThreadSafeCounter.h"
#include "PrimitiveComponentId.h"

class FPrimitiveSceneProxy;

/*
 * All the necessary information for scene primitive to component feedback 
 */
struct FPrimitiveSceneInfoData
{		
	/** The primitive's scene info. */
	FPrimitiveSceneProxy* SceneProxy = nullptr;	

	/** Last time the component was submitted for rendering (called FScene::AddPrimitive). */
	float LastSubmitTime = -1000;
	
	/**
	 * The value of WorldSettings->TimeSeconds for the frame when this component was last rendered.  This is written
	 * from the render thread, which is up to a frame behind the game thread, so you should allow this time to
	 * be at least a frame behind the game thread's world time before you consider the actor non-visible.
	 */
	mutable float LastRenderTime = -1000.0f;

	/** Same as LastRenderTime but only updated if the component is on screen. Used by the texture streamer. */
	mutable float LastRenderTimeOnScreen = -1000.0f;
	

	/**
	* Incremented by the main thread before being attached to the scene, decremented
	* by the rendering thread after removal. This counter exists to assert that 
	* operations are safe in order to help avoid race conditions.
	*
	*           *** Runtime logic should NEVER rely on this value. ***
	*
	* The only safe assertions to make are:
	*
	*     AttachmentCounter == 0: The primitive is not exposed to the rendering
	*                             thread, it is safe to modify shared members.
	*                             This assertion is valid ONLY from the main thread.
	*
	*     AttachmentCounter >= 1: The primitive IS exposed to the rendering
	*                             thread and therefore shared members must not
	*                             be modified. This assertion may be made from
	*                             any thread. Note that it is valid and expected
	*                             for AttachmentCounter to be larger than 1, e.g.
	*                             during reattachment.
	*/
	FThreadSafeCounter AttachmentCounter;

	/** Used by the renderer, to identify a primitive across re-registers. */
	FPrimitiveComponentId PrimitiveSceneId;

	/**
	* Identifier used to track the time that this component was registered with the world / renderer.
	* Updated to unique incremental value each time OnRegister() is called. The value of 0 is unused.
	* */
	int32 RegistrationSerialNumber = -1;

	/** 
	 * Pointer to the last render time variable on the primitive's owning actor or other UObject (if owned), which is written to by the RT and read by the GT.
	 * The value of LastRenderTime will therefore not be deterministic due to race conditions, but the GT uses it in a way that allows this.
	 * Storing a pointer to the UObject member variable only works in the AActor/UPrimitiveComponent case because:
	 *	UPrimitiveComponent's outer is its owning AActor, so it prevents the owner from being garbage collected while the component lives.
	 *  If the UPrimitiveComponent is GC'd during the Actor's lifetime, OwnerLastRenderTime is still valid so there is no issue.
	 *	If the UPrimitiveComponent and the Actor are GC'd together, neither will be deleted until FinishDestroy has been executed on both.
	 *	UPrimitiveComponent's FinishDestroy will not execute until the primitive has been detached from the Scene through it's DetachFence.
	 * In general feedback from the renderer to the game thread like this should be avoided.
	 * 
	 * Any other user of this struct that intends to add it's own primitives in the Scene must provide the same guarantees. 
	 * 
	 */
	float* OwnerLastRenderTimePtr = nullptr;


	void SetLastRenderTime(float InLastRenderTime, bool bUpdateLastRenderTimeOnScreen) const
	{
		LastRenderTime = InLastRenderTime;
		if (bUpdateLastRenderTimeOnScreen)
			LastRenderTimeOnScreen = InLastRenderTime;

		if (OwnerLastRenderTimePtr)
		{
			*OwnerLastRenderTimePtr = InLastRenderTime;
		}
	}	

protected:

	/** Next id to be used by a component. */
	static ENGINE_API FThreadSafeCounter NextPrimitiveId;

	/** Next registration serial number to be assigned to a component when it is registered. */
	static ENGINE_API FThreadSafeCounter NextRegistrationSerialNumber;

public:

	static int32 GetNextRegistrationSerialNumber() { return NextRegistrationSerialNumber.Increment(); }

	FPrimitiveSceneInfoData()
	{
		RegistrationSerialNumber = GetNextRegistrationSerialNumber();
		PrimitiveSceneId.PrimIDValue = NextPrimitiveId.Increment();
	}

};