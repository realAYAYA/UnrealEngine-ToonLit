// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UObject/NameTypes.h"

namespace UE::Net
{

// Some common errors. Licensees should add their own headers if they want to easily share errors between files.
IRISCORE_API extern const FName GNetError_BitStreamOverflow;
IRISCORE_API extern const FName GNetError_BitStreamError;
IRISCORE_API extern const FName GNetError_ArraySizeTooLarge;
IRISCORE_API extern const FName GNetError_InvalidNetHandle;
IRISCORE_API extern const FName GNetError_InvalidValue;

class FNetErrorContext
{
public:
	bool HasError() const;
	/** If an error has already been set calling this function again will be a no-op. */
	IRISCORE_API void SetError(const FName Error);
	FName GetError() const { return Error; }

private:
	FName Error;
};

inline bool FNetErrorContext::HasError() const
{
	return !Error.IsNone();
}

}
