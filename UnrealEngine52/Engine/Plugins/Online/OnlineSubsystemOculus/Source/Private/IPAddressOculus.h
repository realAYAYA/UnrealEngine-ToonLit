// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "IPAddress.h"
#include "Algo/Reverse.h"
#include "Engine/EngineBaseTypes.h"
#include "OnlineSubsystemOculus.h"
#include "OnlineSubsystemOculusTypes.h"
#include "OnlineSubsystemOculusPackage.h"

/**
* Fake an internet ip address but in reality using an ovrID
*/
class FInternetAddrOculus : public FInternetAddr
{
PACKAGE_SCOPE:
	FUniqueNetIdOculusRef OculusId;

	/**
	* Copy Constructor
	*/
	FInternetAddrOculus(const FInternetAddrOculus& Src) :
		OculusId(Src.OculusId)
	{
	}

public:
	/**
	* Constructor. Sets address to default state
	*/
	FInternetAddrOculus() :
		OculusId(FUniqueNetIdOculus::EmptyId())
	{
	}

	/**
	* Constructor
	*/
	explicit FInternetAddrOculus(const FUniqueNetIdOculus& InOculusId) :
		OculusId(InOculusId.AsShared())
	{
	}

	/**
	* Constructor
	*/
	explicit FInternetAddrOculus(ovrID InOculusId) :
		OculusId(FUniqueNetIdOculus::Create(InOculusId))
	{
	}

	static FInternetAddrOculus FromUrl(const FURL& ConnectURL)
	{
		FString Host = ConnectURL.Host;
		// Parse the URL: unreal://<oculus_id>.oculus or unreal://<oculus_id>
		int32 DotIndex;
		FString OculusStringID = (Host.FindChar('.', DotIndex)) ? Host.Left(DotIndex) : Host;
		uint64 OculusUintId = strtoull(TCHAR_TO_ANSI(*OculusStringID), nullptr, 10);
		const FUniqueNetIdOculusRef OculusNetId = FUniqueNetIdOculus::Create(OculusUintId);
		return FInternetAddrOculus(*OculusNetId);
	}

	ovrID GetID() const
	{
		return OculusId->GetID();
	}

	virtual TArray<uint8> GetRawIp() const override
	{
		TArray<uint8> RawAddressArray;
		const uint8* OculusIdWalk = OculusId->GetBytes();
		while (RawAddressArray.Num() < OculusId->GetSize())
		{
			RawAddressArray.Add(*OculusIdWalk);
			++OculusIdWalk;
		}

		// We want to make sure that these arrays are in big endian format.
#if PLATFORM_LITTLE_ENDIAN
		Algo::Reverse(RawAddressArray);
#endif

		return RawAddressArray;
	}

	virtual void SetRawIp(const TArray<uint8>& RawAddr) override
	{
		ovrID NewId = 0;

		// Make a quick copy of the array
		TArray<uint8> WorkingArray = RawAddr;

		// If we're little endian, we need to reverse so that the shifts turn out correct
#if PLATFORM_LITTLE_ENDIAN
		Algo::Reverse(WorkingArray);
#endif

		for (int32 i = 0; i < WorkingArray.Num(); ++i)
		{
			NewId |= (ovrID)WorkingArray[i] << (i * 8);
		}

		OculusId = FUniqueNetIdOculus::Create(NewId);
	}

	/**
	* Sets the ip address from a host byte order uint32
	*
	* @param InAddr the new address to use (must convert to network byte order)
	*/
	void SetIp(uint32 InAddr) override
	{
		/** Not used */
	}

	/**
	* Sets the ip address from a string ("A.B.C.D")
	*
	* @param InAddr the string containing the new ip address to use
	*/
	void SetIp(const TCHAR* InAddr, bool& bIsValid) override
	{
		/** Not used */
	}

	/**
	* Copies the network byte order ip address to a host byte order dword
	*
	* @param OutAddr the out param receiving the ip address
	*/
	void GetIp(uint32& OutAddr) const override
	{
		/** Not used */
	}

	/**
	* Sets the port number from a host byte order int
	*
	* @param InPort the new port to use (must convert to network byte order)
	*/
	void SetPort(int32 InPort) override
	{
		/** Not used */
	}

	/**
	* Copies the port number from this address and places it into a host byte order int
	*
	* @param OutPort the host byte order int that receives the port
	*/
	void GetPort(int32& OutPort) const override
	{
		/** Not used */
	}

	/**
	* Returns the port number from this address in host byte order
	*/
	int32 GetPort() const override
	{
		/** Not used */
		return 0;
	}

	/** Sets the address to be any address */
	void SetAnyAddress() override
	{
		/** Not used */
	}

	/** Sets the address to broadcast */
	void SetBroadcastAddress() override
	{
		/** Not used */
	}

	/** Sets the address to loopback */
	void SetLoopbackAddress() override
	{
		/** Not used */
	}

	/**
	* Converts this internet ip address to string form
	*
	* @param bAppendPort whether to append the port information or not
	*/
	FString ToString(bool bAppendPort) const override
	{
		return OculusId->ToString();
	}

	/**
	* Compares two internet ip addresses for equality
	*
	* @param Other the address to compare against
	*/
	virtual bool operator==(const FInternetAddr& Other) const override
	{
		FInternetAddrOculus& OculusOther = (FInternetAddrOculus&)Other;
		return OculusId == OculusOther.OculusId;
	}

	bool operator!=(const FInternetAddrOculus& Other) const
	{
		return !(FInternetAddrOculus::operator==(Other));
	}

	virtual uint32 GetTypeHash() const override
	{
		return ::GetTypeHash((uint64)GetID());
	}

	virtual FName GetProtocolType() const override
	{
		return FNetworkProtocolTypes::Oculus;
	}

	/**
	* Is this a well formed internet address
	*
	* @return true if a valid Oculus id, false otherwise
	*/
	virtual bool IsValid() const override
	{
		return OculusId->IsValid();
	}

	virtual TSharedRef<FInternetAddr> Clone() const override
	{
		TSharedRef<FInternetAddrOculus> NewAddress = MakeShareable(new FInternetAddrOculus);
		NewAddress->OculusId = OculusId;
		return NewAddress;
	}
};
