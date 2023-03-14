// Copyright Epic Games, Inc. All Rights Reserved.
#include "Online/OnlineError.h"
#include "Online/OnlineErrorCode.h"

namespace UE::Online{
	namespace Errors {
		uint64 ErrorCodeSystem(ErrorCodeType ErrorCode) { return (ErrorCode >> 60 & 0xfull); }
		uint64 ErrorCodeCategory(ErrorCodeType ErrorCode) { return (ErrorCode >> 32 & 0x0fffffffull); }
		uint64 ErrorCodeValue(ErrorCodeType ErrorCode) { return ErrorCode & 0xffffffffull; }

		namespace ErrorCode
		{
			FString ToString(ErrorCodeType ErrorCode)
			{
				const uint64 Source = ErrorCodeSystem(ErrorCode);
				const uint64 Category = ErrorCodeCategory(ErrorCode); 
				const uint64 Code = ErrorCodeValue(ErrorCode); 
				if (Source == 0)
				{
					return FString::Printf(TEXT("%llx.%llx"), Category, Code);
				}
				else
				{
					return FString::Printf(TEXT("%llx.%llx.%llx"), Source, Category, Code);
				}
			}
		} /*namespace ErrorCode */
	} /* namespace Errors */

	bool operator==(const FOnlineError& Lhs, const FOnlineError& Rhs)
	{
		const FOnlineError * Error = &Lhs;
		while(Error)
		{
			if(Rhs == Error->GetErrorCode())
			{
				return true;
			}
			Error = Error->GetInner();
		}
		return false;
	}

	bool operator==(const FOnlineError& Lhs, ErrorCodeType OtherErrorCode)
	{
		if (Lhs.GetErrorCode() == OtherErrorCode)
		{
			return true;
		}

		const FOnlineError* LoopInner = Lhs.GetInner();
		while (LoopInner != nullptr)
		{
			if (LoopInner->GetErrorCode() == OtherErrorCode)
			{
				return true;
			}
			LoopInner = LoopInner->GetInner();
		}

		return false;
	}

} /* namespace UE::Online */

