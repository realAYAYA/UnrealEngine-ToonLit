// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/StringFwd.h"

struct FGuid;
template <typename FuncType> class TFunctionRef;

namespace UE::DerivedData { class IBuildFunction; }

namespace UE::DerivedData
{

/**
 * A build function registry maintains a collection of build functions.
 */
class IBuildFunctionRegistry
{
public:
	virtual ~IBuildFunctionRegistry() = default;

	/** Find a function by name. Returns null if not found. Safe to call from a scheduled job or the main thread. */
	virtual const IBuildFunction* FindFunction(FUtf8StringView Function) const = 0;
	/** Find a function version by name. Returns zero if not found. Safe to call from any thread. */
	virtual FGuid FindFunctionVersion(FUtf8StringView Function) const = 0;
	/** Iterate the complete list of build function versions. Safe to call from any thread. */
	virtual void IterateFunctionVersions(TFunctionRef<void(FUtf8StringView Name, const FGuid& Version)> Visitor) const = 0;
};

} // UE::DerivedData
