// Copyright Epic Games, Inc. All Rights Reserved.

using System.Collections.Generic;
using EpicGames.Core;
using UnrealBuildBase;
using Microsoft.Extensions.Logging;

namespace UnrealBuildTool
{
	/// <summary>
	/// Builds low level tests on one or more targets.
	/// </summary>
	[ToolMode("Test", ToolModeOptions.XmlConfig | ToolModeOptions.BuildPlatforms | ToolModeOptions.SingleInstance | ToolModeOptions.ShowExecutionTime)]
	class TestMode : ToolMode
	{
		/// <summary>
		/// Main entry point
		/// </summary>
		/// <param name="Arguments">Command-line arguments</param>
		/// <param name="Logger"></param>
		public override int Execute(CommandLineArguments Arguments, ILogger Logger)
		{
			Arguments.ApplyTo(this);

			// Create the build configuration object, and read the settings
			BuildConfiguration BuildConfiguration = new BuildConfiguration();
			Arguments.ApplyTo(BuildConfiguration);
			XmlConfig.ApplyTo(BuildConfiguration);

			// Parse all the targets being built
			List<TargetDescriptor> TargetDescriptors = TargetDescriptor.ParseCommandLine(Arguments, false, false, false, Logger);

			BuildTests(TargetDescriptors, BuildConfiguration, Logger);

			return 0;
		}

		/// <summary>
		/// Build tests for a list of targets.
		/// It generates artificial test target descriptors to build a target AND its dependencies's tests in one monolithic executable.
		/// Each module containing a "Tests" folder is included.
		/// The target that we generate the tests executable for must include a main.cpp file and Setup and Teardown methods.
		/// The generated executable is the target name + "Tests", e.g. UnrealEditorTests.exe
		/// Passing UnrealEditor here would build all the tests in all the modules.
		/// </summary>
		/// <param name="TargetDescriptors">Target descriptors</param>
		/// <param name="BuildConfiguration">Current build configuration</param>
		/// <param name="Logger"></param>
		public static void BuildTests(List<TargetDescriptor> TargetDescriptors, BuildConfiguration BuildConfiguration, ILogger Logger)
		{
			List<TargetDescriptor> TestTargetDescriptors = new List<TargetDescriptor>();

			for (int Idx = 0; Idx < TargetDescriptors.Count; ++Idx)
			{
				TargetDescriptor TestsTargetDescriptor = TargetDescriptors[Idx].Copy();
				TestsTargetDescriptor.Name = TargetDescriptors[Idx].Name + "Tests";
				TestsTargetDescriptor.IsTestsTarget = true;
				TestTargetDescriptors.Add(TestsTargetDescriptor);
			}

			using (ISourceFileWorkingSet WorkingSet = SourceFileWorkingSet.Create(Unreal.RootDirectory, new HashSet<DirectoryReference>(), Logger))
			{
				BuildMode.Build(TestTargetDescriptors, BuildConfiguration, WorkingSet, BuildOptions.None, null, Logger);
			}
		}
	}
}

