// Copyright Epic Games, Inc. All Rights Reserved.

#include "EVCamTargetViewportID.h"

#include "Util/LevelViewportUtils.h"

namespace UE::VCamCore
{
#if WITH_EDITOR
	TSharedPtr<SLevelViewport> GetLevelViewport(EVCamTargetViewportID TargetViewport)
	{
		return LevelViewportUtils::Private::GetLevelViewport(TargetViewport);
	}
#endif
}
