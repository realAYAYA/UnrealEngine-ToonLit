// Copyright Epic Games, Inc. All Rights Reserved.

#include "MinimalClient.h"

#include "Misc/FeedbackContext.h"
#include "Misc/NetworkVersion.h"
#include "Engine/Engine.h"
#include "Engine/GameEngine.h"
#include "Engine/ActorChannel.h"
#include "Net/DataChannel.h"

#include "UnitTest.h"
#include "NetcodeUnitTest.h"
#include "UnitTestEnvironment.h"
#include "UnitTestManager.h"
#include "NUTGlobals.h"
#include "NUTUtilDebug.h"
#include "NUTUtilReflection.h"
#include "Net/NUTUtilNet.h"
#include "Net/UnitTestPackageMap.h"
#include "Net/UnitTestChannel.h"
#include "Net/UnitTestActorChannel.h"


// @todo #JohnB: Eventually, you want to remove this classes dependence on the unit test code, and have it be independent.
//					For now though, just allow the dependence, to ease the transition


/**
 * FMinClientParms
 */
void FMinClientParms::ValidateParms()
{
	ValidateMinFlags(MinClientFlags);


	// Validate the rest of the flags which cross-check against non-flag variables, or otherwise should be runtime-only checks

	// You can't use an allow list for client RPC's (i.e. unblock allow listed RPC's), unless all RPC's are blocked by default
	UNIT_ASSERT(!(MinClientFlags & EMinClientFlags::AcceptRPCs) || AllowedClientRPCs.Num() == 0);

#if UE_BUILD_SHIPPING
	// Rejecting actors requires non-shipping mode
	UNIT_ASSERT(!!(MinClientFlags & EMinClientFlags::AcceptActors));
#endif
}


/**
 * UMinimalClient
 */

UMinimalClient::UMinimalClient(const FObjectInitializer& ObjectInitializor)
	: Super(ObjectInitializor)
	, FProtMinClientParms()
	, FProtMinClientHooks()
	, FUnitLogRedirect()
	, bConnected(false)
	, UnitWorld(nullptr)
	, UnitNetDriver(nullptr)
	, UnitConn(nullptr)
	, PendingNetActorChans()
	, bSentBunch(false)
	, InternalNotifyNetworkFailureDelegateHandle()
{
}

bool UMinimalClient::Connect(FMinClientParms Parms, FMinClientHooks Hooks)
{
	bool bSuccess = false;

	// Make sure we're not already setup
	check(Hooks.ConnectedDel.IsBound());
	check(MinClientFlags == EMinClientFlags::None && !ConnectedDel.IsBound());

	Parms.ValidateParms();

	Parms.CopyParms(*this);
	Hooks.CopyHooks(*this);

	if (UnitWorld == nullptr)
	{
		// Make all of this happen in a blank, newly constructed world
		UnitWorld = NUTNet::CreateUnitTestWorld();
	}

	if (UnitWorld != nullptr)
	{
		if (ConnectMinimalClient())
		{
			if (GEngine != nullptr)
			{
				InternalNotifyNetworkFailureDelegateHandle = GEngine->OnNetworkFailure().AddUObject(this,
																&UMinimalClient::InternalNotifyNetworkFailure);
			}

			if (!(MinClientFlags & EMinClientFlags::AcceptRPCs) || !!(MinClientFlags & EMinClientFlags::NotifyProcessNetEvent))
			{
				FProcessEventHook::Get().AddRPCHook(UnitWorld,
					FOnProcessNetEvent::CreateUObject(this, &UMinimalClient::NotifyReceiveRPC));
			}

			bSuccess = true;
		}
		else
		{
			FString LogMsg = TEXT("Failed to create minimal client connection");

			UNIT_LOG(ELogType::StatusFailure, TEXT("%s"), *LogMsg);
			UNIT_STATUS_LOG(ELogType::StatusVerbose, TEXT("%s"), *LogMsg);
		}
	}
	else
	{
		FString LogMsg = TEXT("Failed to create unit test world");

		UNIT_LOG(ELogType::StatusFailure, TEXT("%s"), *LogMsg);
		UNIT_STATUS_LOG(ELogType::StatusVerbose, TEXT("%s"), *LogMsg);
	}

	return bSuccess;
}

void UMinimalClient::Cleanup()
{
	if (UnitConn != nullptr)
	{
		UnitConn->Close();
	}

	DisconnectMinimalClient();

	if (UnitNetDriver != nullptr)
	{
		UnitNetDriver->Notify = nullptr;
	}

	UnitNetDriver = nullptr;
	UnitConn = nullptr;

	PendingNetActorChans.Empty();

	if (GEngine != nullptr)
	{
		GEngine->OnNetworkFailure().Remove(InternalNotifyNetworkFailureDelegateHandle);
	}

	// Immediately cleanup (or rather, at start of next tick, as that's earliest possible time) after sending the RPC
	if (UnitWorld != nullptr)
	{
		FProcessEventHook::Get().RemoveRPCHook(UnitWorld);

		NUTNet::MarkUnitTestWorldForCleanup(UnitWorld);
		UnitWorld = nullptr;
	}
}

