// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

// Includes
#include "Net/Core/Connection/NetResultManager.h"

#include "OodleNetworkFaultHandler.generated.h"


// Forward declarations
namespace UE
{
	namespace Net
	{
		class FNetConnectionFaultRecoveryBase;
	}
}


/**
 * Oodle net error types, for NetConnection fault handling
 */
UENUM()
enum class EOodleNetResult : uint8
{
	Unknown,
	Success,

	/** Oodle Packet decoding failed */
	OodleDecodeFailed,

	/** Oodle Packet payload serialization failed */
	OodleSerializePayloadFail,

	/** Oodle Packet decompressed length overflow */
	OodleBadDecompressedLength,

	/** Oodle Dictionary missing */
	OodleNoDictionary
};

DECLARE_NETRESULT_ENUM(EOodleNetResult);

OODLENETWORKHANDLERCOMPONENT_API const TCHAR* LexToString(EOodleNetResult Enum);


/**
 * Oodle Network Fault Handler - implements fault handling for Oodle net errors, tied to the main NetConnection fault recovery implementation
 */
class FOodleNetworkFaultHandler final : public UE::Net::FNetResultHandler
{
	friend class OodleNetworkHandlerComponent;

private:
	void InitFaultRecovery(UE::Net::FNetConnectionFaultRecoveryBase* InFaultRecovery);

	virtual UE::Net::EHandleNetResult HandleNetResult(UE::Net::FNetResult&& InResult) override;

private:
	/** The NetConnection FaultRecovery instance, which we forward fault counting to */
	UE::Net::FNetConnectionFaultRecoveryBase* FaultRecovery = nullptr;

	/** This fault handlers allocated counter index, within the NetConnection FaultRecovery instance */
	int32 CounterIndex = INDEX_NONE;
};
