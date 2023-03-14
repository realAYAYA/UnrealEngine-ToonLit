// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ConcertSyncSessionDatabase.h"

#include "HAL/FileManager.h"
#include "Misc/AutomationTest.h"
#include "Misc/Paths.h"

namespace UE::ConcertSyncTests
{
	class FScopedSessionDatabase : public FConcertSyncSessionDatabase
	{
		FAutomationTestBase& Test;
		const FString TestSessionPath_Server = FPaths::ProjectIntermediateDir() / TEXT("ConcertDatabaseTest_Server");
		const FGuid EndpointID;
	public:

		FScopedSessionDatabase(FAutomationTestBase& Test)
			: Test(Test)
			, EndpointID(FGuid::NewGuid())
		{
			Open(TestSessionPath_Server);

			FConcertSyncEndpointData EndpointData;
			EndpointData.ClientInfo.Initialize();
			if (!SetEndpoint(EndpointID, EndpointData))
			{
				Test.AddError(FString::Printf(TEXT("Test may be faulty because endpoint could not be set: %s"), *GetLastError()));
			}
		}

		~FScopedSessionDatabase()
		{
			if (IsValid() && !Close())
			{
				Test.AddError(TEXT("Failed to close server database"));
			}
			IFileManager::Get().DeleteDirectory(*TestSessionPath_Server, false, true);
		}

		const FGuid& GetEndpoint() const { return EndpointID; }
	};
}