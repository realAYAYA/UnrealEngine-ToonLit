// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

// @todo #JohnB: This file is missing a high-level description of what MinimalClient's are, and their purpose

// @todo #JohnB: Refactor
/**
 * Refactoring Plan
 *
 * There are still more steps that need to be done, for refactoring the rest of the minimal client features over from ClientUnitTest,
 * and then expanding ClientUnitTest to work with multiple minimal clients - many steps would be useful for the Fortnite MCP support.
 *
 * The plan is to, eventually, completely eliminate nearly all of the MinimalClient delegates that tie back to ClientUnitTest,
 * and to have minimal client functionality implemented through virtual functions in MinimalClient subclasses instead,
 * and then to expand ClientUnitTest to support multiple/unlimited MinimalClient's, with two default beacon/game minimal clients.
 *
 * This will require changing all unit tests (notably, having a new MinimalClient subclass for every unit test, implementing the test),
 * and has a lot of stages that still need doing/planning; it is taking up too much time though,
 * so you will have to implement it piecemeal, and will have to make-do with an imperfect implementation of the Fortnite MCP support,
 * through a UnitTask - implementing that with the final refactored system, later.
 *
 *
 * Required steps:
 *		- Step 0: Not directly related - but implement Fortnite MCP support before any of the below.
 *
 *		- Step 1: Move last of EUnitTestFlags, into MinimalClient
 *			- Possibly, you may end up doing this in a MinimalClient subclass, or moving existing class down to MinimalClientBase,
 *				but this complicates the delegate parameters (which will be removed later anyway, so maybe not problem...),
 *				and EMinClientFlags (flags would define functionality across both base and superclass)
 *
 *			- Specifically, the requirements flags and advanced implementation of notifications, may be more suitable for a subclass,
 *				as it would be nice to keep the barebones connection code within the base class
 *
 *			- For unit-test specific events that are tied up in the middle of the moved code
 *				(like firing off the unit test after conditions are correctly set), you'll have to add new delegates
 *
 *
 *		- Step 2: Implement a virtual function interface in MinimalClient, matching that in ClientUnitTest 
 *			- Keep the delegates around for the moment, just tie them into the virtual interface for now
 *
 *			- All new unit tests should use this interface, instead of the old ClientUnitTest interface
 *
 *
 *		- Step 3: Convert all unit tests to the new interface, and remove the delegate interface when done
 *			- Do this piecemeal, as it is a big job, touching all unit tests
 *
 *
 *		- Step 4: Implement support for multiple/unlimited minimal clients, in ClientUnitTest,
 *			and support for the two default beacon/game minimal clients
 *
 *
 *		- Step 5: Refactor the Fortnite MCP support, into a MinimalClient subclass (for the beacon connection),
 *			perhaps keeping the MCP side within the UnitTask
 */

// Includes
#include "UObject/ObjectPtr.h"
#include "Engine/NetConnection.h"
#include "Engine/PendingNetGame.h"

#include "Net/NUTUtilNet.h"
#include "NUTEnum.h"
#include "UnitLogging.h"

#include "MinimalClient.generated.h"


// Forward declarations
class FFuncReflection;
class UMinimalClient;
class UUnitTestNetConnection;
class UUnitTestChannel;
class UUnitTestActorChannel;
class UUnitTestPackageMap;


// Delegates

/**
 * Delegate for marking the minimal client as having connected fully
 */
DECLARE_DELEGATE(FOnMinClientConnected);

/**
 * Delegate for passing back a network connection failure
 *
 * @param FailureType	The reason for the net failure
 * @param ErrorString	More detailed error information
 */
DECLARE_DELEGATE_TwoParams(FOnMinClientNetworkFailure, ENetworkFailure::Type /*FailureType*/, const FString& /*ErrorString*/);

/**
 * Delegate for hooking the control channel's 'ReceivedBunch' call
 *
 * @param Bunch		The received bunch
 */
DECLARE_DELEGATE_OneParam(FOnMinClientReceivedControlBunch, FInBunch&/* Bunch*/);


