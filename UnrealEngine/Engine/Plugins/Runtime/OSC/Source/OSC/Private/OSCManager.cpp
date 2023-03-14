// Copyright Epic Games, Inc. All Rights Reserved.
#include "OSCManager.h"

#include "IPAddress.h"
#include "OSCMessage.h"
#include "OSCMessagePacket.h"
#include "OSCBundle.h"
#include "OSCBundlePacket.h"
#include "OSCLog.h"
#include "OSCServer.h"
#include "OSCClient.h"

#include "Engine/World.h"
#include "Logging/LogMacros.h"
#include "Misc/Paths.h"
#include "SocketSubsystem.h"
#include "UObject/UObjectIterator.h"


#define OSC_LOG_INVALID_TYPE_AT_INDEX(TypeStr, Index, Msg) UE_LOG(LogOSC, Warning, TEXT("OSC Message Parse Failed: OSCType not %s: index '%i', OSCAddress '%s'"), TypeStr, Index, *Msg.GetAddress().GetFullPath())

namespace OSC
{
	static const int32 DefaultClientPort = 8094;
	static const int32 DefaultServerPort = 8095;

	// Returns true if provided address was null and was able to
	// override with local host address, false if not.
	bool GetLocalHostAddress(FString& InAddress)
	{
		if (!InAddress.IsEmpty() && InAddress != TEXT("0"))
		{
			return false;
		}

		bool bCanBind = false;
		bool bAppendPort = false;
		if (ISocketSubsystem* SocketSys = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM))
		{
			const TSharedPtr<FInternetAddr> Addr = SocketSys->GetLocalHostAddr(*GLog, bCanBind);
			if (Addr.IsValid())
			{
				InAddress = Addr->ToString(bAppendPort);
				return true;
			}
		}

		return false;
	}

	const FOSCType* GetOSCTypeAtIndex(const FOSCMessage& InMessage, const int32 InIndex)
	{
		const TSharedPtr<FOSCMessagePacket>& Packet = StaticCastSharedPtr<FOSCMessagePacket>(InMessage.GetPacket());
		if (Packet.IsValid())
		{
			TArray<FOSCType>& Args = Packet->GetArguments();
			if (InIndex >= Args.Num())
			{
				UE_LOG(LogOSC, Warning, TEXT("Index '%d' out-of-bounds.  Message argument size = '%d'"), InIndex, Args.Num());
				return nullptr;
			}

			return &Args[InIndex];
		}

		return nullptr;
	}
} // namespace OSC


static FAutoConsoleCommand GOSCPrintServers(
	TEXT("osc.servers"),
	TEXT("Prints diagnostic information pertaining to the currently initialized OSC servers objects to the output log."),
	FConsoleCommandDelegate::CreateStatic(
		[]()
		{
			FString LocalAddr;
			OSC::GetLocalHostAddress(LocalAddr);
			UE_LOG(LogOSC, Display, TEXT("Local IP: %s"), *LocalAddr);

			UE_LOG(LogOSC, Display, TEXT("OSC Servers:"));
			for (TObjectIterator<UOSCServer> Itr; Itr; ++Itr)
			{
				if (UOSCServer* Server = *Itr)
				{
					FString ToPrint = TEXT("    ") + Server->GetName();
					ToPrint.Appendf(TEXT(" (Id: %u"), Server->GetUniqueID());
					if (UWorld* World = Server->GetWorld())
					{
						ToPrint.Appendf(TEXT(", World: %s"), *World->GetName());
					}

					ToPrint.Appendf(TEXT(", IP: %s)"), *Server->GetIpAddress(true /* bIncludePort */));
					ToPrint += Server->IsActive() ? TEXT(" [Active]") : TEXT(" [Inactive]");

					UE_LOG(LogOSC, Display, TEXT("%s"), *ToPrint);

					const TArray<FOSCAddress> BoundPatterns = Server->GetBoundOSCAddressPatterns();
					if (BoundPatterns.Num() > 0)
					{
						UE_LOG(LogOSC, Display, TEXT("    Bound Address Patterns:"));
						for (const FOSCAddress& Pattern : BoundPatterns)
						{
							UE_LOG(LogOSC, Display, TEXT("         %s"), *Pattern.GetFullPath());
						}
						UE_LOG(LogOSC, Display, TEXT(""));
					}
				}
			}
		}
	)
);