FOutBunch* UMinimalClient::CreateChannelBunch(EChannelType ChType, int32 ChIndex/*=INDEX_NONE*/)
{
	FName ChName = NAME_None;

	switch (ChType)
	{
	case CHTYPE_Control:
		ChName = NAME_Control;
		break;
	case CHTYPE_Actor:
		ChName = NAME_Actor;
		break;
	case CHTYPE_Voice:
		ChName = NAME_Voice;
		break;
	}

	return CreateChannelBunchByName(ChName, ChIndex);
}

FOutBunch* UMinimalClient::CreateChannelBunchByName(const FName& ChName, int32 ChIndex/*=INDEX_NONE*/)
{
	FOutBunch* ReturnVal = nullptr;
	UChannel* ControlChan = UnitConn != nullptr ? UnitConn->Channels[0] : nullptr;

	if (ControlChan != nullptr)
	{
		if (ChIndex == INDEX_NONE)
		{
			for (ChIndex=0; ChIndex < UnitConn->Channels.Num(); ChIndex++)
			{
				if (UnitConn->Channels[ChIndex] == nullptr)
				{
					break;
				}
			}

			check(UnitConn->Channels[ChIndex] == nullptr);
		}

		// Hack to force IsNetReady
		UnitConn->QueuedBits = -(MAX_PACKET_SIZE * 8);

		if (ControlChan != nullptr && ControlChan->IsNetReady(false))
		{
			int32 BunchSequence = ++UnitConn->OutReliable[ChIndex];

			ReturnVal = new FOutBunch(ControlChan, false);

			ReturnVal->Next = nullptr;
			ReturnVal->Time = 0.0;
			ReturnVal->ReceivedAck = false;
			ReturnVal->PacketId = 0;
			ReturnVal->Channel = nullptr;
			ReturnVal->ChIndex = ChIndex;
			ReturnVal->ChName = ChName;
			ReturnVal->bReliable = 1;
			ReturnVal->ChSequence = BunchSequence;

			// NOTE: Might not cover all bOpen or 'channel already open' cases
			if (UnitConn->Channels[ChIndex] == nullptr)
			{
				ReturnVal->bOpen = 1;
			}
			else if (UnitConn->Channels[ChIndex]->OpenPacketId.First == INDEX_NONE)
			{
				ReturnVal->bOpen = 1;

				UnitConn->Channels[ChIndex]->OpenPacketId.First = BunchSequence;
				UnitConn->Channels[ChIndex]->OpenPacketId.Last = BunchSequence;
			}
		}
	}

	return ReturnVal;
}

void UMinimalClient::DiscardChannelBunch(FOutBunch* Bunch)
{
	if (UnitConn != nullptr && Bunch != nullptr)
	{
		--UnitConn->OutReliable[Bunch->ChIndex];
	}
}

// @todo #JohnB: Deprecate this function when next refactoring (touches unit tests)
bool UMinimalClient::SendControlBunch(FOutBunch* ControlChanBunch)
{
	bool bSuccess = false;

	if (ControlChanBunch != nullptr && UnitConn != nullptr)
	{
		check(ControlChanBunch->ChIndex == 0);

		bSuccess = SendRawBunch(*ControlChanBunch);;
	}

	return bSuccess;
}