/**
 * Delegate for hooking the net connections 'ReceivedRawPacket'
 * 
 * @param Data		The data received
 * @param Count		The number of bytes received
 */
DECLARE_DELEGATE_TwoParams(FOnMinClientReceivedRawPacket, void* /*Data*/, int32& /*Count*/);

/**
 * Delegate for notifying on (and optionally blocking) replicated actor creation
 *
 * @param ActorClass	The class of the actor being replicated
 * @param bActorChannel	Whether or not this actor creation is from an actor channel
 * @param bBlockActor	Whether or not to block creation of the actor (defaults to true)
 */
DECLARE_DELEGATE_ThreeParams(FOnMinClientRepActorSpawn, UClass* /*ActorClass*/, bool /*bActorChannel*/, bool& /*bBlockActor*/);

/**
 * Delegate for notifying AFTER an actor channel actor has been created
 *
 * @param ActorChannel	The actor channel associated with the actor
 * @param Actor			The actor that has just been created
 */
DECLARE_DELEGATE_TwoParams(FOnMinClientNetActor, UActorChannel* /*ActorChannel*/, AActor* /*Actor*/);

/**
 * Delegate for hooking the HandlerClientPlayer event
 *
 * @param PC			The PlayerController being initialized with the net connection
 * @param Connection	The net connection the player is being initialized with
 */
DECLARE_DELEGATE_TwoParams(FOnHandleClientPlayer, APlayerController* /*PC*/, UNetConnection* /*Connection*/);

/**
 * Delegate for notifying of failure during call of SendRPCChecked
 */
DECLARE_DELEGATE(FOnRPCFailure);


/**
 * Parameters for configuring the minimal client - also directly inherited by UMinimalClient
 */
struct FMinClientParms
{
	friend UMinimalClient;


	/** The flags used for configuring the minimal client */
	EMinClientFlags MinClientFlags;

	/** The address of the launched server */
	FString ServerAddress;

	/** The address of the server beacon (if MinClientFlags are set to connect to a beacon) */
	FString BeaconAddress;

	/** The URL options to connect with */
	FString URLOptions;

	/** If connecting to a beacon, the beacon type name we are connecting to */
	FString BeaconType;

	/** If overriding the UID used for joining, this specifies it */
	FString JoinUID;

	/** Clientside RPC's that should be allowed to execute (requires the NotifyProcessNetEvent flag) */
	TArray<FString> AllowedClientRPCs;

	/** The amount of time (in seconds) before the connection should timeout */
	uint32 Timeout;

	/** The net driver class to use (defaults to IpNetDriver, if not specified) */
	FString NetDriverClass;


	/**
	 * Default constructor
	 */
	FMinClientParms()
		: MinClientFlags(EMinClientFlags::None)
		, ServerAddress()
		, BeaconAddress()
		, URLOptions()
		, BeaconType()
		, JoinUID(TEXT("Dud"))
		, Timeout(30)
		, NetDriverClass(TEXT(""))
	{
	}

protected:
	/**
	 * Verify that the parameters specified to this struct are valid
	 */
	void ValidateParms();

	void CopyParms(FMinClientParms& Target)
	{
		Target.MinClientFlags = MinClientFlags;
		Target.ServerAddress = ServerAddress;
		Target.BeaconAddress = BeaconAddress;
		Target.URLOptions = URLOptions;
		Target.BeaconType = BeaconType;
		Target.JoinUID = JoinUID;
		Target.AllowedClientRPCs = AllowedClientRPCs;
		Target.Timeout = Timeout;
		Target.NetDriverClass = NetDriverClass;
	}
};


/**
 * Delegate hooks for the minimal client - also directly inherited by UMinimalClient
 */
struct FMinClientHooks
{
	friend UMinimalClient;


	/** Delegate for notifying of successful minimal client connection*/
	FOnMinClientConnected ConnectedDel;

	/** Delegate notifying of network failure */
	FOnMinClientNetworkFailure NetworkFailureDel;

