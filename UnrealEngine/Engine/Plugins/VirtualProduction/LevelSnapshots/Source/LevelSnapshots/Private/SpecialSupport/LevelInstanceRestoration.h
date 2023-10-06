// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

namespace UE::LevelSnapshots
{
	class ILevelSnapshotsModule;
}

namespace UE::LevelSnapshots::Private::LevelInstanceRestoration
{
	void Register(ILevelSnapshotsModule& Module);
}