bool UMinimalClient::SendRawBunch(FOutBunch& Bunch, bool bAllowPartial/*=false*/)
{
	bool bSuccess = false;

	if (UnitConn != nullptr)
	{
		TArray<FOutBunch*> SendBunches;
		const int32 MAX_SINGLE_BUNCH_SIZE_BITS = UnitConn->GetMaxSingleBunchSizeBits();
		int32 BunchNumBits = Bunch.GetNumBits();

		if (bAllowPartial && BunchNumBits > MAX_SINGLE_BUNCH_SIZE_BITS)
		{
			const int32 MAX_SINGLE_BUNCH_SIZE_BYTES = MAX_SINGLE_BUNCH_SIZE_BITS / 8;
			const int32 MAX_PARTIAL_BUNCH_SIZE_BITS = MAX_SINGLE_BUNCH_SIZE_BYTES * 8;
			int32 NumSerializedBits = 0;

			// Discard the original bunch before splitting it, in order to have the correct packet sequence for the new packets
			DiscardChannelBunch(&Bunch);

			while (NumSerializedBits < BunchNumBits)
			{
				FOutBunch* NewBunch = CreateChannelBunchByName(Bunch.ChName, Bunch.ChIndex);
				int32 NewNumBits = FMath::Min(BunchNumBits - NumSerializedBits, MAX_PARTIAL_BUNCH_SIZE_BITS);

				NewBunch->SerializeBits(Bunch.GetData() + (NumSerializedBits >> 3), NewNumBits);
				NumSerializedBits += NewNumBits;


				NewBunch->bReliable = Bunch.bReliable;
				NewBunch->bOpen = Bunch.bOpen;
				NewBunch->bClose = Bunch.bClose;
				NewBunch->CloseReason = Bunch.CloseReason;
				NewBunch->bIsReplicationPaused = Bunch.bIsReplicationPaused;
				NewBunch->ChIndex = Bunch.ChIndex;
				NewBunch->ChName = Bunch.ChName;

				if (!NewBunch->bHasPackageMapExports)
				{
					NewBunch->bHasMustBeMappedGUIDs |= Bunch.bHasMustBeMappedGUIDs;
				}

				NewBunch->bPartial = 1;
				NewBunch->bPartialInitial = SendBunches.Num() == 0;
				NewBunch->bPartialFinal = NumSerializedBits >= BunchNumBits;
				NewBunch->bOpen &= SendBunches.Num() == 0;
				NewBunch->bClose = Bunch.bClose && NumSerializedBits >= BunchNumBits;


				SendBunches.Add(NewBunch);
				NewBunch->SetDebugString(FString::Printf(TEXT("Partial[%d]: %s"), SendBunches.Num(), *Bunch.GetDebugString()));
			}

			UNIT_LOG(, TEXT("SendRawBunch: Split oversized bunch (%i bits) into '%i' partial packets."), BunchNumBits,
						SendBunches.Num());
		}
		else
		{
			SendBunches.Add(&Bunch);
		}

		FOutBunch* FirstBunch = SendBunches[0];
		int32 ChIdx = FirstBunch->ChIndex;
		bool bAddToOutRec = FirstBunch->bReliable && UnitConn->Channels[ChIdx] != nullptr;

		for (int32 BunchIdx = 0; BunchIdx < SendBunches.Num(); BunchIdx++)
		{
			FOutBunch* CurBunch = SendBunches[BunchIdx];

			UnitConn->SendRawBunch(*CurBunch, SendBunches.Num() == 1);

			if (BunchIdx > 0 && bAddToOutRec)
			{
				SendBunches[BunchIdx-1]->Next = CurBunch;
			}

			bSuccess = true;
		}


		// Since the packet is being sent abnormally, append it to the channels OutRec manually
		if (bAddToOutRec)
		{
			UChannel* CurChan = UnitConn->Channels[ChIdx];
			FOutBunch*& OutRec = CurChan->OutRec;

			if (OutRec == nullptr)
			{
				OutRec = FirstBunch;
			}
			else
			{
				for (FOutBunch* CurOut=OutRec; CurOut!=nullptr; CurOut=CurOut->Next)
				{
					if (CurOut->Next == nullptr)
					{
						CurOut->Next = FirstBunch;
						break;
					}
				}
			}

			CurChan->NumOutRec += SendBunches.Num();
		}
		// @todo #JohnB: Remove this, eventually - you just want to enforce use of bReliable bunches for now, otherwise it memleaks.
#if 1
		else
		{
			check(false);
		}
#endif

		// @todo #JohnB: You need to add detection here, to see if the bunch was added to GC handling somewhere (e.g. OutRec),
		//					and if not, then add some kind of handling for that (or destroy the bunch immediately, perhaps?),
		//					otherwise the bunch is memleaked.
	}

	return bSuccess;
}

bool UMinimalClient::SendRPCChecked(UObject* Target, const TCHAR* FunctionName, void* Parms, int16 ParmsSize,
										int16 ParmsSizeCorrection/*=0*/)
{
	bool bSuccess = false;
	UFunction* TargetFunc = Target->FindFunction(FName(FunctionName));

	PreSendRPC();

	if (TargetFunc != nullptr)
	{
		FString FuncParms = NUTUtilRefl::FunctionParmsToString(TargetFunc, Parms);

		if (TargetFunc->ParmsSize == ParmsSize + ParmsSizeCorrection)
		{
			Target->ProcessEvent(TargetFunc, Parms);

			// Do not add back IsNetReady checks, unless omitting them breaks something - reliable stuff should still get sent.
#if 0
			if (UnitConn != nullptr)
			{
				if (UnitConn->IsNetReady(false))
				{
					Target->ProcessEvent(TargetFunc, Parms);
				}
				else
				{
					UNIT_LOG(ELogType::StatusFailure,
								TEXT("Failed to send RPC '%s', network saturated (QueuedBits: %i, SendBuffer.Num: %i)."),
								FunctionName, UnitConn->QueuedBits, UnitConn->SendBuffer.GetNumBits());
					UNIT_LOG(ELogType::StatusFailure, TEXT("     '%s' parameters: %s"), FunctionName, *FuncParms);
				}
			}
#endif
		}
		else
		{
			UNIT_LOG(ELogType::StatusFailure, TEXT("Failed to send RPC '%s', mismatched parameters: '%i' vs '%i' (%i - %i)."),
						FunctionName, TargetFunc->ParmsSize, ParmsSize + ParmsSizeCorrection, ParmsSize, -ParmsSizeCorrection);
			UNIT_LOG(ELogType::StatusFailure, TEXT("     '%s' parameters: %s"), FunctionName, *FuncParms);
		}
	}
	else
	{
		UNIT_LOG(ELogType::StatusFailure, TEXT("Failed to send RPC, could not find RPC: %s"), FunctionName);
	}

	bSuccess = PostSendRPC(FunctionName, Target);

	return bSuccess;
}

bool UMinimalClient::SendRPCChecked(UObject* Target, FFuncReflection& FuncRefl)
{
	bool bSuccess = false;

	if (FuncRefl.IsValid())
	{
		bSuccess = SendRPCChecked(Target, *FuncRefl.Function->GetName(), FuncRefl.GetParms(), FuncRefl.Function->ParmsSize);
	}
	else
	{
		UNIT_LOG(ELogType::StatusFailure, TEXT("Failed to send RPC '%s', function reflection failed."), FuncRefl.FunctionName);
	}

	return bSuccess;
}

