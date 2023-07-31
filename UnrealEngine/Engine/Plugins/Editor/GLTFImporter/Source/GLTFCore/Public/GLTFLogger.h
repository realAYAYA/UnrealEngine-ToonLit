// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/Array.h"
#include "Templates/Tuple.h"

namespace GLTF
{
	enum class EMessageSeverity
	{
		Warning,
		Error,
	};
	using FLogMessage = TTuple<EMessageSeverity, FString>;

	class GLTFCORE_API FBaseLogger
	{
	public:
		const TArray<FLogMessage>& GetLogMessages() const;

	protected:
		mutable TArray<FLogMessage> Messages;
	};

	inline const TArray<FLogMessage>& FBaseLogger::GetLogMessages() const
	{
		return Messages;
	}
}  // namespace GLTF
