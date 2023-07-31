// Copyright Epic Games, Inc. All Rights Reserved.

#include "OnlineBeaconClient.h"
#include "TimerManager.h"
#include "GameFramework/OnlineReplStructs.h"
#include "OnlineBeaconHostObject.h"
#include "EngineGlobals.h"
#include "Engine/Engine.h"
#include "PacketHandlers/StatelessConnectHandlerComponent.h"
#include "Engine/PackageMapClient.h"
#include "Engine/LocalPlayer.h"
#include "Net/DataChannel.h"
#include "Misc/NetworkVersion.h"
#include "Interfaces/OnlineIdentityInterface.h"
#include "OnlineSubsystemUtils.h"
#include "Containers/StringFwd.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(OnlineBeaconClient)

#define BEACON_RPC_TIMEOUT 15.0f

/** For backwards compatibility with newer engine encryption API */
#ifndef NETCONNECTION_HAS_SETENCRYPTIONKEY
	#define NETCONNECTION_HAS_SETENCRYPTIONKEY 0
#endif

AOnlineBeaconClient::AOnlineBeaconClient(const FObjectInitializer& ObjectInitializer) :
	Super(ObjectInitializer),
	BeaconOwner(nullptr),
	BeaconConnection(nullptr),
	ConnectionState(EBeaconConnectionState::Invalid)
{
	NetDriverName = FName(TEXT("BeaconDriverClient"));
	bOnlyRelevantToOwner = true;

	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bAllowTickOnDedicatedServer = false;
	PrimaryActorTick.bStartWithTickEnabled = true;
}

FString AOnlineBeaconClient::GetBeaconType() const
{
	return GetClass()->GetName();
}

AOnlineBeaconHostObject* AOnlineBeaconClient::GetBeaconOwner() const
{
	return BeaconOwner;
}

void AOnlineBeaconClient::SetBeaconOwner(AOnlineBeaconHostObject* InBeaconOwner)
{
	BeaconOwner = InBeaconOwner;
}

const AActor* AOnlineBeaconClient::GetNetOwner() const
{
	return BeaconOwner;
}

class UNetConnection* AOnlineBeaconClient::GetNetConnection() const
{
	return BeaconConnection;
}

bool AOnlineBeaconClient::DestroyNetworkActorHandled()
{
	if (BeaconConnection && BeaconConnection->GetConnectionState() != USOCK_Closed)
	{
		// This will be cleaned up in ~2 sec by UNetConnection Tick
		BeaconConnection->bPendingDestroy = true;
		return true;
	}

	// The UNetConnection is gone or has been closed (NetDriver destroyed) and needs to go away now
	return false;
}

const FUniqueNetIdRepl& AOnlineBeaconClient::GetUniqueId() const
{
	if (BeaconConnection)
	{
		return BeaconConnection->PlayerId;
	}

	static FUniqueNetIdRepl EmptyId;
	return EmptyId;
}

EBeaconConnectionState AOnlineBeaconClient::GetConnectionState() const
{
	return ConnectionState;
}

void AOnlineBeaconClient::SetConnectionState(EBeaconConnectionState NewConnectionState)
{
	ConnectionState = NewConnectionState;
}

