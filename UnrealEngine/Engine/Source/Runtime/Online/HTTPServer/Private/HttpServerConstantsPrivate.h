// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"

namespace FHttpServerHeaderKeys
{
	const FString CONTENT_TYPE = TEXT("content-type");
	const FString CONTENT_LENGTH = TEXT("content-length");
	const FString CONNECTION = TEXT("connection");
	const FString KEEP_ALIVE = TEXT("keep-alive");
};

namespace FHttpServerErrorStrings
{
	// Connection
	const FString SocketClosedFailure = TEXT("errors.com.epicgames.httpserver.socket_closed_failure");
	const FString SocketRecvFailure = TEXT("errors.com.epicgames.httpserver.socket_recv_failure");
	const FString SocketSendFailure = TEXT("errors.com.epicgames.httpserver.socket_send_failure");

	// Routing
	const FString NotFound = TEXT("errors.com.epicgames.httpserver.route_handler_not_found");

	// Serialization
	const FString MalformedRequestSize = TEXT("errors.com.epicgames.httpserver.malformed_request_size");
	const FString MalformedRequestHeaders = TEXT("errors.com.epicgames.httpserver.malformed_request_header");
	const FString MissingRequestHeaders = TEXT("errors.com.epicgames.httpserver.missing_request_headers");
	const FString MalformedRequestBody = TEXT("errors.com.epicgames.httpserver.malformed_request_body");
	const FString UnknownRequestVerb = TEXT("errors.com.epicgames.httpserver.unknown_request_verb");
	const FString UnsupportedHttpVersion = TEXT("errors.com.epicgames.httpserver.unsupported_http_version");
	const FString InvalidContentLengthHeader = TEXT("errors.com.epicgames.httpserver.invalid_content_length_header");
	const FString MissingContentLengthHeader = TEXT("errors.com.epicgames.httpserver.missing_content_length_header");
	const FString MismatchedContentLengthBodyTooLarge =  TEXT("errors.com.epicgames.httpserver.mismatched_content_length_body_too_large");
};

