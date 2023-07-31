// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "HttpPath.h"
#include "HttpServerHttpVersion.h"

enum class EHttpServerRequestVerbs : uint16
{
	VERB_NONE   = 0,
	VERB_GET    = 1 << 0,
	VERB_POST   = 1 << 1,
	VERB_PUT    = 1 << 2,
	VERB_PATCH  = 1 << 3,
	VERB_DELETE = 1 << 4,
	VERB_OPTIONS = 1 << 5
};

ENUM_CLASS_FLAGS(EHttpServerRequestVerbs);

struct FHttpServerRequest
{
public:

	/** Constructor */
	FHttpServerRequest() { };

	/** The handler-route-relative HTTP path */
	FHttpPath RelativePath;

	/** The HTTP-compliant verb  */
	EHttpServerRequestVerbs Verb;

	/** The request HTTP protocol version */
	HttpVersion::EHttpServerHttpVersion HttpVersion;

	/** The HTTP headers */
	TMap<FString, TArray<FString>> Headers;

	/** The query parameters */
	TMap<FString, FString> QueryParams;

	/** The path parameters */
	TMap<FString, FString> PathParams;

	/** The raw body contents */
	TArray<uint8> Body;

};

