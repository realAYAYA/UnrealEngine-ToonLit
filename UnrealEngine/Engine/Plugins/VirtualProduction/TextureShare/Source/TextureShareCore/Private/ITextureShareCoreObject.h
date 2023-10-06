// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once
#include "CoreMinimal.h"
#include "Containers/TextureShareCoreContainers.h"

/**
 * TextureShareCore object API
 */
class TEXTURESHARECORE_API ITextureShareCoreObject
{
public:
	virtual ~ITextureShareCoreObject() = default;

public:
	//////////////////// State ////////////////////

	/**
	* Return the name of the TextureShare.
	*/
	virtual const FString& GetName() const = 0;

	/**
	* Returns detailed information about the TextureShare object.
	*/
	virtual const FTextureShareCoreObjectDesc& GetObjectDesc() const = 0;

	/**
	* Returns detailed information about the TextureShare proxy object.
	*/
	virtual const FTextureShareCoreObjectDesc& GetObjectDesc_RenderThread() const = 0;

	/**
	 * Returns true if the TextureShare object is ready to be used.
	 */
	virtual bool IsActive() const = 0;

	/**
	 * Returns true if the TextureShare proxy object is ready to be used.
	 */
	virtual bool IsActive_RenderThread() const = 0;

	/**
	 * Returns true if the TextureShareCore object has started a session and processes are connected for this frame.
	 */
	virtual bool IsFrameSyncActive() const = 0;

	/**
	 * Returns true if the TextureShareCore proxy object has started a session and processes are connected for this frame.
	 */
	virtual bool IsFrameSyncActive_RenderThread() const = 0;

	/**
	 * Returns true if the BeginFrameSync() function can be called at this moment
	 */
	virtual bool IsBeginFrameSyncActive() const = 0;

	/**
	 * Returns true if the BeginFrameSync_RenderThread() function can be called at this moment
	 */
	virtual bool IsBeginFrameSyncActive_RenderThread() const = 0;

public:
	//////////////////// Settings ////////////////////

	/**
	 * Change the process name for this TextureShare object.
	 * If the object is synced now, then the changes are deferred and executed in the next frame.
	 *
	 * @param InProcessId - New process name
	 *
	 * @return true, if success
	 */
	virtual bool SetProcessId(const FString& InProcessId) = 0;

	/**
	 * Assign a device type for this TextureShare object.
	 *
	 * @param InDeviceType - render device type
	 */
	virtual bool SetDeviceType(const ETextureShareDeviceType InDeviceType) = 0;

	/**
	 * Change the sync settings for this TextureShare object.
	 * If the object is synced now, then the changes are deferred and executed in the next frame.
	 *
	 * @param InSyncSetting - New sync settings
	 *
	 * @return true, if success
	 */
	virtual bool SetSyncSetting(const FTextureShareCoreSyncSettings& InSyncSetting) = 0;

	/**
	* Return the sync settings of the TextureShare.
	*/
	virtual const FTextureShareCoreSyncSettings& GetSyncSetting() const = 0;

	/**
	 * Get TextureShare default sync settings by template type
	 * The settings of this template are not related to the current settings of the object
	 *
	 * @param InType - Sync settings template type
	 *
	 * @return structure with sync logic settings
	 */
	virtual FTextureShareCoreFrameSyncSettings GetFrameSyncSettings(const ETextureShareFrameSyncTemplate InType) const = 0;

	/**
	 * Find skipped frame sync step for object
	 *
	 * @param InSyncStep           - Sync step value
	 * @param bIsRenderThreadFrame - TS has two sync steps groups: for the game thread and for the rendering thread.
	 * @param OutSkippedSyncStep   - Skipped sync step value
	 *
	 * @return true, if skipped sync step found
	 */
	virtual bool FindSkippedSyncStep(const ETextureShareSyncStep InSyncStep, ETextureShareSyncStep& OutSkippedSyncStep) = 0;

	/**
	 * Find skipped frame sync step for proxy object
	 *
	 * @param InSyncStep           - Sync step value
	 * @param bIsRenderThreadFrame - TS has two sync steps groups: for the game thread and for the rendering thread.
	 * @param OutSkippedSyncStep   - Skipped sync step value
	 *
	 * @return true, if skipped sync step found
	 */
	virtual bool FindSkippedSyncStep_RenderThread(const ETextureShareSyncStep InSyncStep, ETextureShareSyncStep& OutSkippedSyncStep) = 0;

public:
	//////////////////// Session ////////////////////
	
