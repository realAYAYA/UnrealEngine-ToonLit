// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using EpicGames.Core;

namespace UnrealBuildTool
{
	/// <summary>
	/// Stores information about where a module rules object came from, and how it can be used. 
	/// </summary>
	class ModuleRulesContext
	{
		/// <summary>
		/// The scope for this module. Used to validate references to other modules.
		/// </summary>
		public RulesScope Scope;

		/// <summary>
		/// The default directory for output files
		/// </summary>
		public DirectoryReference DefaultOutputBaseDir;

		/// <summary>
		/// The plugin that this module belongs to
		/// </summary>
		public PluginInfo? Plugin;

		/// <summary>
		/// Whether this module should be included in the default hot reload set
		/// </summary>
		public bool bCanHotReload;

		/// <summary>
		/// Whether this module should be compiled with optimization disabled in DebugGame configurations (ie. whether it's a game module).
		/// </summary>
		public bool bCanBuildDebugGame;

		/// <summary>
		/// Whether this module can be used for generating shared PCHs
		/// </summary>
		public bool bCanUseForSharedPCH;

		/// <summary>
		/// Whether to treat this module as a game module for UHT ordering
		/// </summary>
		public bool bClassifyAsGameModuleForUHT;

		/// <summary>
		/// The default module type for UnrealHeaderTool. Do not use this for inferring other things about the module.
		/// </summary>
		public UHTModuleType? DefaultUHTModuleType;

		/// <summary>
		/// Constructor
		/// </summary>
		public ModuleRulesContext(RulesScope Scope, DirectoryReference DefaultOutputBaseDir)
		{
			this.Scope = Scope;
			this.DefaultOutputBaseDir = DefaultOutputBaseDir;
			this.bCanUseForSharedPCH = true;
		}

		/// <summary>
		/// Copy constructor
		/// </summary>
		/// <param name="Other">The context to copy from</param>
		public ModuleRulesContext(ModuleRulesContext Other)
		{
			this.Scope = Other.Scope;
			this.DefaultOutputBaseDir = Other.DefaultOutputBaseDir;
			this.Plugin = Other.Plugin;
			this.bCanHotReload = Other.bCanHotReload;
			this.bCanBuildDebugGame = Other.bCanBuildDebugGame;
			this.bCanUseForSharedPCH = Other.bCanUseForSharedPCH;
			this.bClassifyAsGameModuleForUHT = Other.bClassifyAsGameModuleForUHT;
			this.DefaultUHTModuleType = Other.DefaultUHTModuleType;
		}
	}
}
