// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#ifndef WANTS_MRMESH_UVS
	#if PLATFORM_WINDOWS
		#define WANTS_MRMESH_UVS 1
	#elif PLATFORM_HOLOLENS
		#define WANTS_MRMESH_UVS 0
	#elif PLATFORM_IOS
		#define WANTS_MRMESH_UVS 0
	#endif
#endif

#ifndef WANTS_MRMESH_TANGENTS
	#if PLATFORM_WINDOWS
		#define WANTS_MRMESH_TANGENTS 1
	#elif PLATFORM_HOLOLENS
		#define WANTS_MRMESH_TANGENTS 0
	#elif PLATFORM_IOS
		#define WANTS_MRMESH_TANGENTS 0
	#endif
#endif

#ifndef WANTS_MRMESH_COLORS
	#if PLATFORM_WINDOWS
		#define WANTS_MRMESH_COLORS 1
	#elif PLATFORM_HOLOLENS
		#define WANTS_MRMESH_COLORS 0
	#elif PLATFORM_IOS
		#define WANTS_MRMESH_COLORS 0
	#endif
#endif

#ifndef MRMESH_INDEX_TYPE
	#if PLATFORM_HOLOLENS
		#define MRMESH_INDEX_TYPE uint16
	#else
		#define MRMESH_INDEX_TYPE uint32
	#endif
#endif
