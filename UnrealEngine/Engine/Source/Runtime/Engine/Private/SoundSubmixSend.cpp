// Copyright Epic Games, Inc. All Rights Reserved.
#include "Sound/SoundSubmixSend.h"


#include UE_INLINE_GENERATED_CPP_BY_NAME(SoundSubmixSend)

FSoundSubmixSendInfoBase::FSoundSubmixSendInfoBase()
	: SendLevelControlMethod(ESendLevelControlMethod::Manual)
	, SoundSubmix(nullptr)
	, SendLevel(1.0f)
	, DisableManualSendClamp(false)
	, MinSendLevel(0.0f)
	, MaxSendLevel(1.0f)
	, MinSendDistance(100.0f)
	, MaxSendDistance(1000.0f)
{
}

