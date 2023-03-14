// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include <iostream>
#include <sstream>

/**
 * Redirect std::cout to UE_LOG (since this class is constructed until it is destructed/goes out of scope).
 * Undefined behavior (i.e., potential crashes) if this class is constructed more than once simultaneously.
 */
class THIRDPARTYHELPERANDDLLLOADER_API FRedirectCoutAndCerrToUeLog
{
public:
	FRedirectCoutAndCerrToUeLog();
	~FRedirectCoutAndCerrToUeLog();

private:
	std::streambuf* BackupStreamReadBufCout;
	std::streambuf* BackupStreamReadBufCerr;

	class LStream : public std::stringbuf
	{
	public:
		LStream(const bool bInTrueIfCoutFalseIfCerr);

	protected:
		int sync();

	private:
		const bool bTrueIfCoutFalseIfCerr;
	};

	LStream StreamCout;
	LStream StreamCerr;
};
