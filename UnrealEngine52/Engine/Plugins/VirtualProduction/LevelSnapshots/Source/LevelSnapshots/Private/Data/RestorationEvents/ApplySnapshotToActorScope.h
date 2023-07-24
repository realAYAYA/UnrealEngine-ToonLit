// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Interfaces/IRestorationListener.h"
#include "Templates/UnrealTemplate.h"

struct FPropertySelectionMap;

namespace UE::LevelSnapshots::Private
{
	/**
	 * Convenience type that calls FLevelSnaphshotsModule::OnPreApplySnapshot and FLevelSnaphshotsModule::OnPostApplySnapshot.
	 */
	class FApplySnapshotToActorScope : public FNoncopyable
	{
		const FApplySnapshotToActorParams Params;
	public:

		FApplySnapshotToActorScope(const FApplySnapshotToActorParams& Params);
		~FApplySnapshotToActorScope();
	};
}