static FAutoConsoleCommand GOSCServerConnect(
	TEXT("osc.server.connect"),
	TEXT("Connects or reconnects the osc mix server with the provided name\n"
		"(see \"osc.servers\" for a list of available servers and their respective names). Args:\n"
		"Name - Object name of server to (re)connect\n"
		"Address - IP Address to connect to (default: LocalHost)\n"
		"Port - Port to connect to (default: 8095)"),
	FConsoleCommandWithArgsDelegate::CreateStatic(
		[](const TArray<FString>& Args)
		{
			FString SrvName;
			if (Args.Num() > 0)
			{
				SrvName = Args[0];
			}

			FString IPAddr;
			OSC::GetLocalHostAddress(IPAddr);
			if (Args.Num() > 1)
			{
				IPAddr = Args[1];
			}

			int32 Port = OSC::DefaultServerPort;
			if (Args.Num() > 2)
			{
				Port = FCString::Atoi(*Args[2]);
			}

			for (TObjectIterator<UOSCServer> Itr; Itr; ++Itr)
			{
				if (UOSCServer* Server = *Itr)
				{
					if (Server->GetName() == SrvName)
					{
						Server->Stop();
						if (Server->SetAddress(IPAddr, Port))
						{
							Server->Listen();
						}
						return;
					}
				}
			}

			UE_LOG(LogOSC, Warning, TEXT("Server object with name '%s' not found, (re)connect not performed."), *SrvName);
		}
	)
);

static FAutoConsoleCommand GOSCServerConnectById(
	TEXT("osc.server.connectById"),
	TEXT("Connects or reconnects the osc mix server with the provided object id\n"
		"(see \"osc.servers\" for a list of available servers and their respective ids). Args:\n"
		"Id - Object Id of client to (re)connect\n"
		"Address - IP Address to (re)connect to (default: LocalHost)\n"
		"Port - Port to (re)connect to (default: 8095)"),
	FConsoleCommandWithArgsDelegate::CreateStatic(
		[](const TArray<FString>& Args)
		{
			if (Args.Num() == 0)
			{
				return;
			}

			const int32 SrvId = FCString::Atoi(*Args[0]);

			FString IPAddr;
			OSC::GetLocalHostAddress(IPAddr);
			if (Args.Num() > 1)
			{
				IPAddr = Args[1];
			}

			int32 Port = OSC::DefaultServerPort;
			if (Args.Num() > 2)
			{
				Port = FCString::Atoi(*Args[2]);
			}

			for (TObjectIterator<UOSCServer> Itr; Itr; ++Itr)
			{
				if (UOSCServer* Server = *Itr)
				{
					if (Server->GetUniqueID() == SrvId)
					{
						Server->Stop();
						if (Server->SetAddress(IPAddr, Port))
						{
							Server->Listen();
						}
						return;
					}
				}
			}

			UE_LOG(LogOSC, Warning, TEXT("Server object with id '%u' not found, (re)connect not performed."), SrvId);
		}
	)
);

static FAutoConsoleCommand GOSCPrintClients(
	TEXT("osc.clients"),
	TEXT("Prints diagnostic information pertaining to the currently initialized OSC client objects to the output log."),
	FConsoleCommandDelegate::CreateStatic(
		[]()
		{
			FString LocalAddr;
			OSC::GetLocalHostAddress(LocalAddr);
			UE_LOG(LogOSC, Display, TEXT("Local IP: %s"), *LocalAddr);

			UE_LOG(LogOSC, Display, TEXT("OSC Clients:"));
			for (TObjectIterator<UOSCClient> Itr; Itr; ++Itr)
			{
				if (UOSCClient* Client = *Itr)
				{
					FString ToPrint = TEXT("    ") + Client->GetName();
					ToPrint.Appendf(TEXT(" (Id: %u"), Client->GetUniqueID());
					if (UWorld* World = Client->GetWorld())
					{
						ToPrint.Appendf(TEXT(", World: %s"), *World->GetName());
					}

					FString IPAddrStr;
					int32 Port;
					Client->GetSendIPAddress(IPAddrStr, Port);
					ToPrint += TEXT(", Send IP: ") + IPAddrStr + TEXT(":");
					ToPrint.AppendInt(Port);
					ToPrint += Client->IsActive() ? TEXT(") [Active]") : TEXT(") [Inactive]");

					UE_LOG(LogOSC, Display, TEXT("%s"), *ToPrint);
				}
			}
		}
	)
);

