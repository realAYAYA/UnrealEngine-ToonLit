// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#if defined(_MSC_VER)
	#define EXPORTLIB __declspec(dllexport)
	#define IMPORTLIB __declspec(dllimport)
#elif defined(__GNUC__)
	#define EXPORTLIB __attribute__((visibility("default")))
	#define IMPORTLIB
#else
	#define EXPORTLIB
	#define IMPORTLIB
#endif
