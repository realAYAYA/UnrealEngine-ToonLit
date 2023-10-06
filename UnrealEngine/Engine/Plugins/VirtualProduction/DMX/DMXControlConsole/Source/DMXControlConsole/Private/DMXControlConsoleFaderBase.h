// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "DMXProtocolTypes.h"
#include "IDMXControlConsoleFaderGroupElement.h"
#include "Tickable.h"
#include "UObject/Object.h"

#include "DMXControlConsoleFaderBase.generated.h"

enum class EDMXFixtureSignalFormat : uint8;
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
	virtual int32 GetUniverseID() const override { return UniverseID; }
	virtual int32 GetStartingAddress() const override { return StartingAddress; }
	virtual int32 GetEndingAddress() const override { return EndingAddress; }
	virtual void Destroy() override;
	//~ End IDMXControlConsoleFaderGroupElement interface

	/** Gets the name of the Fader */
	const FString& GetFaderName() const { return FaderName; };

	/** Sets the name of the Fader */
	virtual void SetFaderName(const FString& NewName);
	
	/** Gets Fader's Data Type */
	virtual EDMXFixtureSignalFormat GetDataType() const { return DataType; }

	/** Returns the current value of the fader */
	uint32 GetValue() const { return Value; }

	/** Sets the current value of the fader */
	virtual void SetValue(const uint32 NewValue);

	/** Gets the min value of the fader */
	uint32 GetMinValue() const { return MinValue; }

	/** Sets Fader's Min Value */
	virtual void SetMinValue(uint32 NewMinValue);

	/** Gets the max value of the fader */
	uint32 GetMaxValue() const { return MaxValue; }

	/** Sets Fader's Max Value */
	virtual void SetMaxValue(uint32 NewMaxValue);

	/** Gets wheter this Fader uses LSB mode or not */
	bool GetUseLSBMode() const { return bUseLSBMode; }

	/** Gets wheter this Fader can send DMX data */
	bool IsMuted() const { return bIsMuted; }

	/** Sets mute state of this fader */
	virtual void SetMute(bool bMute);

	/** Mutes/Unmutes this fader */
	virtual void ToggleMute();

	/** Gets wheter this Fader's Value can be changed */
	bool IsLocked() const { return bIsLocked; }

	/** Sets lock state of this fader */
	virtual void SetLock(bool bLock);

	/** Locks/Unlocks this fader */
	virtual void ToggleLock();

	/** Resets the fader to its default value */
	void ResetToDefault();

	// Property Name getters
	FORCEINLINE static FName GetFaderNamePropertyName() { return GET_MEMBER_NAME_CHECKED(UDMXControlConsoleFaderBase, FaderName); }
	FORCEINLINE static FName GetDataTypePropertyName() { return GET_MEMBER_NAME_CHECKED(UDMXControlConsoleFaderBase, DataType); }
	FORCEINLINE static FName GetUniverseIDPropertyName() { return GET_MEMBER_NAME_CHECKED(UDMXControlConsoleFaderBase, UniverseID); }
	FORCEINLINE static FName GetStartingAddressPropertyName() { return GET_MEMBER_NAME_CHECKED(UDMXControlConsoleFaderBase, StartingAddress); }
	FORCEINLINE static FName GetEndingAddressPropertyName() { return GET_MEMBER_NAME_CHECKED(UDMXControlConsoleFaderBase, EndingAddress); }
	FORCEINLINE static FName GetValuePropertyName() { return GET_MEMBER_NAME_CHECKED(UDMXControlConsoleFaderBase, Value); }
	FORCEINLINE static FName GetMinValuePropertyName() { return GET_MEMBER_NAME_CHECKED(UDMXControlConsoleFaderBase, MinValue); }
	FORCEINLINE static FName GetMaxValuePropertyName() { return GET_MEMBER_NAME_CHECKED(UDMXControlConsoleFaderBase, MaxValue); }
	FORCEINLINE static FName GetUseLSBModePropertyName() { return GET_MEMBER_NAME_CHECKED(UDMXControlConsoleFaderBase, bUseLSBMode); }