static FAutoConsoleCommand GOSCClientConnect(
	TEXT("osc.client.connect"),
	TEXT("Connects (or reconnects) the osc mix client with the provided name\n"
		"(see \"osc.clients\" for a list of available clients and their respective ids). Args:\n"
		"Name - Object name of client to (re)connect\n"
		"Address - IP Address to (re)connect to (default: LocalHost)\n"
		"Port - Port to (re)connect to (default: 8094)"),
	FConsoleCommandWithArgsDelegate::CreateStatic(
		[](const TArray< FString >& Args)
		{
			if (Args.Num() == 0)
			{
				return;
			}
			const FString CliName = Args[0];

			FString IPAddr;
			OSC::GetLocalHostAddress(IPAddr);
			if (Args.Num() > 1)
			{
				IPAddr = Args[1];
			}

			int32 Port = OSC::DefaultClientPort;
			if (Args.Num() > 2)
			{
				Port = FCString::Atoi(*Args[2]);
			}

			for (TObjectIterator<UOSCClient> Itr; Itr; ++Itr)
			{
				if (UOSCClient* Client = *Itr)
				{
					if (Client->GetName() == CliName)
					{
						Client->Connect();
						Client->SetSendIPAddress(IPAddr, Port);
						return;
					}
				}
			}

			UE_LOG(LogOSC, Warning, TEXT("Client object with name '%s' not found, (re)connect not performed."), *CliName);
		}
	)
);

static FAutoConsoleCommand GOSCClientConnectById(
	TEXT("osc.client.connectById"),
	TEXT("Connects (or reconnects) the osc mix client with the provided object id\n"
		"(see \"osc.clients\" for a list of available clients and their respective ids). Args:\n"
		"Id - Object Id of client to (re)connect\n"
		"Address - IP Address to (re)connect to (default: LocalHost)\n"
		"Port - Port to (re)connect to (default: 8094)"),
	FConsoleCommandWithArgsDelegate::CreateStatic(
		[](const TArray< FString >& Args)
		{
			int32 CliId = INDEX_NONE;
			if (Args.Num() > 0)
			{
				CliId = FCString::Atoi(*Args[0]);
			}

			FString IPAddr;
			OSC::GetLocalHostAddress(IPAddr);
			if (Args.Num() > 1)
			{
				IPAddr = Args[1];
			}

			int32 Port = OSC::DefaultClientPort;
			if (Args.Num() > 2)
			{
				Port = FCString::Atoi(*Args[2]);
			}

			for (TObjectIterator<UOSCClient> Itr; Itr; ++Itr)
			{
				if (UOSCClient* Client = *Itr)
				{
					if (Client->GetUniqueID() == CliId)
					{
						Client->Connect();
						Client->SetSendIPAddress(IPAddr, Port);
						return;
					}
				}
			}

			UE_LOG(LogOSC, Warning, TEXT("Client object with id '%u' not found, (re)connect not performed."), CliId);
		}
	)
);

UOSCServer* UOSCManager::CreateOSCServer(FString InReceiveIPAddress, int32 InPort, bool bInMulticastLoopback, bool bInStartListening, FString ServerName, UObject* Outer)
{
	if (OSC::GetLocalHostAddress(InReceiveIPAddress))
	{
		UE_LOG(LogOSC, Display, TEXT("OSCServer ReceiveAddress not specified. Using LocalHost IP: '%s'"), *InReceiveIPAddress);
	}

	if (ServerName.IsEmpty())
	{
		ServerName = FString::Printf(TEXT("OSCServer_%s"), *FGuid::NewGuid().ToString(EGuidFormats::Short));
	}

	UOSCServer* NewOSCServer = nullptr;
	if (Outer)
	{
		NewOSCServer = NewObject<UOSCServer>(Outer, *ServerName, RF_StrongRefOnFrame);
	}
	else
	{
		UE_LOG(LogOSC, Warning, TEXT("Outer object not set.  OSCServer may be garbage collected if not referenced."));
		NewOSCServer = NewObject<UOSCServer>(GetTransientPackage(), *ServerName);
	}

	if (NewOSCServer)
	{
		NewOSCServer->SetMulticastLoopback(bInMulticastLoopback);
		if (NewOSCServer->SetAddress(InReceiveIPAddress, InPort))
		{
			if (bInStartListening)
			{
				NewOSCServer->Listen();
			}
		}
		else
		{
			UE_LOG(LogOSC, Warning, TEXT("Failed to parse ReceiveAddress '%s' for OSCServer."), *InReceiveIPAddress);
		}

		return NewOSCServer;
	}

	return nullptr;
}

