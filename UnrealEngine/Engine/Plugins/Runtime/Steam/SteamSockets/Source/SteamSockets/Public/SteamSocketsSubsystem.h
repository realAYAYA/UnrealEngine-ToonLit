// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "SocketSubsystem.h"
#include "IPAddress.h"
#include "Containers/Ticker.h"
#include "Containers/Map.h"
#include "Containers/Array.h"
#include "UObject/WeakObjectPtr.h"
#include "SteamSocketsPackage.h"
#include "SteamSocketsTaskManagerInterface.h"
#include "SteamSocketsTypes.h"
#include "SocketTypes.h"

// Forward declare some internal types for bookkeeping.
class FSteamSocket;
class USteamSocketsNetDriver;

/**
 * Steam Sockets specific socket subsystem implementation. 
 * This class can only be used with the SteamSocketsNetDriver and the SteamSocketsNetConnection classes.
 * This subsystem does not support mixing any other NetDriver/NetConnection format. Doing so will cause this protocol to not function.
 */
class STEAMSOCKETS_API FSteamSocketsSubsystem : public ISocketSubsystem, public FTSTickerObjectBase, public FSelfRegisteringExec
{
public:

	FSteamSocketsSubsystem() :
		LastSocketError(0),
		bShouldTestPeek(false),
		SteamEventManager(nullptr),
		bUseRelays(true),
		SteamAPIClientHandle(nullptr),
		SteamAPIServerHandle(nullptr)
	{
	}

	//~ Begin SocketSubsystem Interface
	virtual bool Init(FString& Error) override;
	virtual void Shutdown() override;

	virtual class FSocket* CreateSocket(const FName& SocketType, const FString& SocketDescription, const FName& ProtocolType) override;
	virtual void DestroySocket(class FSocket* Socket) override;

	virtual FAddressInfoResult GetAddressInfo(const TCHAR* HostName, const TCHAR* ServiceName = nullptr,
		EAddressInfoFlags QueryFlags = EAddressInfoFlags::Default,
		const FName ProtocolTypeName = NAME_None,
		ESocketType SocketType = ESocketType::SOCKTYPE_Unknown) override;
	virtual TSharedPtr<FInternetAddr> GetAddressFromString(const FString& IPAddress) override;

	virtual bool GetHostName(FString& HostName) override;

	virtual TSharedRef<FInternetAddr> CreateInternetAddr() override;
	virtual TSharedRef<FInternetAddr> CreateInternetAddr(const FName RequestedProtocol) override;

	virtual const TCHAR* GetSocketAPIName() const override;

	virtual ESocketErrors GetLastErrorCode() override {	return (ESocketErrors)LastSocketError; }
	virtual ESocketErrors TranslateErrorCode(int32 Code) override {	return (ESocketErrors)Code;	}

	virtual bool GetLocalAdapterAddresses(TArray<TSharedPtr<FInternetAddr>>& OutAddresses) override;
	virtual TArray<TSharedRef<FInternetAddr>> GetLocalBindAddresses() override;
	virtual bool HasNetworkDevice() override { return true; }
	virtual bool IsSocketWaitSupported() const override { return false; }
	virtual bool RequiresChatDataBeSeparate() override { return false; }
	virtual bool RequiresEncryptedPackets() override { return false; }
	//~ End SocketSubsystem Interface

	//~ Begin FTickerObject Interface
	virtual bool Tick(float DeltaTime) override;
	//~ End FTickerObject Interface

	//~ Begin FSelfRegisteringExec Interface
	virtual bool Exec(class UWorld* InWorld, const TCHAR* Cmd, FOutputDevice& Ar) override;
	//~ End FSelfRegisteringExec Interface

	/** Returns if the application is using the SteamSocket relays */
	bool IsUsingRelayNetwork() const { return bUseRelays; }

	/** Basic function to determine if Steam has been initialized properly. */
	bool IsSteamInitialized() const { return SteamAPIClientHandle.IsValid() || SteamAPIServerHandle.IsValid(); }