bool AOnlineBeaconClient::InitClient(FURL& URL)
{
	bool bSuccess = false;

	if(URL.Valid)
	{
		if (InitBase() && NetDriver)
		{
			FString Error;
			if (NetDriver->InitConnect(this, URL, Error))
			{
				check(NetDriver->ServerConnection);
				UWorld* World = GetWorld();
				if (World)
				{
					BeaconConnection = NetDriver->ServerConnection;

					if (IsRunningDedicatedServer())
					{
						IOnlineIdentityPtr IdentityPtr = Online::GetIdentityInterface(World);
						if (IdentityPtr.IsValid())
						{
							BeaconConnection->PlayerId = IdentityPtr->GetUniquePlayerId(DEDICATED_SERVER_USER_INDEX);
						}
					}
					else
					{
						ULocalPlayer* LocalPlayer = GEngine->GetFirstGamePlayer(World);
						if (LocalPlayer)
						{
							// Send the player unique Id at login
							BeaconConnection->PlayerId = LocalPlayer->GetPreferredUniqueNetId();
						}
						else
						{
							// Fall back to querying the identity interface.
							if (IOnlineIdentityPtr IdentityPtr = Online::GetIdentityInterface(World))
							{
								TArray<TSharedPtr<FUserOnlineAccount> > UserAccounts = IdentityPtr->GetAllUserAccounts();
								if (!UserAccounts.IsEmpty())
								{
									BeaconConnection->PlayerId = UserAccounts[0]->GetUserId();
								}
							}
						}
					}

#if NETCONNECTION_HAS_SETENCRYPTIONKEY
					if (!EncryptionData.Identifier.IsEmpty())
					{
						BeaconConnection->SetEncryptionData(EncryptionData);
					}
#endif

					SetConnectionState(EBeaconConnectionState::Pending);

					// Kick off the connection handshake
					bool bSentHandshake = false;

					if (BeaconConnection->Handler.IsValid())
					{
						BeaconConnection->Handler->BeginHandshaking(
							FPacketHandlerHandshakeComplete::CreateUObject(this, &AOnlineBeaconClient::SendInitialJoin));

						bSentHandshake = true;
					}

					if (NetDriver)
					{
						NetDriver->SetWorld(World);
						NetDriver->Notify = this;
						NetDriver->InitialConnectTimeout = BeaconConnectionInitialTimeout;
						NetDriver->ConnectionTimeout = BeaconConnectionTimeout;

						if (!bSentHandshake)
						{
							SendInitialJoin();
						}

						bSuccess = true;
					}
					else
					{
						// an error must have occurred during BeginHandshaking
						UE_LOG(LogBeacon, Warning, TEXT("AOnlineBeaconClient::InitClient BeginHandshaking failed"));

						// if the connection is still pending, notify of failure
						if (GetConnectionState() == EBeaconConnectionState::Pending)
						{
							SetConnectionState(EBeaconConnectionState::Invalid);
							OnFailure();
						}
					}
				}
				else
				{
					// error initializing the network stack, no world...
					UE_LOG(LogBeacon, Log, TEXT("AOnlineBeaconClient::InitClient failed - no world"));
					SetConnectionState(EBeaconConnectionState::Invalid);
					OnFailure();
				}
			}
			else
			{
				// error initializing the network stack...
				UE_LOG(LogBeacon, Log, TEXT("AOnlineBeaconClient::InitClient failed"));
				SetConnectionState(EBeaconConnectionState::Invalid);
				OnFailure();
			}
		}
	}

	return bSuccess;
}

void AOnlineBeaconClient::Tick(float DeltaTime)
{
	if (NetDriver != nullptr && NetDriver->ServerConnection != nullptr)
	{
		// Monitor for close bunches sent by the server which close down the connection in UChannel::Cleanup
		// See similar code in UWorld::TickNetClient
		if (((ConnectionState == EBeaconConnectionState::Pending) || (ConnectionState == EBeaconConnectionState::Open)) &&
			(NetDriver->ServerConnection->GetConnectionState() == USOCK_Closed))
		{
			UE_LOG(LogBeacon, Verbose, TEXT("Client beacon (%s) socket has closed, triggering failure."), *GetName());
			OnFailure();
		}
	}
}

void AOnlineBeaconClient::SetEncryptionToken(const FString& InEncryptionToken)
{
	EncryptionData.Identifier = InEncryptionToken;
}

void AOnlineBeaconClient::SetEncryptionKey(TArrayView<uint8> InEncryptionKey)
{
	if (CVarNetAllowEncryption.GetValueOnGameThread() != 0)
	{
		EncryptionData.Key.Reset(InEncryptionKey.Num());
		EncryptionData.Key.Append(InEncryptionKey.GetData(), InEncryptionKey.Num());
	}
}

void AOnlineBeaconClient::SetEncryptionData(const FEncryptionData& InEncryptionData)
{
	if (CVarNetAllowEncryption.GetValueOnGameThread() != 0)
	{
		EncryptionData = InEncryptionData;
	}
}

