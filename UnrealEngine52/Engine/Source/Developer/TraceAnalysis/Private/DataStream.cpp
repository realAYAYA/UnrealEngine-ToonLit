// Copyright Epic Games, Inc. All Rights Reserved.

#include "Trace/DataStream.h"
#include "GenericPlatform/GenericPlatformFile.h"
#include "HAL/PlatformFileManager.h"

namespace UE {
namespace Trace {

FFileDataStream::FFileDataStream()
	: Handle(nullptr)
	, Remaining(0)
{
}

FFileDataStream::~FFileDataStream()
{
}

bool FFileDataStream::Open(const TCHAR* Path)
{
	Handle.Reset(FPlatformFileManager::Get().GetPlatformFile().OpenRead(Path));
	if (Handle == nullptr)
	{
		return false;
	}
	Remaining = Handle->Size();
	return true;
}

int32 FFileDataStream::Read(void* Data, uint32 Size)
{
	if (Handle == nullptr)
	{
		return -1;
	}

	if (Remaining <= 0)
	{
		return 0;
	}

	if (Size > Remaining)
	{
		Size = static_cast<uint32>(Remaining);
	}
	Remaining -= Size;

	if (!Handle->Read((uint8*)Data, Size))
	{
		Close();
		return -1;
	}

	return Size;
}

void FFileDataStream::Close()
{
	Handle.Reset();
}

} // namespace Trace
} // namespace UE
