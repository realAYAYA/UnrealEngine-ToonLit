// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#if WITH_WEBM_LIBS

#include "CoreMinimal.h"
#include "HAL/PlatformFileManager.h"
#include "GenericPlatform/GenericPlatformFile.h"

THIRD_PARTY_INCLUDES_START
#include "mkvparser/mkvparser.h"
THIRD_PARTY_INCLUDES_END

class FMkvFileReader : public mkvparser::IMkvReader
{
public:
	bool Open(const TCHAR* Filename);

	static FTimespan GetVideoFrameDuration(const mkvparser::VideoTrack& track);

public:
	// IMkvReader interface
	virtual int Read(long long Position, long Lenght, unsigned char* Buffer) override;
	virtual int Length(long long* Total, long long* Available) override;

private:
	TUniquePtr<IFileHandle> m_file;
};

#endif // WITH_WEBM_LIBS