void AOnlineBeaconClient::SendInitialJoin()
{
	UNetConnection* ServerConn = NetDriver != nullptr ? NetDriver->ServerConnection : nullptr;

	if (ensure(ServerConn != nullptr))
	{
		uint8 IsLittleEndian = uint8(PLATFORM_LITTLE_ENDIAN);
		check(IsLittleEndian == !!IsLittleEndian); // should only be one or zero

		const int32 AllowEncryption = CVarNetAllowEncryption.GetValueOnGameThread();
		uint32 LocalNetworkVersion = FNetworkVersion::GetLocalNetworkVersion();

		if (AllowEncryption == 0)
		{
			EncryptionData.Identifier.Empty();
		}

		bool bEncryptionRequirementsFailure = false;

		if (EncryptionData.Identifier.IsEmpty())
		{
			EEncryptionFailureAction FailureResult = EEncryptionFailureAction::Default;

			if (FNetDelegates::OnReceivedNetworkEncryptionFailure.IsBound())
			{
				FailureResult = FNetDelegates::OnReceivedNetworkEncryptionFailure.Execute(ServerConn);
			}

			const bool bGameplayDisableEncryptionCheck = FailureResult == EEncryptionFailureAction::AllowConnection;

			bEncryptionRequirementsFailure = NetDriver->IsEncryptionRequired() && !bGameplayDisableEncryptionCheck;
		}

		if (!bEncryptionRequirementsFailure)
		{
			EEngineNetworkRuntimeFeatures LocalNetworkFeatures = NetDriver->GetNetworkRuntimeFeatures();
			FNetControlMessage<NMT_Hello>::Send(NetDriver->ServerConnection, IsLittleEndian, LocalNetworkVersion, EncryptionData.Identifier, LocalNetworkFeatures);

			ServerConn->FlushNet();
		}
		else if (GetConnectionState() == EBeaconConnectionState::Pending)
		{
			UE_LOG(LogNet, Error, TEXT("AOnlineBeaconClient::SendInitialJoin: EncryptionToken is empty when 'net.AllowEncryption' requires it."));

			SetConnectionState(EBeaconConnectionState::Invalid);
			OnFailure();
		}
	}
}

void AOnlineBeaconClient::OnFailure()
{
	UE_LOG(LogBeacon, Verbose, TEXT("Client beacon (%s) connection failure, handling connection timeout."), *GetName());
	SetConnectionState(EBeaconConnectionState::Invalid);
	HostConnectionFailure.ExecuteIfBound();
	Super::OnFailure();
}


/// @cond DOXYGEN_WARNINGS

void AOnlineBeaconClient::ClientOnConnected_Implementation()
{
	SetConnectionState(EBeaconConnectionState::Open);
	BeaconConnection->SetConnectionState(USOCK_Open);

	SetRole(ROLE_Authority);
	SetReplicates(true);
	SetAutonomousProxy(true);

	// Fail safe for connection to server but no client connection RPC
	GetWorldTimerManager().ClearTimer(TimerHandle_OnFailure);

	// Call the overloaded function for this client class
	OnConnected();
}

/// @endcond

bool AOnlineBeaconClient::UseShortConnectTimeout() const
{
	return ConnectionState == EBeaconConnectionState::Open;
}

void AOnlineBeaconClient::DestroyBeacon()
{
	SetConnectionState(EBeaconConnectionState::Closed);
	SetActorTickEnabled(false);

	UWorld* World = GetWorld();
	if (World)
	{
		// Fail safe for connection to server but no client connection RPC
		GetWorldTimerManager().ClearTimer(TimerHandle_OnFailure);
	}

	Super::DestroyBeacon();
}

void AOnlineBeaconClient::OnNetCleanup(UNetConnection* Connection)
{
	// During the garbage collection of UNetConnection OnNetCleanup may be triggered.
	// When this happens BeaconConnection will have already been set to nullptr by GC.
	ensure(NetDriver == nullptr || Connection == BeaconConnection);
	SetConnectionState(EBeaconConnectionState::Closed);

	AOnlineBeaconHostObject* BeaconHostObject = GetBeaconOwner();
	if (BeaconHostObject)
	{
		BeaconHostObject->NotifyClientDisconnected(this);
	}

	BeaconConnection = nullptr;
	Destroy(true);
}

