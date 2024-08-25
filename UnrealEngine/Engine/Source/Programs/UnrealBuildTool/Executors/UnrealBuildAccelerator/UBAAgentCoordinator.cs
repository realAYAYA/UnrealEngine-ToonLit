// Copyright Epic Games, Inc. All Rights Reserved.

using EpicGames.Core;
using System;
using System.Threading.Tasks;

namespace UnrealBuildTool
{

	interface IUBAAgentCoordinator
	{
		DirectoryReference? GetUBARootDir();

		Task InitAsync(UBAExecutor executor);

		void Start(ImmediateActionQueue queue, Func<LinkedAction, bool> canRunRemotely);

		void Stop();
	
		Task CloseAsync();
	}
}