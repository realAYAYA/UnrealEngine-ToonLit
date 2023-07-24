// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

// Includes
#include "CoreMinimal.h"
#include "Templates/PimplPtr.h"


// Forward declarations
namespace UE
{
	namespace Net
	{
		struct FNetResult;
	}
}


/**
 * Contains metadata and flags, which provide information on the traits of a packet - what it contains and how to process it.
 */
struct FOutPacketTraits
{
	/** Flags - may trigger modification of packet and traits */

	/** Whether or not the packet can/should be compressed */
	bool bAllowCompression;


	/** Traits */

	/** The number of ack bits in the packet - reflecting UNetConnection.NumAckBits */
	uint32 NumAckBits;

	/** The number of bunch bits in the packet - reflecting UNetConnection.NumBunchBits */
	uint32 NumBunchBits;

	/** Whether or not this is a keepalive packet */
	bool bIsKeepAlive;

	/** Whether or not the packet has been compressed */
	bool bIsCompressed;


	/** Default constructor */
	FOutPacketTraits()
		: bAllowCompression(true)
		, NumAckBits(0)
		, NumBunchBits(0)
		, bIsKeepAlive(false)
		, bIsCompressed(false)
	{
	}
};

/**
 * Contains metadata and flags, which provide information on the traits of a packet - what it contains and how to process it.
 */
struct FInPacketTraits
{
	/** Traits */

	/** This packet is not associated with a connection */
	bool bConnectionlessPacket = false;

	/** This is a connectionless packet, from a recently disconnected connection. */
	bool bFromRecentlyDisconnected = false;

	/** If there was an error processing the incoming packet, any additional error information is stored here */
	TPimplPtr<UE::Net::FNetResult, EPimplPtrMode::DeepCopy> ExtendedError;
};
