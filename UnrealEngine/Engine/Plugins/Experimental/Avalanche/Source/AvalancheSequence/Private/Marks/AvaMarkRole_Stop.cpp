// Copyright Epic Games, Inc. All Rights Reserved.

#include "AvaMarkRole_Stop.h"
#include "AvaSequence.h"

EAvaMarkRoleReply FAvaMarkRole_Stop::Execute()
{
	Context->JumpToSelf();
	Context->Pause();
	return EAvaMarkRoleReply::Executed;
}