UOSCClient* UOSCManager::CreateOSCClient(FString InSendIPAddress, int32 InPort, FString ClientName, UObject* Outer)
{
	if (OSC::GetLocalHostAddress(InSendIPAddress))
	{
		UE_LOG(LogOSC, Display, TEXT("OSCClient SendAddress not specified. Using LocalHost IP: '%s'"), *InSendIPAddress);
	}

	if (ClientName.IsEmpty())
	{
		ClientName = FString::Printf(TEXT("OSCClient_%s"), *FGuid::NewGuid().ToString(EGuidFormats::Short));
	}

	UOSCClient* NewOSCClient = nullptr;
	if (Outer)
	{
		NewOSCClient = NewObject<UOSCClient>(Outer, *ClientName, RF_StrongRefOnFrame);
	}
	else
	{
		UE_LOG(LogOSC, Warning, TEXT("Outer object not set.  OSCClient '%s' may be garbage collected if not referenced."), *ClientName);
		NewOSCClient = NewObject<UOSCClient>(GetTransientPackage(), *ClientName);
	}

	if (NewOSCClient)
	{
		NewOSCClient->Connect();
		if (!NewOSCClient->SetSendIPAddress(InSendIPAddress, InPort))
		{
			UE_LOG(LogOSC, Warning, TEXT("Failed to parse SendAddress '%s' for OSCClient. Client unable to send new messages."), *InSendIPAddress);
		}

		return NewOSCClient;
	}

	return nullptr;
}

FOSCMessage& UOSCManager::ClearMessage(FOSCMessage& OutMessage)
{
	const TSharedPtr<FOSCMessagePacket>& Packet = StaticCastSharedPtr<FOSCMessagePacket>(OutMessage.GetPacket());
	if (Packet.IsValid())
	{
		Packet->GetArguments().Reset();
	}

	return OutMessage;
}

FOSCBundle& UOSCManager::ClearBundle(FOSCBundle& OutBundle)
{
	const TSharedPtr<FOSCBundlePacket>& Packet = StaticCastSharedPtr<FOSCBundlePacket>(OutBundle.GetPacket());
	if (Packet.IsValid())
	{
		Packet->GetPackets().Reset();
	}

	return OutBundle;
}

FOSCBundle& UOSCManager::AddMessageToBundle(const FOSCMessage& InMessage, FOSCBundle& Bundle)
{
	const TSharedPtr<FOSCBundlePacket>& BundlePacket = StaticCastSharedPtr<FOSCBundlePacket>(Bundle.GetPacket());
	const TSharedPtr<FOSCMessagePacket>& MessagePacket = StaticCastSharedPtr<FOSCMessagePacket>(InMessage.GetPacket());

	if (MessagePacket.IsValid() && BundlePacket.IsValid())
	{
		BundlePacket->GetPackets().Add(MessagePacket);
	}

	return Bundle;
}

FOSCBundle& UOSCManager::AddBundleToBundle(const FOSCBundle& InBundle, FOSCBundle& OutBundle)
{
	const TSharedPtr<FOSCBundlePacket>& InBundlePacket = StaticCastSharedPtr<FOSCBundlePacket>(InBundle.GetPacket());
	const TSharedPtr<FOSCBundlePacket>& OutBundlePacket = StaticCastSharedPtr<FOSCBundlePacket>(OutBundle.GetPacket());

	if (InBundlePacket.IsValid() && OutBundlePacket.IsValid())
	{
		InBundlePacket->GetPackets().Add(OutBundlePacket);
	}

	return OutBundle;
}

FOSCMessage& UOSCManager::AddFloat(FOSCMessage& OutMessage, float InValue)
{
	const TSharedPtr<FOSCMessagePacket>& MessagePacket = StaticCastSharedPtr<FOSCMessagePacket>(OutMessage.GetPacket());
	MessagePacket->GetArguments().Add(FOSCType(InValue));
	return OutMessage;
}

