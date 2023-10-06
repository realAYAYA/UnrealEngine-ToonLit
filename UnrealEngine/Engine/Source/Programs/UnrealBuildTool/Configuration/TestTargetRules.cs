// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.IO;
using System.Reflection;
using System.Runtime.Serialization;
using EpicGames.Core;
using UnrealBuildBase;

namespace UnrealBuildTool
{
	/// <summary>
	/// TargetRules extension for low level tests.
	/// </summary>
	public class TestTargetRules : TargetRules
	{
		/// <summary>
		/// Keeps track if low level tests executable must build with the Editor.
		/// </summary>
		internal bool bTestsRequireEditor = false;

		/// <summary>
		/// Keeps track if low level tests executable must build with the Engine.
		/// </summary>
		internal bool bTestsRequireEngine = false;

		/// <summary>
		/// Keeps track if low level tests executable must build with the ApplicationCore.
		/// </summary>
		internal bool bTestsRequireApplicationCore = false;

		/// <summary>
		/// Keeps track if low level tests executable must build with the CoreUObject.
		/// </summary>
		internal bool bTestsRequireCoreUObject = false;

		/// <summary>
		/// Keep track of low level test runner module instance.
		/// </summary>
		internal ModuleRules? LowLevelTestsRunnerModule;

		/// <summary>
		/// Associated tested target of this test target, if defined.
		/// </summary>
		protected TargetRules? TestedTarget { get; private set; }

		/// <summary>
		/// Test target override for bCompileAgainstApplicationCore.
		/// It is set to true if there is any reference to ApplicationCore in the dependency graph.
		/// </summary>
		public override bool bCompileAgainstApplicationCore
		{
			get => bTestsRequireApplicationCore;
			set => bTestsRequireApplicationCore = value;
		}

		/// <summary>
		/// If set to true, it will not compile against ApplicationCore even if "ApplicationCore" is in the dependency graph.
		/// </summary>
		public static bool bNeverCompileAgainstApplicationCore = false;

		/// <summary>
		/// Test target override for bCompileAgainstCoreUObject.
		/// It is set to true if there is any reference to CoreUObject in the dependency graph.
		/// </summary>
		public override bool bCompileAgainstCoreUObject
		{
			get => bTestsRequireCoreUObject;
			set => bTestsRequireCoreUObject = value;
		}

		/// <summary>
		/// If set to true, it will not compile against CoreUObject even if "CoreUObject" is in the dependency graph.
		/// </summary>
		public static bool bNeverCompileAgainstCoreUObject = false;

		/// <summary>
		/// Test target override for bCompileAgainstEngine.
		/// It is set to true if there is any reference to Engine in the dependency graph.
		/// </summary>
		public override bool bCompileAgainstEngine
		{
			get => bTestsRequireEngine;
			set => bTestsRequireEngine = value;
		}

		/// <summary>
		/// If set to true, it will not compile against engine even if "Engine" is in the dependency graph.
		/// </summary>
		public static bool bNeverCompileAgainstEngine = false;

		/// <summary>
		/// Test target override for bCompileAgainstEditor.
		/// It is set to true if there is any reference to UnrealEd in the dependency graph.
		/// </summary>
		public override bool bCompileAgainstEditor
		{
			get => bTestsRequireEditor;
			set => bTestsRequireEditor = value;
		}

		/// <summary>
		/// If set to true, it will not compile against editor even if "UnrealEd" is in the dependency graph.
		/// </summary>
		public static bool bNeverCompileAgainstEditor = false;

		/// <summary>
		/// Whether to stub the platform file.
		/// </summary>
		public bool bUsePlatformFileStub
		{
			get => bUsePlatformFileStubPrivate;
			set
			{
				bUsePlatformFileStubPrivate = value;
				GlobalDefinitions.Remove("UE_LLT_USE_PLATFORM_FILE_STUB=0");
				GlobalDefinitions.Remove("UE_LLT_USE_PLATFORM_FILE_STUB=1");
				GlobalDefinitions.Add($"UE_LLT_USE_PLATFORM_FILE_STUB={Convert.ToInt32(bUsePlatformFileStubPrivate)}");
			}
		}
		private bool bUsePlatformFileStubPrivate = false;

