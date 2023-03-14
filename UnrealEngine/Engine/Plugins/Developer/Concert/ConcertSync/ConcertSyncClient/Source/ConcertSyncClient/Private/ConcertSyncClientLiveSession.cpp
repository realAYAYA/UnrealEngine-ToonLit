// Copyright Epic Games, Inc. All Rights Reserved.

#include "ConcertSyncClientLiveSession.h"
#include "ConcertSyncSessionDatabase.h"
#include "IConcertSession.h"

FConcertSyncClientLiveSession::FConcertSyncClientLiveSession(const TSharedRef<IConcertClientSession>& InSession, const EConcertSyncSessionFlags InSessionFlags)
	: Session(InSession)
	, SessionFlags(InSessionFlags)
	, SessionDatabase(MakeUnique<FConcertSyncSessionDatabase>())
{
	SessionDatabase->Open(Session->GetSessionWorkingDirectory());
}

FConcertSyncClientLiveSession::~FConcertSyncClientLiveSession()
{
	SessionDatabase->Close();
	SessionDatabase.Reset();
}

bool FConcertSyncClientLiveSession::IsValidSession() const
{
	return SessionDatabase->IsValid();
}
