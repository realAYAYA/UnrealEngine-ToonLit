// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Linq;
using System.Threading.Tasks;
using EpicGames.Core;
using Microsoft.Extensions.Logging;
using UnrealBuildBase;

namespace UnrealBuildTool
{
	using IncludeChainMap = Dictionary<UEBuildModule, List<string>>;

	/// <summary>
	/// Builds low level tests on one or more targets.
	/// </summary>
	[ToolMode("Test", ToolModeOptions.XmlConfig | ToolModeOptions.BuildPlatforms | ToolModeOptions.SingleInstance | ToolModeOptions.ShowExecutionTime)]
	class TestMode : ToolMode
	{
		private readonly string ENGINE_MODULE = "Engine";
		private readonly string APPLICATION_CORE_MODULE = "ApplicationCore";
		private readonly string COREUOBJECT_MODULE = "CoreUObject";
		private readonly string EDITOR_MODULE = "UnrealEd";

		/// <summary>
		/// Whether we're building implicit tests (default is explicit)
		/// </summary>
		[CommandLine("-Implicit")]
		bool bImplicit = false;

		/// <summary>
		/// Whether we're cleaning tests instead of just building
		/// </summary>
		[CommandLine("-CleanTests")]
		bool bCleanTests = false;

		/// <summary>
		/// Whether we're rebuilding tests instead of just building
		/// </summary>
		[CommandLine("-RebuildTests")]
		bool bRebuildTests = false;

		/// <summary>
		/// Main entry point
		/// </summary>
		/// <param name="Arguments">Command-line arguments</param>
		/// <param name="Logger"></param>
		public override async Task<int> ExecuteAsync(CommandLineArguments Arguments, ILogger Logger)
		{
			Arguments.ApplyTo(this);

			// Create the build configuration object, and read the settings
			BuildConfiguration BuildConfiguration = new BuildConfiguration();
			Arguments.ApplyTo(BuildConfiguration);
			XmlConfig.ApplyTo(BuildConfiguration);

			// Parse all the targets being built
			List<TargetDescriptor> TargetDescriptors = TargetDescriptor.ParseCommandLine(Arguments, BuildConfiguration, Logger);

			if (TargetDescriptors.Count > 1)
			{
				throw new BuildException($"Only one target must be specified in test mode but we got {string.Join(", ", TargetDescriptors.Select(T => T.Name))} instead.");
			}

			TargetDescriptor TestTargetDescriptor = TargetDescriptors.First();

			if (bImplicit)
			{
				TestTargetDescriptor.Name += "Tests";
				TestTargetDescriptor.IsTestsTarget = true;
			}

			// Analyzing dependency graph before any build commands to automatically set compilation flags.
			// Extracted from AnalyzeMode for simplicity.
			AnalyzeDependencyGraph(TargetDescriptors.First(), BuildConfiguration, Logger);

			using (ISourceFileWorkingSet WorkingSet = SourceFileWorkingSet.Create(Unreal.RootDirectory, new HashSet<DirectoryReference>(), Logger))
			{
				if (bCleanTests)
				{
					new CleanMode().Clean(TargetDescriptors, BuildConfiguration, Logger);
				}
				else if (bRebuildTests)
				{
					new CleanMode().Clean(TargetDescriptors, BuildConfiguration, Logger);
					await BuildMode.BuildAsync(TargetDescriptors, BuildConfiguration, WorkingSet, BuildOptions.None, null, Logger);
				}
				else
				{
					await BuildMode.BuildAsync(TargetDescriptors, BuildConfiguration, WorkingSet, BuildOptions.None, null, Logger);
				}
			}
			return 0;
		}

