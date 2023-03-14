// Copyright Epic Games, Inc. All Rights Reserved.

#include "OSCModulationMixingStatics.h"

#include "OSCClient.h"
#include "OSCManager.h"
#include "OSCModulationMixing.h"
#include "OSCServer.h"

#include "AudioAnalytics.h"
#include "HAL/IConsoleManager.h"
#include "SoundControlBus.h"
#include "SoundControlBusMix.h"
#include "UObject/WeakObjectPtrTemplates.h"


namespace OSCModulation
{
	namespace Addresses
	{
		static const FOSCAddress MixLoad	 = FString(TEXT("/Mix/Load"));
		static const FOSCAddress ProfileLoad = FString(TEXT("/Mix/Profile/Load"));
		static const FOSCAddress ProfileSave = FString(TEXT("/Mix/Profile/Save"));
	} // namespace Addresses
} // namespace OSCModulation

UOSCModulationMixingStatics::UOSCModulationMixingStatics(const FObjectInitializer& ObjectInitializer)
: Super(ObjectInitializer)
{
}

void UOSCModulationMixingStatics::CopyStagesToOSCBundle(UObject* WorldContextObject, const FOSCAddress& InPathAddress, const TArray<FSoundControlBusMixStage>& InStages, FOSCBundle& OutBundle)
{
	FOSCMessage RequestMessage;
	RequestMessage.SetAddress(OSCModulation::Addresses::MixLoad);
	UOSCManager::AddMessageToBundle(RequestMessage, OutBundle);

	FOSCMessage Message;
	Message.SetAddress(InPathAddress);
	UOSCManager::AddMessageToBundle(Message, OutBundle);

	for (const FSoundControlBusMixStage& Stage : InStages)
	{
		if (Stage.Bus)
		{
			FOSCMessage StageMessage;
			StageMessage.SetAddress(UOSCManager::OSCAddressFromObjectPath(Stage.Bus));

			UOSCManager::AddFloat(StageMessage, Stage.Value.AttackTime);
			UOSCManager::AddFloat(StageMessage, Stage.Value.ReleaseTime);
			UOSCManager::AddFloat(StageMessage, Stage.Value.TargetValue);

			UClass* BusClass = Stage.Bus->GetClass();
			const FString ClassName = BusClass ? BusClass->GetName() : FString();
			UOSCManager::AddString(StageMessage, ClassName);

			UOSCManager::AddMessageToBundle(StageMessage, OutBundle);
		}
	}
}

void UOSCModulationMixingStatics::CopyMixToOSCBundle(UObject* WorldContextObject, USoundControlBusMix* InMix, UPARAM(ref) FOSCBundle& OutBundle)
{
	if (!InMix)
	{
		return;
	}

	CopyStagesToOSCBundle(WorldContextObject, UOSCManager::OSCAddressFromObjectPath(InMix), InMix->MixStages, OutBundle);
}

FOSCAddress UOSCModulationMixingStatics::GetProfileLoadPath()
{
	return OSCModulation::Addresses::ProfileLoad;
}

FOSCAddress UOSCModulationMixingStatics::GetProfileSavePath()
{
	Audio::Analytics::RecordEvent_Usage(TEXT("OSCModulationMixing.GetProfileSavePath"));
	return OSCModulation::Addresses::ProfileSave;
}

FOSCAddress UOSCModulationMixingStatics::GetMixLoadPattern()
{
	return FOSCAddress(OSCModulation::Addresses::MixLoad.GetFullPath() / TEXT("*"));
}

EOSCModulationBundle UOSCModulationMixingStatics::GetOSCBundleType(const FOSCBundle& InBundle)
{
	TArray<FOSCMessage> Messages = UOSCManager::GetMessagesFromBundle(InBundle);
	if (Messages.Num() == 0)
	{
		return EOSCModulationBundle::Invalid;
	}

	FOSCAddress BundleAddress = UOSCManager::GetOSCMessageAddress(Messages[0]);
	if (BundleAddress == OSCModulation::Addresses::MixLoad)
	{
		return EOSCModulationBundle::LoadMix;
	}
	static_assert(static_cast<int32>(EOSCModulationBundle::Count) == 2, "Possible missing bundle case coverage");
	// Add additional bundle types here

	return EOSCModulationBundle::Invalid;
}

void UOSCModulationMixingStatics::RequestMix(UObject* WorldContextObject, UOSCClient* InClient, const FOSCAddress& InMixPath)
{
	if (InClient)
	{
		FOSCMessage Message;

		const FOSCAddress MixAddress = UOSCManager::OSCAddressFromObjectPathString(InMixPath.GetFullPath());
		Message.SetAddress(OSCModulation::Addresses::MixLoad / MixAddress);

		InClient->SendOSCMessage(Message);
	}
}

TArray<FSoundModulationMixValue> UOSCModulationMixingStatics::OSCBundleToStageValues(UObject* WorldContextObject, const FOSCBundle& InBundle, FOSCAddress& OutMixPath, TArray<FOSCAddress>& OutBusPaths, TArray<FString>& OutBusClassNames)
{
	TArray<FSoundModulationMixValue> StageArray;
	OutBusPaths.Reset();

	const TArray<FOSCMessage> Messages = UOSCManager::GetMessagesFromBundle(InBundle);
	if (Messages.Num() > 1)
	{
		if (Messages[0].GetAddress() == OSCModulation::Addresses::MixLoad)
		{
			OutMixPath = Messages[1].GetAddress();

			for (int32 i = 2; i < Messages.Num(); ++i)
			{
				FSoundModulationMixValue Value;
				UOSCManager::GetFloat(Messages[i], 0, Value.AttackTime);
				UOSCManager::GetFloat(Messages[i], 1, Value.ReleaseTime);
				UOSCManager::GetFloat(Messages[i], 2, Value.TargetValue);

				FString BusClass;
				UOSCManager::GetString(Messages[i], 3, BusClass);
				OutBusClassNames.Add(BusClass);

				OutBusPaths.Add(UOSCManager::GetOSCMessageAddress(Messages[i]));
				StageArray.Add(Value);
			}
			return StageArray;
		}
	}

	OutMixPath = FOSCAddress();
	return StageArray;
}
