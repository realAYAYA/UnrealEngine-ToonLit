// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	MultiGPU.cpp: Multi-GPU support
=============================================================================*/

#include "MultiGPU.h"

#if WITH_MGPU
uint32 GNumExplicitGPUsForRendering = 1;
uint32 GVirtualMGPU = 0;
#endif