void UMinimalClient::PreSendRPC()
{
	// The failure delegate must be bound - otherwise there's no point in having a 'checked' RPC call function
	check(RPCFailureDel.IsBound());

	// Must be able to send RPC's
	check(!!(MinClientFlags & EMinClientFlags::SendRPCs));

	// Flush before and after, so no queued data is counted as a send, and so that the queued RPC is immediately sent and detected
	if (UnitConn != nullptr)
	{
		UnitConn->FlushNet();
	}

	bSentBunch = false;
}

bool UMinimalClient::PostSendRPC(FString RPCName, UObject* Target/*=nullptr*/)
{
	bool bSuccess = false;
	UActorComponent* TargetComponent = Cast<UActorComponent>(Target);
	AActor* TargetActor = (TargetComponent != nullptr ? TargetComponent->GetOwner() : Cast<AActor>(Target));
	UChannel* TargetChan = nullptr;

	if (UnitConn != nullptr)
	{
		TargetChan = UnitConn->FindActorChannelRef(TargetActor);

		UnitConn->FlushNet();

		// Just hack-erase bunch overflow tracking for this actors channel
		if (TargetChan != nullptr)
		{
			TargetChan->NumOutRec = 0;
		}
	}

	// If sending failed, trigger an overall unit test failure
	if (!bSentBunch)
	{
		FString LogMsg = FString::Printf(TEXT("Failed to send RPC '%s', unit test needs update."), *RPCName);

		// If specific/known failure cases are encountered, append them to the log message, to aid debugging
		// (try to enumerate all possible cases)
		if (TargetActor != nullptr)
		{
			FString LogAppend = TEXT("");
			UWorld* TargetWorld = TargetActor->GetWorld();

			if (IsGarbageCollecting())
			{
				LogAppend += TEXT(", IsGarbageCollecting() returned TRUE");
			}

			if (TargetWorld == nullptr)
			{
				LogAppend += TEXT(", TargetWorld == nullptr");
			}
			else if (!TargetWorld->AreActorsInitialized() && !GAllowActorScriptExecutionInEditor)
			{
				LogAppend += TEXT(", AreActorsInitialized() returned FALSE");
			}

			if (!IsValid(TargetActor))
			{
				LogAppend += TEXT(", IsValid() returned FALSE");
			}

			UFunction* TargetFunc = RPCName.StartsWith(TEXT("UnitTestServer_")) ?
									TargetActor->FindFunction(FName(TEXT("ServerExecute"))) :
									TargetActor->FindFunction(FName(*RPCName));

			if (TargetFunc == nullptr)
			{
				LogAppend += TEXT(", TargetFunc == nullptr");
			}
			else
			{
				int32 Callspace = TargetActor->GetFunctionCallspace(TargetFunc, nullptr);

				if (!(Callspace & FunctionCallspace::Remote))
				{
					LogAppend += FString::Printf(TEXT(", GetFunctionCallspace() returned non-remote, value: %i (%s)"), Callspace,
													FunctionCallspace::ToString((FunctionCallspace::Type)Callspace));
				}
			}

			if (TargetActor->GetNetDriver() == nullptr)
			{
				FName TargetNetDriver = TargetActor->GetNetDriverName();

				LogAppend += FString::Printf(TEXT(", GetNetDriver() returned nullptr - NetDriverName: %s"),
												*TargetNetDriver.ToString());

				if (TargetNetDriver == NAME_GameNetDriver && TargetWorld != nullptr && TargetWorld->GetNetDriver() == nullptr)
				{
					LogAppend += FString::Printf(TEXT(", TargetWorld->GetNetDriver() returned nullptr - World: %s"),
													*TargetWorld->GetFullName());
				}
			}

			UNetConnection* TargetConn = TargetActor->GetNetConnection();

			if (TargetConn == nullptr)
			{
				LogAppend += TEXT(", GetNetConnection() returned nullptr");
			}
			else if (!TargetConn->IsNetReady(false))
			{
				LogAppend += TEXT(", IsNetReady() returned FALSE (NOTE: This should not block reliable packets!)");
			}

			if (TargetChan == nullptr)
			{
				LogAppend += TEXT(", TargetChan == nullptr");
			}
			else if (TargetChan->OpenPacketId.First == INDEX_NONE)
			{
				LogAppend += TEXT(", Channel not open");
			}


			if (LogAppend.Len() > 0)
			{
				LogMsg += FString::Printf(TEXT(" (%s)"), *LogAppend.Mid(2));
			}
		}

		UNIT_LOG(ELogType::StatusFailure, TEXT("%s"), *LogMsg);
		UNIT_STATUS_LOG(ELogType::StatusVerbose, TEXT("%s"), *LogMsg);

		RPCFailureDel.Execute();

		bSuccess = false;
	}
	else
	{
		bSuccess = true;
	}

	return bSuccess;
}

void UMinimalClient::ResetConnTimeout(float Duration)
{
	UNetDriver* UnitDriver = (UnitConn != nullptr ? ToRawPtr(UnitConn->Driver) : nullptr);

	if (UnitDriver != nullptr && UnitConn->GetConnectionState() != USOCK_Closed)
	{
		// @todo #JohnBHack: This is a slightly hacky way of setting the timeout to a large value, which will be overridden by newly
		//				received packets, making it unsuitable for most situations (except crashes - but that could still be subject
		//				to a race condition)
		const double NewLastReceiveTime = UnitDriver->GetElapsedTime() + Duration;

		UnitConn->LastReceiveTime = FMath::Max(NewLastReceiveTime, UnitConn->LastReceiveTime);
	}
}

