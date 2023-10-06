// Copyright Epic Games, Inc. All Rights Reserved.

#include "RCTypeHandler.h"
#include "Controller/RCController.h"

void FRCTypeHandler::OnControllerPropertyModified(URCVirtualPropertyBase* InVirtualProperty)
{
	if (URCController* Controller = Cast<URCController>(InVirtualProperty))
	{
		Controller->OnModifyPropertyValue();
	}
}
