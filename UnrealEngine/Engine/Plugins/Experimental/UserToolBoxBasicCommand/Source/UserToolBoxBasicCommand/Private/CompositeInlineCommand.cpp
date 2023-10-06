// Copyright Epic Games, Inc. All Rights Reserved.


#include "CompositeInlineCommand.h"




void UCompositeInlineCommand::Execute()
{

	for (UUTBBaseCommand* Command:Commands)
	{
		
		
		if (!IsValid(Command))
		{
			continue;	
		}
		Command->ExecuteCommand();
	}
}

