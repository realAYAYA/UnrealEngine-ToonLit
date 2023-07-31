// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Serialization/ArchiveProxy.h"
#include "Templates/UnrealTemplate.h"

namespace UE
{

template <typename HashBuilderType>
class THashingArchiveProxy : public FArchiveProxy
{
public:
	using FArchiveProxy::FArchiveProxy;

	void Serialize(void* V, int64 Length) override
	{
		FArchiveProxy::Serialize(V, Length);
		HashBuilder.Update(V, uint64(Length));
	}

	void Seek(int64 InPos) override
	{
		checkf(InPos == Tell(), TEXT("A hash cannot be computed when serialization relies on seeking."));
		FArchiveProxy::Seek(InPos);
	}

	auto GetHash() -> decltype(DeclVal<HashBuilderType>().Finalize())
	{
		return HashBuilder.Finalize();
	}

private:
	HashBuilderType HashBuilder;
};

} // UE
