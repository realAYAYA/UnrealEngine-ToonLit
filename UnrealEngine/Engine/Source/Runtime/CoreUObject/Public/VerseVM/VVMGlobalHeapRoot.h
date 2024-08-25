// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#if WITH_VERSE_VM || defined(__INTELLISENSE__)

#include "HAL/Platform.h"

namespace Verse
{
struct FAbstractVisitor;
struct FMarkStackVisitor;

// If you have a class that is meant to be used exclusively for global variables (like TUniqueConstructor), then
// subclass this to give that class the ability to mark its referenced cells.
//
// Never use this for things that aren't truly global.
struct FGlobalHeapRoot
{
	COREUOBJECT_API FGlobalHeapRoot();

	// Implement these visit any strong references in the root
	virtual void Visit(FMarkStackVisitor& Visitor) = 0;
	virtual void Visit(FAbstractVisitor& Visitor) = 0;
};

} // namespace Verse
#endif // WITH_VERSE_VM
