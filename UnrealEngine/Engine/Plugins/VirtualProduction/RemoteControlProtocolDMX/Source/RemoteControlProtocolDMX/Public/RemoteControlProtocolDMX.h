// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "RemoteControlProtocol.h"

#include "DMXProtocolCommon.h"
#include "RemoteControlProtocolBinding.h"
#include "IO/DMXInputPortReference.h"
#include "Library/DMXEntityFixtureType.h"

#include "RemoteControlProtocolDMX.generated.h"

class FRemoteControlProtocolDMX;

/**
 * Using as an inner struct for details customization.
 * Useful to have type customization for the struct
 */
USTRUCT()
struct FRemoteControlDMXProtocolEntityExtraSetting
{
	GENERATED_BODY();


	/** DMX universe id */
	UPROPERTY(EditAnywhere, Category = Mapping, Meta = (ClampMin = "0", UIMin = "0"))
	int32 Universe = 1;

	/** Starting channel */
	UPROPERTY(EditAnywhere, Category = Mapping, Meta = (ClampMin = "1", ClampMax = "512", UIMin = "1", UIMax = "512"))
	int32 StartingChannel = 1;

	/**
	 * Least Significant Byte mode makes the individual bytes (channels) of the function be
	 * interpreted with the first bytes being the lowest part of the number.
	 * Most Fixtures use MSB (Most Significant Byte).
	 */
	UPROPERTY(EditAnywhere, Category = Mapping)
	bool bUseLSB = false;

	/** Defines the used number of channels (bytes) */
	UPROPERTY(EditAnywhere, Category = Mapping)
	EDMXFixtureSignalFormat DataType = EDMXFixtureSignalFormat::E8Bit;

	/** If set to true, uses the default input port set in Remote Control Protocol project settings */
	UPROPERTY(EditAnywhere, Category = Mapping)
	bool bUseDefaultInputPort = true;

	/** Reference of an input DMX port id */
	UPROPERTY(EditAnywhere, Category = Mapping)
	FGuid InputPortId;

};

/**
 * DMX protocol entity for remote control binding
 */
USTRUCT()
struct FRemoteControlDMXProtocolEntity : public FRemoteControlProtocolEntity
{
	GENERATED_BODY()

	friend class FRemoteControlProtocolDMX;

public:
	//~ Begin FRemoteControlProtocolEntity interface
	virtual FName GetRangePropertyName() const override { return NAME_UInt32Property; }
	virtual uint8 GetRangePropertySize() const override;
	virtual const FString& GetRangePropertyMaxValue() const override;

#if WITH_EDITOR

	/** Register(s) all the widgets of this protocol entity. */
	virtual void RegisterProperties() override;

#endif // WITH_EDITOR
	//~ End FRemoteControlProtocolEntity interface

	/** Initialize struct and delegates */
	void Initialize();

	/** Try to get the port ID from dmx protocol settings */
	void UpdateInputPort();

	/** Extra protocol settings. Primary using for customization */
	UPROPERTY(EditAnywhere, Category = Mapping, meta = (ShowOnlyInnerProperties))
	FRemoteControlDMXProtocolEntityExtraSetting ExtraSetting;

	/** DMX range input property template, used for binding. */
	UPROPERTY(Transient)
	uint32 RangeInputTemplate = 0;

private:
	/** DMX entity cache buffer. From 1 up to 4 channels, based on DataType */
	TArray<uint8> CacheDMXBuffer;

	/** A single, generic DMX signal. One universe of raw DMX data received */
	FDMXSignalSharedPtr LastSignalPtr;

public:
	/** Called when the struct is serialized */
	bool Serialize(FArchive& Ar);

	/** Called after the struct is serialized */
	void PostSerialize(const FArchive& Ar);

	// DEPRECATED MEMBERS
	// Deprecated 5.0
	UPROPERTY(Meta = (DeprecatedProperty, DeprecationMessage = "This Property is deprecated and will be removed in a future release. It was moved to the ExtraSetting struct member so the property can be customized."))
	int32 Universe_DEPRECATED = 1;
	
	UPROPERTY(Meta = (DeprecatedProperty, DeprecationMessage = "This Property is deprecated and will be removed in a future release. It was moved to the ExtraSetting struct member so the property can be customized."))
	bool bUseLSB_DEPRECATED = false;

	UPROPERTY(Meta = (DeprecatedProperty, DeprecationMessage = "This Property is deprecated and will be removed in a future release. It was moved to the ExtraSetting struct member so the property can be customized."))
	EDMXFixtureSignalFormat DataType_DEPRECATED = EDMXFixtureSignalFormat::E8Bit;

	UPROPERTY(Meta = (DeprecatedProperty, DeprecationMessage = "This Property is deprecated and will be removed in a future release. It was moved to the ExtraSetting struct member so the property can be customized."))
	bool bUseDefaultInputPort_DEPRECATED = true;
	
	UPROPERTY(Meta = (DeprecatedProperty, DeprecationMessage = "This Property is deprecated and will be removed in a future release. It was moved to the ExtraSetting struct member so the property can be customized."))
	FGuid InputPortId_DEPRECATED;
};
template<>
struct TStructOpsTypeTraits<FRemoteControlDMXProtocolEntity> : public TStructOpsTypeTraitsBase2<FRemoteControlDMXProtocolEntity>
{
	enum
	{
		WithSerializer = true,
		WithPostSerialize = true
	};
};

/**
 * DMX protocol implementation for Remote Control
 */
class FRemoteControlProtocolDMX : public FRemoteControlProtocol
{
public:
	FRemoteControlProtocolDMX()
		: FRemoteControlProtocol(ProtocolName)
	{}
	
	//~ Begin IRemoteControlProtocol interface
	virtual void Bind(FRemoteControlProtocolEntityPtr InRemoteControlProtocolEntityPtr) override;
	virtual void Unbind(FRemoteControlProtocolEntityPtr InRemoteControlProtocolEntityPtr) override;
	virtual void UnbindAll() override;
	virtual UScriptStruct* GetProtocolScriptStruct() const override { return FRemoteControlDMXProtocolEntity::StaticStruct(); }
	virtual void OnEndFrame() override;
	//~ End IRemoteControlProtocol interface

private:
	/**
	 * Apply dmx channel data to the bound property, potentially resize the cache buffer
	 * @param InSignal				DMX signal buffer pointer
	 * @param InDMXOffset			Byte offset in signal buffer
	 * @param InProtocolEntityPtr	Protocol entity pointer
	 */
	void ProcessAndApplyProtocolValue(const FDMXSignalSharedPtr& InSignal, int32 InDMXOffset, const FRemoteControlProtocolEntityPtr& InProtocolEntityPtr);

#if WITH_EDITOR
	/**
	 * Process the AutoBinding to the Remote Control Entity
	 * @param InProtocolEntityPtr	Protocol entity pointer
	 */
	void ProcessAutoBinding(const FRemoteControlProtocolEntityPtr& InProtocolEntityPtr);

protected:

	/** Populates protocol specific columns. */
	virtual void RegisterColumns() override;

#endif // WITH_EDITOR

private:
	/** Binding for the DMX protocol */
	TArray<FRemoteControlProtocolEntityWeakPtr> ProtocolsBindings;

#if WITH_EDITORONLY_DATA
	/** DMX universe cache buffer.*/
	TArray<uint8, TFixedAllocator<DMX_UNIVERSE_SIZE>> CacheUniverseDMXBuffer;
#endif

public:
	/** DMX protocol name */
	static const FName ProtocolName;
};
