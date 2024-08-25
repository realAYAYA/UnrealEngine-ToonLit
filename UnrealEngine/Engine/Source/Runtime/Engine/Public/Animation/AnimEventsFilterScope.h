// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AnimNodeMessages.h"
#include "AnimNotifyQueue.h"

namespace UE { namespace Anim
{

class IAnimEventsFilterContext : public IAnimNotifyEventContextDataInterface
{
	DECLARE_NOTIFY_CONTEXT_INTERFACE_API(IAnimEventsFilterContext, ENGINE_API)

public:
	
	virtual bool ShouldFilterNotify(const FAnimNotifyEventReference & InNotifyEventRef) const = 0;
};
	
}}