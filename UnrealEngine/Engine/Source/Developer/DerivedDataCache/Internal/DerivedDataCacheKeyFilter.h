// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "Templates/PimplPtr.h"

#define UE_API DERIVEDDATACACHE_API

namespace UE::DerivedData { struct FCacheKey; }
namespace UE::DerivedData::Private { struct FCacheKeyFilterState; }

namespace UE::DerivedData
{

/** A type that filters cache keys by their bucket/type and at a configurable match rate. */
class FCacheKeyFilter
{
public:
	/**
	 * Parses a filter from a configuration string.
	 *
	 * Example:
	 * - Config = "-DDC-Verify=Texture@2.5+StaticMesh -Unused -DDC-Verify=Audio@75"
	 * - Prefix = "-DDC-Verify"
	 * - DefaultRate = 50.0f
	 * - Filter matches Texture at a rate of 2.5%, StaticMesh at a rate of 100%,
	 *   Audio at a rate of 75%, and everything else at a rate of 50%.
	 *
	 * @param Config        The configuration in the form of command line parameters.
	 * @param Prefix        The prefix of the parameter to parse the filter from.
	 * @param DefaultRate   The default rate at which to apply the filter. [0, 100]
	 */
	UE_API static FCacheKeyFilter Parse(const TCHAR* Config, const TCHAR* Prefix, float DefaultRate = 0.0f);

	/**
	 * Accesses the salt that is combined with the cache key for filtering.
	 *
	 * The filter will always produce the same output for a fixed pair of salt and key. To change
	 * the behavior of the filter between sessions, the salt is assigned randomly by default.
	 */
	UE_API void SetSalt(uint32 Salt);
	UE_API uint32 GetSalt() const;

	/** Returns true if the filter matches the key. The output is the same until the salt changes. */
	UE_API bool IsMatch(const FCacheKey& Key) const;

	/** Returns true if this filter can ever match a cache key. */
	inline explicit operator bool() const { return State.IsValid(); }

private:
	TPimplPtr<Private::FCacheKeyFilterState, EPimplPtrMode::DeepCopy> State;
};

} // UE::DerivedData

#undef UE_API
