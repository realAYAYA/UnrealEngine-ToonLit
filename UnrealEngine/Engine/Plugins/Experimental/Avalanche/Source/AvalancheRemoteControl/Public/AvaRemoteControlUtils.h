// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "HAL/Platform.h"

class URemoteControlPreset;

struct FAvaRemoteControlUtils
{
	/**
	 *	Register the given RemoteControlPreset in the RemoteControl module.	
	 *	Registered RemoteControlPreset are made accessible, by name, to the other systems, like the Web interface.
	 *	
	 *	@param InRemoteControlPreset Preset to register
	 *	@param bInEnsureUniqueId The registered preset will be given a unique id if another preset instance has already been registered.
	 *	@return whether registration was successful.
	 */
	AVALANCHEREMOTECONTROL_API static bool RegisterRemoteControlPreset(URemoteControlPreset* InRemoteControlPreset, bool bInEnsureUniqueId);

	/**
	 *	Unregister the given RemoteControlPreset from the RemoteControl module.
	 */
	AVALANCHEREMOTECONTROL_API static void UnregisterRemoteControlPreset(URemoteControlPreset* InRemoteControlPreset);
};
