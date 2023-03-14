// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "common/Logging.h"

#include "AxFFileImporter.h"

class FAxFImporter
{
public:
	FAxFImporter(const FString& PluginPath);
	~FAxFImporter();

	IAxFFileImporter* Create();

	bool IsLoaded();

	void* AxFDecodingHandle = nullptr;
};