		/// <summary>
		/// Whether to mock engine default instances for materials, AI controller etc.
		/// </summary>
		public bool bMockEngineDefaults
		{
			get => bMockEngineDefaultsPrivate;
			set
			{
				bMockEngineDefaultsPrivate = value;
				GlobalDefinitions.Remove("UE_LLT_WITH_MOCK_ENGINE_DEFAULTS=0");
				GlobalDefinitions.Remove("UE_LLT_WITH_MOCK_ENGINE_DEFAULTS=1");
				GlobalDefinitions.Add($"UE_LLT_WITH_MOCK_ENGINE_DEFAULTS={Convert.ToInt32(bMockEngineDefaultsPrivate)}");
			}
		}
		private bool bMockEngineDefaultsPrivate = false;

		/// <summary>
		/// Constructor that explicit targets can inherit from.
		/// </summary>
		/// <param name="Target"></param>
		public TestTargetRules(TargetInfo Target) : base(Target)
		{
			SetupCommonProperties(Target);

			ExeBinariesSubFolder = LaunchModuleName = Name + (ExplicitTestsTarget ? String.Empty : "Tests");

			if (ExplicitTestsTarget)
			{
				bBuildInSolutionByDefault = true;
				if (ProjectFile != null)
				{
					DirectoryReference SamplesDirectory = DirectoryReference.Combine(Unreal.RootDirectory, "Samples");
					if (!ProjectFile.IsUnderDirectory(Unreal.EngineDirectory))
					{
						if (ProjectFile.IsUnderDirectory(SamplesDirectory))
						{
							SolutionDirectory = Path.Combine("Samples", ProjectFile.Directory.GetDirectoryName(), "LowLevelTests");
						}
						else
						{
							SolutionDirectory = Path.Combine("Games", ProjectFile.Directory.GetDirectoryName(), "LowLevelTests");
						}
					}
					else
					{
						SolutionDirectory = "Programs/LowLevelTests";
					}
				}
				else
				{
					SolutionDirectory = "Programs/LowLevelTests";
				}

				// Default to true for explicit targets to reduce compilation times.
				// Selective module compilation will automatically detect if Engine is required based on Engine include files in tests.
				bNeverCompileAgainstEngine = true;
			}

			if (Target.Platform == UnrealTargetPlatform.Win64)
			{
				string OutputName = "$(TargetName)";
				if (Target.Configuration != UndecoratedConfiguration)
				{
					OutputName = OutputName + "-" + Target.Platform + "-" + Target.Configuration;
				}
				if (File != null)
				{
					DirectoryReference OutputDirectory;
					if (ProjectFile != null && File.IsUnderDirectory(ProjectFile.Directory))
					{
						OutputDirectory = UEBuildTarget.GetOutputDirectoryForExecutable(ProjectFile.Directory, File);
					}
					else
					{
						OutputDirectory = UEBuildTarget.GetOutputDirectoryForExecutable(Unreal.EngineDirectory, File);
					}
					FileReference OutputTestFile = FileReference.Combine(OutputDirectory, "Binaries", Target.Platform.ToString(), ExeBinariesSubFolder, $"{OutputName}.exe.is_unreal_test");
					PostBuildSteps.Add("echo > " + OutputTestFile.FullName);
				}
			}
		}

		/// <summary>
		/// Constructs a valid TestTargetRules instance from an existent TargetRules instance.
		/// </summary>
		public static TestTargetRules Create(TargetRules Rules, TargetInfo TargetInfo)
		{
			Type TestTargetRulesType = typeof(TestTargetRules);
			TestTargetRules TestRules = (TestTargetRules)FormatterServices.GetUninitializedObject(TestTargetRulesType);

			// Initialize the logger before calling the constructor
			TestRules.Logger = Rules.Logger;

			ConstructorInfo? Constructor = TestTargetRulesType.GetConstructor(new Type[] { typeof(TargetRules), typeof(TargetInfo) });
			if (Constructor == null)
			{
				throw new BuildException("No constructor found on {0} which takes first argument of type TargetRules and second of type TargetInfo.", TestTargetRulesType.Name);
			}

			try
			{
				Constructor.Invoke(TestRules, new object[] { Rules, TargetInfo });
			}
			catch (Exception Ex)
			{
				throw new BuildException(Ex, "Unable to instantiate instance of '{0}' object type from compiled assembly '{1}'.  Unreal Build Tool creates an instance of your module's 'Rules' object in order to find out about your module's requirements.  The CLR exception details may provide more information:  {2}", TestTargetRulesType.Name, Path.GetFileNameWithoutExtension(TestTargetRulesType.Assembly?.Location), Ex.ToString());
			}

			return TestRules;
		}

