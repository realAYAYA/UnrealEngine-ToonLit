// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Misc/Optional.h"
#include "Online/TitleFileCommon.h"

namespace UE::Online {

class FOnlineServicesNull;

class ONLINESERVICESNULL_API FTitleFileNull : public FTitleFileCommon
{
public:
	using Super = FTitleFileCommon;

	FTitleFileNull(FOnlineServicesNull& InOwningSubsystem);

	// IOnlineComponent
	virtual void UpdateConfig() override;

	// ITitleFile
	virtual TOnlineAsyncOpHandle<FTitleFileEnumerateFiles> EnumerateFiles(FTitleFileEnumerateFiles::Params&& Params) override;
	virtual TOnlineResult<FTitleFileGetEnumeratedFiles> GetEnumeratedFiles(FTitleFileGetEnumeratedFiles::Params&& Params) override;
	virtual TOnlineAsyncOpHandle<FTitleFileReadFile> ReadFile(FTitleFileReadFile::Params&& Params) override;

protected:
	TMap<FString, FTitleFileContentsRef> TitleFiles;

	bool bEnumerated = false;
};

/* UE::Online */ }
