// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

class FArchive;
class UObject;

namespace SerializeObjectState
{
	/**
	 * Serializes the object and all its subobjects.
	 */
	void SerializeWithSubobjects(FArchive& Archive, UObject* Root);
}


