// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "EOSVoiceChat.h"

#if WITH_EOSVOICECHAT

#include "VoiceChatResult.h"

#define EOSVOICECHAT_ERROR(...) FVoiceChatResult::CreateError(TEXT("errors.com.epicgames.voicechat.eos"), __VA_ARGS__)

enum class EOS_EResult : int32_t;

FVoiceChatResult ResultFromEOSResult(const EOS_EResult EosResult);

#endif // WITH_EOSVOICECHAT
