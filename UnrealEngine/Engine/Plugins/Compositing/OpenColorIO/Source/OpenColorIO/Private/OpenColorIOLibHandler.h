// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once


class FOpenColorIOLibHandler
{
public:
	static bool Initialize();
	static bool IsInitialized();
	static void Shutdown();

private:
	static void* LibHandle;
};
