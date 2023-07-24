// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Kismet/BlueprintFunctionLibrary.h"

#include "OSCAddress.h"
#include "OSCBundle.h"
#include "OSCClient.h"
#include "OSCMessage.h"
#include "SoundControlBusMix.h"
#include "SoundModulationValue.h"

#include "OSCModulationMixingStatics.generated.h"

// Forward Declarations
class USoundControlBusMix;

UENUM()
enum class EOSCModulationBundle : uint8
{
	Invalid,
	LoadMix,
	Count UMETA(Hidden)
};

UENUM()
enum class EOSCModulationMessage : uint8
{
	Invalid,
	LoadProfile,
	SaveProfile,
	Count UMETA(Hidden)
};

UCLASS()
class OSCMODULATIONMIXING_API UOSCModulationMixingStatics : public UBlueprintFunctionLibrary
{
	GENERATED_UCLASS_BODY()
	
	/** Returns OSC Address pattern for loading a mix */
	UFUNCTION(BlueprintCallable, Category = "Audio|OSC|Modulation", DisplayName = "Get Load Mix Pattern")
	static FOSCAddress GetMixLoadPattern();

	/** Returns OSC Address path for loading a profile */
	UFUNCTION(BlueprintCallable, Category = "Audio|OSC|Modulation", DisplayName = "Get Load Profile Path")
	static FOSCAddress GetProfileLoadPath();

	/** Returns OSC Address path for saving a profile */
	UFUNCTION(BlueprintCallable, Category = "Audio|OSC|Modulation", DisplayName = "Get Save Profile Path")
	static FOSCAddress GetProfileSavePath();

	/** Converts stage array to OSCBundle representation to send over network via OSC protocol */
	UFUNCTION(BlueprintCallable, Category = "Audio|OSC|Modulation", DisplayName = "Copy Mix Stages to OSC Bundle", meta = (WorldContext = "WorldContextObject"))
	static void CopyStagesToOSCBundle(UObject* WorldContextObject, const FOSCAddress& PathAddress, const TArray<FSoundControlBusMixStage>& Stages, UPARAM(ref) FOSCBundle& Bundle);

	/** Converts Control Bus Mix to OSCBundle representation to send over network via OSC protocol */
	UFUNCTION(BlueprintCallable, Category = "Audio|OSC|Modulation", DisplayName = "Copy Mix State to OSC Bundle", meta = (WorldContext = "WorldContextObject"))
	static void CopyMixToOSCBundle(UObject* WorldContextObject, USoundControlBusMix* Mix, UPARAM(ref) FOSCBundle& Bundle);

	/** Returns whether bundle contains recognized payload of OSC Modulation Data */
	UFUNCTION(BlueprintCallable, Category = "Audio|OSC|Modulation", DisplayName = "Get OSC Modulation Bundle Type")
	static EOSCModulationBundle GetOSCBundleType(const FOSCBundle& Bundle);

	/** Request mix update from server with loaded content. */
	UFUNCTION(BlueprintCallable, Category = "Audio|OSC|Modulation", DisplayName = "Request Mix State Update", meta = (WorldContext = "WorldContextObject"))
	static void RequestMix(UObject* WorldContextObject, UOSCClient* Client, const FOSCAddress& MixPath);

	/** Converts OSCBundle to Control Bus Values & Mix Path from which it came */
	UFUNCTION(BlueprintCallable, Category = "Audio|OSC|Modulation", DisplayName = "OSCBundle To Stage Values", meta = (WorldContext = "WorldContextObject"))
	static UPARAM(DisplayName = "Bus Values") TArray<FSoundModulationMixValue> OSCBundleToStageValues(UObject* WorldContextObject, const FOSCBundle& Bundle, FOSCAddress& MixPath, TArray<FOSCAddress>& BusPaths, TArray<FString>& BusClassNames);
};
