// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IDisplayClusterStringSerializable.h"


/**
 * Synchronizable object interface
 */
class IDisplayClusterClusterSyncObject
	: public IDisplayClusterStringSerializable
{
public:
	virtual ~IDisplayClusterClusterSyncObject() = default;

public:
	// Need to sync this object?
	virtual bool IsActive() const = 0;
	// Unique ID of synced object
	virtual FString GetSyncId() const = 0;
	// Check if object has changed since last ClearDirty call
	virtual bool IsDirty() const = 0;
	// Cleans dirty flag making it 'not changed yet'
	virtual void ClearDirty() = 0;
};
