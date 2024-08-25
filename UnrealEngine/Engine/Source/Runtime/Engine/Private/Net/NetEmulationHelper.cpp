// Copyright Epic Games, Inc. All Rights Reserved.

#include "NetEmulationHelper.h"

#include "Engine/Engine.h"
#include "Engine/NetworkSettings.h"

#if DO_ENABLE_NET_TEST

namespace UE::Net::Private::NetEmulationHelper
{
/** Global that stores the network emulation values outside the NetDriver lifetime */
static TOptional<FPacketSimulationSettings> PersistentPacketSimulationSettings;

void CreatePersistentSimulationSettings()
{
	if (!PersistentPacketSimulationSettings.IsSet())
	{
		PersistentPacketSimulationSettings.Emplace(FPacketSimulationSettings());
	}
}

void ApplySimulationSettingsOnNetDrivers(UWorld* World, const FPacketSimulationSettings& Settings)
{
	// Execute on all active NetDrivers
	FWorldContext& Context = GEngine->GetWorldContextFromWorldChecked(World);
	for (const FNamedNetDriver& ActiveNetDriver : Context.ActiveNetDrivers)
	{
		if (ActiveNetDriver.NetDriver)
		{
			ActiveNetDriver.NetDriver->SetPacketSimulationSettings(Settings);
		}
	}
}

bool HasPersistentPacketEmulationSettings()
{
	return PersistentPacketSimulationSettings.IsSet();
}

void ApplyPersistentPacketEmulationSettings(UNetDriver* NetDriver)
{
	NetDriver->SetPacketSimulationSettings(PersistentPacketSimulationSettings.GetValue());
}

FAutoConsoleCommandWithWorldArgsAndOutputDevice NetEmulationPktEmulationProfile(TEXT("NetEmulation.PktEmulationProfile"), TEXT("Apply a preconfigured emulation profile."),
FConsoleCommandWithWorldArgsAndOutputDeviceDelegate::CreateStatic([](const TArray<FString>& Args, UWorld* World, FOutputDevice& Output)
{
	bool bProfileApplied(false);
	if (Args.Num() > 0)
	{
		FString CmdParams = FString::Printf(TEXT("PktEmulationProfile=%s"), *(Args[0]));

		CreatePersistentSimulationSettings();

		bProfileApplied = PersistentPacketSimulationSettings.GetValue().ParseSettings(*CmdParams, nullptr);

		if (bProfileApplied)
		{
			ApplySimulationSettingsOnNetDrivers(World, PersistentPacketSimulationSettings.GetValue());
		}
		else
		{
			Output.Log(FString::Printf(TEXT("EmulationProfile: %s was not found in Engine.ini"), *(Args[0])));
		}
	}
	else
	{
		Output.Log(FString::Printf(TEXT("Missing emulation profile name")));
	}

	if (!bProfileApplied)
	{
		if (const UNetworkSettings* NetworkSettings = GetDefault<UNetworkSettings>())
		{
			Output.Log(TEXT("List of some supported emulation profiles:"));
			for (const FNetworkEmulationProfileDescription& ProfileDesc : NetworkSettings->NetworkEmulationProfiles)
			{
				Output.Log(FString::Printf(TEXT("%s"), *ProfileDesc.ProfileName));
			}
		}
	}
}));

FAutoConsoleCommandWithWorld NetEmulationOff(TEXT("NetEmulation.Off"), TEXT("Turn off network emulation"),
FConsoleCommandWithWorldDelegate::CreateStatic([](UWorld* World)
{
	CreatePersistentSimulationSettings();
	PersistentPacketSimulationSettings.GetValue().ResetSettings();
	ApplySimulationSettingsOnNetDrivers(World, PersistentPacketSimulationSettings.GetValue());
}));


FAutoConsoleCommandWithWorldArgsAndOutputDevice NetEmulationDropNothingFunction(TEXT("NetEmulation.DropNothing"), TEXT("Disables any RPC drop settings previously set."),
FConsoleCommandWithWorldArgsAndOutputDeviceDelegate::CreateStatic([](const TArray<FString>& Args, UWorld* World, FOutputDevice& Output)
{
	UNetDriver* NetDriver = World->NetDriver;
	if (!NetDriver)
	{
		return;
	}

	NetDriver->SendRPCDel.Unbind();
}));

FAutoConsoleCommandWithWorldArgsAndOutputDevice NetEmulationDropAnyUnreliableFunction(TEXT("NetEmulation.DropAnyUnreliable"), TEXT("Drop any sent unreliable RPCs. (optional)<0-100> to set the drop percentage (default is 20)."),
FConsoleCommandWithWorldArgsAndOutputDeviceDelegate::CreateStatic([](const TArray<FString>& Args, UWorld* World, FOutputDevice& Output)
{
	UNetDriver* NetDriver = World->NetDriver;
	if (!NetDriver)
	{
		return;
	}

	float DropPercentage = 20.f;
	if (Args.Num() > 0)
	{
		float DropParam = DropPercentage;
		LexFromString(DropParam, *Args[0]);
		if (DropParam > 0.0f)
		{
			DropPercentage = FMath::Min(DropParam, 100.f);
		}
	}

	NetDriver->SendRPCDel.BindLambda([DropPercentage](AActor* Actor, UFunction* Function, void* Parameters, FOutParmRec* OutParms, FFrame* Stack, UObject* SubObject, bool& bOutBlockSendRPC)
	{
		if ((Function->FunctionFlags & FUNC_NetReliable) == 0)
		{
			const float RandValue = FMath::FRand();
			bOutBlockSendRPC = RandValue > (DropPercentage * 0.01f);

			if (SubObject)
			{
				UE_LOG(LogNetTraffic, Log, TEXT("      Dropped unreliable RPC %s::%s : %s"), *GetFullNameSafe(Actor), *GetNameSafe(SubObject), *GetNameSafe(Function));
			}
			else
			{
				UE_LOG(LogNetTraffic, Log, TEXT("      Dropped unreliable RPC %s : %s"), *GetFullNameSafe(Actor), *GetNameSafe(Function));
			}
		}
	});

	UE_LOG(LogNetTraffic, Warning, TEXT("Will start dropping %.2f%% of all unreliable RPCs"), DropPercentage);
}));

FAutoConsoleCommandWithWorldArgsAndOutputDevice NetEmulationDropUnreliableOfActorClassFunction(TEXT("NetEmulation.DropUnreliableOfActorClass"),
	TEXT("Drop random unreliable RPCs sent on actors of the given class type. "
		"<ActorClassName> Class name to match with (can be a substring). "
		"(optional)<0-100> to set the drop percentage (default is 20)."),
FConsoleCommandWithWorldArgsAndOutputDeviceDelegate::CreateStatic([](const TArray<FString>& Args, UWorld* World, FOutputDevice& Output)
{
	UNetDriver* NetDriver = World->NetDriver;
	if (!NetDriver)
	{
		return;
	}

	if (Args.Num() < 1)
	{
		UE_LOG(LogNet, Warning, TEXT("No class name parameter passed to NetEmulation.DropUnreliableOfActorClass"));
		return;
	}

	FString ClassNameParam = Args[0];

	float DropPercentage = 20.f;
	if (Args.Num() > 1)
	{
		float DropParam = DropPercentage;
		LexFromString(DropParam, *Args[1]);
		if (DropParam > 0.0f)
		{
			DropPercentage = FMath::Min(DropParam, 100.f);
		}
	}

	NetDriver->SendRPCDel.BindLambda([ClassNameParam, DropPercentage](AActor* Actor, UFunction* Function, void* Parameters, FOutParmRec* OutParms, FFrame* Stack, UObject* SubObject, bool& bOutBlockSendRPC)
	{
		if ((Function->FunctionFlags & FUNC_NetReliable) == 0)
		{
			const float RandValue = FMath::FRand();
			if (RandValue <= (DropPercentage * 0.01f))
			{
				return;
			}

			bool bMatches = false;

			UClass* Class = Actor->GetClass();
			while (Class && !bMatches)
			{
				bMatches = Class->GetName().Contains(*ClassNameParam);
				Class = Class->GetSuperClass();
			}

			if (bMatches)
			{
				bOutBlockSendRPC = true;

				if (SubObject)
				{
					UE_LOG(LogNetTraffic, Log, TEXT("      Dropped unreliable RPC %s::%s : %s"), *GetFullNameSafe(Actor), *GetNameSafe(SubObject), *GetNameSafe(Function));
				}
				else
				{
					UE_LOG(LogNetTraffic, Log, TEXT("      Dropped unreliable RPC %s : %s"), *GetFullNameSafe(Actor), *GetNameSafe(Function));
				}
			}
		}
	});

	UE_LOG(LogNetTraffic, Warning, TEXT("Will start dropping %.2f%% of all unreliable RPCs of actors of class: %s"), DropPercentage, *ClassNameParam);
}));

FAutoConsoleCommandWithWorldArgsAndOutputDevice NetEmulationDropUnreliableRPCFunction(TEXT("NetEmulation.DropUnreliableRPC"),
	TEXT("Drop randomly the unreliable RPCs of the given name. "
		"<RPCName> The name of the RPC (can be a substring). "
		"(optional)<0-100> to set the drop percentage (default is 20)."),
FConsoleCommandWithWorldArgsAndOutputDeviceDelegate::CreateStatic([](const TArray<FString>& Args, UWorld* World, FOutputDevice& Output)
{
	UNetDriver* NetDriver = World->NetDriver;
	if (!NetDriver)
	{
		return;
	}

	if (Args.Num() < 1)
	{
		UE_LOG(LogNet, Warning, TEXT("No RPC name parameter passed to NetEmulation.DropUnreliableRPC"));
		return;
	}

	FString RPCNameParam = Args[0];

	float DropPercentage = 20.f;
	if (Args.Num() > 1)
	{
		float DropParam = DropPercentage;
		LexFromString(DropParam, *Args[1]);
		if (DropParam > 0.0f)
		{
			DropPercentage = FMath::Min(DropParam, 100.f);
		}
	}

	NetDriver->SendRPCDel.BindLambda([RPCNameParam, DropPercentage](AActor* Actor, UFunction* Function, void* Parameters, FOutParmRec* OutParms, FFrame* Stack, UObject* SubObject, bool& bOutBlockSendRPC)
	{
		if ((Function->FunctionFlags & FUNC_NetReliable) == 0)
		{
			const float RandValue = FMath::FRand();
			if (RandValue <= (DropPercentage * 0.01f))
			{
				return;
			}

			if (Function->GetName().Contains(*RPCNameParam))
			{
				bOutBlockSendRPC = true;

				if (SubObject)
				{
					UE_LOG(LogNetTraffic, Log, TEXT("      Dropped unreliable RPC %s::%s : %s"), *GetFullNameSafe(Actor), *GetNameSafe(SubObject), *GetNameSafe(Function));
				}
				else
				{
					UE_LOG(LogNetTraffic, Log, TEXT("      Dropped unreliable RPC %s : %s"), *GetFullNameSafe(Actor), *GetNameSafe(Function));
				}
			}
		}
	});

	UE_LOG(LogNetTraffic, Warning, TEXT("Will start dropping %.2f%% of all unreliable RPCs named: %s"), DropPercentage, *RPCNameParam);
}));

FAutoConsoleCommandWithWorldArgsAndOutputDevice NetEmulationDropUnreliableOfSubObjectClassFunction(TEXT("NetEmulation.DropUnreliableOfSubObjectClass"),
	TEXT("Drop randomly the unreliable RPCs of a subobject of the given class. "
		"<SubObjectClassName> The name of the RPC (can be a substring). "
		"(optional)<0-100> to set the drop percentage (default is 20)."),
FConsoleCommandWithWorldArgsAndOutputDeviceDelegate::CreateStatic([](const TArray<FString>& Args, UWorld* World, FOutputDevice& Output)
{
	UNetDriver* NetDriver = World->NetDriver;
	if (!NetDriver)
	{
		return;
	}

	if (Args.Num() < 1)
	{
		UE_LOG(LogNet, Warning, TEXT("No SubObject name parameter passed to NetEmulation.DropUnreliableOfSubObjectClass"));
		return;
	}

	FString SubObjectClassNameParam = Args[0];

	float DropPercentage = 20.f;
	if (Args.Num() > 1)
	{
		float DropParam = DropPercentage;
		LexFromString(DropParam, *Args[1]);
		if (DropParam > 0.0f)
		{
			DropPercentage = FMath::Min(DropParam, 100.f);
		}
	}

	NetDriver->SendRPCDel.BindLambda([SubObjectClassNameParam, DropPercentage](AActor* Actor, UFunction* Function, void* Parameters, FOutParmRec* OutParms, FFrame* Stack, UObject* SubObject, bool& bOutBlockSendRPC)
	{
		if (SubObject && (Function->FunctionFlags & FUNC_NetReliable) == 0)
		{
			const float RandValue = FMath::FRand();
			if (RandValue <= (DropPercentage * 0.01f))
			{
				return;
			}

			bool bMatches = false;

			UClass* Class = SubObject->GetClass();
			while (Class && !bMatches)
			{
				bMatches = Class->GetName().Contains(*SubObjectClassNameParam);
				Class = Class->GetSuperClass();
			}

			if (bMatches)
			{
				bOutBlockSendRPC = true;

				UE_LOG(LogNetTraffic, Log, TEXT("      Dropped unreliable RPC %s::%s : %s"), *GetFullNameSafe(Actor), *GetNameSafe(SubObject), *GetNameSafe(Function));
			}
		}
	});

	UE_LOG(LogNetTraffic, Warning, TEXT("Will start dropping %.2f%% of all unreliable RPCs for subobjects: %s"), DropPercentage, *SubObjectClassNameParam);
}));

#define BUILD_NETEMULATION_CONSOLE_COMMAND(CommandName, CommandHelp) FAutoConsoleCommandWithWorldAndArgs NetEmulation##CommandName(TEXT("NetEmulation."#CommandName), TEXT(CommandHelp), \
	FConsoleCommandWithWorldAndArgsDelegate::CreateStatic([](const TArray<FString>& Args, UWorld* World) \
	{ \
		if (Args.Num() > 0) \
		{ \
			CreatePersistentSimulationSettings(); \
			FString CmdParams = FString::Printf(TEXT(#CommandName"=%s"), *(Args[0])); \
			PersistentPacketSimulationSettings.GetValue().ParseSettings(*CmdParams, nullptr); \
			ApplySimulationSettingsOnNetDrivers(World, PersistentPacketSimulationSettings.GetValue()); \
		} \
	}));

	BUILD_NETEMULATION_CONSOLE_COMMAND(PktLoss, "Simulates network packet loss");
	BUILD_NETEMULATION_CONSOLE_COMMAND(PktOrder, "Simulates network packets received out of order");
	BUILD_NETEMULATION_CONSOLE_COMMAND(PktDup, "Simulates sending/receiving duplicate network packets");
	BUILD_NETEMULATION_CONSOLE_COMMAND(PktLag, "Simulates network packet lag");
	BUILD_NETEMULATION_CONSOLE_COMMAND(PktLagVariance, "Simulates variable network packet lag");
	BUILD_NETEMULATION_CONSOLE_COMMAND(PktLagMin, "Sets minimum outgoing packet latency");
	BUILD_NETEMULATION_CONSOLE_COMMAND(PktLagMax, "Sets maximum outgoing packet latency)");
	BUILD_NETEMULATION_CONSOLE_COMMAND(PktIncomingLagMin, "Sets minimum incoming packet latency");
	BUILD_NETEMULATION_CONSOLE_COMMAND(PktIncomingLagMax, "Sets maximum incoming packet latency");
	BUILD_NETEMULATION_CONSOLE_COMMAND(PktIncomingLoss, "Simulates incoming packet loss");
	BUILD_NETEMULATION_CONSOLE_COMMAND(PktJitter, "Simulates outgoing packet jitter");


} // end namespace UE::Net::Private::NetEmulationHelper

#endif //#if DO_ENABLE_NET_TEST