// Copyright Epic Games, Inc. All Rights Reserved.

#include "Animation/AnimNotifyEndDataContext.h"
#include "Animation/AnimNodeMessages.h"

namespace UE::Anim {

IMPLEMENT_NOTIFY_CONTEXT_INTERFACE(FAnimNotifyEndDataContext)

FAnimNotifyEndDataContext::FAnimNotifyEndDataContext(bool bInReachedEnd)
	: bReachedEnd(bInReachedEnd)
{
}
	
}	// namespace UE::Anim