FOSCMessage& UOSCManager::AddInt32(FOSCMessage& OutMessage, int32 InValue)
{
	const TSharedPtr<FOSCMessagePacket>& MessagePacket = StaticCastSharedPtr<FOSCMessagePacket>(OutMessage.GetPacket());
	MessagePacket->GetArguments().Add(FOSCType(InValue));
	return OutMessage;
}

FOSCMessage& UOSCManager::AddInt64(FOSCMessage& OutMessage, int64 InValue)
{
	const TSharedPtr<FOSCMessagePacket>& MessagePacket = StaticCastSharedPtr<FOSCMessagePacket>(OutMessage.GetPacket());
	MessagePacket->GetArguments().Add(FOSCType(InValue));
	return OutMessage;
}

FOSCMessage& UOSCManager::AddAddress(FOSCMessage& OutMessage, const FOSCAddress& InValue)
{
	const TSharedPtr<FOSCMessagePacket>& MessagePacket = StaticCastSharedPtr<FOSCMessagePacket>(OutMessage.GetPacket());
	MessagePacket->GetArguments().Add(FOSCType(InValue.GetFullPath()));
	return OutMessage;
}

FOSCMessage& UOSCManager::AddString(FOSCMessage& OutMessage, FString InValue)
{
	const TSharedPtr<FOSCMessagePacket>& MessagePacket = StaticCastSharedPtr<FOSCMessagePacket>(OutMessage.GetPacket());
	MessagePacket->GetArguments().Add(FOSCType(InValue));
	return OutMessage;
}

FOSCMessage& UOSCManager::AddBlob(FOSCMessage& OutMessage, const TArray<uint8>& OutValue)
{
	const TSharedPtr<FOSCMessagePacket>& MessagePacket = StaticCastSharedPtr<FOSCMessagePacket>(OutMessage.GetPacket());
	MessagePacket->GetArguments().Add(FOSCType(OutValue));
	return OutMessage;
}

FOSCMessage& UOSCManager::AddBool(FOSCMessage& OutMessage, bool InValue)
{
	const TSharedPtr<FOSCMessagePacket>& MessagePacket = StaticCastSharedPtr<FOSCMessagePacket>(OutMessage.GetPacket());
	MessagePacket->GetArguments().Add(FOSCType(InValue));
	return OutMessage;
}

TArray<FOSCBundle> UOSCManager::GetBundlesFromBundle(const FOSCBundle& InBundle)
{
	TArray<FOSCBundle> Bundles;
	if (InBundle.GetPacket().IsValid())
	{
		const TSharedPtr<FOSCBundlePacket>& BundlePacket = StaticCastSharedPtr<FOSCBundlePacket>(InBundle.GetPacket());
		for (int32 i = 0; i < BundlePacket->GetPackets().Num(); i++)
		{
			const TSharedPtr<IOSCPacket>& Packet = BundlePacket->GetPackets()[i];
			if (Packet->IsBundle())
			{
				Bundles.Emplace(StaticCastSharedPtr<FOSCMessagePacket>(Packet));
			}
		}
	}

	return Bundles;
}

FOSCMessage UOSCManager::GetMessageFromBundle(const FOSCBundle& InBundle, int32 InIndex, bool& bOutSucceeded)
{
	if (InBundle.GetPacket().IsValid())
	{
		const TSharedPtr<FOSCBundlePacket>& BundlePacket = StaticCastSharedPtr<FOSCBundlePacket>(InBundle.GetPacket());
		int32 Count = 0;
		for (const TSharedPtr<IOSCPacket>& Packet : BundlePacket->GetPackets())
		{
			if (Packet->IsMessage())
			{
				if (InIndex == Count)
				{
					bOutSucceeded = true;
					return FOSCMessage(StaticCastSharedPtr<FOSCMessagePacket>(Packet));
				}
				Count++;
			}
		}
	}

	bOutSucceeded = false;
	return FOSCMessage();
}

TArray<FOSCMessage> UOSCManager::GetMessagesFromBundle(const FOSCBundle& OutBundle)
{
	TArray<FOSCMessage> Messages;
	if (OutBundle.GetPacket().IsValid())
	{
		const TSharedPtr<FOSCBundlePacket>& BundlePacket = StaticCastSharedPtr<FOSCBundlePacket>(OutBundle.GetPacket());
		for (int32 i = 0; i < BundlePacket->GetPackets().Num(); i++)
		{
			const TSharedPtr<IOSCPacket>& Packet = BundlePacket->GetPackets()[i];
			if (Packet->IsMessage())
			{
				Messages.Emplace(StaticCastSharedPtr<FOSCMessagePacket>(Packet));
			}
		}
	}
	
	return Messages;
}

