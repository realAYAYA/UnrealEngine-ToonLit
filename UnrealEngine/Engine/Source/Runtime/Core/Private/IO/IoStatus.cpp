// Copyright Epic Games, Inc. All Rights Reserved.

#include "IO/IoStatus.h"
#include "HAL/Platform.h"
#include "HAL/PlatformString.h"
#include "HAL/UnrealMemory.h"
#include "Logging/LogCategory.h"
#include "Logging/LogMacros.h"
#include "Math/UnrealMathUtility.h"

DEFINE_LOG_CATEGORY_STATIC(LogIoStatus, Log, All);

//////////////////////////////////////////////////////////////////////////

const FIoStatus FIoStatus::Ok		{ EIoErrorCode::Ok,				TEXT("OK")				};
const FIoStatus FIoStatus::Unknown	{ EIoErrorCode::Unknown,		TEXT("Unknown Status")	};
const FIoStatus FIoStatus::Invalid	{ EIoErrorCode::InvalidCode,	TEXT("Invalid Code")	};

//////////////////////////////////////////////////////////////////////////

FIoStatus::FIoStatus()
{
}

FIoStatus::~FIoStatus()
{
}

FIoStatus::FIoStatus(EIoErrorCode Code)
:	ErrorCode(Code)
{
	ErrorMessage[0] = TEXT('\0');
}

FIoStatus::FIoStatus(EIoErrorCode Code, const FStringView& InErrorMessage)
: ErrorCode(Code)
{
	const int32 ErrorMessageLength = FMath::Min(MaxErrorMessageLength - 1, InErrorMessage.Len());
	FPlatformString::Convert(ErrorMessage, ErrorMessageLength, InErrorMessage.GetData(), ErrorMessageLength);
	ErrorMessage[ErrorMessageLength] = TEXT('\0');
}

FIoStatus& FIoStatus::operator=(const FIoStatus& Other)
{
	ErrorCode = Other.ErrorCode;
	FMemory::Memcpy(ErrorMessage, Other.ErrorMessage, MaxErrorMessageLength * sizeof(TCHAR));

	return *this;
}

FIoStatus& FIoStatus::operator=(const EIoErrorCode InErrorCode)
{
	ErrorCode = InErrorCode;
	ErrorMessage[0] = TEXT('\0');

	return *this;
}

bool FIoStatus::operator==(const FIoStatus& Other) const
{
	return ErrorCode == Other.ErrorCode &&
		FPlatformString::Stricmp(ErrorMessage, Other.ErrorMessage) == 0;
}

FString FIoStatus::ToString() const
{
	return FString::Format(TEXT("{0} ({1})"), { ErrorMessage, GetIoErrorText(ErrorCode) });
}

void StatusOrCrash(const FIoStatus& Status)
{
	UE_LOG(LogIoStatus, Fatal, TEXT("I/O Error '%s'"), *Status.ToString());
}

//////////////////////////////////////////////////////////////////////////

FIoStatusBuilder::FIoStatusBuilder(EIoErrorCode InStatusCode)
:	StatusCode(InStatusCode)
{
}

FIoStatusBuilder::FIoStatusBuilder(const FIoStatus& InStatus, FStringView String)
:	StatusCode(InStatus.ErrorCode)
{
	Message.Append(String.GetData(), String.Len());
}

FIoStatusBuilder::~FIoStatusBuilder()
{
}

FIoStatusBuilder::operator FIoStatus()
{
	return FIoStatus(StatusCode, Message);
}

FIoStatusBuilder& FIoStatusBuilder::operator<<(FStringView String)
{
	Message.Append(String.GetData(), String.Len());

	return *this;
}

FIoStatusBuilder operator<<(const FIoStatus& Status, FStringView String)
{ 
	return FIoStatusBuilder(Status, String);
}
