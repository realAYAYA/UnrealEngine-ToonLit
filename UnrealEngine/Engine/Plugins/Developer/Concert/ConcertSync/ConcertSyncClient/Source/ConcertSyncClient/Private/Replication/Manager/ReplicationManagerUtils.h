// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Async/Future.h"

struct FConcertReplication_ChangeAuthority_Response;
struct FConcertReplication_ChangeAuthority_Request;

namespace UE::ConcertSyncClient::Replication
{
	/** Creates a fulfilled future which rejects all items in FConcertChangeAuthority_Request::TakeAuthority. */
	TFuture<FConcertReplication_ChangeAuthority_Response> RejectAll(FConcertReplication_ChangeAuthority_Request&& Args);
};
