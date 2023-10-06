// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once
#include "Containers/Array.h"
#include "Containers/ArrayView.h"
#include "Containers/Map.h"
#include "Containers/UnrealString.h"
#include "CoreMinimal.h"
#include "HAL/Platform.h"
#include "HttpServerConstants.h"
#include "HttpServerHttpVersion.h"
#include "Templates/UniquePtr.h"
#include "Templates/UnrealTemplate.h"

struct FHttpServerResponse final
{
public:
	/**
	 * Constructor
	 */
	FHttpServerResponse() 
	{ }

	/**
	 * Constructor
	 * Facilitates in-place body construction
     *
	 * @param InBody The r-value body data
	 */
	FHttpServerResponse(TArray<uint8>&& InBody)
		: Body(MoveTemp(InBody))
	{ }

	/** Http protocol version */
	HttpVersion::EHttpServerHttpVersion HttpVersion;

	/** Http Response Code */
	EHttpServerResponseCodes Code;

	/** Http Headers */
	TMap<FString, TArray<FString>> Headers;

	/** Http Body Content */
	TArray<uint8> Body;

public:

	/**
	 * Creates an FHttpServerResponse from a string
	 * 
	 * @param  Text         The text to serialize
	 * @param  ContentType  The HTTP response content type
	 * @return              A unique pointer to an initialized response object
	 */
	HTTPSERVER_API static TUniquePtr<FHttpServerResponse> Create(const FString& Text, FString ContentType);

	/**
	 * Creates an FHttpServerResponse from a raw byte buffer
	 *
	 * @param  RawBytes     The byte buffer to serialize
	 * @param  ContentType  The HTTP response content type
	 * @return              A unique pointer to an initialized response object
	 */
	HTTPSERVER_API static TUniquePtr<FHttpServerResponse> Create(TArray<uint8>&& RawBytes, FString ContentType);

	/**
	 * Creates an FHttpServerResponse from a raw byte buffer
	 *
	 * @param  RawBytes     The byte buffer view to serialize
	 * @param  ContentType  The HTTP response content type
	 * @return              A unique pointer to an initialized response object
	 */
	HTTPSERVER_API static TUniquePtr<FHttpServerResponse> Create(const TArrayView<uint8>& RawBytes, FString ContentType);

	/**
	 * Creates an FHttpServerResponse 204
	 * 
	 * @return A unique pointer to an initialized response object
	 */
	HTTPSERVER_API static TUniquePtr<FHttpServerResponse> Ok();

	/**
    * Creates an FHttpServerResponse with the caller-supplied response and error codes
	*
	* @param ResponseCode The HTTP response code
	* @param ErrorCode    The machine-readable error code
	* @param ErrorMessage The contextually descriptive error message
    * @return A unique pointer to an initialized response object
    */
	HTTPSERVER_API static TUniquePtr<FHttpServerResponse> Error(EHttpServerResponseCodes ResponseCode, const FString& ErrorCode = TEXT(""), const FString& ErrorMessage = TEXT(""));
};


