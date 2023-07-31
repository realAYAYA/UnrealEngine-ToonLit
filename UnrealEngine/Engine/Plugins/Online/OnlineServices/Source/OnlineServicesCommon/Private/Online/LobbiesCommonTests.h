// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Online/LobbiesCommonTypes.h"
#include "Online/OnlineAsyncOpHandle.h"
#include "Online/OnlineMeta.h"

namespace UE::Online {

class IAuth;
class FLobbiesCommon;
struct FLobbyEvents;

#if LOBBIES_FUNCTIONAL_TEST_ENABLED

TOnlineAsyncOpHandle<FFunctionalTestLobbies> RunLobbyFunctionalTest(IAuth& AuthInterface, FLobbiesCommon& LobbiesCommon, FLobbyEvents& LobbyEvents);

#endif // LOBBIES_FUNCTIONAL_TEST_ENABLED

/* UE::Online */ }