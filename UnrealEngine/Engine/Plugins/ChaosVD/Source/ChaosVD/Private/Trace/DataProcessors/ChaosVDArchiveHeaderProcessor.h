// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once
#include "Trace/DataProcessors/IChaosVDDataProcessor.h"

/**
 * Data processor implementation that is able to deserialize traced Archive headers
 */
class FChaosVDArchiveHeaderProcessor final : public IChaosVDDataProcessor
{
public:
	explicit FChaosVDArchiveHeaderProcessor();
	
	virtual bool ProcessRawData(const TArray<uint8>& InData) override;
};