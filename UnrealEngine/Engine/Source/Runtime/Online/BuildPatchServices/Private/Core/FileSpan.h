// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

namespace BuildPatchServices
{
	struct FFileSpan
	{
		FFileSpan(const FString& InFilename, uint64 InSize, uint64 InStartIdx, bool InIsUnixExecutable, const FString& InSymlinkTarget)
			: Filename(InFilename)
			, Size(InSize)
			, StartIdx(InStartIdx)
			, IsUnixExecutable(InIsUnixExecutable)
			, SymlinkTarget(InSymlinkTarget)
		{
		}

		FFileSpan(const FFileSpan& CopyFrom)
			: Filename(CopyFrom.Filename)
			, Size(CopyFrom.Size)
			, StartIdx(CopyFrom.StartIdx)
			, SHAHash(CopyFrom.SHAHash)
			, IsUnixExecutable(CopyFrom.IsUnixExecutable)
			, SymlinkTarget(CopyFrom.SymlinkTarget)
		{
		}

		FFileSpan(FFileSpan&& MoveFrom)
			: Filename(MoveTemp(MoveFrom.Filename))
			, Size(MoveTemp(MoveFrom.Size))
			, StartIdx(MoveTemp(MoveFrom.StartIdx))
			, SHAHash(MoveTemp(MoveFrom.SHAHash))
			, IsUnixExecutable(MoveTemp(MoveFrom.IsUnixExecutable))
			, SymlinkTarget(MoveTemp(MoveFrom.SymlinkTarget))
		{
		}

		FFileSpan()
			: Size(0)
			, StartIdx(0)
			, IsUnixExecutable(false)
		{
		}

		FORCEINLINE FFileSpan& operator=(const FFileSpan& CopyFrom)
		{
			Filename = CopyFrom.Filename;
			Size = CopyFrom.Size;
			StartIdx = CopyFrom.StartIdx;
			SHAHash = CopyFrom.SHAHash;
			IsUnixExecutable = CopyFrom.IsUnixExecutable;
			SymlinkTarget = CopyFrom.SymlinkTarget;
			return *this;
		}

		FORCEINLINE FFileSpan& operator=(FFileSpan&& MoveFrom)
		{
			Filename = MoveTemp(MoveFrom.Filename);
			Size = MoveTemp(MoveFrom.Size);
			StartIdx = MoveTemp(MoveFrom.StartIdx);
			SHAHash = MoveTemp(MoveFrom.SHAHash);
			IsUnixExecutable = MoveTemp(MoveFrom.IsUnixExecutable);
			SymlinkTarget = MoveTemp(MoveFrom.SymlinkTarget);
			return *this;
		}

		FString Filename;
		uint64 Size;
		uint64 StartIdx;
		FSHAHash SHAHash;
		bool IsUnixExecutable;
		FString SymlinkTarget;
	};
}
