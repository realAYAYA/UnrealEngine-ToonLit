// Copyright Epic Games, Inc. All Rights Reserved.

#include "Experimental/Coroutine/Coroutine.h"

#include "Experimental/Coroutine/CoroLocalVariable.h"

namespace CoroTask_Detail
{

struct FCoroId
{
	static constexpr uint64 BatchSize = 1024 * 1024;
	static std::atomic<uint64> Id;

	uint64 StartOffset = 0;
	uint64 Cursor = BatchSize;
};
std::atomic<uint64> FCoroId::Id = {0};

struct FCombinedCoroLocalState
{
	FCoroId CoroId;
	FCoroLocalState* CoroLocalState = nullptr;
};
thread_local FCombinedCoroLocalState CombinedCoroLocalState;

FCoroLocalState& FCoroLocalState::GetCoroLocalState()
{
	return *CombinedCoroLocalState.CoroLocalState;
}

FCoroLocalState* FCoroLocalState::SetCoroLocalState(FCoroLocalState* NewCoroLocalState)
{
	FCoroLocalState* OldCoroLocalState = CombinedCoroLocalState.CoroLocalState;
	CombinedCoroLocalState.CoroLocalState = NewCoroLocalState;
	return OldCoroLocalState;
}

uint64 FCoroLocalState::GenerateCoroId()
{
	FCoroId& LocalCoroId = CombinedCoroLocalState.CoroId;
	if (LocalCoroId.Cursor >= FCoroId::BatchSize)
	{
		LocalCoroId.StartOffset = FCoroId::Id.fetch_add(FCoroId::BatchSize, std::memory_order_relaxed);
		LocalCoroId.Cursor = 0;
	}
	return LocalCoroId.StartOffset + (LocalCoroId.Cursor++);
}

bool FCoroLocalState::IsCoroLaunchedTask()
{
	return CombinedCoroLocalState.CoroLocalState != nullptr;
}

}