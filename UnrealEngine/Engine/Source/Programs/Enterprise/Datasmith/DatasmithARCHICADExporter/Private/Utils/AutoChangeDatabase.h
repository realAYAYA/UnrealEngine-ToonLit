// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AddonTools.h"

BEGIN_NAMESPACE_UE_AC

// Stack class to change current database and restoring previous one when destroyed
class FAutoChangeDatabase
{
  public:
	FAutoChangeDatabase(API_DatabaseTypeID InDbType, bool* OutSucceed = nullptr);

	~FAutoChangeDatabase();

	API_DatabaseInfo PreviousDB;
	bool			 bWasDifferent;
};

END_NAMESPACE_UE_AC
