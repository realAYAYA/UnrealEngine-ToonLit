// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "Templates/SharedPointer.h"



/**
 * Interface for sockets supporting direct communication between the game running
 * on the target device and a connected PC.
 * 
 * It represents a custom communication channel and may not be implemented on all platforms.
 * 
 * It is meant to be used in development ONLY.
 * 
 * @see IPlatformHostCommunication
 */
class IPlatformHostSocket
{
public:

	/**
	  * Status values returned by Send and Receive members.
	  * @see Send, Receive
	  */
	enum class EResultNet : uint8
	{
		Ok,							// Communication successful.
		ErrorUnknown,				// Unknown error.
		ErrorInvalidArgument,		// Incorrect parameters provided to a function (shouldn't happen assuming the socket object is valid).
		ErrorInvalidConnection,		// Incorrect socket id used (shouldn't happen assuming the socket object is valid).
		ErrorInterrupted,			// Data transfer interrupted e.g. a networking issue.
		ErrorHostNotConnected		// Host PC is not connected (not connected yet or has already disconnected).
	};

	/**
	  * State of the socket determining its ability to send/receive data.
	  * @see GetState
	  */
	enum class EConnectionState : uint8
	{
		Unknown,		// Default state (shouldn't be returned).
		Created,		// Socket has been created by cannot communicate yet (no host pc connected yet).
		Connected,		// Socket ready for communication.
		Disconnected,	// Host PC has disconnected (no communication possible, socket should be closed).
		Closed,			// Socket has already been closed and shouldn't be used.
	};

public:

	/**
	 * Send data to the connected host PC (blocking operation).
	 * 
	 * @param Buffer      Data to be sent.
	 * @param BytesToSend The number of bytes to send.
	 * @return            Status value indicating error or success.
	 */
	virtual EResultNet Send(const void* Buffer, uint64 BytesToSend) = 0;

	/**
	 * Receive data from the connected host PC (blocking operation).
	 * 
	 * @param Buffer         Data to be sent.
	 * @param BytesToReceive The number of bytes to receive (Buffer has to be large enough).
	 * @return               Status value indicating error or success.
	 */
	virtual EResultNet Receive(void* Buffer, uint64 BytesToReceive) = 0;

	/**
	 * Get the state of the socket (determines if the host pc is connected and communication is possible).
	 */
	virtual EConnectionState GetState() const = 0;

	/**
	 * Destructor.
	 */
	virtual ~IPlatformHostSocket()
	{
	}
};


// Type definition for shared references to instances of IPlatformHostSocket.
typedef TSharedRef<IPlatformHostSocket, ESPMode::ThreadSafe> IPlatformHostSocketRef;

// Type definition for shared pointers to instances of IPlatformHostSocket.
typedef TSharedPtr<IPlatformHostSocket, ESPMode::ThreadSafe> IPlatformHostSocketPtr;
