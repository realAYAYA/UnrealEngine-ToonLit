// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#define ONLINE_ERROR_LEGACY 1

namespace EOnlineServerConnectionStatus {
	enum Type : uint8;
}

/**
 * Common error results
 */
enum class EOnlineErrorResult : uint8
{
	/** Successful result. no further error processing needed */
	Success,
	
	/** Failed due to no connection */
	NoConnection,
	/** */
	RequestFailure,
	/** */
	InvalidCreds,
	/** Failed due to invalid or missing user */
	InvalidUser,
	/** Failed due to invalid or missing auth for user */
	InvalidAuth,
	/** Failed due to invalid access */
	AccessDenied,
	/** Throttled due to too many requests */
	TooManyRequests,
	/** Async request was already pending */
	AlreadyPending,
	/** Invalid parameters specified for request */
	InvalidParams,
	/** Data could not be parsed for processing */
	CantParse,
	/** Invalid results returned from the request. Parsed but unexpected results */
	InvalidResults,
	/** Incompatible client for backend version */
	IncompatibleVersion,
	/** Not configured correctly for use */
	NotConfigured,
	/** Feature not available on this implementation */
	NotImplemented,
	/** Interface is missing */
	MissingInterface,
	/** Operation was canceled (likely by user) */
	Canceled,
	/** Extended error. More info can be found in the results or by looking at the ErrorCode */
	FailExtended,

	/** Default state */
	Unknown
};

#define ONLINE_ERROR_CONTEXT_SEPARATOR TEXT(":")

/** Generic Error response for OSS calls */
struct ONLINESUBSYSTEM_API FOnlineError
{

private:

	/** Ctors. Use ONLINE_ERROR macro instead */
	
	explicit FOnlineError(EOnlineErrorResult InResult, const FString& InErrorCode, const FText& InErrorMessage);

public:

#if ONLINE_ERROR_LEGACY
	explicit FOnlineError(const TCHAR* const ErrorCode);
	explicit FOnlineError(const int32 ErrorCode);
	void SetFromErrorCode(const int32 ErrorCode);
	void SetFromErrorMessage(const FText& ErrorMessage, const int32 ErrorCode);
#endif

	explicit FOnlineError(EOnlineErrorResult InResult = EOnlineErrorResult::Unknown);

	/** Create factory for proper namespacing. Use ONLINE_ERROR macro  */
	static FOnlineError CreateError(const FString& ErrorNamespace, EOnlineErrorResult Result, const FString& ErrorCode, const FText& ErrorMessage = FText::GetEmpty());
	
	/** Use a default error code / display text */
	static FOnlineError CreateError(const FString& ErrorNamespace, EOnlineErrorResult Result);

	// helpers for the most common error types
	static const FOnlineError& Success();

	explicit FOnlineError(bool bSucceeded);
	explicit FOnlineError(const FString& ErrorCode);
	explicit FOnlineError(FString&& ErrorCode);
	explicit FOnlineError(const FText& ErrorMessage);

	/** Same as the Ctors but can be called any time (does NOT set bSucceeded to false) */
	void SetFromErrorCode(const FString& ErrorCode);
	void SetFromErrorCode(FString&& ErrorCode);
	void SetFromErrorMessage(const FText& ErrorMessage);

	/** Accessors */
	inline EOnlineErrorResult GetErrorResult() const { return Result; }
	inline const FText& GetErrorMessage() const { return ErrorMessage; }
	inline const FString& GetErrorRaw() const { return ErrorRaw; }
	inline const FString& GetErrorCode() const { return ErrorCode; }
	inline bool WasSuccessful() const { return bSucceeded || Result == EOnlineErrorResult::Success; }

	/** Setters for adding the raw error */
	inline FOnlineError& SetErrorRaw(const FString& Val) { ErrorRaw = Val; return *this; }

	/** Code useful when all you have is raw error info from old APIs */
	static const FString GenericErrorCode;

	/** prints out everything, need something like this!!! */
	FString GetErrorLegacy();
	/** Call this if you want to log this out (will pick the best string representation) */
	FString ToLogString() const;

	bool operator==(const FOnlineError& Other) const
	{
		return Result == Other.Result && ErrorCode == Other.ErrorCode;
	}

	bool operator!=(const FOnlineError& Other) const
	{
		return !(FOnlineError::operator==(Other));
	}

	FOnlineError operator+ (const FOnlineError& RHS) const
	{
		FOnlineError Copy(*this);
		Copy += RHS;
		return Copy;
	}

	FOnlineError operator+ (const FString& RHS) const
	{
		FOnlineError Copy(*this);
		Copy += RHS;
		return Copy;
	}

  	FOnlineError& operator+= (const FOnlineError& RHS)
  	{
 		ErrorRaw += ONLINE_ERROR_CONTEXT_SEPARATOR + RHS.ErrorRaw;
 		ErrorCode += ONLINE_ERROR_CONTEXT_SEPARATOR + RHS.ErrorCode;
  		return *this;
  	}
 
 	FOnlineError& operator+= (const FString& RHS)
 	{
 		ErrorCode += ONLINE_ERROR_CONTEXT_SEPARATOR + RHS;
 		return *this;
 	}

public:
	/** Did the request succeed fully. If this is true the rest of the struct probably doesn't matter */
	bool bSucceeded;

	/** The raw unparsed error message from server. Used for pass-through error processing by other systems. */
	FString ErrorRaw;

	/** Intended to be interpreted by code. */
	FString ErrorCode;

	/** Suitable for display to end user. Guaranteed to be in the current locale (or empty) */
	FText ErrorMessage;

protected:

	static FString DefaultErrorCode(EOnlineErrorResult Result);
	/** Default messaging for common errors */
	static FText DefaultErrorMsg(EOnlineErrorResult Result);
	/** Default namespace for online errors */
	static const FString& GetDefaultErrorNamespace();

	/** Setters for updating individual values directly */
	inline FOnlineError& SetResult(EOnlineErrorResult Val) { Result = Val; return *this; }
	inline FOnlineError& SetErrorCode(const FString& Val) { ErrorCode = Val; return *this; }
	inline FOnlineError& SetErrorMessage(const FText& Val) { ErrorMessage = Val; return *this; }

	/** Helpers for constructing errors */
	void SetFromErrorCode(EOnlineErrorResult InResult);
	void SetFromErrorCode(EOnlineErrorResult InResult, const FString& InErrorCode);
	void SetFromErrorCode(EOnlineErrorResult InResult, const FString& InErrorCode, const FText& InErrorText);

	/** If successful result then the rest of the struct probably doesn't matter */
	EOnlineErrorResult Result;
};

/** must be defined to a valid namespace for using ONLINE_ERROR factory macro */
#undef ONLINE_ERROR_NAMESPACE
#define ONLINE_ERROR(...) FOnlineError::CreateError(TEXT(ONLINE_ERROR_NAMESPACE), __VA_ARGS__)