#if WITH_EDITOR
	FORCEINLINE static FName GetFloatOscillatorClassPropertyName() { return GET_MEMBER_NAME_CHECKED(UDMXControlConsoleFaderBase, FloatOscillatorClass); }
#endif // WITH_EDITOR
	FORCEINLINE static FName GetFloatOscillatorPropertyName() { return GET_MEMBER_NAME_CHECKED(UDMXControlConsoleFaderBase, FloatOscillator); }
	FORCEINLINE static FName GetIsMutedPropertyName() { return GET_MEMBER_NAME_CHECKED(UDMXControlConsoleFaderBase, bIsMuted); }
	FORCEINLINE static FName GetIsLockedPropertyName() { return GET_MEMBER_NAME_CHECKED(UDMXControlConsoleFaderBase, bIsLocked); }

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

	/** Sets a new universe ID, checking for its validity */
	virtual void SetUniverseID(int32 InUniversID);

	/** Sets min/max range, according to the number of channels */
	virtual void SetValueRange();

	/** Sets this fader's number of channels */
	virtual void SetDataType(EDMXFixtureSignalFormat InDataType);

	/** Sets starting/ending address range, according to the number of channels  */
	virtual void SetAddressRange(int32 InStartingAddress);

	/** Cached Name of the Fader */
	UPROPERTY(EditAnywhere, meta = (DisplayPriority = "1"), Category = "DMX Fader")
	FString FaderName;

	/** The number of channels this Fader uses */
	UPROPERTY(EditAnywhere, meta = (DisplayPriority = "2", HideEditConditionToggle, EditCondition = "bCanEditDMXAssignment"), Category = "DMX Fader")
	EDMXFixtureSignalFormat DataType;

	/** The universe the should send to fader */
	UPROPERTY(EditAnywhere, meta = (DisplayPriority = "3", HideEditConditionToggle, EditCondition = "bCanEditDMXAssignment"), Category = "DMX Fader")
	int32 UniverseID = 1;

	/** The starting channel Address to send DMX to */
	UPROPERTY(EditAnywhere, meta = (DisplayPriority = "4", HideEditConditionToggle, EditCondition = "bCanEditDMXAssignment"), Category = "DMX Fader")
	int32 StartingAddress = 1;

	/** The end channel Address to send DMX to */
	UPROPERTY(VisibleAnywhere, meta = (DisplayPriority = "5", HideEditConditionToggle, EditCondition = "bCanEditDMXAssignment"), Category = "DMX Fader")
	int32 EndingAddress = 1;

	/** The current Fader Value */
	UPROPERTY(EditAnywhere, meta = (DisplayPriority = "6", HideEditConditionToggle, EditCondition = "!bIsLocked"), Category = "DMX Fader")
	uint32 Value = 0;

	/** Fader's default Value */
	UPROPERTY()
	uint32 DefaultValue = 0;

	/** The minimum Fader Value */
	UPROPERTY(EditAnywhere, meta = (DisplayPriority = "7", EditCondition = "!bIsLocked"), Category = "DMX Fader")
	uint32 MinValue = 0;

	/** The maximum Fader Value */
	UPROPERTY(EditAnywhere, meta = (DisplayPriority = "8", EditCondition = "!bIsLocked"), Category = "DMX Fader")
	uint32 MaxValue = 255;

	/** Use Least Significant Byte mode. Individual bytes(channels) be interpreted with the first bytes being the lowest part of the number(endianness). */
	UPROPERTY(EditAnywhere, meta = (DisplayPriority = "9", HideEditConditionToggle, EditCondition = "bCanEditDMXAssignment"), Category = "DMX Fader")
	bool bUseLSBMode = false;

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

	UPROPERTY()
	/** If true, the fader doesn't send DMX */
	bool bIsMuted = false;

	UPROPERTY(EditAnywhere, Category = "DMX Fader")
	/** If true, Fader's value can't be changed */
	bool bIsLocked = false;

#if WITH_EDITORONLY_DATA
	UPROPERTY()
	/** If true, the property is editable in editor */
	bool bCanEditDMXAssignment = false;
#endif // WITH_EDITORONLY_DATA
};

