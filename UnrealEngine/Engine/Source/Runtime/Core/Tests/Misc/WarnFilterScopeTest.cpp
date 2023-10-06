// Copyright Epic Games, Inc. All Rights Reserved.

#if WITH_LOW_LEVEL_TESTS

#include "TestHarness.h"
#include "LowLevelTestsRunner/WarnFilterScope.h"
#include "Logging/LogMacros.h"

DECLARE_LOG_CATEGORY_EXTERN(LogTesting, Log, All);
DEFINE_LOG_CATEGORY(LogTesting);

TEST_CASE("UE::Testing::WarnFilterScope")
{
	bool bFiltered = false;
	UE::Testing::FWarnFilterScope _([&bFiltered](const TCHAR* Message, ELogVerbosity::Type Verbosity, const FName& Category)
		{
			bFiltered = FCString::Strcmp(Message, TEXT("Test Message")) == 0 && Verbosity == ELogVerbosity::Type::Warning && Category == TEXT("LogTesting");
			return bFiltered;
		});
	UE_LOG(LogTesting, Warning, TEXT("Test Message"));
	CHECK(bFiltered);
}

#endif
