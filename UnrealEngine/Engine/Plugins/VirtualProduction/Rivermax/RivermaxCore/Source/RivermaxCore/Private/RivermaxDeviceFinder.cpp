// Copyright Epic Games, Inc. All Rights Reserved.

#include "RivermaxDeviceFinder.h"

#include "Misc/LazySingleton.h"
#include "IRivermaxCoreModule.h"
#include "RivermaxLog.h"


#if PLATFORM_WINDOWS
#include "Windows/AllowWindowsPlatformTypes.h"
#include "Windows/PreWindowsApi.h"

#include <winsock2.h>
#include <Iphlpapi.h>
#include <ws2tcpip.h>

#include "Windows/PostWindowsApi.h"
#include "Windows/HideWindowsPlatformTypes.h"
#endif


namespace UE::RivermaxCore::Private
{
	FRivermaxDeviceFinder::FRivermaxDeviceFinder()
	{
		FindDevices();
	}

	bool FRivermaxDeviceFinder::IsValidIP(const FString& SourceIP) const
	{
		const uint32 IPHash = GetTypeHash(SourceIP);
		if (FQueriedIPInfo* FoundQuery = QueriesCache.Find(IPHash))
		{
			return FoundQuery->bIsValidIP;
		}

		TArray<FString> SourceTokens;
		bool bIsValid = SourceIP.ParseIntoArray(SourceTokens, TEXT("."), false /*CullEmpty*/) == 4;
		if (bIsValid)
		{
			for (const FString& Token : SourceTokens)
			{
				if (FCString::IsNumeric(*Token) == false && Token.Equals(TEXT("*")) == false)
				{
					bIsValid = false;
					break;
				}
			}
		}

		FQueriedIPInfo& FoundQuery = QueriesCache.FindOrAdd(IPHash);
		FoundQuery.bIsValidIP = bIsValid;
		FoundQuery.SourceIP = SourceIP;
		FoundQuery.Tokens = MoveTemp(SourceTokens);

		return bIsValid;
	}

	bool FRivermaxDeviceFinder::ResolveIP(const FString& SourceIP, FString& OutDeviceIP) const
	{
		if (IsValidIP(SourceIP))
		{
			const uint32 IPHash = GetTypeHash(SourceIP);
			FQueriedIPInfo& FoundQuery = QueriesCache[IPHash];
			if (FoundQuery.ResolvedDeviceIPHash.IsSet())
			{
				// Device cache should have this entry if the hash has been set in the IP cache
				OutDeviceIP = DevicesCache[FoundQuery.ResolvedDeviceIPHash.GetValue()].DeviceIP;
				return true;
			}

			for (const FRivermaxDeviceInfo& Device : Devices)
			{
				const uint32 DeviceIPHash = GetTypeHash(Device.InterfaceAddress);
				if (DevicesCache.Contains(DeviceIPHash) == false)
				{
					FDeviceInfo& NewDevice = DevicesCache.FindOrAdd(DeviceIPHash);
					NewDevice.DeviceIP = Device.InterfaceAddress;
					check(FIPv4Address::Parse(NewDevice.DeviceIP, NewDevice.Address));
					NewDevice.AddressTokens[0] = NewDevice.Address.A;
					NewDevice.AddressTokens[1] = NewDevice.Address.B;
					NewDevice.AddressTokens[2] = NewDevice.Address.C;
					NewDevice.AddressTokens[3] = NewDevice.Address.D;
				}

				const FDeviceInfo& CachedDevice = DevicesCache[DeviceIPHash];

				bool bMatchingDeviceFound = true;
				for (int32 TokenIndex = 0; TokenIndex < 4; ++TokenIndex)
				{
					if (FoundQuery.Tokens[TokenIndex].Equals(TEXT("*")) == false)
					{
						// If not a wildcard, token must match by value
						const uint8 TokenValue = FCString::Atoi(*FoundQuery.Tokens[TokenIndex]);
						const uint8 DeviceTokenValue = CachedDevice.AddressTokens[TokenIndex];
						if (TokenValue != DeviceTokenValue)
						{
							bMatchingDeviceFound = false;
							break;
						}
					}
				}

				if (bMatchingDeviceFound)
				{
					FoundQuery.ResolvedDeviceIPHash = DeviceIPHash;
					OutDeviceIP = CachedDevice.DeviceIP;
					return true;
				}
			}
		}

		return false;
	}

	TConstArrayView<FRivermaxDeviceInfo> FRivermaxDeviceFinder::GetDevices() const
	{
		return Devices;
	}

	void FRivermaxDeviceFinder::FindDevices()
	{
		constexpr ULONG Flags = GAA_FLAG_INCLUDE_PREFIX | GAA_FLAG_SKIP_MULTICAST | GAA_FLAG_SKIP_ANYCAST | GAA_FLAG_SKIP_DNS_SERVER | GAA_FLAG_SKIP_FRIENDLY_NAME;
		constexpr ULONG Family = AF_INET;

		// Based on SocketBSD implementation
		// determine the required size of the address list buffer
		ULONG Size = 0;
		ULONG Result = GetAdaptersAddresses(Family, Flags, NULL, NULL, &Size);

		if (Result != ERROR_BUFFER_OVERFLOW)
		{
			return;
		}

		PIP_ADAPTER_ADDRESSES AdapterAddresses = (PIP_ADAPTER_ADDRESSES)FMemory::Malloc(Size);

		// get the actual list of adapters
		Result = GetAdaptersAddresses(Family, Flags, NULL, AdapterAddresses, &Size);

		if (Result == ERROR_SUCCESS)
		{
			// extract the list of physical addresses from each adapter
			for (PIP_ADAPTER_ADDRESSES AdapterAddress = AdapterAddresses; AdapterAddress != NULL; AdapterAddress = AdapterAddress->Next)
			{
				if (AdapterAddress->OperStatus == IfOperStatusUp)
				{
					const auto AdapterFilterFunc = [](const FString& Description) -> bool
					{
						// We could also look for Mellanox but it would also include management port which we would need to discard
						if (Description.Contains(TEXT("ConnectX")))
						{
							return true;
						}

						return false;
					};

					const FString Description = StringCast<TCHAR>(AdapterAddress->Description).Get();
					if (AdapterFilterFunc(Description))
					{
						for (PIP_ADAPTER_UNICAST_ADDRESS UnicastAddress = AdapterAddress->FirstUnicastAddress; UnicastAddress != NULL; UnicastAddress = UnicastAddress->Next)
						{
							sockaddr_storage* RawAddress = reinterpret_cast<sockaddr_storage*>(UnicastAddress->Address.lpSockaddr);

							// Verify if it's IPV4 or 6
							if (RawAddress->ss_family == AF_INET)
							{
								const sockaddr* AdapterUniAddress = reinterpret_cast<const sockaddr*>(RawAddress);
								char IPString[NI_MAXHOST];
								if (getnameinfo(AdapterUniAddress, sizeof(sockaddr_in), IPString, NI_MAXHOST, nullptr, 0, NI_NUMERICHOST) == 0)
								{
									FRivermaxDeviceInfo NewDevice;
									NewDevice.Description = Description;
									NewDevice.InterfaceAddress = StringCast<TCHAR>(IPString).Get();

									UE_LOG(LogRivermax, Log, TEXT("Found adapter: Name: '%s', IP: '%s'"), *NewDevice.Description, *NewDevice.InterfaceAddress);
									Devices.Add(MoveTemp(NewDevice));
								}
							}
						}
					}
				}
			}
		}

		FMemory::Free(AdapterAddresses);
	}


}


