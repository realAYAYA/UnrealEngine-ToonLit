// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Online/LobbiesCommonTypes.h"

namespace UE::Online {

class IAuth;
class FLobbiesCommon;
struct FLobbyEvents;

#if LOBBIES_FUNCTIONAL_TEST_ENABLED

TOnlineAsyncOpHandle<FFunctionalTestLobbies> RunLobbyFunctionalTest(IAuth& AuthInterface, FLobbiesCommon& LobbiesCommon, FLobbyEvents& LobbyEvents);

#endif // LOBBIES_FUNCTIONAL_TEST_ENABLED

/* UE::Online */ }