	/** Delegate for notifying/controlling RPC receives */
	FOnProcessNetEvent ReceiveRPCDel;

#if !UE_BUILD_SHIPPING
	/** Delegate for notifying/controlling RPC sends */
	FOnSendRPC SendRPCDel;
#endif

	/** Delegate for notifying of control channel bunches */
	FOnMinClientReceivedControlBunch ReceivedControlBunchDel;

	/** Delegate for notifying of net connection raw packet receives */
	FOnMinClientReceivedRawPacket ReceivedRawPacketDel;

#if !UE_BUILD_SHIPPING
	/** Delegate for notifying of net connection low level packet sends */
	FOnLowLevelSend LowLevelSendDel;
#endif

	/** Delegate for notifying/controlling replicated actor spawning */
	FOnMinClientRepActorSpawn RepActorSpawnDel;

	/** Delegate for notifying AFTER net actor creation */
	FOnMinClientNetActor NetActorDel;

	/** Delegate for notifying of the net connection HandlerClientPlayer event */
	FOnHandleClientPlayer HandleClientPlayerDel;

	/** Delegate for notifying of failure during call of SendRPCChecked */
	FOnRPCFailure RPCFailureDel;


	/**
	 * Default constructor
	 */
	FMinClientHooks()
		: ConnectedDel()
		, NetworkFailureDel()
		, ReceiveRPCDel()
#if !UE_BUILD_SHIPPING
		, SendRPCDel()
#endif
		, ReceivedControlBunchDel()
		, ReceivedRawPacketDel()
#if !UE_BUILD_SHIPPING
		, LowLevelSendDel()
#endif
		, RepActorSpawnDel()
		, NetActorDel()
		, HandleClientPlayerDel()
		, RPCFailureDel()
	{
	}

protected:
	void CopyHooks(FMinClientHooks& Target)
	{
		Target.ConnectedDel = ConnectedDel;
		Target.NetworkFailureDel = NetworkFailureDel;
		Target.ReceiveRPCDel = ReceiveRPCDel;
#if !UE_BUILD_SHIPPING
		Target.SendRPCDel = SendRPCDel;
#endif
		Target.ReceivedControlBunchDel = ReceivedControlBunchDel;
		Target.ReceivedRawPacketDel = ReceivedRawPacketDel;
#if !UE_BUILD_SHIPPING
		Target.LowLevelSendDel = LowLevelSendDel;
#endif
		Target.RepActorSpawnDel = RepActorSpawnDel;
		Target.NetActorDel = NetActorDel;
		Target.HandleClientPlayerDel = HandleClientPlayerDel;
		Target.RPCFailureDel = RPCFailureDel;
	}
};

// Sidestep header parser enforcement of 'public' on inheritance below
struct FProtMinClientParms : protected FMinClientParms {};
struct FProtMinClientHooks : protected FMinClientHooks {};


/**
 * Base class for implementing a barebones/stripped-down game client, capable of connecting to a regular game server,
 * but stripped/locked-down so that the absolute minimum of client/server netcode functionality is executed, for connecting the client.
 */
