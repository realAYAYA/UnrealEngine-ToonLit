// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Containers/Map.h"
#include "Misc/Variant.h"

/***************************************************************************************************************************************************/
#include "Logging/LogMacros.h"

DECLARE_LOG_CATEGORY_EXTERN(LogLibAV, Log, All);
/***************************************************************************************************************************************************/

class ILibavDecoder
{
public:
	static LIBAV_API void Startup();
	static LIBAV_API void Shutdown();
	static LIBAV_API bool IsLibAvAvailable();
	static LIBAV_API void LogLibraryNeeded();

	virtual ~ILibavDecoder() = default;

	struct FInputAccessUnit
	{
		const void* Data = nullptr;
		int32 DataSize = 0;
		int64 DTS = 0;
		int64 PTS = 0;
		int64 Duration = 0;
		uint64 UserValue = 0;
	};

	enum class EDecoderError
	{
		// No error, all is well.
		None,
		// End of data processed
		EndOfData,
		// No buffer available when sending access unit to decode or no output available yet (more input required)
		NoBuffer,
		// Method not supported
		NotSupported,
		// An internal decoder error occurred. Call GetLastLibraryError() to get the error code.
		Error
	};

	enum class EOutputStatus
	{
		// Output is available.
		Available,
		// Output is not available. Provide more input.
		NeedInput,
		// All output has been provided.
		EndOfData
	};

	virtual int32 GetLastLibraryError() const = 0;
};
