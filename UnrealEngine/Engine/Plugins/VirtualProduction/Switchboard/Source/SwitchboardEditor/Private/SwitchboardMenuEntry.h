// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"


class FSwitchboardMenuEntry
{
public:
	static void Register();
	static void AddMenu();
	static void RemoveMenu();
	static void Unregister();
};
