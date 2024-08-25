// Copyright Epic Games, Inc. All Rights Reserved.
#include "UniversalObjectLocatorEditor.h"
#include "UniversalObjectLocator.h"

namespace UE::UniversalObjectLocator
{
	FUniversalObjectLocator ILocatorEditor::MakeDefaultLocator() const
	{
		return FUniversalObjectLocator();
	}
}