// Copyright Epic Games, Inc. All Rights Reserved.

#include "Engine/NetworkDelegates.h"

FNetDelegates::FReceivedNetworkEncryptionToken FNetDelegates::OnReceivedNetworkEncryptionToken;
FNetDelegates::FReceivedNetworkEncryptionAck FNetDelegates::OnReceivedNetworkEncryptionAck;
FNetDelegates::FReceivedNetworkEncryptionFailure FNetDelegates::OnReceivedNetworkEncryptionFailure;
FNetDelegates::FOnPendingNetGameConnectionCreated FNetDelegates::OnPendingNetGameConnectionCreated;
FNetDelegates::FNetworkCheatDetected FNetDelegates::OnNetworkCheatDetected;
FNetDelegates::FOnSyncLoadDetected FNetDelegates::OnSyncLoadDetected;