// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

class IConcertServerSession;
class IConcertServer;
struct FConcertSessionClientInfo;

namespace ConcertUtil
{

/** Delete a directory tree via a move and delete */
CONCERT_API bool DeleteDirectoryTree(const TCHAR* InDirectoryToDelete, const TCHAR* InMoveToDirBeforeDelete = nullptr);

/** Copy the specified data size from a source archive into a destination archive. */
CONCERT_API bool Copy(FArchive& DstAr, FArchive& SrcAr, int64 Size);

/** Turn on verbose logging for all loggers (including console loggers). */
CONCERT_API void SetVerboseLogging(bool bInState);

/**
 * Get the list of clients for a session
 * @param Server The server to look on
 * @param SessionId The session ID
 * @return A list of clients connected to the session
 */
CONCERT_API TArray<FConcertSessionClientInfo> GetSessionClients(IConcertServer& Server, const FGuid& SessionId);

/**
 * Gets a connected client's info.
 * @param Server The server to look on
 * @param ClientEndpointId The client's endpoint ID
 * @return The client's info if found (it is not found if the client is not connected to any session)
 */
CONCERT_API TOptional<FConcertSessionClientInfo> GetConnectedClientInfo(IConcertServer& Server, const FGuid& ClientEndpointId);

/**
 * Retrieves the server session object from the given connected client.
 * @param Server The server to look on
 * @param ClientEndpointId The client's endpoint ID
 * @return The session the client is connected to if any
 */
CONCERT_API TSharedPtr<IConcertServerSession> GetLiveSessionClientConnectedTo(IConcertServer& Server, const FGuid& ClientEndpointId);
} // namespace ConcertUtil
