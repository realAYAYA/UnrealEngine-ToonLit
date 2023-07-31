// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

namespace SerializeObjectState
{
	/**
	 * Serializes the object and all its subobjects.
	 */
	void SerializeWithSubobjects(FArchive& Archive, UObject* Root);
}