		private void AnalyzeDependencyGraph(TargetDescriptor TargetDescriptor, BuildConfiguration BuildConfiguration, ILogger Logger)
		{
			UEBuildTarget Target = UEBuildTarget.Create(TargetDescriptor, BuildConfiguration, Logger);
			DirectoryReference.CreateDirectory(Target.ReceiptFileName.Directory);

			HashSet<UEBuildModule> Visited = new();
			IncludeChainMap Chains = new();

			if (Target.Rules.LaunchModuleName != null)
			{
				AnalyzeModuleDependencies(Target.Rules.LaunchModuleName, new List<string>() { "target" }, Target, Chains, Visited, Logger);
			}

			foreach (string RootModuleName in Target.Rules.ExtraModuleNames)
			{
				AnalyzeModuleDependencies(RootModuleName, new List<string>() { "target" }, Target, Chains, Visited, Logger);
			}

			foreach (UEBuildPlugin Plugin in Target.BuildPlugins!)
			{
				foreach (UEBuildModule Module in Plugin.Modules)
				{
					if (!Chains.ContainsKey(Module))
					{
						AnalyzeModuleDependencies(Module.Name, new List<string>() { "target", Plugin.ReferenceChain, Plugin.File.GetFileName() }, Target, Chains, Visited, Logger);
					}
				}
			}

			if (Target.Rules.bBuildAllModules)
			{
				foreach (UEBuildBinary Binary in Target.Binaries)
				{
					foreach (UEBuildModule Module in Binary.Modules)
					{
						if (!Chains.ContainsKey(Module))
						{
							List<string> IncludeChain = Enumerable.Repeat(String.Empty, 1000).ToList();
							IncludeChain.Add("allmodules option");
							if (Module.Rules.Plugin != null)
							{
								IncludeChain.Add(Module.Rules.Plugin.File.GetFileName());
								AnalyzeModuleDependencies(Module.Name, IncludeChain, Target, Chains, Visited, Logger);
							}
							else
							{
								AnalyzeModuleDependencies(Module.Name, IncludeChain, Target, Chains, Visited, Logger);
							}
						}
					}
				}
			}

			foreach(var Module in Visited)
			{
				SetupTestsTargetForModule(Module, Logger);
			}
		}

		private void AnalyzeModuleDependencies(string ModuleName, List<string> ParentChain, UEBuildTarget Target, IncludeChainMap Chains, HashSet<UEBuildModule> Visited, ILogger Logger)
		{
			if (ParentChain.Contains(ModuleName))
			{
				return;
			}

			UEBuildModule Module = Target.GetModuleByName(ModuleName);

			List<string> CurrentChain = new(ParentChain) { Module.Name };

			if (!Chains.ContainsKey(Module))
			{
				Chains.Add(Module, CurrentChain);
			}

			if (Chains[Module].Count > CurrentChain.Count)
			{
				List<UEBuildModule> RecheckModules = new();
				Module.GetAllDependencyModules(RecheckModules, new(), true, false, true);
				Visited.ExceptWith(RecheckModules);
				Chains[Module] = CurrentChain;
			}
			else if (Visited.Contains(Module))
			{
				return;
			}

			Visited.Add(Module);

			List<UEBuildModule> TargetModules = new();
			Module.GetAllDependencyModules(TargetModules, new(), true, false, true);
			TargetModules.ForEach(x => AnalyzeModuleDependencies(x.Name, CurrentChain, Target, Chains, Visited, Logger));
		}

		private void SetupTestsTargetForModule(UEBuildModule Module, ILogger Logger)
		{
			if (Module.Name == ENGINE_MODULE)
			{
				TestTargetRules.bTestsRequireEngine = true;
				Logger.LogInformation("{Module} module found in dependency graph, will compile against it.", ENGINE_MODULE);
			}
			else if (Module.Name == APPLICATION_CORE_MODULE)
			{
				TestTargetRules.bTestsRequireApplicationCore = true;
				Logger.LogInformation("{Module} module found in dependency graph, will compile against it.", APPLICATION_CORE_MODULE);
			}
			else if (Module.Name == COREUOBJECT_MODULE)
			{
				TestTargetRules.bTestsRequireCoreUObject = true;
				Logger.LogInformation("{Module} module found in dependency graph, will compile against it.", COREUOBJECT_MODULE);
			}
			else if (Module.Name == EDITOR_MODULE)
			{
				TestTargetRules.bTestsRequireEditor = true;
				Logger.LogInformation("{Module} module found in dependency graph, will compile against it.", EDITOR_MODULE);
			}
		}
	}
}

