// Copyright Epic Games, Inc. All Rights Reserved.

#include "Animation/ActiveMontageInstanceScope.h"
#include "Animation/AnimNodeMessages.h"

namespace UE::Anim {

IMPLEMENT_NOTIFY_CONTEXT_INTERFACE(FAnimNotifyMontageInstanceContext)

FAnimNotifyMontageInstanceContext::FAnimNotifyMontageInstanceContext(const int32 InMontageInstanceID)
	: MontageInstanceID(InMontageInstanceID)
{
}
	
}	// namespace UE::Anim
