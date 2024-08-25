// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Online/CoreOnline.h"

/** Unique net id for OnlineSubsystemEOS */
class IUniqueNetIdEOSPlus : public FUniqueNetId
{
public:
	/**
	 * Get the Base aka Platform net id. May be null.
	 * @return the Base net id, or null.
	 */
	virtual FUniqueNetIdPtr GetBaseNetId() const = 0;
	/**
	 * Get the EOS net Id. May be null.
	 * @return the EOS net id, or null.
	 */
	virtual FUniqueNetIdPtr GetEOSNetId() const = 0;
};