bool UOSCManager::GetAddress(const FOSCMessage& InMessage, const int32 InIndex, FOSCAddress& OutValue)
{
	if (const FOSCType* OSCType = OSC::GetOSCTypeAtIndex(InMessage, InIndex))
	{
		if (OSCType->IsString())
		{
			OutValue = FOSCAddress(OSCType->GetString());
			return OutValue.IsValidPath();
		}
		OSC_LOG_INVALID_TYPE_AT_INDEX(TEXT("String (OSCAddress)"), InIndex, InMessage);
	}

	OutValue = FOSCAddress();
	return false;
}

void UOSCManager::GetAllAddresses(const FOSCMessage& InMessage, TArray<FOSCAddress>& OutValues)
{
	if (InMessage.GetPacket().IsValid())
	{
		const TSharedPtr<FOSCMessagePacket>& MessagePacket = StaticCastSharedPtr<FOSCMessagePacket>(InMessage.GetPacket());
		const TArray<FOSCType>& Args = MessagePacket->GetArguments();
		for (int32 i = 0; i < Args.Num(); i++)
		{
			const FOSCType& OSCType = Args[i];
			if (OSCType.IsString())
			{
				FOSCAddress AddressToAdd = FOSCAddress(OSCType.GetString());
				if (AddressToAdd.IsValidPath())
				{
					OutValues.Add(MoveTemp(AddressToAdd));
				}
			}
		}
	}
}

bool UOSCManager::GetFloat(const FOSCMessage& InMessage, const int32 InIndex, float& OutValue)
{
	OutValue = 0.0f;
	if (const FOSCType* OSCType = OSC::GetOSCTypeAtIndex(InMessage, InIndex))
	{
		if (OSCType->IsFloat())
		{
			OutValue = OSCType->GetFloat();
			return true;
		}
		OSC_LOG_INVALID_TYPE_AT_INDEX(TEXT("Float"), InIndex, InMessage);
	}

	return false;
}

void UOSCManager::GetAllFloats(const FOSCMessage& InMessage, TArray<float>& OutValues)
{
	if (InMessage.GetPacket().IsValid())
	{
		const TSharedPtr<FOSCMessagePacket>& MessagePacket = StaticCastSharedPtr<FOSCMessagePacket>(InMessage.GetPacket());
		const TArray<FOSCType>& Args = MessagePacket->GetArguments();
		for (int32 i = 0; i < Args.Num(); i++)
		{
			const FOSCType& OSCType = Args[i];
			if (OSCType.IsFloat())
			{
				OutValues.Add(OSCType.GetFloat());
			}
		}
	}
}

bool UOSCManager::GetInt32(const FOSCMessage& InMessage, const int32 InIndex, int32& OutValue)
{
	OutValue = 0;
	if (const FOSCType* OSCType = OSC::GetOSCTypeAtIndex(InMessage, InIndex))
	{
		if (OSCType->IsInt32())
		{
			OutValue = OSCType->GetInt32();
			return true;
		}
		OSC_LOG_INVALID_TYPE_AT_INDEX(TEXT("Int32"), InIndex, InMessage);
	}

	return false;
}

void UOSCManager::GetAllInt32s(const FOSCMessage& InMessage, TArray<int32>& OutValues)
{
	if (InMessage.GetPacket().IsValid())
	{
		const TSharedPtr<FOSCMessagePacket>& MessagePacket = StaticCastSharedPtr<FOSCMessagePacket>(InMessage.GetPacket());
		const TArray<FOSCType>& Args = MessagePacket->GetArguments();
		for (int32 i = 0; i < Args.Num(); i++)
		{
			const FOSCType& OSCType = Args[i];
			if (OSCType.IsInt32())
			{
				OutValues.Add(OSCType.GetInt32());
			}
		}
	}
}

bool UOSCManager::GetInt64(const FOSCMessage& InMessage, const int32 InIndex, int64& OutValue)
{
	OutValue = 0l;
	if (const FOSCType* OSCType = OSC::GetOSCTypeAtIndex(InMessage, InIndex))
	{
		if (OSCType->IsInt64())
		{
			OutValue = OSCType->GetInt64();
			return true;
		}
		OSC_LOG_INVALID_TYPE_AT_INDEX(TEXT("Int64"), InIndex, InMessage);
	}

	return false;
}

