// Copyright Epic Games, Inc. All Rights Reserved.

#include "AutoChangeDatabase.h"

BEGIN_NAMESPACE_UE_AC

FAutoChangeDatabase::FAutoChangeDatabase(API_DatabaseTypeID InDbType, bool* OutSucceed)
	: bWasDifferent(false)
{
	// Save current DB
	Zap(&PreviousDB);
	UE_AC_TestGSError(ACAPI_Database(APIDb_GetCurrentDatabaseID, &PreviousDB, NULL));

	if (PreviousDB.typeID != InDbType)
	{
		// Switch to the specified DB
		API_DatabaseInfo NewDB;
		Zap(&NewDB);
		NewDB.typeID = InDbType;
		GSErrCode GSErr = ACAPI_Database(APIDb_ChangeCurrentDatabaseID, &NewDB, NULL);
		if (GSErr == NoError)
		{
			bWasDifferent = true;
		}
		else
		{
			if (OutSucceed)
			{
				*OutSucceed = false;
			}
			else
			{
				UE_AC_TestGSError(GSErr);
			}
		}
	}
	if (OutSucceed)
	{
		*OutSucceed = true;
	}
}

FAutoChangeDatabase::~FAutoChangeDatabase()
{
	if (bWasDifferent)
	{
		// Bring back to previous DB
		bWasDifferent = false;
		GSErrCode GSErr = ACAPI_Database(APIDb_ChangeCurrentDatabaseID, &PreviousDB, NULL);
		if (GSErr != NoError)
		{
			UE_AC_DebugF("FAutoChangeDatabase::~FAutoChangeDatabase - Error %d\n", GSErr);
		}
	}
}

END_NAMESPACE_UE_AC
