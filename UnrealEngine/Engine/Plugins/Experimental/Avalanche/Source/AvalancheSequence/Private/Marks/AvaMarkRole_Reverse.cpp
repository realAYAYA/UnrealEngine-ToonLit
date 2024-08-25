// Copyright Epic Games, Inc. All Rights Reserved.

#include "AvaMarkRole_Reverse.h"

EAvaMarkRoleReply FAvaMarkRole_Reverse::Execute()
{
	Context->JumpToSelf();
	Context->Reverse();
	Context->Continue();
	return EAvaMarkRoleReply::Executed;
}