		/// <summary>
		/// Constructor for TestTargetRules based on existing target.
		/// TestTargetRules is setup as a program and is linked monolithically.
		/// It removes a lot of default compilation behavior in order to produce a minimal test environment.
		/// </summary>
		public TestTargetRules(TargetRules TestedTarget, TargetInfo Target) : base(Target)
		{
			if (TestedTarget is TestTargetRules)
			{
				throw new BuildException("TestedTarget can't be of type TestTargetRules.");
			}

			this.TestedTarget = TestedTarget;

			TargetFiles = TestedTarget.TargetFiles;

			ExeBinariesSubFolder = Name = TestedTarget.Name + "Tests";
			TargetSourceFile = File = TestedTarget.File;
			if (TestedTarget.LaunchModuleName != null)
			{
				LaunchModuleName = TestedTarget.LaunchModuleName + "Tests";
			}

			ManifestFileNames = TestedTarget.ManifestFileNames;

			WindowsPlatform = TestedTarget.WindowsPlatform;

			SetupCommonProperties(Target);
		}

		private void SetupCommonProperties(TargetInfo Target)
		{
			IncludeOrderVersion = EngineIncludeOrderVersion.Latest;

			bIsTestTargetOverride = true;

			VSTestRunSettingsFile = FileReference.Combine(Unreal.EngineDirectory, "Source", "Programs", "LowLevelTests", "vstest.runsettings");

			DefaultBuildSettings = BuildSettingsVersion.Latest;

			Type = TargetType.Program;
			LinkType = TargetLinkType.Monolithic;

			bBuildInSolutionByDefault = false;

			bDeployAfterCompile = Target.Platform != UnrealTargetPlatform.Android;
			bIsBuildingConsoleApplication = true;

			// Disabling default true flags that aren't necessary for tests

			// Lean and Mean mode
			bBuildDeveloperTools = false;

			// No localization
			bCompileICU = false;

			// No need for shaders by default
			bForceBuildShaderFormats = false;

			// Do not compile against the engine, editor etc
			bCompileAgainstEngine = false;
			bCompileAgainstEditor = false;
			bCompileAgainstCoreUObject = false;
			bCompileAgainstApplicationCore = false;
			bCompileCEF3 = false;

			// No mixing with Functional Test framework
			bForceDisableAutomationTests = true;

			bDebugBuildsActuallyUseDebugCRT = true;

			// Allow logging in shipping
			bUseLoggingInShipping = true;

			// Allow exception handling
			bForceEnableExceptions = true;

			bBuildWithEditorOnlyData = false;
			bBuildRequiresCookedData = true;
			bBuildDeveloperTools = false;

			// Useful for debugging test failures
			if (Target.Configuration == UnrealTargetConfiguration.Debug)
			{
				bDebugBuildsActuallyUseDebugCRT = true;
			}

			if (!ExplicitTestsTarget && TestedTarget != null)
			{
				GlobalDefinitions.AddRange(TestedTarget.GlobalDefinitions);
			}

			GlobalDefinitions.Add("STATS=0");
			GlobalDefinitions.Add("TEST_FOR_VALID_FILE_SYSTEM_MEMORY=0");

			// LLT Globals
			GlobalDefinitions.Add("UE_LLT_USE_PLATFORM_FILE_STUB=0");
			GlobalDefinitions.Add("UE_LLT_WITH_MOCK_ENGINE_DEFAULTS=0");

			// Platform specific setup
			if (Target.Platform == UnrealTargetPlatform.Android)
			{
				UndecoratedConfiguration = Target.Configuration;

				GlobalDefinitions.Add("USE_ANDROID_INPUT=0");
				GlobalDefinitions.Add("USE_ANDROID_OPENGL=0");
				GlobalDefinitions.Add("USE_ANDROID_LAUNCH=0");
				GlobalDefinitions.Add("USE_ANDROID_JNI=0");
			}
			else if (Target.Platform == UnrealTargetPlatform.IOS)
			{
				bIsBuildingConsoleApplication = false;
			}
		}
	}
}
