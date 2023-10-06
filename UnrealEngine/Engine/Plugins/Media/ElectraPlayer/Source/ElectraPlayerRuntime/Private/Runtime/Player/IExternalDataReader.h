// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include <CoreMinimal.h>
#include <Templates/SharedPointer.h>
#include <Containers/Array.h>
#include "PlayerCore.h"

namespace Electra
{

class IExternalDataReader
{
public:
	virtual ~IExternalDataReader() = default;

	struct FReadParams
	{
		FString URI;
		int64 AbsoluteFileOffset = 0;
		int64 NumBytesToRead = 0;
		void* Custom = nullptr;
	};

	using FResponseDataPtr = TSharedPtr<TArray<uint8>, ESPMode::ThreadSafe>;
	DECLARE_DELEGATE_ThreeParams(FElectraExternalDataReadCompleted, FResponseDataPtr /*ResponseData*/, int64 /*TotalFileSize*/, const FReadParams& FromRequestParams);

	virtual void ReadData(const FReadParams& InReadParam, FElectraExternalDataReadCompleted OutCompletionDelegate) = 0;
};

} // namespace Electra