UCLASS()
class NETCODEUNITTEST_API UMinimalClient :
	public UObject, public FNetworkNotify, public FProtMinClientParms, public FProtMinClientHooks, public FUnitLogRedirect
{
	friend UUnitTestNetConnection;
	friend UUnitTestChannel;
	friend UUnitTestActorChannel;
	friend UUnitTestPackageMap;

	GENERATED_UCLASS_BODY()

public:
	/**
	 * Connects the minimal client to a server, with Parms specifying the server details and minimal client configuration,
	 * and passing back the low level netcode events specified by Hooks.
	 *
	 * @param Parms		The server parameters and minimal client configuration.
	 * @param Hooks		The delegates for hooking low level netcode events in the minimal client.
	 * @return			Whether or not the connection kicked off successfully.
	 */
	bool Connect(FMinClientParms Parms, FMinClientHooks Hooks);

	/**
	 * Disconnects and cleans up the minimal client.
	 */
	void Cleanup();


	/**
	 * Creates a bunch for the specified channel, with the ability to create the channel as well
	 * WARNING: Can return nullptr! (e.g. if the control channel is saturated)
	 *
	 * @param ChType			Specify the type of channel the bunch will be sent on
	 * @param ChIndex			Specify the index of the channel to send the bunch on (otherwise, picks the next free/unused channel)
	 * @return					Returns the created bunch, or nullptr if it couldn't be created
	 */
	UE_DEPRECATED(4.22, "Use CreateChannelBunchByName")
	FOutBunch* CreateChannelBunch(EChannelType ChType, int32 ChIndex=INDEX_NONE);

	/**
	 * Creates a bunch for the specified channel, with the ability to create the channel as well
	 * WARNING: Can return nullptr! (e.g. if the control channel is saturated)
	 *
	 * @param ChName			Specify the type of channel the bunch will be sent on
	 * @param ChIndex			Specify the index of the channel to send the bunch on (otherwise, picks the next free/unused channel)
	 * @return					Returns the created bunch, or nullptr if it couldn't be created
	 */
	FOutBunch* CreateChannelBunchByName(const FName& ChName, int32 ChIndex=INDEX_NONE);

	/**
	 * Discards a created bunch, without sending it - don't use this for sent packets
	 *
	 * @param Bunch		The bunch to discard
	 */
	void DiscardChannelBunch(FOutBunch* Bunch);

	/**
	 * Sends a bunch over the control channel
	 *
	 * @param ControlChanBunch	The bunch to send over the control channel
	 * @return					Whether or not the bunch was sent successfully
	 */
	bool SendControlBunch(FOutBunch* ControlChanBunch);

	/**
	 * Sends a raw packet bunch, directly to the NetConnection, with the option of automatically  splitting into partial bunches.
	 *
	 * @param Bunch				The bunch to send
	 * @param bAllowPartial		Whether to allow automatic splitting of the packet into partial packets, if it exceeds the maximum size
	 * @return					Whether or not the bunch was sent successfully
	 */
	bool SendRawBunch(FOutBunch& Bunch, bool bAllowPartial=false);


	// @todo #JohnB: Time to deprecate ParmsSize/ParmsSizeCorrection, and force reliance on reflection?

	/**
	 * Sends the specified RPC for the specified actor, and verifies that the RPC was sent (triggering RPCFailureDel if not)
	 *
	 * @param Target				The Actor or ActorComponent which will send the RPC
	 * @param FunctionName			The name of the RPC
	 * @param Parms					The RPC parameters (same as would be specified to ProcessEvent)
	 * @param ParmsSize				The size of the RPC parameters, for verifying binary compatibility
	 * @param ParmsSizeCorrection	Some parameters are compressed to a different size. Verify Parms matches, and use this to correct.
	 * @return						Whether or not the RPC was sent successfully
	 */
	bool SendRPCChecked(UObject* Target, const TCHAR* FunctionName, void* Parms, int16 ParmsSize, int16 ParmsSizeCorrection=0);

	/**
	 * As above, except optimized for use with reflection
	 *
	 * @param Target	The Actor or ActorComponent which will send the RPC
	 * @param FuncRefl	The function reflection instance, containing the function to be called and its assigned parameters.
	 * @return			Whether or not the RPC was sent successfully
	 */
	bool SendRPCChecked(UObject* Target, FFuncReflection& FuncRefl);


	/**
	 * Internal function, for preparing for a checked RPC call
	 */
	void PreSendRPC();

	/**
	 * Internal function, for handling the aftermath of a checked RPC call
	 *
	 * @param RPCName	The name of the RPC, for logging purposes
	 * @param Target	The Actor or ActorComponent the RPC is being called on (optional - for internal logging/debugging)
	 * @return			Whether or not the RPC was sent successfully
	 */
	bool PostSendRPC(FString RPCName, UObject* Target=nullptr);


	/**
	 * Resets the net connection timeout
	 *
	 * @param Duration	The duration which the timeout reset should last
	 */
	void ResetConnTimeout(float Duration);


	// Accessors

	/**
	 * Retrieve the value of MinClientFlags
	 */
	FORCEINLINE EMinClientFlags GetMinClientFlags()
	{
		return MinClientFlags;
	}

	/**
	 * Retrieve the value of UnitWorld
	 */
	FORCEINLINE UWorld* GetUnitWorld()
	{
		return UnitWorld;
	}

	/**
	 * Retrieve the value of UnitConn
	 */
	FORCEINLINE UNetConnection* GetConn()
	{
		return UnitConn;
	}

	/**
	 * Whether or not the minimal client is connected to the server
	 */
	FORCEINLINE bool IsConnected()
	{
		return bConnected;
	}


	/**
	 * Ticks the minimal client, through the owner unit tests UnitTick function
	 */
	void UnitTick(float DeltaTime);

	/**
	 * Whether or not the minimal client requires ticking.
	 */
	bool IsTickable() const;


protected:
	/**
	 * Creates the minimal client state, and connects to the server
	 *
	 * @return	Whether or not the connection was successfully kicked off
	 */
	bool ConnectMinimalClient();

	/**
	 * Creates a net driver for the minimal client
	 */
	void CreateNetDriver();

	/**
	 * Sends the packet for triggering the initial join (usually is delayed, by the PacketHandler)
	 */
	void SendInitialJoin();

	/**
	 * Writes the NMT_Login message, as a part of SendInitialJoin (separated to make it overridable)
	 */
	virtual void WriteControlLogin(FOutBunch* ControlChanBunch);


	/**
	 * Disconnects the minimal client - including destructing the net driver and such (tears down the whole connection)
	 * NOTE: Based upon the HandleDisconnect function, except removing parts that are undesired (e.g. which trigger level switch)
	 */
	void DisconnectMinimalClient();

	/**
	 * See FOnLowLevelSend
	 */
	void NotifySocketSend(void* Data, int32 Count, bool& bBlockSend);

	/**
	 * See FOnReceivedRawPacket
	 */
	void NotifyReceivedRawPacket(void* Data, int32 Count, bool& bBlockReceive);

	/**
	 * See FOnProcessNetEvent
	 */
	void NotifyReceiveRPC(AActor* Actor, UFunction* Function, void* Parameters, bool& bBlockRPC);


	/**
	 * See FOnSendRPC
	 */
	void NotifySendRPC(AActor* Actor, UFunction* Function, void* Parameters, FOutParmRec* OutParms, FFrame* Stack, UObject* SubObject,
						bool& bBlockSendRPC);


protected:
	void InternalNotifyNetworkFailure(UWorld* InWorld, UNetDriver* InNetDriver, ENetworkFailure::Type FailureType,
										const FString& ErrorString);



	// FNetworkNotify
protected:
	virtual EAcceptConnection::Type NotifyAcceptingConnection() override
	{
		return EAcceptConnection::Ignore;
	}

	virtual void NotifyAcceptedConnection(UNetConnection* Connection) override
	{
	}

	virtual bool NotifyAcceptingChannel(UChannel* Channel) override;

	virtual void NotifyControlMessage(UNetConnection* Connection, uint8 MessageType, FInBunch& Bunch) override
	{
	}


	/** Runtime variables */
protected:
	/** Whether or not the minimal client is connected */
	bool bConnected;

	/** Stores a reference to the created empty world, for execution and later cleanup */
	UPROPERTY()
	TObjectPtr<UWorld> UnitWorld;

	/** Stores a reference to the created unit test net driver, for execution and later cleanup */
	UPROPERTY()
	TObjectPtr<UNetDriver> UnitNetDriver;

	/** Stores a reference to the server connection */
	UPROPERTY()
	TObjectPtr<UNetConnection> UnitConn;

	/** If notifying of net actor creation, this keeps track of new actor channel indexes pending notification */
	TArray<int32> PendingNetActorChans;

	/** Whether or not a bunch was successfully sent */
	bool bSentBunch;

private:
	/** Handle to the registered InternalNotifyNetworkFailure delegate */
	FDelegateHandle InternalNotifyNetworkFailureDelegateHandle;
};