	/**
	 * Start a TextureShare session for the specified device
	 *
	 * @param InDeviceType     - Shared resources render device type
	 *
	 * @return True if the success
	 */
	virtual bool BeginSession() = 0;

	/**
	 * End the TextureShare session and the resources in use are also removed.
	 *
	 * @return True if the success
	 */
	virtual bool EndSession() = 0;

	/**
	 * Returns true if the session is currently valid
	 */
	virtual bool IsSessionActive() const = 0;

public:
	//////////////////// Thread sync support ////////////////////

	/**
	 * Support for multi-threaded implementation.
	 * The mutex of the specified type will be locked (and created for the first time)
	 *
	 * @param InThreadMutex - Mutex type
	 * @param bForceLockNoWait - lock without waiting for unlock
	 *
	 * @return True if the success
	 */
	virtual bool LockThreadMutex(const ETextureShareThreadMutex InThreadMutex, bool bForceLockNoWait = false) = 0;

	/**
	 * Support for multi-threaded implementation.
	 * The mutex of the specified type will be unlocked
	 *
	 * @param InThreadMutex - Mutex type
	 *
	 * @return True if the success
	 */
	virtual bool UnlockThreadMutex(const ETextureShareThreadMutex InThreadMutex) = 0;

public:
	//////////////////// Interprocess Synchronization ////////////////////
	
	/**
	 * Begin sync logic in range FrameBegin..FrameEnd
	 * The list of connected processes for the current frame will also be updated.
	 * Returns true if the frame is connected to other processes that match the synchronization settings.
	 */
	virtual bool BeginFrameSync() = 0;

	/**
	 * Synchronize connected processes that support this sync step
	 * Data from remote processes will be read at the time the barrier is synchronized.
	 * Missed sync steps from the sync settings will also be called.
	 * 
	 * @param InSyncStep - Sync step value
	 *
	 * @return True if the success
	 */
	virtual bool FrameSync(const ETextureShareSyncStep InSyncStep) = 0;

	/**
	 * Finalize sync logic in range FrameBegin..FrameEnd
	 * Missed sync steps from the sync settings will also be called.
	 */
	virtual bool EndFrameSync() = 0;

	/**
	 * Begin sync logic in range FrameProxyBegin..FrameProxyEnd
	 */
	virtual bool BeginFrameSync_RenderThread() = 0;

	/**
	 * Synchronize connected processes that support this sync step
	 * ProxyData from remote processes will be read at the time the barrier is synchronized.
	 * Missed sync steps from the sync settings will also be called.
	 *
	 * @param InSyncStep - Sync step value
	 *
	 * @return True if the success
	 */
	virtual bool FrameSync_RenderThread(const ETextureShareSyncStep InSyncStep) = 0;

	/**
	 * Finalize sync logic in range FrameProxyBegin..FrameProxyEnd
	 * Missed sync steps from the sync settings will also be called.
	 */
	virtual bool EndFrameSync_RenderThread() = 0;

	/**
	 * Returns a list of handles to the interprocess TextureShare objects that are currently connected to this object.
	 *
	 * @param OutInterprocessObjects - Output array with handles to TextureShare objects
	 *
	 * @return true, if success
	 */
	virtual const TArraySerializable<FTextureShareCoreObjectDesc>& GetConnectedInterprocessObjects() const = 0;

public:
	//////////////////// Data Containers ////////////////////

	/**
	 * Reference to Data.
	 * Object data for the current frame in the GameThread.
	 */
	virtual FTextureShareCoreData& GetData() = 0;

	/**
	 * Reference to ProxyData.
	 * Object proxy data for the current frame in the RenderThread.
	 */
	virtual FTextureShareCoreProxyData& GetProxyData_RenderThread() = 0;

	/**
	 * Received Data from connected process objects
	 */
	virtual const TArraySerializable<FTextureShareCoreObjectData>& GetReceivedData() const = 0;

	/**
	 * Received ProxyData from connected process objects
	 */
	virtual const TArraySerializable<FTextureShareCoreObjectProxyData>& GetReceivedProxyData_RenderThread() const= 0;

public:
	//////////////////// Destructor ////////////////////
	
	/**
	 * Remove TextureShareCore object
	 * call ITextureShareCoreAPI::Remove()
	 */
	virtual bool RemoveObject() = 0;
};