	static class ISteamNetworkingSockets* GetSteamSocketsInterface();

PACKAGE_SCOPE:
	/** A struct for holding steam socket information and managing bookkeeping on the protocol. */
	struct FSteamSocketInformation
	{
		FSteamSocketInformation(TSharedPtr<FInternetAddr> InAddr, FSteamSocket* InSocket, FSteamSocket* InParent = nullptr) :
			Addr(InAddr),
			Socket(InSocket),
			Parent(InParent),
			NetDriver(nullptr),
			bMarkedForDeletion(false)
		{
		}

		void MarkForDeletion();

		bool IsMarkedForDeletion() const { return bMarkedForDeletion; }

		bool operator==(const FSteamSocket* RHS) const
		{
			return Socket == RHS;
		}

		bool operator==(const FInternetAddr& InAddr) const;

		bool operator==(const FSteamSocketInformation& RHS) const
		{
			return RHS.Addr == Addr && RHS.Socket == Socket;
		}

		bool IsValid() const
		{
			return Addr.IsValid() && Parent != nullptr;
		}

		FString ToString() const;

		TSharedPtr<FInternetAddr> Addr;
		FSteamSocket* Socket;
		// Sockets created from a listener have a parent
		FSteamSocket* Parent;
		// The NetDriver for this connection.
		TWeakObjectPtr<USteamSocketsNetDriver> NetDriver;
	private:
		bool bMarkedForDeletion;
	};

	// Steam socket queriers
	FSteamSocketInformation* GetSocketInfo(SteamSocketHandles InternalSocketHandle);
	FSteamSocketInformation* GetSocketInfo(const FInternetAddr& ForAddress);

	// Steam socket bookkeeping modifiers
	void AddSocket(const FInternetAddr& ForAddr, FSteamSocket* NewSocket, FSteamSocket* ParentSocket = nullptr);
	void RemoveSocketsForListener(FSteamSocket* ListenerSocket);
	void QueueRemoval(SteamSocketHandles SocketHandle);
	void LinkNetDriver(FSocket* Socket, USteamSocketsNetDriver* NewNetDriver);

	// Delayed listen socket helpers.
	void AddDelayedListener(FSteamSocket* ListenSocket, USteamSocketsNetDriver* NewNetDriver);
	void OnServerLoginComplete(bool bWasSuccessful);

	// Returns this machine's identity in the form of a FInternetAddrSteamSockets
	TSharedPtr<FInternetAddr> GetIdentityAddress();
	
	// Returns if our account is currently logged into the Steam network
	bool IsLoggedInToSteam() const;

	/** Last error set by the socket subsystem or one of its sockets */
	int32 LastSocketError;

	// Singleton helpers
	static FSteamSocketsSubsystem* Create();
	static void Destroy();

	// SteamAPI internals handler
	void SteamSocketEventHandler(struct SteamNetConnectionStatusChangedCallback_t* ConnectionEvent);

	/** Flag for testing peek messaging (only usable in non-shipping builds) */
	bool bShouldTestPeek;

protected:
	void CleanSocketInformation(bool bForceClean);
	void DumpSocketInformationMap() const;

	/** Single instantiation of this subsystem */
	static FSteamSocketsSubsystem* SocketSingleton;

	/** Event manager for Steam tasks */
	TUniquePtr<class FSteamSocketsTaskManagerInterface> SteamEventManager;

	/** Determines if the connections are going to be using the relay network */
	bool bUseRelays;

	/** Steam Client API Handle */
	TSharedPtr<class FSteamClientInstanceHandler> SteamAPIClientHandle;

	/** Steam Server API Handle */
	TSharedPtr<class FSteamServerInstanceHandler> SteamAPIServerHandle;

	/** Active connection bookkeeping */
	typedef TMap<SteamSocketHandles, FSteamSocketInformation> SocketHandleInfoMap;
	SocketHandleInfoMap SocketInformationMap;

	/** Structure for handling sockets that cannot be established due to platform login (for listener sockets) */
	struct FSteamPendingSocketInformation
	{
		FSteamSocket* Socket;
		TWeakObjectPtr<USteamSocketsNetDriver> NetDriver;

		FString ToString() const;
	};

	// Array of listeners we need to activate.
	TArray<FSteamPendingSocketInformation> PendingListenerArray;

	// Delegate handle for handling when a dedicated server logs into the Steam platform
	FDelegateHandle SteamServerLoginDelegateHandle;
};
