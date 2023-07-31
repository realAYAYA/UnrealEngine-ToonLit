// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

/**
* Interface to Hardware Sync Device
*/

class IMediaIOCoreHardwareSync
{
public:
	/** Is hardware valid, and ready to synchronize. */
	virtual bool IsValid() const = 0;

	/** Wait on hardware synchronization. */
	virtual bool WaitVSync() = 0;

public:
	/** Virtual destructor for the interface. */ 
	virtual ~IMediaIOCoreHardwareSync() {}
};