void AOnlineBeaconClient::NotifyControlMessage(UNetConnection* Connection, uint8 MessageType, class FInBunch& Bunch)
{
	if(NetDriver->ServerConnection)
	{
		check(Connection == NetDriver->ServerConnection);

		// We are the client
#if !(UE_BUILD_SHIPPING && WITH_EDITOR)
		UE_LOG(LogBeacon, Log, TEXT("%s[%s] Client received: %s"), *GetName(), *Connection->GetName(), FNetControlMessageInfo::GetName(MessageType));
#endif
		switch (MessageType)
		{
		case NMT_EncryptionAck:
			{
				// Enable encryption using the beacons encryption data when set.
				if (!EncryptionData.Identifier.IsEmpty())
				{
					FEncryptionKeyResponse Response;
					Response.Response = EEncryptionResponse::Success;
					Response.EncryptionData = EncryptionData;
					FinalizeEncryptedConnection(Response, Connection);
				}
				else
				{
					if (FNetDelegates::OnReceivedNetworkEncryptionAck.IsBound())
					{
						TWeakObjectPtr<UNetConnection> WeakConnection = Connection;
						FNetDelegates::OnReceivedNetworkEncryptionAck.Execute(FOnEncryptionKeyResponse::CreateUObject(this, &ThisClass::FinalizeEncryptedConnection, WeakConnection));
					}
					else
					{
						// Force close the session
						UE_LOG(LogBeacon, Warning, TEXT("%s: No delegate available to handle encryption ack, disconnecting."), *Connection->GetName());
						OnFailure();
					}
				}
				break;
			}
		case NMT_Challenge:
		{
			// Challenged by server.
			if (FNetControlMessage<NMT_Challenge>::Receive(Bunch, Connection->Challenge))
			{
				// build a URL
				FURL URL(nullptr, TEXT(""), TRAVEL_Absolute);
				FString URLString;

				// Append authentication token to URL options
				IOnlineIdentityPtr IdentityPtr = Online::GetIdentityInterface(GetWorld());
				if (IdentityPtr.IsValid())
				{
					TSharedPtr<FUserOnlineAccount> UserAcct = IdentityPtr->GetUserAccount(*Connection->PlayerId);
					if (UserAcct.IsValid())
					{
						URL.AddOption(*FString::Printf(TEXT("AuthTicket=%s"), *UserAcct->GetAccessToken()));
					}
				}
				URLString = URL.ToString();

				// compute the player's online platform name
				FName OnlinePlatformName = NAME_None;
				if (const FWorldContext* const WorldContext = GEngine->GetWorldContextFromWorld(GetWorld()))
				{
					if (WorldContext->OwningGameInstance)
					{
						OnlinePlatformName = WorldContext->OwningGameInstance->GetOnlinePlatformName();
					}
				}
				FString OnlinePlatformNameString = OnlinePlatformName.ToString();

				// send NMT_Login
				Connection->ClientResponse = TEXT("0");
				FNetControlMessage<NMT_Login>::Send(Connection, Connection->ClientResponse, URLString, Connection->PlayerId, OnlinePlatformNameString);
				NetDriver->ServerConnection->FlushNet();
			}
			else
			{
				// Force close the session
				UE_LOG(LogBeacon, Warning, TEXT("%s: Unable to parse challenge request message."), *Connection->GetName());
				OnFailure();
			}
			break;
		}
		case NMT_BeaconWelcome:
			{
				Connection->ClientResponse = TEXT("0");
				FNetControlMessage<NMT_Netspeed>::Send(Connection, Connection->CurrentNetSpeed);

				FString BeaconType = GetBeaconType();
				if (!BeaconType.IsEmpty())
				{
					FNetControlMessage<NMT_BeaconJoin>::Send(Connection, BeaconType, Connection->PlayerId);
					NetDriver->ServerConnection->FlushNet();
				}
				else
				{
					// Force close the session
					UE_LOG(LogBeacon, Log, TEXT("Beacon close from invalid beacon type"));
					OnFailure();
				}
				break;
			}
		case NMT_BeaconAssignGUID:
			{
				FNetworkGUID NetGUID;

				if (FNetControlMessage<NMT_BeaconAssignGUID>::Receive(Bunch, NetGUID))
				{
					if (NetGUID.IsValid())
					{
						Connection->Driver->GuidCache->RegisterNetGUID_Client(NetGUID, this);

						FString BeaconType = GetBeaconType();
						FNetControlMessage<NMT_BeaconNetGUIDAck>::Send(Connection, BeaconType);
						// Server will send ClientOnConnected() when it gets this control message

						// Fail safe for connection to server but no client connection RPC
						FTimerDelegate TimerDelegate = FTimerDelegate::CreateUObject(this, &AOnlineBeaconClient::OnFailure);
						GetWorldTimerManager().SetTimer(TimerHandle_OnFailure, TimerDelegate, BEACON_RPC_TIMEOUT, false);
					}
					else
					{
						// Force close the session
						UE_LOG(LogBeacon, Log, TEXT("Beacon close from invalid NetGUID"));
						OnFailure();
					}
				}

				break;
			}
		case NMT_Upgrade:
			{
				// Report mismatch.
				uint32 RemoteNetworkVersion;
				EEngineNetworkRuntimeFeatures RemoteNetworkFeatures = EEngineNetworkRuntimeFeatures::None;

				if (FNetControlMessage<NMT_Upgrade>::Receive(Bunch, RemoteNetworkVersion, RemoteNetworkFeatures))
				{
					TStringBuilder<128> RemoteFeaturesDescription;
					FNetworkVersion::DescribeNetworkRuntimeFeaturesBitset(RemoteNetworkFeatures, RemoteFeaturesDescription);

					TStringBuilder<128> LocalFeaturesDescription;
					FNetworkVersion::DescribeNetworkRuntimeFeaturesBitset(NetDriver->GetNetworkRuntimeFeatures(), LocalFeaturesDescription);

					UE_LOG(LogBeacon, Error, TEXT("Beacon is incompatible with the local version of the game: RemoteNetworkVersion=%u, RemoteNetworkFeatures=%s vs LocalNetworkVersion=%u, LocalNetworkFeatures=%s"), 
						RemoteNetworkVersion, RemoteFeaturesDescription.ToString(),
						FNetworkVersion::GetLocalNetworkVersion(), LocalFeaturesDescription.ToString()
					);
				
					// Upgrade
					const FString ConnectionError = NSLOCTEXT("Engine", "ClientOutdated",
						"The match you are trying to join is running an incompatible version of the game.  Please try upgrading your game version.").ToString();

					GEngine->BroadcastNetworkFailure(GetWorld(), NetDriver, ENetworkFailure::OutdatedClient, ConnectionError);
				}

				break;
			}
		case NMT_Failure:
			{
				FString ErrorMsg;

				if (FNetControlMessage<NMT_Failure>::Receive(Bunch, ErrorMsg))
				{
					if (ErrorMsg.IsEmpty())
					{
						ErrorMsg = NSLOCTEXT("NetworkErrors", "GenericBeaconConnectionFailed", "Beacon Connection Failed.").ToString();
					}

					// Force close the session
					UE_LOG(LogBeacon, Log, TEXT("Beacon close from NMT_Failure %s"), *ErrorMsg);
					OnFailure();
				}

				break;
			}
		case NMT_BeaconJoin:
		case NMT_BeaconNetGUIDAck:
		default:
			{
				// Force close the session
				UE_LOG(LogBeacon, Log, TEXT("Beacon close from unexpected control message"));
				OnFailure();
				break;
			}
		}
	}	
}

