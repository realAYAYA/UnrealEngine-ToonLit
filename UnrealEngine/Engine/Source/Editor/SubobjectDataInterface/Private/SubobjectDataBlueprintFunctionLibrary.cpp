// Copyright Epic Games, Inc. All Rights Reserved.

#include "SubobjectDataBlueprintFunctionLibrary.h"

#include "Templates/SharedPointer.h"

void USubobjectDataBlueprintFunctionLibrary::GetData(const FSubobjectDataHandle& DataHandle, FSubobjectData& OutData)
{
	// Copy the underlying subobject data - probably to the stack so that script can manipulate it
	TSharedPtr<FSubobjectData> DataPtr = DataHandle.GetSharedDataPtr();
	if(DataPtr.IsValid())
	{
		OutData = *DataPtr.Get();
	}
}

const UObject* USubobjectDataBlueprintFunctionLibrary::GetObjectForBlueprint(const FSubobjectData& Data, UBlueprint* Blueprint) 
{
	return Data.GetObjectForBlueprint(Blueprint); 
}
