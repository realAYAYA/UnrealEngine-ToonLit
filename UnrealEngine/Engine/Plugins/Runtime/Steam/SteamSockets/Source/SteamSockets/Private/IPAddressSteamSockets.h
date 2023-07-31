// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "IPAddress.h"
#include "SteamSocketsPrivate.h"
#include "SteamSocketsPackage.h"
#include "SteamSocketsTypes.h"
#include "Engine/EngineBaseTypes.h"

class FInternetAddrSteamSockets : public FInternetAddr
{
PACKAGE_SCOPE:
	SteamNetworkingIdentity Addr;
	int32 P2PVirtualPort;
	FName ProtocolType;

public:
	FInternetAddrSteamSockets(const FName RequestedProtocol = NAME_None) :
		P2PVirtualPort(0),
		ProtocolType(RequestedProtocol)
	{
		Addr.Clear();
	}

	FInternetAddrSteamSockets(const FInternetAddrSteamSockets& In) :
		Addr(In.Addr),
		P2PVirtualPort(In.P2PVirtualPort),
		ProtocolType(In.ProtocolType)
	{
	}

	FInternetAddrSteamSockets(const SteamNetworkingIdentity& NewAddress) :
		Addr(NewAddress),
		P2PVirtualPort(0),
		ProtocolType(NewAddress.GetIPAddr() == nullptr ? FNetworkProtocolTypes::SteamSocketsP2P : FNetworkProtocolTypes::SteamSocketsIP)
	{
	}

	FInternetAddrSteamSockets(const SteamNetworkingIPAddr& IPAddr) :
		P2PVirtualPort(0),
		ProtocolType(FNetworkProtocolTypes::SteamSocketsIP)
	{
		Addr.SetIPAddr(IPAddr);
	}

	explicit FInternetAddrSteamSockets(uint64& SteamID) :
		P2PVirtualPort(0),
		ProtocolType(FNetworkProtocolTypes::SteamSocketsP2P)
	{
		Addr.SetSteamID64(SteamID);
	}
	
	virtual TArray<uint8> GetRawIp() const override;

	virtual void SetRawIp(const TArray<uint8>& RawAddr) override;

	virtual void SetIp(uint32 InAddr) override
	{
		/** Not used */
	}

	/**
	* Sets the ip address from a string ("A.B.C.D") or a steam id "steam.STEAMID" or "STEAMID:PORT"
	*
	* @param InAddr the string containing the new ip address to use
	*/
	virtual void SetIp(const TCHAR* InAddr, bool& bIsValid) override;

	/**
	* Copies the network byte order ip address to a host byte order dword
	*
	* @param OutAddr the out param receiving the ip address
	*/
	virtual void GetIp(uint32& OutAddr) const override
	{
		/** Not used */
	}

	/**
	* Sets the port number from a host byte order int
	*
	* @param InPort the new port to use (must convert to network byte order)
	*/
	virtual void SetPort(int32 InPort) override;

	/**
	* Returns the port number from this address in host byte order
	*/
	virtual int32 GetPort() const override;

	/**
	 * Set Platform specific port data
	 */
	virtual void SetPlatformPort(int32 InPort) override
	{
		P2PVirtualPort = (int16)InPort;
	}

	/**
	 * Get platform specific port data.
	 */
	virtual int32 GetPlatformPort() const override
	{
		return (int32)P2PVirtualPort;
	}

	/** Sets the address to be any address */
	virtual void SetAnyAddress() override;

	/** Sets the address to broadcast */
	virtual void SetBroadcastAddress() override
	{
		/** Not used */
	}

	/** Sets the address to loopback */
	virtual void SetLoopbackAddress() override
	{
		Addr.SetLocalHost();
	}

	/**
	* Converts this internet ip address to string form
	*
	* @param bAppendPort whether to append the port information or not
	*/
	virtual FString ToString(bool bAppendPort) const override;

	/**
	* Compares two internet ip addresses for equality
	*
	* @param Other the address to compare against
	*/
	virtual bool operator==(const FInternetAddr& Other) const override
	{
		FInternetAddrSteamSockets& SteamOther = (FInternetAddrSteamSockets&)Other;
		return Addr == SteamOther.Addr;
	}

	virtual uint32 GetTypeHash() const override;

	virtual FName GetProtocolType() const override;

	virtual bool IsValid() const override
	{
		return !(Addr.IsInvalid());
	}

	operator const SteamNetworkingIPAddr() const
	{
		const SteamNetworkingIPAddr* IPAddr = Addr.GetIPAddr();
		if (IPAddr == nullptr)
		{
			SteamNetworkingIPAddr EmptyAddr;
			EmptyAddr.Clear();
			return EmptyAddr;
		}
		return *IPAddr;
	}

	operator const SteamNetworkingIdentity() const
	{
		return Addr;
	}

	virtual TSharedRef<FInternetAddr> Clone() const override
	{
		TSharedRef<FInternetAddrSteamSockets> NewAddress = MakeShareable(new FInternetAddrSteamSockets(ProtocolType));
		NewAddress->Addr = Addr;
		NewAddress->P2PVirtualPort = P2PVirtualPort;
		return NewAddress;
	}
};
