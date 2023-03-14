// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Misc/EnumClassFlags.h"

namespace UE::Virtualization
{

/** This enum describes the reasons why a payload may not be virtualized */
enum class EPayloadFilterReason : uint16
{
	/** Not filtered, the payload can be virtualized */
	None				= 0,
	/** Filtered due to the asset type of the owning UObject */
	Asset				= 1 << 0,
	/** Filtered due to the path of the owning UPackage */
	Path				= 1 << 1,
	/** Filtered because the payload size is below the minimum size for virtualization */
	MinSize				= 1 << 2,
	/** Filtered because the owning editor bulkdata had virtualization disabled programmatically */
	EditorBulkDataCode	= 1 << 3,
	/** Filtered because the package is either a UMap or the owning editor bulkdata is under a UMapBuildDataRegistry */
	MapContent			= 1 << 4,
};

ENUM_CLASS_FLAGS(EPayloadFilterReason);

} //UE::Virtualization