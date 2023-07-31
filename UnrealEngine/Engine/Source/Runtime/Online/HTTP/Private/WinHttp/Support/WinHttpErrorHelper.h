// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#if WITH_WINHTTP

#include "CoreMinimal.h"

class HTTP_API FWinHttpErrorHelper
{
public:
	/**
	 * Log that an WinHttpOpen call failed with the specified ErrorCode
	 *
	 * @param ErrorCode The error that occured
	 */
	static void LogWinHttpOpenFailure(const uint32 ErrorCode);

	/**
	 * Log that an WinHttpCloseHandle call failed with the specified ErrorCode
	 *
	 * @param ErrorCode The error that occured
	 */
	static void LogWinHttpCloseHandleFailure(const uint32 ErrorCode);

	/**
	 * Log that an WinHttpSetTimeouts call failed with the specified ErrorCode
	 *
	 * @param ErrorCode The error that occured
	 */
	static void LogWinHttpSetTimeoutsFailure(const uint32 ErrorCode);

	/**
	 * Log that an WinHttpConnect call failed with the specified ErrorCode
	 *
	 * @param ErrorCode The error that occured
	 */
	static void LogWinHttpConnectFailure(const uint32 ErrorCode);

	/**
	 * Log that an WinHttpOpenRequest call failed with the specified ErrorCode
	 *
	 * @param ErrorCode The error that occured
	 */
	static void LogWinHttpOpenRequestFailure(const uint32 ErrorCode);

	/**
	 * Log that an WinHttpSetOption call failed with the specified ErrorCode
	 *
	 * @param ErrorCode The error that occured
	 */
	static void LogWinHttpSetOptionFailure(const uint32 ErrorCode);

	/**
	 * Log that an WinHttpAddRequestHeaders call failed with the specified ErrorCode
	 *
	 * @param ErrorCode The error that occured
	 */
	static void LogWinHttpAddRequestHeadersFailure(const uint32 ErrorCode);

	/**
	 * Log that an WinHttpSetStatusCallback call failed with the specified ErrorCode
	 *
	 * @param ErrorCode The error that occured
	 */
	static void LogWinHttpSetStatusCallbackFailure(const uint32 ErrorCode);

	/**
	 * Log that an WinHttpSendRequest call failed with the specified ErrorCode
	 *
	 * @param ErrorCode The error that occured
	 */
	static void LogWinHttpSendRequestFailure(const uint32 ErrorCode);

	/**
	 * Log that an WinHttpReceiveResponse call failed with the specified ErrorCode
	 *
	 * @param ErrorCode The error that occured
	 */
	static void LogWinHttpReceiveResponseFailure(const uint32 ErrorCode);

	/**
	 * Log that an WinHttpQueryHeaders call failed with the specified ErrorCode
	 *
	 * @param ErrorCode The error that occured
	 */
	static void LogWinHttpQueryHeadersFailure(const uint32 ErrorCode);

	/**
	 * Log that an WinHttpQueryDataAvailable call failed with the specified ErrorCode
	 *
	 * @param ErrorCode The error that occured
	 */
	static void LogWinHttpQueryDataAvailableFailure(const uint32 ErrorCode);

	/**
	 * Log that an WinHttpReadData call failed with the specified ErrorCode
	 *
	 * @param ErrorCode The error that occured
	 */
	static void LogWinHttpReadDataFailure(const uint32 ErrorCode);

	/**
	 * Log that an WinHttpWriteData call failed with the specified ErrorCode
	 *
	 * @param ErrorCode The error that occured
	 */
	static void LogWinHttpWriteDataFailure(const uint32 ErrorCode);

private:
	/** This class should not be instantiated */
	FWinHttpErrorHelper() = delete;
};


#endif // WITH_WINHTTP
