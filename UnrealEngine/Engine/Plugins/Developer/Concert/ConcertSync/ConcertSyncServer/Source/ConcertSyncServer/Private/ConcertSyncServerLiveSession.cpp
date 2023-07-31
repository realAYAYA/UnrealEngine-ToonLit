// Copyright Epic Games, Inc. All Rights Reserved.

#include "ConcertSyncServerLiveSession.h"
#include "ConcertSyncSessionDatabase.h"
#include "IConcertSession.h"

FConcertSyncServerLiveSession::FConcertSyncServerLiveSession(const TSharedRef<IConcertServerSession>& InSession, const EConcertSyncSessionFlags InSessionFlags)
	: Session(InSession)
	, SessionFlags(InSessionFlags)
	, SessionDatabase(MakeUnique<FConcertSyncSessionDatabase>())
{
	SessionDatabase->Open(Session->GetSessionWorkingDirectory());
}

FConcertSyncServerLiveSession::~FConcertSyncServerLiveSession()
{
	SessionDatabase->Close();
	SessionDatabase.Reset();
}

bool FConcertSyncServerLiveSession::IsValidSession() const
{
	return SessionDatabase->IsValid();
}