void UMinimalClient::UnitTick(float DeltaTimm)
{
	if (!!(MinClientFlags & EMinClientFlags::NotifyNetActors) && PendingNetActorChans.Num() > 0 && UnitConn != nullptr)
	{
		for (int32 i=PendingNetActorChans.Num()-1; i>=0; i--)
		{
			UActorChannel* CurChan = Cast<UActorChannel>(UnitConn->Channels[PendingNetActorChans[i]]);

			if (CurChan != nullptr && CurChan->Actor != nullptr)
			{
				NetActorDel.ExecuteIfBound(CurChan, CurChan->Actor);
				PendingNetActorChans.RemoveAt(i);
			}
		}
	}
}

bool UMinimalClient::IsTickable() const
{
	return (!!(MinClientFlags & EMinClientFlags::NotifyNetActors) && PendingNetActorChans.Num() > 0);
}

bool UMinimalClient::ConnectMinimalClient()
{
	bool bSuccess = false;

	check(UnitWorld != nullptr);

	CreateNetDriver();

	if (UnitNetDriver != nullptr)
	{
		// Hack: Replace the control and actor channels, with stripped down unit test channels
		UnitNetDriver->ChannelDefinitionMap[NAME_Control].ChannelClass = UUnitTestChannel::StaticClass();
		UnitNetDriver->ChannelDefinitionMap[NAME_Actor].ChannelClass = UUnitTestActorChannel::StaticClass();

		// @todo #JohnB: Block voice channel?

		UnitNetDriver->InitialConnectTimeout = FMath::Max(UnitNetDriver->InitialConnectTimeout, (float)Timeout);
		UnitNetDriver->ConnectionTimeout = FMath::Max(UnitNetDriver->ConnectionTimeout, (float)Timeout);

#if !UE_BUILD_SHIPPING
		if (!(MinClientFlags & EMinClientFlags::SendRPCs) || !!(MinClientFlags & EMinClientFlags::DumpSendRPC))
		{
			UnitNetDriver->SendRPCDel.BindUObject(this, &UMinimalClient::NotifySendRPC);
		}

		if (!EnumHasAnyFlags(MinClientFlags, EMinClientFlags::AcceptRepNotifies))
		{
			UnitNetDriver->SkipRepNotifiesDel.BindLambda(
				[]()
				{
					return true;
				});
		}
#endif

		bool bBeaconConnect = !!(MinClientFlags & EMinClientFlags::BeaconConnect);
		FString ConnectAddress = (bBeaconConnect ? BeaconAddress : ServerAddress) + URLOptions;
		FURL DefaultURL;
		FURL TravelURL(&DefaultURL, *ConnectAddress, TRAVEL_Absolute);
		FString ConnectionError;
		UClass* NetConnClass = UnitNetDriver->NetConnectionClass;
		UNetConnection* DefConn = NetConnClass != nullptr ? Cast<UNetConnection>(NetConnClass->GetDefaultObject()) : nullptr;

		bSuccess = DefConn != nullptr;

		if (bSuccess)
		{
			// Replace the package map class
			TSubclassOf<UPackageMap> OldClass = DefConn->PackageMapClass;
			FProperty* OldPostConstructLink = NetConnClass->PostConstructLink;
			FProperty* PackageMapProp = FindFieldChecked<FProperty>(NetConnClass, TEXT("PackageMapClass"));

			// Hack - force property initialization for the PackageMapClass property, so changing its default value works.
			check(PackageMapProp != nullptr && PackageMapProp->PostConstructLinkNext == nullptr);

			PackageMapProp->PostConstructLinkNext = NetConnClass->PostConstructLink;
			NetConnClass->PostConstructLink = PackageMapProp;
			DefConn->PackageMapClass = UUnitTestPackageMap::StaticClass();

			bSuccess = UnitNetDriver->InitConnect(this, TravelURL, ConnectionError);

			DefConn->PackageMapClass = OldClass;
			NetConnClass->PostConstructLink = OldPostConstructLink;
			PackageMapProp->PostConstructLinkNext = nullptr;
		}
		else
		{
			UNIT_LOG(ELogType::StatusFailure, TEXT("Failed to replace PackageMapClass, minimal client connection failed."));
		}

		if (bSuccess)
		{
			UnitConn = UnitNetDriver->ServerConnection;

			check(UnitConn != nullptr);
			check(UnitConn->PackageMapClass == UUnitTestPackageMap::StaticClass());

			UUnitTestPackageMap* PackageMap = CastChecked<UUnitTestPackageMap>(UnitConn->PackageMap);

			PackageMap->MinClient = this;

			FString LogMsg = FString::Printf(TEXT("Successfully created minimal client connection to IP '%s'"), *ConnectAddress);

			UNIT_LOG(ELogType::StatusImportant, TEXT("%s"), *LogMsg);
			UNIT_STATUS_LOG(ELogType::StatusVerbose, TEXT("%s"), *LogMsg);


#if !UE_BUILD_SHIPPING
			UnitConn->ReceivedRawPacketDel = FOnReceivedRawPacket::CreateUObject(this, &UMinimalClient::NotifyReceivedRawPacket);
			UnitConn->LowLevelSendDel = FOnLowLevelSend::CreateUObject(this, &UMinimalClient::NotifySocketSend);
#endif

			// Work around a minor UNetConnection bug, where QueuedBits is not initialized, until after the first Tick
			UnitConn->QueuedBits = -(MAX_PACKET_SIZE * 8);

			UUnitTestChannel* UnitControlChan = CastChecked<UUnitTestChannel>(UnitConn->Channels[0]);

			UnitControlChan->MinClient = this;

			if (UnitConn->Handler.IsValid())
			{
				UnitConn->Handler->BeginHandshaking(
										FPacketHandlerHandshakeComplete::CreateUObject(this, &UMinimalClient::SendInitialJoin));
			}
			else
			{
				SendInitialJoin();
			}
		}
		else
		{
			UNIT_LOG(ELogType::StatusFailure, TEXT("Failed to kickoff connect to IP '%s', error: %s"), *ConnectAddress,
							*ConnectionError);
		}
	}
	else
	{
		UNIT_LOG(ELogType::StatusFailure, TEXT("Failed to create an instance of the unit test net driver"));
	}

	return bSuccess;
}

