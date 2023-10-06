// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

/**
 * true if intelTBB (a.k.a MallocTBB) can be used (different to the platform actually supporting it)
 */
#ifndef TBB_ALLOCATOR_ALLOWED
	#define TBB_ALLOCATOR_ALLOWED 1
#endif

 /**
  * true if mimalloc can be used (different to the platform actually supporting it)
  */
#ifndef MIMALLOC_ALLOCATOR_ALLOWED
	#define MIMALLOC_ALLOCATOR_ALLOWED 1
#endif
