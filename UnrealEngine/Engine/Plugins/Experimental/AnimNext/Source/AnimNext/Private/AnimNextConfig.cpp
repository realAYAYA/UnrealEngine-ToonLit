// Copyright Epic Games, Inc. All Rights Reserved.

#include "AnimNextConfig.h"
#include "Param/ObjectProxyFactory.h"

#if WITH_EDITOR

void UAnimNextConfig::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	using namespace UE::AnimNext;
	
	if(PropertyChangedEvent.Property->GetFName() == GET_MEMBER_NAME_CHECKED(UAnimNextConfig, ExposedClasses))
	{
		FObjectProxyFactory::Refresh();
	}

	SaveConfig();
}

#endif