void UMinimalClient::CreateNetDriver()
{
	check(UnitNetDriver == nullptr);

	UGameEngine* GameEngine = Cast<UGameEngine>(GEngine);

	if (GameEngine != nullptr && UnitWorld != nullptr)
	{
		int32& UnitTestNetDriverCount = UNUTGlobals::Get().UnitTestNetDriverCount;

		// Setup a new driver name entry
		bool bFoundDef = false;
		FString DriverClassName = (NetDriverClass.Len() > 0 ? *NetDriverClass : TEXT("/Script/OnlineSubsystemUtils.IpNetDriver"));
		int32 ClassLoc = DriverClassName.Find(TEXT("."));
		FName UnitDefName = *(FString(TEXT("UnitTestNetDriver_")) + DriverClassName.Mid(ClassLoc+1));

		for (int32 i=0; i<GameEngine->NetDriverDefinitions.Num(); i++)
		{
			if (GameEngine->NetDriverDefinitions[i].DefName == UnitDefName)
			{
				bFoundDef = true;
				break;
			}
		}

		if (!bFoundDef)
		{
			FNetDriverDefinition NewDriverEntry;

			NewDriverEntry.DefName = UnitDefName;
			NewDriverEntry.DriverClassName = (NetDriverClass.Len() > 0 ? *NetDriverClass :
												TEXT("/Script/OnlineSubsystemUtils.IpNetDriver"));

			// Don't allow fallbacks for the MinimalClient
			NewDriverEntry.DriverClassNameFallback = NewDriverEntry.DriverClassName;

			GameEngine->NetDriverDefinitions.Add(NewDriverEntry);
		}


		FName NewDriverName = *FString::Printf(TEXT("UnitTestNetDriver_%i"), UnitTestNetDriverCount++);

		// Now create a reference to the driver
		if (GameEngine->CreateNamedNetDriver(UnitWorld, NewDriverName, UnitDefName))
		{
			UnitNetDriver = GameEngine->FindNamedNetDriver(UnitWorld, NewDriverName);
		}


		if (UnitNetDriver != nullptr)
		{
			UnitNetDriver->SetWorld(UnitWorld);
			UnitWorld->SetNetDriver(UnitNetDriver);

			UnitNetDriver->InitConnectionClass();


			FLevelCollection* Collection = (FLevelCollection*)UnitWorld->GetActiveLevelCollection();

			// Hack-set the net driver in the worlds level collection
			if (Collection != nullptr)
			{
				Collection->SetNetDriver(UnitNetDriver);
			}
			else
			{
				UNIT_LOG(ELogType::StatusWarning,
								TEXT("CreateNetDriver: No LevelCollection found for created world, may block replication."));
			}

			UNIT_LOG(, TEXT("CreateNetDriver: Created named net driver: %s, NetDriverName: %s, for World: %s"),
							*UnitNetDriver->GetFullName(), *UnitNetDriver->NetDriverName.ToString(), *UnitWorld->GetFullName());
		}
		else
		{
			UNIT_LOG(ELogType::StatusFailure, TEXT("CreateNetDriver: CreateNamedNetDriver failed"));
		}
	}
	else if (GameEngine == nullptr)
	{
		UNIT_LOG(ELogType::StatusFailure, TEXT("CreateNetDriver: GameEngine is nullptr"));
	}
	else //if (UnitWorld == nullptr)
	{
		UNIT_LOG(ELogType::StatusFailure, TEXT("CreateNetDriver: UnitWorld is nullptr"));
	}
}