void AOnlineBeaconClient::FinalizeEncryptedConnection(const FEncryptionKeyResponse& Response, TWeakObjectPtr<UNetConnection> WeakConnection)
{
	UNetConnection* Connection = WeakConnection.Get();
	if (Connection)
	{
		if (Connection->GetConnectionState() != USOCK_Invalid && Connection->GetConnectionState() != USOCK_Closed && Connection->Driver)
		{
			if (Response.Response == EEncryptionResponse::Success)
			{
				Connection->EnableEncryption(Response.EncryptionData);
			}
			else
			{
				FString ResponseStr(LexToString(Response.Response));
				UE_LOG(LogBeacon, Warning, TEXT("AOnlineBeaconClient::FinalizeEncryptedConnection: encryption failure [%s] %s"), *ResponseStr, *Response.ErrorMsg);
				OnFailure();
			}
		}
		else
		{
			UE_LOG(LogBeacon, Warning, TEXT("AOnlineBeaconClient::FinalizeEncryptedConnection: connection in invalid state. %s"), *Connection->Describe());
			OnFailure();
		}
	}
	else
	{
		UE_LOG(LogBeacon, Warning, TEXT("AOnlineBeaconClient::FinalizeEncryptedConnection: Connection is null."));
		OnFailure();
	}
}

