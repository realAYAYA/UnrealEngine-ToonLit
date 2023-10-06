// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Context.h"
#include "CallNest.h"

namespace AutoRTFM
{

template<typename TTryFunctor>
void FCallNest::Try(const TTryFunctor& TryFunctor)
{
	AbortJump.TryCatch(
		[&]()
		{
			TryFunctor();
			ASSERT(Context->GetStatus() == EContextStatus::OnTrack);
		},
		[&]()
		{
			ASSERT(Context->GetStatus() != EContextStatus::Idle);
			ASSERT(Context->GetStatus() != EContextStatus::OnTrack);
		});
}

} // namespace AutoRTFM
