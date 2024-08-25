// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AutoRTFM/AutoRTFM.h"

namespace AutoRTFM
{

struct FAutoRTFMMetrics
{
	int64 NumTransactionsStarted;
	int64 NumTransactionsCommitted;

	int64 NumTransactionsAborted;
	int64 NumTransactionsAbortedByRequest;
	int64 NumTransactionsAbortedByLanguage;
};

// reset the internal metrics tracking back to zero
void ResetAutoRTFMMetrics();

// get a snapshot of the current internal metrics
FAutoRTFMMetrics GetAutoRTFMMetrics();

}