void UMinimalClient::SendInitialJoin()
{
	// Make sure to flush any existing packet buffers, as Fortnite connect URL's can be very large and trigger an overflow
	if (UnitConn != nullptr)
	{
		UnitConn->FlushNet();
	}

	FOutBunch* ControlChanBunch = CreateChannelBunchByName(NAME_Control, 0);

	if (ControlChanBunch != nullptr)
	{
		// Need to send 'NMT_Hello' to start off the connection (the challenge is not replied to)
		uint8 IsLittleEndian = uint8(PLATFORM_LITTLE_ENDIAN);

		// We need to construct the NMT_Hello packet manually, for the initial connection
		uint8 MessageType = NMT_Hello;

		// Allow the bunch to resize, and be split into partial bunches in SendRawBunch - for Fortnite
		ControlChanBunch->SetAllowResize(true);

		*ControlChanBunch << MessageType;
		*ControlChanBunch << IsLittleEndian;

		uint32 LocalNetworkVersion = FNetworkVersion::GetLocalNetworkVersion();
		*ControlChanBunch << LocalNetworkVersion;

		FString EncryptionToken = TEXT("");

		*ControlChanBunch << EncryptionToken;

		bool bSkipControlJoin = !!(MinClientFlags & EMinClientFlags::SkipControlJoin);
		bool bBeaconConnect = !!(MinClientFlags & EMinClientFlags::BeaconConnect);

		if (bBeaconConnect)
		{
			if (!bSkipControlJoin)
			{
				MessageType = NMT_BeaconJoin;
				*ControlChanBunch << MessageType;
				*ControlChanBunch << BeaconType;

				uint8 EncType = 0;

				*ControlChanBunch << EncType;
				*ControlChanBunch << JoinUID;

				// Also immediately ack the beacon GUID setup; we're just going to let the server setup the client beacon,
				// through the actor channel
				MessageType = NMT_BeaconNetGUIDAck;
				*ControlChanBunch << MessageType;
				*ControlChanBunch << BeaconType;
			}
		}
		else
		{
			// Then send NMT_Login
			WriteControlLogin(ControlChanBunch);

			// Now send NMT_Join, which will spawn the PlayerController, which should then trigger replication of basic actor channels
			if (!bSkipControlJoin)
			{
				MessageType = NMT_Join;
				*ControlChanBunch << MessageType;
			}
		}

		SendRawBunch(*ControlChanBunch, true);

		// Immediately flush, so that Fortnite doesn't trigger an overflow
		if (UnitConn != nullptr)
		{
			UnitConn->FlushNet();
		}


		// At this point, fire of notification that we are connected
		bConnected = true;

		ConnectedDel.ExecuteIfBound();
	}
	else
	{
		UNIT_LOG(ELogType::StatusFailure, TEXT("Failed to kickoff connection, could not create control channel bunch."));
	}
}

void UMinimalClient::WriteControlLogin(FOutBunch* ControlChanBunch)
{
	uint8 MessageType = NMT_Login;
	FString BlankStr = TEXT("");
	FString OnlinePlatformName = TEXT("Dud");
	uint8 EncType = 0;

	// @todo #JohnB: It would be nice to remove this last UnitTest dependency (may be opportune to, when doing MCP connect URL)
	// @todo #JohnB: Update the existing environments that use this, to use a UnitTask with AlterMinClient instead, like Fortnite,
	//					then completely remove this method (and the unit test dependency).
	//					Problem is, this will require retesting UT and such.
	FString ConnectURL = UUnitTest::UnitEnv->GetDefaultClientConnectURL() + URLOptions;

	UNIT_LOG(, TEXT("Sending NMT_Login with parameters: ClientResponse: %s, ConnectURL: %s, UID: %s, OnlinePlatformName: %s"),
							*BlankStr, *ConnectURL, *JoinUID, *OnlinePlatformName);

	*ControlChanBunch << MessageType;
	*ControlChanBunch << BlankStr;
	*ControlChanBunch << ConnectURL;
	*ControlChanBunch << EncType;
	*ControlChanBunch << JoinUID;
	*ControlChanBunch << OnlinePlatformName;
}

void UMinimalClient::DisconnectMinimalClient()
{
	if (UnitWorld != nullptr && UnitNetDriver != nullptr)
	{
		GEngine->DestroyNamedNetDriver(UnitWorld, UnitNetDriver->NetDriverName);
	}
}

void UMinimalClient::NotifySocketSend(void* Data, int32 Count, bool& bBlockSend)
{
	if (!!(MinClientFlags & EMinClientFlags::DumpSendRaw))
	{
		UNIT_LOG(ELogType::StatusDebug, TEXT("NotifySocketSend: Packet dump:"));

		UNIT_LOG_BEGIN(this, ELogType::StatusDebug | ELogType::StyleMonospace);
		NUTDebug::LogHexDump((const uint8*)Data, Count, true, true);
		UNIT_LOG_END();
	}

#if !UE_BUILD_SHIPPING
	LowLevelSendDel.ExecuteIfBound(Data, Count, bBlockSend);
#endif

	bSentBunch = !bBlockSend;
}