void UOSCManager::GetAllInt64s(const FOSCMessage& InMessage, TArray<int64>& OutValues)
{
	const TSharedPtr<FOSCMessagePacket>& MessagePacket = StaticCastSharedPtr<FOSCMessagePacket>(InMessage.GetPacket());
	if (MessagePacket.IsValid())
	{
		const TArray<FOSCType>& Args = MessagePacket->GetArguments();
		for (int32 i = 0; i < Args.Num(); i++)
		{
			const FOSCType& OSCType = Args[i];
			if (OSCType.IsInt64())
			{
				OutValues.Add(OSCType.GetInt64());
			}
		}
	}
}

bool UOSCManager::GetString(const FOSCMessage& InMessage, const int32 InIndex, FString& OutValue)
{
	if (const FOSCType* OSCType = OSC::GetOSCTypeAtIndex(InMessage, InIndex))
	{
		if (OSCType->IsString())
		{
			OutValue = OSCType->GetString();
			return true;
		}
		OSC_LOG_INVALID_TYPE_AT_INDEX(TEXT("String"), InIndex, InMessage);
	}

	OutValue.Reset();
	return false;
}

void UOSCManager::GetAllStrings(const FOSCMessage& InMessage, TArray<FString>& OutValues)
{
	const TSharedPtr<FOSCMessagePacket>& MessagePacket = StaticCastSharedPtr<FOSCMessagePacket>(InMessage.GetPacket());
	if (MessagePacket.IsValid())
	{
		const TArray<FOSCType>& Args = MessagePacket->GetArguments();
		for (int32 i = 0; i < Args.Num(); i++)
		{
			const FOSCType& OSCType = Args[i];
			if (OSCType.IsString())
			{
				OutValues.Add(OSCType.GetString());
			}
		}
	}
}

bool UOSCManager::GetBool(const FOSCMessage& InMessage, const int32 InIndex, bool& OutValue)
{
	OutValue = false;
	if (const FOSCType* OSCType = OSC::GetOSCTypeAtIndex(InMessage, InIndex))
	{
		if (OSCType->IsBool())
		{
			OutValue = OSCType->GetBool();
			return true;
		}
		OSC_LOG_INVALID_TYPE_AT_INDEX(TEXT("Bool"), InIndex, InMessage);
	}

	return false;
}

void UOSCManager::GetAllBools(const FOSCMessage& InMessage, TArray<bool>& OutValues)
{
	const TSharedPtr<FOSCMessagePacket>& MessagePacket = StaticCastSharedPtr<FOSCMessagePacket>(InMessage.GetPacket());
	if (MessagePacket.IsValid())
	{
		const TArray<FOSCType>& Args = MessagePacket->GetArguments();
		for (int32 i = 0; i < Args.Num(); i++)
		{
			const FOSCType& OSCType = Args[i];
			if (OSCType.IsBool())
			{
				OutValues.Add(OSCType.GetBool());
			}
		}
	}
}

bool UOSCManager::GetBlob(const FOSCMessage& InMessage, const int32 InIndex, TArray<uint8>& OutValue)
{
	OutValue.Reset();
	if (const FOSCType* OSCType = OSC::GetOSCTypeAtIndex(InMessage, InIndex))
	{
		if (OSCType->IsBlob())
		{
			OutValue = OSCType->GetBlob();
			return true;
		}
		OSC_LOG_INVALID_TYPE_AT_INDEX(TEXT("Blob"), InIndex, InMessage);
	}

	return false;
}

bool UOSCManager::OSCAddressIsValidPath(const FOSCAddress& InAddress)
{
	return InAddress.IsValidPath();
}

bool UOSCManager::OSCAddressIsValidPattern(const FOSCAddress& InAddress)
{
	return InAddress.IsValidPattern();
}

FOSCAddress UOSCManager::ConvertStringToOSCAddress(const FString& InString)
{
	return FOSCAddress(InString);
}

