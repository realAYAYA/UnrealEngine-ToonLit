// Copyright Epic Games, Inc. All Rights Reserved.

// Includes
#include "NetAddressResolution.h"
#include "SocketSubsystem.h"
#include "IpNetDriver.h"
#include "Sockets.h"


// Stats
DECLARE_CYCLE_STAT(TEXT("IpConnection Address Synthesis"), STAT_IpConnection_AddressSynthesis, STATGROUP_Net);


// CVars
#if !UE_BUILD_SHIPPING
TAutoConsoleVariable<FString> CVarNetDebugAddResolverAddress(
	TEXT("net.DebugAppendResolverAddress"),
	TEXT(""),
	TEXT("If this is set, all IP address resolution methods will add the value of this CVAR to the list of results.")
		TEXT("This allows for testing resolution functionality across all multiple addresses with the end goal of having a successful result")
		TEXT("(being the value of this CVAR)"),
	ECVF_Default | ECVF_Cheat);
#endif

namespace UE::Net::Private
{

/**
 * FNetDriverAddressResolution
 */

bool FNetDriverAddressResolution::InitBindSockets(FCreateAndBindSocketFunc CreateAndBindSocketFunc, EInitBindSocketsFlags Flags,
													ISocketSubsystem* SocketSubsystem, FString& Error)
{
	TArray<TSharedRef<FInternetAddr>> BindAddresses = SocketSubsystem->GetLocalBindAddresses();

	// Handle potentially empty arrays
	if (BindAddresses.Num() == 0)
	{
		Error = TEXT("No binding addresses could be found or grabbed for this platform! Sockets could not be created!");
		return false;
	}

	// Create sockets for every bind address
	for (TSharedRef<FInternetAddr>& BindAddr : BindAddresses)
	{
		FUniqueSocket NewSocket = CreateAndBindSocketFunc(BindAddr, Error);

		if (NewSocket.IsValid())
		{
			UE_LOG(LogNet, Log, TEXT("Created socket for bind address: %s"), ToCStr(BindAddr->ToString(true)));

			BoundSockets.Emplace(NewSocket.Release(), FSocketDeleter(NewSocket.GetDeleter()));
		}
		else
		{
			UE_LOG(LogNet, Warning, TEXT("Could not create socket for bind address %s, got error %s"), ToCStr(BindAddr->ToString(false)),
					ToCStr(Error));

			Error = TEXT("");

			continue;
		}

		// Servers should only have one socket that they bind on in our code.
		if (EnumHasAnyFlags(Flags, EInitBindSocketsFlags::Server))
		{
			break;
		}
	}

	if (!Error.IsEmpty() || BoundSockets.Num() == 0)
	{
		UE_LOG(LogNet, Warning, TEXT("Encountered an error while creating sockets for the bind addresses. %s"), *Error);
		
		// Make sure to destroy all sockets that we don't end up using.
		BoundSockets.Reset();

		return false;
	}

	return true;
}

void FNetDriverAddressResolution::InitConnect(UNetConnection* ServerConnection, ISocketSubsystem* SocketSubsystem, const FSocket* ActiveSocket,
												const FURL& ConnectURL)
{
	UIpConnection* IPConnection = CastChecked<UIpConnection>(ServerConnection);
	int32 DestinationPort = ConnectURL.Port;

	if (IPConnection->Resolver->IsAddressResolutionEnabled())
	{
		IPConnection->Resolver->BindSockets = MoveTemp(BoundSockets);

		// Create a weakobj so that we can pass the Connection safely to the lambda for later
		TWeakObjectPtr<UIpConnection> SafeConnectionPtr(IPConnection);

		auto AsyncResolverHandler = [SafeConnectionPtr, SocketSubsystem, DestinationPort](FAddressInfoResult Results)
		{
			FGCScopeGuard Guard;

			// Check if we still have a valid pointer
			if (!SafeConnectionPtr.IsValid())
			{
				// If we got in here, we are already in some sort of exiting state typically.
				// We shouldn't have to do any more other than not do any sort of operations on the connection
				UE_LOG(LogNet, Warning, TEXT("GAI Resolver Lambda: The NetConnection class has become invalid after results for %s were grabbed."), *Results.QueryHostName);
				return;
			}


			UIpConnection* Connection = SafeConnectionPtr.Get();
			
			if (Results.ReturnCode == SE_NO_ERROR)
			{
				TArray<TSharedRef<FInternetAddr>> AddressResults;
				for (auto& Result : Results.Results)
				{
					AddressResults.Add(Result.Address);
				}

#if !UE_BUILD_SHIPPING
				// This is useful for injecting a good result into the array to test the resolution system
				const FString DebugAddressAddition = CVarNetDebugAddResolverAddress.GetValueOnAnyThread();
				if (!DebugAddressAddition.IsEmpty())
				{
					TSharedPtr<FInternetAddr> SpecialResultAddr = SocketSubsystem->GetAddressFromString(DebugAddressAddition);
					if (SpecialResultAddr.IsValid())
					{
						SpecialResultAddr->SetPort(DestinationPort);
						AddressResults.Add(SpecialResultAddr.ToSharedRef());
						UE_LOG(LogNet, Log, TEXT("Added additional result address %s to resolver list"), *SpecialResultAddr->ToString(false));
					}
				}
#endif

				Connection->Resolver->ResolverResults = MoveTemp(AddressResults);
				Connection->Resolver->ResolutionState = EAddressResolutionState::TryNextAddress;
			}
			else
			{
				Connection->Resolver->ResolutionState = EAddressResolutionState::Error;

				Connection->Close(ENetCloseResult::AddressResolutionFailed);
			}
		};

		SocketSubsystem->GetAddressInfoAsync(AsyncResolverHandler, *ConnectURL.Host, *FString::Printf(TEXT("%d"), DestinationPort),
			EAddressInfoFlags::AllResultsWithMapping | EAddressInfoFlags::OnlyUsableAddresses, NAME_None, ESocketType::SOCKTYPE_Datagram);
	}
	else
	{
		// Clean up any potential multiple sockets we have created when resolution was disabled.
		// InitBase could have created multiple sockets and if so, we'll want to clean them up.
		for (int32 SockIdx=BoundSockets.Num()-1; SockIdx >= 0; SockIdx--)
		{
			if (BoundSockets[SockIdx].Get() != ActiveSocket)
			{
				BoundSockets.RemoveAt(SockIdx);
			}
		}
	}
}

void FNetDriverAddressResolution::SetRetrieveTimestamp(bool bRetrieveTimestamp)
{
	for (TSharedPtr<FSocket>& CurSocket : BoundSockets)
	{
		CurSocket->SetRetrieveTimestamp(bRetrieveTimestamp);
	}
}

FNetConnectionAddressResolution* FNetDriverAddressResolution::GetConnectionResolver(UIpConnection* Connection)
{
	return Connection != nullptr ? Connection->Resolver.Get() : nullptr;
}


/**
 * FNetConnectionAddressResolution
 */

FNetConnectionAddressResolution::FNetConnectionAddressResolution(const FSocket* const & InDeprecatedSocket)
	: DeprecatedSocket(InDeprecatedSocket)
	, ResolutionState(EAddressResolutionState::None)
{
}

bool FNetConnectionAddressResolution::InitLocalConnection(ISocketSubsystem* SocketSubsystem, FSocket* InSocket, const FURL& InURL)
{
	bool bValidInit = true;

	// If resolution is disabled, fall back to address synthesis
	if (!IsAddressResolutionEnabled())
	{
		// Figure out IP address from the host URL
		bValidInit = false;

		// Get numerical address directly.
		RemoteAddr = SocketSubsystem->CreateInternetAddr();
		RemoteAddr->SetIp(*InURL.Host, bValidInit);

		// If the protocols do not match, attempt to synthesize the address so they do.
		FName SocketProtocol = InSocket->GetProtocol();

		if ((bValidInit && SocketProtocol != RemoteAddr->GetProtocolType()) || !bValidInit)
		{
			SCOPE_CYCLE_COUNTER(STAT_IpConnection_AddressSynthesis);

			// We want to use GAI to create the address with the correct protocol.
			const FAddressInfoResult MapRequest = SocketSubsystem->GetAddressInfo(*InURL.Host, nullptr,
				EAddressInfoFlags::AllResultsWithMapping | EAddressInfoFlags::OnlyUsableAddresses, SocketProtocol);

			// Set the remote addr provided we have information.
			if (MapRequest.ReturnCode == SE_NO_ERROR && MapRequest.Results.Num() > 0)
			{
				RemoteAddr = MapRequest.Results[0].Address->Clone();
				bValidInit = true;
			}
			else
			{
				UE_LOG(LogNet, Warning, TEXT("IpConnection::InitConnection: Address protocols do not match and cannot be synthesized to a ")
						TEXT("similar address, this will likely lead to issues!"));
			}
		}

		RemoteAddr->SetPort(InURL.Port);

		if (!bValidInit)
		{
			UE_LOG(LogNet, Verbose, TEXT("IpConnection::InitConnection: Unable to resolve %s"), ToCStr(InURL.Host));
		}
	}
	else
	{
		ResolutionState = EAddressResolutionState::WaitingForResolves;
	}

	return bValidInit;
}

EEAddressResolutionHandleResult FNetConnectionAddressResolution::NotifyTimeout()
{
	EEAddressResolutionHandleResult Result = EEAddressResolutionHandleResult::CallerShouldHandle;

	if (CanContinueResolution())
	{
		ResolutionState = EAddressResolutionState::TryNextAddress;
		Result = EEAddressResolutionHandleResult::HandledInternally;
	}

	return Result;
}

EEAddressResolutionHandleResult FNetConnectionAddressResolution::NotifyReceiveError()
{
	EEAddressResolutionHandleResult Result = EEAddressResolutionHandleResult::CallerShouldHandle;

	if (CanContinueResolution())
	{
		ResolutionState = EAddressResolutionState::TryNextAddress;
		Result = EEAddressResolutionHandleResult::HandledInternally;
	}
	else
	{
		ResolutionState = EAddressResolutionState::Error;
	}

	return Result;
}

EEAddressResolutionHandleResult FNetConnectionAddressResolution::NotifySendError()
{
	EEAddressResolutionHandleResult Result = EEAddressResolutionHandleResult::CallerShouldHandle;

	if (CanContinueResolution())
	{
		ResolutionState = EAddressResolutionState::TryNextAddress;
		Result = EEAddressResolutionHandleResult::HandledInternally;
	}
	else
	{
		ResolutionState = EAddressResolutionState::Error;
	}

	return Result;
}

ECheckAddressResolutionResult FNetConnectionAddressResolution::CheckAddressResolution()
{
	ECheckAddressResolutionResult Result = ECheckAddressResolutionResult::None;

	if (ResolutionState == EAddressResolutionState::TryNextAddress)
	{
		if (CurrentAddressIndex >= ResolverResults.Num())
		{
			UE_LOG(LogNet, Warning, TEXT("Exhausted the number of resolver results, closing the connection now."));
			ResolutionState = EAddressResolutionState::Error;
			return ECheckAddressResolutionResult::None;
		}

		RemoteAddr = ResolverResults[CurrentAddressIndex];

		ResolutionSocket.Reset();

		for (const TSharedPtr<FSocket>& BindSocket : BindSockets)
		{
			if (BindSocket->GetProtocol() == RemoteAddr->GetProtocolType())
			{
				ResolutionSocket = BindSocket;
				break;
			}
		}

		if (ResolutionSocket.IsValid())
		{
			ResolutionState = EAddressResolutionState::Connecting;

			if (CurrentAddressIndex == 0)
			{
				Result = ECheckAddressResolutionResult::TryFirstAddress;
			}
			else
			{
				Result = ECheckAddressResolutionResult::TryNextAddress;
			}

			++CurrentAddressIndex;
		}
		else
		{
			UE_LOG(LogNet, Error, TEXT("Unable to find a binding socket for the resolve address result %s"), ToCStr(RemoteAddr->ToString(true)));

			ResolutionState = EAddressResolutionState::Error;
			Result = ECheckAddressResolutionResult::FindSocketError;
		}
	}
	else if (ResolutionState == EAddressResolutionState::Connected)
	{
		ResolutionState = EAddressResolutionState::Done;
		Result = ECheckAddressResolutionResult::Connected;

		CleanupResolutionSockets(ECleanupResolutionSocketsFlags::CleanInactive);
	}
	else if (ResolutionState == EAddressResolutionState::Error)
	{
		UE_LOG(LogNet, Warning, TEXT("Encountered an error, cleaning up this connection now"));

		ResolutionState = EAddressResolutionState::Done;
		Result = ECheckAddressResolutionResult::Error;
	}

	return Result;
}

void FNetConnectionAddressResolution::NotifyAddressResolutionConnected()
{
	if (ResolutionState == EAddressResolutionState::Connecting)
	{
		ResolutionState = EAddressResolutionState::Connected;
	}
}

void FNetConnectionAddressResolution::CleanupResolutionSockets(ECleanupResolutionSocketsFlags CleanupFlags
																/*=ECleanupResolutionSocketsFlags::CleanAll*/)
{
	if (IsAddressResolutionEnabled())
	{
		if (EnumHasAnyFlags(CleanupFlags, ECleanupResolutionSocketsFlags::CleanAll))
		{
			// Remove when UIpConnection.Socket is deprecated
#if !UE_BUILD_SHIPPING && !PLATFORM_ANDROID
			if ( (!GIsBuildMachine) && (!IsRunningCommandlet()) ) // make sure we don't hit the ensure while on the Android testing... 
			{
				ensureMsgf(DeprecatedSocket == nullptr,
					TEXT("All codepaths passing 'CleanAll', must call 'UIpConnection::CleanupDeprecatedSocket' first."));
			}
#endif

			ResolutionSocket.Reset();
		}
	
		BindSockets.Reset();
		ResolverResults.Empty();
	}
}

}
