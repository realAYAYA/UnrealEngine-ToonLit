// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using EpicGames.Core;
using Microsoft.Extensions.Logging;

namespace UnrealBuildTool
{
	/// <summary>
	/// A module that is never compiled by us, and is only used to group include paths and libraries into a dependency unit.
	/// </summary>
	class UEBuildModuleExternal : UEBuildModule
	{
		public UEBuildModuleExternal(ModuleRules Rules, DirectoryReference IntermediateDirectory, ILogger Logger)
			: base(Rules, IntermediateDirectory, Logger)
		{
		}
	}
}
