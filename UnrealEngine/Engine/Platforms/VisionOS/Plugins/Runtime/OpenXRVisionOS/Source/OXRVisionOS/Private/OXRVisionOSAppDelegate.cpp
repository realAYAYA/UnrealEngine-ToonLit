// Copyright Epic Games, Inc. All Rights Reserved.

#include "OXRVisionOSAppDelegate.h"
#include "IOS/IOSAppDelegate.h"

CP_OBJECT_cp_layer_renderer* OXRVisionOS::GetSwiftLayerRenderer()
{
	return [IOSAppDelegate GetDelegate].SwiftLayer;
}

int OXRVisionOS::GetSwiftNumViewports()
{
	return  [IOSAppDelegate GetDelegate].SwiftLayerViewports.count;
}
CGRect OXRVisionOS::GetSwiftViewportRect(int Index)
{
	return  [[[IOSAppDelegate GetDelegate].SwiftLayerViewports objectAtIndex:Index] CGRectValue];
}

