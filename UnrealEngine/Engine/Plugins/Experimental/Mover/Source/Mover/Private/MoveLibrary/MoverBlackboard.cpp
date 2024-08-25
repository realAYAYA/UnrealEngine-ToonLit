// Copyright Epic Games, Inc. All Rights Reserved.


#include "MoveLibrary/MoverBlackboard.h"



void UMoverBlackboard::Invalidate(FName ObjName)
{
	ObjectsByName.Remove(ObjName);
}

void UMoverBlackboard::Invalidate(EInvalidationReason Reason)
{
	switch (Reason)
	{
		default:
		case EInvalidationReason::FullReset:
			ObjectsByName.Empty();
			break;

		// TODO: Support other reasons
	}
}