UObject* UOSCManager::FindObjectAtOSCAddress(const FOSCAddress& InAddress)
{
	FSoftObjectPath Path(ObjectPathFromOSCAddress(InAddress));
	if (Path.IsValid())
	{
		return Path.TryLoad();
	}

	UE_LOG(LogOSC, Verbose, TEXT("Failed to load object from OSCAddress '%s'"), *InAddress.GetFullPath());
	return nullptr;
}

FOSCAddress UOSCManager::OSCAddressFromObjectPath(UObject* InObject)
{
	const FString Path = FPaths::ChangeExtension(InObject->GetPathName(), FString());
	return FOSCAddress(Path);
}

FOSCAddress UOSCManager::OSCAddressFromObjectPathString(const FString& InPathName)
{
	TArray<FString> PartArray;
	InPathName.ParseIntoArray(PartArray, TEXT("\'"));

	// Type declaration at beginning of path. Assumed to be in the form <SomeTypeContainer1'/Container2/ObjectName.ObjectName'>
	if (PartArray.Num() > 1)
	{
		const FString NoExtPath = FPaths::SetExtension(PartArray[1], TEXT(""));
		return FOSCAddress(NoExtPath);
	}

	// No type declaration at beginning of path. Assumed to be in the form <Container1/Container2/ObjectName.ObjectName>
	if (PartArray.Num() > 0)
	{
		const FString NoExtPath = FPaths::SetExtension(PartArray[0], TEXT(""));
		return FOSCAddress(NoExtPath);
	}

	// Invalid address
	return FOSCAddress();
}

FString UOSCManager::ObjectPathFromOSCAddress(const FOSCAddress& InAddress)
{
	const FString Path = InAddress.GetFullPath() + TEXT(".") + InAddress.GetMethod();
	return Path;
}

FOSCAddress& UOSCManager::OSCAddressPushContainer(FOSCAddress& OutAddress, const FString& InToAppend)
{
	OutAddress.PushContainer(InToAppend);
	return OutAddress;
}

FOSCAddress& UOSCManager::OSCAddressPushContainers(FOSCAddress& OutAddress, const TArray<FString>& InToAppend)
{
	OutAddress.PushContainers(InToAppend);
	return OutAddress;
}

FString UOSCManager::OSCAddressPopContainer(FOSCAddress& OutAddress)
{
	return OutAddress.PopContainer();
}

TArray<FString> UOSCManager::OSCAddressPopContainers(FOSCAddress& OutAddress, int32 InNumContainers)
{
	return OutAddress.PopContainers(InNumContainers);
}

FOSCAddress& UOSCManager::OSCAddressRemoveContainers(FOSCAddress& OutAddress, int32 InIndex, int32 InCount)
{
	OutAddress.RemoveContainers(InIndex, InCount);
	return OutAddress;
}

bool UOSCManager::OSCAddressPathMatchesPattern(const FOSCAddress& InPattern, const FOSCAddress& InPath)
{
	return InPattern.Matches(InPath);
}

FOSCAddress UOSCManager::GetOSCMessageAddress(const FOSCMessage& InMessage)
{
	return InMessage.GetAddress();
}

FOSCMessage& UOSCManager::SetOSCMessageAddress(FOSCMessage& OutMessage, const FOSCAddress& InAddress)
{
	OutMessage.SetAddress(InAddress);
	return OutMessage;
}

FString UOSCManager::GetOSCAddressContainer(const FOSCAddress& InAddress, int32 InIndex)
{
	return InAddress.GetContainer(InIndex);
}

TArray<FString> UOSCManager::GetOSCAddressContainers(const FOSCAddress& InAddress)
{
	TArray<FString> Containers;
	InAddress.GetContainers(Containers);
	return Containers;
}

FString UOSCManager::GetOSCAddressContainerPath(const FOSCAddress& InAddress)
{
	return InAddress.GetContainerPath();
}

FString UOSCManager::GetOSCAddressFullPath(const FOSCAddress& InAddress)
{
	return InAddress.GetFullPath();
}

FString UOSCManager::GetOSCAddressMethod(const FOSCAddress& InAddress)
{
	return InAddress.GetMethod();
}

FOSCAddress& UOSCManager::ClearOSCAddressContainers(FOSCAddress& OutAddress)
{
	OutAddress.ClearContainers();
	return OutAddress;
}

FOSCAddress& UOSCManager::SetOSCAddressMethod(FOSCAddress& OutAddress, const FString& InMethod)
{
	OutAddress.SetMethod(InMethod);
	return OutAddress;
}