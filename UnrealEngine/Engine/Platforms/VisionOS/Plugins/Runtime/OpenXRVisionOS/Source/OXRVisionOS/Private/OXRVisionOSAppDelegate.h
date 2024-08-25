// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#import <CompositorServices/CompositorServices.h>

namespace OXRVisionOS
{
	CP_OBJECT_cp_layer_renderer* GetSwiftLayerRenderer();

	int GetSwiftNumViewports();

	CGRect GetSwiftViewportRect(int Index);
}
