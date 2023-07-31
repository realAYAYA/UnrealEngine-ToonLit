// Copyright Epic Games, Inc. All Rights Reserved.

#if ELECTRA_HTTPSTREAM_WINHTTP

#include "WinHttp/WinHttpLogError.h"
#include "ElectraHTTPStreamModule.h"

#include "Windows/AllowWindowsPlatformTypes.h"
#include <errhandlingapi.h>
#include <winhttp.h>
#include "Windows/HideWindowsPlatformTypes.h"


namespace ElectraHTTPStreamWinHttp
{

FString GetSecurityErrorMessage(uint32 Flags)
{
	auto PrependDelimiter = [](FString& s) -> void
	{
		if (s.Len())
		{
			s.InsertAt(0, TEXT("; "));
		}
	};

	FString msg;
	if ((Flags & WINHTTP_CALLBACK_STATUS_FLAG_CERT_REV_FAILED) != 0)
	{
		msg.Append(TEXT("Certification revocation check failed"));
	}
	if ((Flags & WINHTTP_CALLBACK_STATUS_FLAG_INVALID_CERT) != 0)
	{
		PrependDelimiter(msg);
		msg.Append(TEXT("SSL certificate is invalid"));
	}
	if ((Flags & WINHTTP_CALLBACK_STATUS_FLAG_CERT_REVOKED) != 0)
	{
		PrependDelimiter(msg);
		msg.Append(TEXT("SSL certificate was revoked"));
	}
	if ((Flags & WINHTTP_CALLBACK_STATUS_FLAG_INVALID_CA) != 0)
	{
		PrependDelimiter(msg);
		msg.Append(TEXT("Unfamiliar Certificate Authority that generated the server's certificate"));
	}
	if ((Flags & WINHTTP_CALLBACK_STATUS_FLAG_CERT_CN_INVALID) != 0)
	{
		PrependDelimiter(msg);
		msg.Append(TEXT("SSL certificate common name (host name field) is incorrect"));
	}
	if ((Flags & WINHTTP_CALLBACK_STATUS_FLAG_CERT_DATE_INVALID) != 0)
	{
		PrependDelimiter(msg);
		msg.Append(TEXT("SSL certificate date is expired"));
	}
	if ((Flags & WINHTTP_CALLBACK_STATUS_FLAG_CERT_WRONG_USAGE) != 0)
	{
		PrependDelimiter(msg);
		msg.Append(TEXT("Wrong SSL certificate usage"));
	}
	if ((Flags & WINHTTP_CALLBACK_STATUS_FLAG_SECURITY_CHANNEL_ERROR) != 0)
	{
		PrependDelimiter(msg);
		msg.Append(TEXT("The application experienced an internal error loading the SSL libraries"));
	}
	return msg;
}

FString GetApiName(uint32 ApiNameCode)
{
	switch(ApiNameCode)
	{
		case API_RECEIVE_RESPONSE:		return TEXT("RECEIVE_RESPONSE");
		case API_QUERY_DATA_AVAILABLE:	return TEXT("QUERY_DATA_AVAILABLE");
		case API_READ_DATA:				return TEXT("READ_DATA");
		case API_WRITE_DATA:			return TEXT("WRITE_DATA");
		case API_SEND_REQUEST:			return TEXT("SEND_REQUEST");
		case API_GET_PROXY_FOR_URL:		return TEXT("GET_PROXY_FOR_URL");
		default:						return FString::Printf(TEXT("Unknown API name code %u"), ApiNameCode);
	}
}


FString GetErrorMessage(uint32 ErrorCode)
{
	switch(ErrorCode)
	{
		case ERROR_WINHTTP_OUT_OF_HANDLES:							return TEXT("OUT_OF_HANDLES");
		case ERROR_WINHTTP_TIMEOUT:									return TEXT("TIMEOUT");
		case ERROR_WINHTTP_INTERNAL_ERROR:							return TEXT("INTERNAL_ERROR");
		case ERROR_WINHTTP_INVALID_URL:								return TEXT("INVALID_URL");
		case ERROR_WINHTTP_UNRECOGNIZED_SCHEME:						return TEXT("UNRECOGNIZED_SCHEME");
		case ERROR_WINHTTP_NAME_NOT_RESOLVED:						return TEXT("NAME_NOT_RESOLVED");
		case ERROR_WINHTTP_INVALID_OPTION:							return TEXT("INVALID_OPTION");
		case ERROR_WINHTTP_OPTION_NOT_SETTABLE:						return TEXT("OPTION_NOT_SETTABLE");
		case ERROR_WINHTTP_SHUTDOWN:								return TEXT("SHUTDOWN");
		case ERROR_WINHTTP_LOGIN_FAILURE:							return TEXT("LOGIN_FAILURE");
		case ERROR_WINHTTP_OPERATION_CANCELLED:						return TEXT("OPERATION_CANCELLED");
		case ERROR_WINHTTP_INCORRECT_HANDLE_TYPE:					return TEXT("INCORRECT_HANDLE_TYPE");
		case ERROR_WINHTTP_INCORRECT_HANDLE_STATE:					return TEXT("INCORRECT_HANDLE_STATE");
		case ERROR_WINHTTP_CANNOT_CONNECT:							return TEXT("CANNOT_CONNECT");
		case ERROR_WINHTTP_CONNECTION_ERROR:						return TEXT("CONNECTION_ERROR");
		case ERROR_WINHTTP_RESEND_REQUEST:							return TEXT("RESEND_REQUEST");
		case ERROR_WINHTTP_CLIENT_AUTH_CERT_NEEDED:					return TEXT("CLIENT_AUTH_CERT_NEEDED");
		case ERROR_WINHTTP_CANNOT_CALL_BEFORE_OPEN:					return TEXT("CANNOT_CALL_BEFORE_OPEN");
		case ERROR_WINHTTP_CANNOT_CALL_BEFORE_SEND:					return TEXT("CANNOT_CALL_BEFORE_SEND");
		case ERROR_WINHTTP_CANNOT_CALL_AFTER_SEND:					return TEXT("CANNOT_CALL_AFTER_SEND");
		case ERROR_WINHTTP_CANNOT_CALL_AFTER_OPEN:					return TEXT("CANNOT_CALL_AFTER_OPEN");
		case ERROR_WINHTTP_HEADER_NOT_FOUND:						return TEXT("HEADER_NOT_FOUND");
		case ERROR_WINHTTP_INVALID_SERVER_RESPONSE:					return TEXT("INVALID_SERVER_RESPONSE");
		case ERROR_WINHTTP_INVALID_HEADER:							return TEXT("INVALID_HEADER");
		case ERROR_WINHTTP_INVALID_QUERY_REQUEST:					return TEXT("INVALID_QUERY_REQUEST");
		case ERROR_WINHTTP_HEADER_ALREADY_EXISTS:					return TEXT("HEADER_ALREADY_EXISTS");
		case ERROR_WINHTTP_REDIRECT_FAILED:							return TEXT("REDIRECT_FAILED");
		case ERROR_WINHTTP_AUTO_PROXY_SERVICE_ERROR:				return TEXT("AUTO_PROXY_SERVICE_ERROR");
		case ERROR_WINHTTP_BAD_AUTO_PROXY_SCRIPT:					return TEXT("BAD_AUTO_PROXY_SCRIPT");
		case ERROR_WINHTTP_UNABLE_TO_DOWNLOAD_SCRIPT:				return TEXT("UNABLE_TO_DOWNLOAD_SCRIPT");
		case ERROR_WINHTTP_UNHANDLED_SCRIPT_TYPE:					return TEXT("UNHANDLED_SCRIPT_TYPE");
		case ERROR_WINHTTP_SCRIPT_EXECUTION_ERROR:					return TEXT("SCRIPT_EXECUTION_ERROR");
		case ERROR_WINHTTP_NOT_INITIALIZED:							return TEXT("NOT_INITIALIZED");
		case ERROR_WINHTTP_SECURE_FAILURE:							return TEXT("SECURE_FAILURE");
		case ERROR_WINHTTP_SECURE_CERT_DATE_INVALID:				return TEXT("SECURE_CERT_DATE_INVALID");
		case ERROR_WINHTTP_SECURE_CERT_CN_INVALID:					return TEXT("SECURE_CERT_CN_INVALID");
		case ERROR_WINHTTP_SECURE_INVALID_CA:						return TEXT("SECURE_INVALID_CA");
		case ERROR_WINHTTP_SECURE_CERT_REV_FAILED:					return TEXT("SECURE_CERT_REV_FAILED");
		case ERROR_WINHTTP_SECURE_CHANNEL_ERROR:					return TEXT("SECURE_CHANNEL_ERROR");
		case ERROR_WINHTTP_SECURE_INVALID_CERT:						return TEXT("SECURE_INVALID_CERT");
		case ERROR_WINHTTP_SECURE_CERT_REVOKED:						return TEXT("SECURE_CERT_REVOKED");
		case ERROR_WINHTTP_SECURE_CERT_WRONG_USAGE:					return TEXT("SECURE_CERT_WRONG_USAGE");
		case ERROR_WINHTTP_AUTODETECTION_FAILED:					return TEXT("AUTODETECTION_FAILED");
		case ERROR_WINHTTP_HEADER_COUNT_EXCEEDED:					return TEXT("HEADER_COUNT_EXCEEDED");
		case ERROR_WINHTTP_HEADER_SIZE_OVERFLOW:					return TEXT("HEADER_SIZE_OVERFLOW");
		case ERROR_WINHTTP_CHUNKED_ENCODING_HEADER_SIZE_OVERFLOW:	return TEXT("CHUNKED_ENCODING_HEADER_SIZE_OVERFLOW");
		case ERROR_WINHTTP_RESPONSE_DRAIN_OVERFLOW:					return TEXT("RESPONSE_DRAIN_OVERFLOW");
		case ERROR_WINHTTP_CLIENT_CERT_NO_PRIVATE_KEY:				return TEXT("CLIENT_CERT_NO_PRIVATE_KEY");
		case ERROR_WINHTTP_CLIENT_CERT_NO_ACCESS_PRIVATE_KEY:		return TEXT("CLIENT_CERT_NO_ACCESS_PRIVATE_KEY");
		case ERROR_WINHTTP_CLIENT_AUTH_CERT_NEEDED_PROXY:			return TEXT("CLIENT_AUTH_CERT_NEEDED_PROXY");
		case ERROR_WINHTTP_SECURE_FAILURE_PROXY:					return TEXT("SECURE_FAILURE_PROXY");
		case ERROR_WINHTTP_HTTP_PROTOCOL_MISMATCH:					return TEXT("HTTP_PROTOCOL_MISMATCH");
		case 5:														return TEXT("ACCESS_DENIED");		// When an unsafe operation is attempted on a secure handle
		default:													return FString::Printf(TEXT("Unknown error code 0x%08x"), ErrorCode);
	}
}

FString GetErrorLogMessage(const TCHAR* const Method, uint32 ErrorCode)
{
	if (Method)
	{
		return FString::Printf(TEXT("Error calling WinHttp method %s: %s"), Method, *GetErrorMessage(ErrorCode));
	}
	else
	{
		return FString::Printf(TEXT("Error calling WinHttp method: %s"), *GetErrorMessage(ErrorCode));
	}
}

void LogError(const FString& Message)
{
	UE_LOG(LogElectraHTTPStream, Error, TEXT("%s"), *Message);
}

void LogError(const TCHAR* const Method, uint32 ErrorCode)
{
	UE_LOG(LogElectraHTTPStream, Error, TEXT("%s"), *GetErrorLogMessage(Method, ErrorCode));
}

}

#endif // ELECTRA_HTTPSTREAM_WINHTTP
