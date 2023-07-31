// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Delegates/Delegate.h"
#include "Modules/ModuleInterface.h"

class UMediaPlayer;
class UObject;

/**
 * Interface for the MediaAssets module.
 */
class IMediaAssetsModule
	: public IModuleInterface
{
public:
	/** Delegate to get a player from a UObject. */
	DECLARE_DELEGATE_RetVal_TwoParams(UMediaPlayer*, FOnGetPlayerFromObject, UObject*, UObject*& /*PlayerProxy*/);

	/**
	 * Plugins should call this so they can provide a function to get a media player from an object.
	 * 
	 * @param Delegate		Delegate to get a media player.
	 * @return ID to pass in to UnregisterGetPlayerFromObject.
	 */
	virtual int32 RegisterGetPlayerFromObject(const FOnGetPlayerFromObject& Delegate) = 0;

	/**
	 * Call this to unregister a delegate.
	 *
	 * @param DelegateID	ID returned from RegisterGetPlayerFromObject.
	 */
	virtual void UnregisterGetPlayerFromObject(int32 DelegateID) = 0;

	/**
	 * Call this to get a media player (and proxy object) from an object.
	 * This will query any plugins that have called RegisterGetPlayerFromObject.
	 * 
	 * The proxy object will implement IMediaPlayerProxyInterface.
	 * 
	 * @param Object		Object to get the player from.
	 * @param PlayerProxy	Will be set to the proxy object (or nullptr if none).
	 * @return Media player, or nullptr if none found. 
	 */
	virtual UMediaPlayer* GetPlayerFromObject(UObject* Object, UObject*& PlayerProxy) = 0;

	/** Virtual destructor. */
	virtual ~IMediaAssetsModule() { }
};
