// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

class FConcertLocalIdentifierTable;

typedef TSharedRef<FConcertLocalIdentifierTable, ESPMode::ThreadSafe> FConcertLocalIdentifierTableRef;
typedef TSharedPtr<FConcertLocalIdentifierTable, ESPMode::ThreadSafe> FConcertLocalIdentifierTablePtr;
typedef TSharedRef<const FConcertLocalIdentifierTable, ESPMode::ThreadSafe> FConcertLocalIdentifierTableConstRef;
typedef TSharedPtr<const FConcertLocalIdentifierTable, ESPMode::ThreadSafe> FConcertLocalIdentifierTableConstPtr;
