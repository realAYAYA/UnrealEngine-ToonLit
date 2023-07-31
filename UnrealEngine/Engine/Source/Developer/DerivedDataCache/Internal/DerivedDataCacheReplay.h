// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "Templates/PimplPtr.h"

#define UE_API DERIVEDDATACACHE_API

class FArchive;
class FCbObjectView;

namespace UE::DerivedData { class FCacheKeyFilter; }
namespace UE::DerivedData { class FCacheMethodFilter; }
namespace UE::DerivedData { class ILegacyCacheStore; }
namespace UE::DerivedData { enum class ECacheMethod : uint8; }
namespace UE::DerivedData { enum class ECachePolicy : uint32; }
namespace UE::DerivedData { enum class EPriority : uint8; }

namespace UE::DerivedData
{

class FCacheReplayReader
{
public:
	static constexpr uint64 DefaultScratchSize = 1024;

	UE_API explicit FCacheReplayReader(ILegacyCacheStore* TargetCache);

	UE_API void SetKeyFilter(FCacheKeyFilter KeyFilter);
	UE_API void SetMethodFilter(FCacheMethodFilter MethodFilter);
	UE_API void SetPolicyTransform(ECachePolicy AddFlags, ECachePolicy RemoveFlags);
	UE_API void SetPriorityOverride(EPriority Priority);

	UE_API void ReadFromFileAsync(const TCHAR* ReplayPath, uint64 ScratchSize = DefaultScratchSize);
	UE_API bool ReadFromFile(const TCHAR* ReplayPath, uint64 ScratchSize = DefaultScratchSize);
	UE_API bool ReadFromArchive(FArchive& ReplayAr, uint64 ScratchSize = DefaultScratchSize);
	UE_API bool ReadFromObject(FCbObjectView Object);

private:
	class FState;
	TPimplPtr<FState> State;
};

} // UE::DerivedData

#undef UE_API