void UMinimalClient::NotifyReceivedRawPacket(void* Data, int32 Count, bool& bBlockReceive)
{
#if !UE_BUILD_SHIPPING
	// This check isn't needed, but added to satisfy PVS-Studio, as their warning-disable macro's are sketchy.
	if (UnitConn != nullptr)
	{
		GActiveReceiveUnitConnection = UnitConn;

		ReceivedRawPacketDel.ExecuteIfBound(Data, Count);

		if (!!(MinClientFlags & EMinClientFlags::DumpReceivedRaw))
		{
			UNIT_LOG(ELogType::StatusDebug, TEXT("NotifyReceivedRawPacket: Packet dump:"));

			UNIT_LOG_BEGIN(this, ELogType::StatusDebug | ELogType::StyleMonospace);
			NUTDebug::LogHexDump((const uint8*)Data, Count, true, true);
			UNIT_LOG_END();
		}


		// The rest of the original ReceivedRawPacket function call is blocked, so temporarily disable the delegate,
		// and re-trigger it here, so that we correctly encapsulate its call with GActiveReceiveUnitConnection
		FOnReceivedRawPacket TempDel = UnitConn->ReceivedRawPacketDel;

		UnitConn->ReceivedRawPacketDel.Unbind();

		UnitConn->UNetConnection::ReceivedRawPacket(Data, Count);

		// Null check, in case connection closes while receiving
		if (UnitConn != nullptr)
		{
			UnitConn->ReceivedRawPacketDel = TempDel;
		}


		GActiveReceiveUnitConnection = nullptr;

		// Block the original function call - replaced with the above
		bBlockReceive = true;
	}
#endif
}

void UMinimalClient::NotifyReceiveRPC(AActor* Actor, UFunction* Function, void* Parameters, bool& bBlockRPC)
{
	UNIT_EVENT_BEGIN(this);

	// If specified, block RPC's by default - the delegate below has a chance to override this
	if (!(MinClientFlags & EMinClientFlags::AcceptRPCs))
	{
		bBlockRPC = true;
	}


	ReceiveRPCDel.ExecuteIfBound(Actor, Function, Parameters, bBlockRPC);

	FString FuncName = Function->GetName();

	if (bBlockRPC && AllowedClientRPCs.Contains(FuncName))
	{
		bBlockRPC = false;
	}

	if (bBlockRPC)
	{
		FString FuncParms = NUTUtilRefl::FunctionParmsToString(Function, Parameters);

		UNIT_LOG(, TEXT("Blocking receive RPC '%s' for actor '%s'"), *FuncName, *Actor->GetFullName());

		if (FuncParms.Len() > 0)
		{
			UNIT_LOG(, TEXT("     '%s' parameters: %s"), *FuncName, *FuncParms);
		}
	}


	if (!!(MinClientFlags & EMinClientFlags::DumpReceivedRPC) && !bBlockRPC)
	{
		FString FuncParms = NUTUtilRefl::FunctionParmsToString(Function, Parameters);

		UNIT_LOG(ELogType::StatusDebug, TEXT("Received RPC '%s' for actor '%s'"), *FuncName, *Actor->GetFullName());

		if (FuncParms.Len() > 0)
		{
			UNIT_LOG(, TEXT("     '%s' parameters: %s"), *FuncName, *FuncParms);
		}
	}

	UNIT_EVENT_END;
}

void UMinimalClient::NotifySendRPC(AActor* Actor, UFunction* Function, void* Parameters, FOutParmRec* OutParms, FFrame* Stack,
									UObject* SubObject, bool& bBlockSendRPC)
{
	bBlockSendRPC = !(MinClientFlags & EMinClientFlags::SendRPCs);

#if !UE_BUILD_SHIPPING
	// Pass on to the delegate, and give it an opportunity to override whether the RPC is sent
	SendRPCDel.ExecuteIfBound(Actor, Function, Parameters, OutParms, Stack, SubObject, bBlockSendRPC);
#endif

	if (!bBlockSendRPC)
	{
		if (!!(MinClientFlags & EMinClientFlags::DumpSendRPC))
		{
			UNIT_LOG(ELogType::StatusDebug, TEXT("Send RPC '%s' for actor '%s' (SubObject '%s')"), *Function->GetName(),
						*Actor->GetFullName(), (SubObject != NULL ? *SubObject->GetFullName() : TEXT("nullptr")));
		}
	}
	else
	{
		if (!(MinClientFlags & EMinClientFlags::SendRPCs))
		{
			UNIT_LOG(, TEXT("Blocking send RPC '%s' in actor '%s' (SubObject '%s')"), *Function->GetName(),
						*Actor->GetFullName(), (SubObject != NULL ? *SubObject->GetFullName() : TEXT("nullptr")));
		}
	}
}

void UMinimalClient::InternalNotifyNetworkFailure(UWorld* InWorld, UNetDriver* InNetDriver, ENetworkFailure::Type FailureType,
													const FString& ErrorString)
{
	if (InNetDriver == (UNetDriver*)UnitNetDriver)
	{
		UNIT_EVENT_BEGIN(this);

		NetworkFailureDel.ExecuteIfBound(FailureType, ErrorString);

		UNIT_EVENT_END;
	}
}

bool UMinimalClient::NotifyAcceptingChannel(UChannel* Channel)
{
	bool bAccepted = false;

	if (Channel->ChName == NAME_Actor)
	{
		UUnitTestActorChannel* ActorChan = Cast<UUnitTestActorChannel>(Channel);

		if (ActorChan != nullptr)
		{
			ActorChan->MinClient = this;
		}

		bAccepted = !!(MinClientFlags & EMinClientFlags::AcceptActors);

		if (bAccepted && !!(MinClientFlags & EMinClientFlags::NotifyNetActors))
		{
			PendingNetActorChans.Add(Channel->ChIndex);
		}
	}

	return bAccepted;
}

