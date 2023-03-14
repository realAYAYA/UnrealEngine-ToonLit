// Copyright Epic Games, Inc. All Rights Reserved.

/*================================================================================================
VirtualShadowMapDefinitions.h: used in virtual shadow map shaders and C++ code to define common constants
!!! Changing this file requires recompilation of the engine !!!
=================================================================================================*/

#pragma once

#define VIRTUAL_SHADOW_MAP_VISUALIZE_NONE						0
#define VIRTUAL_SHADOW_MAP_VISUALIZE_SHADOW_FACTOR				(1 << 0)
#define VIRTUAL_SHADOW_MAP_VISUALIZE_CLIPMAP_OR_MIP				(1 << 1)
#define VIRTUAL_SHADOW_MAP_VISUALIZE_VIRTUAL_PAGE				(1 << 2)
#define VIRTUAL_SHADOW_MAP_VISUALIZE_CACHED_PAGE				(1 << 3)
#define VIRTUAL_SHADOW_MAP_VISUALIZE_SMRT_RAY_COUNT				(1 << 4)
#define VIRTUAL_SHADOW_MAP_VISUALIZE_CLIPMAP_VIRTUAL_SPACE		(1 << 5)
#define VIRTUAL_SHADOW_MAP_VISUALIZE_GENERAL_DEBUG				(1 << 6)
#define VIRTUAL_SHADOW_MAP_VISUALIZE_DIRTY_PAGE					(1 << 7)
#define VIRTUAL_SHADOW_MAP_VISUALIZE_GPU_INVALIDATED_PAGE		(1 << 8)
#define VIRTUAL_SHADOW_MAP_VISUALIZE_MERGED_PAGE				(1 << 9)


#define VSM_PROJ_FLAG_CURRENT_DISTANT_LIGHT (1U << 0)
#define VSM_PROJ_FLAG_UNCACHED (1U << 1) // Used to indicate that the light is uncached and should only render to dynamic pages

// Hard limit for max distant lights supported 8k for now - we may revise later. We need to keep them in a fixed range for now to make allocation easy and minimize overhead for indexing.
#define VSM_MAX_SINGLE_PAGE_SHADOW_MAPS (1024U * 8U)
