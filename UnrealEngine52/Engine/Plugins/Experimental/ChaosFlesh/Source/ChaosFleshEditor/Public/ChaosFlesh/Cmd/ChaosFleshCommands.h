// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once
#include "Engine/World.h"

/**
* The public interface to this module
*/
class CHAOSFLESHEDITOR_API FChaosFleshCommands
{
public:


	/**
	*  Import file. 
	*/
	static void ImportFile(const TArray<FString>& Args, UWorld* World);

};