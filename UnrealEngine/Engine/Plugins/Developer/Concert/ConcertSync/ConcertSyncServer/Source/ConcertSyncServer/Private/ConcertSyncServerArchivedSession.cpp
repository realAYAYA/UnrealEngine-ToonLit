// Copyright Epic Games, Inc. All Rights Reserved.

#include "ConcertSyncServerArchivedSession.h"
#include "ConcertSyncSessionDatabase.h"

FConcertSyncServerArchivedSession::FConcertSyncServerArchivedSession(const FString& InSessionRootPath, const FConcertSessionInfo& InSessionInfo)
	: SessionRootPath(InSessionRootPath)
	, SessionInfo(InSessionInfo)
	, SessionDatabase(MakeUnique<FConcertSyncSessionDatabase>())
{
	SessionDatabase->Open(SessionRootPath);
}

FConcertSyncServerArchivedSession::~FConcertSyncServerArchivedSession()
{
	SessionDatabase->Close();
	SessionDatabase.Reset();
}

bool FConcertSyncServerArchivedSession::IsValidSession() const
{
	return SessionDatabase->IsValid();
}
