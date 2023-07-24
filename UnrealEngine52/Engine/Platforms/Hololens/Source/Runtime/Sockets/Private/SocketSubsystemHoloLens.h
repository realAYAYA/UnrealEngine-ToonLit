// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "SocketSubsystem.h"
#include "BSDSockets/SocketSubsystemBSD.h"
#include "BSDSockets/SocketsBSD.h"
#include "SocketSubsystemPackage.h"

/**
 * HoloLens specific socket subsystem implementation
 */
class FSocketSubsystemHoloLens
	: public FSocketSubsystemBSD
{
public:

	/** Default Constructor. */
	FSocketSubsystemHoloLens() :
		bTriedToInit(false)
	{ }

	/** Virtual destructor. */
	virtual ~FSocketSubsystemHoloLens() { }

public:

	// FSocketSubsystemBSD overrides

	virtual class FSocket* CreateSocket(const FName& SocketType, const FString& SocketDescription, const FName& ProtocolType) override;
	virtual bool HasNetworkDevice() override;
	virtual ESocketErrors GetLastErrorCode() override;
	virtual const TCHAR* GetSocketAPIName() const override;
	virtual bool Init(FString& Error) override;
	virtual void Shutdown() override;
	virtual ESocketErrors TranslateErrorCode(int32 Code) override;

	virtual FName GetDefaultSocketProtocolFamily() const override
	{
		return FNetworkProtocolTypes::IPv6;
	}

PACKAGE_SCOPE:

	/**
	* Singleton interface for this subsystem
	*
	* @return the only instance of this subsystem
	*/
	static FSocketSubsystemHoloLens* Create();

	/** Performs HoloLens specific socket clean up. */
	static void Destroy();

protected:

	/** Holds a flag indicating whether Init() has been called before or not. */
	bool bTriedToInit;

	/** Holds the single instantiation of this subsystem. */
	static FSocketSubsystemHoloLens* SocketSingleton;
};