// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IDMXControlConsoleFaderGroupElement.h"

#include "DMXProtocolTypes.h"

#include "Tickable.h"
#include "UObject/Object.h"

#include "DMXControlConsoleFaderBase.generated.h"

class UDMXControlConsoleFaderGroup;
class UDMXControlConsoleFloatOscillator;


/** Base class for a Fader in the DMX Control Console. */
UCLASS(Abstract, AutoExpandCategories = ("DMX Fader", "DMX Fader|Oscillator"))
class DMXCONTROLCONSOLE_API UDMXControlConsoleFaderBase
	: public UObject
	, public FTickableGameObject
	, public IDMXControlConsoleFaderGroupElement
{
	GENERATED_BODY()

public:
	/** Constructor */
	UDMXControlConsoleFaderBase();

	//~ Being IDMXControlConsoleFaderGroupElement interface
	virtual UDMXControlConsoleFaderGroup& GetOwnerFaderGroupChecked() const override;
	virtual int32 GetIndex() const override;
	virtual const TArray<UDMXControlConsoleFaderBase*>& GetFaders() const override { return ThisFaderAsArray; }
	virtual int32 GetUniverseID() const override PURE_VIRTUAL(UDMXControlConsoleFaderBase::GetUniverse, return 1;);
	virtual int32 GetStartingAddress() const override PURE_VIRTUAL(UDMXControlConsoleFaderBase::GetStartingAddress, return 1;);
	virtual int32 GetEndingAddress() const override { return EndingAddress; }
	virtual void Destroy() override;
	//~ End IDMXControlConsoleFaderGroupElement interface

	/** Gets the name of the Fader */
	const FString& GetFaderName() const { return FaderName; };

	/** Sets the name of the Fader */
	virtual void SetFaderName(const FString& NewName); 

	/** Returns the current value of the fader */
	uint32 GetValue() const { return Value; }

	/** Sets the current value of the fader */
	virtual void SetValue(const uint32 NewValue);

	/** Gets the min value of the fader */
	uint32 GetMinValue() const { return MinValue; }

	/** Gets the max value of the fader */
	uint32 GetMaxValue() const { return MaxValue; }

	/** Gets wheter this Fader cans send DMX data */
	bool IsMuted() const { return bIsMuted; }

	/** Sets mute state of thi fader */
	virtual void SetMute(bool bMute);

	/** Mutes/Unmutes this fader */
	virtual void ToggleMute();

	/** Returns the data type of this fader */
	virtual EDMXFixtureSignalFormat GetDataType() const PURE_VIRTUAL(UDMXControlConsoleFaderBase::GetSignalFormat, return EDMXFixtureSignalFormat::E8Bit;);

	// Property Name getters
	FORCEINLINE static FName GetFaderNamePropertyName() { return GET_MEMBER_NAME_CHECKED(UDMXControlConsoleFaderBase, FaderName); }
	FORCEINLINE static FName GetEndingAddressPropertyName() { return GET_MEMBER_NAME_CHECKED(UDMXControlConsoleFaderBase, EndingAddress); }
	FORCEINLINE static FName GetValuePropertyName() { return GET_MEMBER_NAME_CHECKED(UDMXControlConsoleFaderBase, Value); }
	FORCEINLINE static FName GetMinValuePropertyName() { return GET_MEMBER_NAME_CHECKED(UDMXControlConsoleFaderBase, MinValue); }
	FORCEINLINE static FName GetMaxValuePropertyName() { return GET_MEMBER_NAME_CHECKED(UDMXControlConsoleFaderBase, MaxValue); }

protected:
	//~ Begin of UObject interface
	virtual void PostInitProperties() override;
#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif // WITH_EDITOR
	//~ End of UObject interface

	// ~ Begin FTickableGameObject interface
	virtual void Tick(float DeltaTime) override;
	virtual bool IsTickable() const override;
	virtual bool IsTickableInEditor() const override { return true; };
	virtual ETickableTickType GetTickableTickType() const override;
	virtual TStatId GetStatId() const override;
	// ~ End FTickableGameObject interface

	/** Cached Name of the Fader */
	UPROPERTY(EditAnywhere, meta = (DisplayPriority = "1"), Category = "DMX Fader")
	FString FaderName;

	/** The end channel Address to send DMX to */
	UPROPERTY(VisibleAnywhere, meta = (DisplayPriority = "5"), Category = "DMX Fader")
	int32 EndingAddress = 1;

	/** The current Fader Value */
	UPROPERTY(EditAnywhere, meta = (DisplayPriority = "6"), Category = "DMX Fader")
	uint32 Value = 0;

	/** The minimum Fader Value */
	UPROPERTY(VisibleAnywhere, meta = (DisplayPriority = "7"), Category = "DMX Fader")
	uint32 MinValue = 0;

	/** The maximum Fader Value */
	UPROPERTY(VisibleAnywhere, meta = (DisplayPriority = "8"), Category = "DMX Fader")
	uint32 MaxValue = 255;

#if WITH_EDITORONLY_DATA
	/** Oscillator that is used for this fader */
	UPROPERTY(EditAnywhere, meta = (DisplayName = "Oscillator Class", ShowDisplayNames), Category = "DMX Fader|Oscillator")
	TSoftClassPtr<UDMXControlConsoleFloatOscillator> FloatOscillatorClass;
#endif // WITH_EDITORONLY_DATA

	/** Float Oscillator applied to this channel */
	UPROPERTY(VisibleAnywhere, Instanced, Meta = (DisplayName = "Oscillator"), Category = "DMX Fader|Oscillator")
	TObjectPtr<UDMXControlConsoleFloatOscillator> FloatOscillator;

	/** This fader as an array for fast access */
	TArray<UDMXControlConsoleFaderBase*> ThisFaderAsArray;

	/** If true, the fader doesn't send DMX */
	UPROPERTY()
	bool bIsMuted = false;
};
