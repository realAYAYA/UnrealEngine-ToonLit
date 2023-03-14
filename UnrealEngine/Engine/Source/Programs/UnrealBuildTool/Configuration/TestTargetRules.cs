// Copyright Epic Games, Inc. All Rights Reserved.

using EpicGames.Core;
using System;
using System.IO;
using System.Reflection;
using System.Runtime.Serialization;
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
		internal static bool bTestsRequireEditor = false;

		/// <summary>
		/// Keeps track if low level tests executable must build with the Engine.
		/// </summary>
		internal static bool bTestsRequireEngine = false;

		/// <summary>
		/// Keeps track if low level tests executable must build with the ApplicationCore.
		/// </summary>
		internal static bool bTestsRequireApplicationCore = false;

		/// <summary>
		/// Keeps track if low level tests executable must build with the CoreUObject.
		/// </summary>
		internal static bool bTestsRequireCoreUObject = false;

		/// <summary>
		/// Keep track of low level test runner module instance.
		/// </summary>
		internal static ModuleRules? LowLevelTestsRunnerModule;

		/// <summary>
		/// Associated tested target of this test target, if defined.
		/// </summary>
		protected TargetRules? TestedTarget { get; private set; }

		/// <summary>
		/// Test target override for bCompileAgainstApplicationCore.
		/// It is set to true if there is any reference to ApplicationCore in the reference chain.
		/// </summary>
		public override bool bCompileAgainstApplicationCore
		{
			get { return bTestsRequireApplicationCore; }
			set { bTestsRequireApplicationCore = value; }
		}

		/// <summary>
		/// Test target override for bCompileAgainstCoreUObject.
		/// It is set to true if there is any reference to CoreUObject in the reference chain.
		/// </summary>
		public override bool bCompileAgainstCoreUObject
		{
			get { return bTestsRequireCoreUObject; }
			set { bTestsRequireCoreUObject = value; }
		}

		/// <summary>
		/// Test target override for bCompileAgainstEngine.
		/// It is set to true if there is any reference to Engine in the reference chain.
		/// </summary>
		public override bool bCompileAgainstEngine
		{
			get { return bTestsRequireEngine; }
			set { bTestsRequireEngine = value; }
		}

		/// <summary>
		/// Test target override for bCompileAgainstEditor.
		/// It is set to true if there is any reference to UnrealEd in the reference chain.
		/// </summary>
		public override bool bCompileAgainstEditor
		{
			get { return bTestsRequireEditor; }
			set { bTestsRequireEditor = value; }
		}

		/// <summary>
		/// Constructor for TestTargetRules as own target.
		/// </summary>
		/// <param name="Target"></param>
		public TestTargetRules(TargetInfo Target) : base(Target)
		{
			SetupCommonProperties(Target);

			bExplicitTestsTargetOverride = this.GetType() != typeof(TestTargetRules);

			ExeBinariesSubFolder = LaunchModuleName = Name + (bExplicitTestsTargetOverride ? string.Empty : "Tests");

			if (bExplicitTestsTargetOverride)
			{
				bBuildInSolutionByDefault = true;
				SolutionDirectory = "Programs/LowLevelTests";
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
			bIsTestTargetOverride = true;

			VSTestRunSettingsFile = FileReference.Combine(Unreal.EngineDirectory, "Source", "Programs", "LowLevelTests", "vstest.runsettings");

			DefaultBuildSettings = BuildSettingsVersion.V2;

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

			// Do not link against the engine, no Chromium Embedded Framework etc.
			bCompileAgainstEngine = false;
			bCompileCEF3 = false;
			bCompileAgainstCoreUObject = false;
			bCompileAgainstApplicationCore = false;
			bUseLoggingInShipping = true;

			// Allow exception handling
			bForceEnableExceptions = true;

			bool bDebugOrDevelopment = Target.Configuration == UnrealTargetConfiguration.Debug || Target.Configuration == UnrealTargetConfiguration.Development;
			bBuildWithEditorOnlyData = Target.Platform.IsInGroup(UnrealPlatformGroup.Desktop) && bDebugOrDevelopment;

			// Disable malloc profiling in tests
			bUseMallocProfiler = false;

			// Useful for debugging test failures
			if (Target.Configuration == UnrealTargetConfiguration.Debug)
			{
				bDebugBuildsActuallyUseDebugCRT = true;
			}

			GlobalDefinitions.Add("STATS=0");
			GlobalDefinitions.Add("TEST_FOR_VALID_FILE_SYSTEM_MEMORY=0");

			// Platform specific setup
			if (Target.Platform == UnrealTargetPlatform.Android)
			{
				UndecoratedConfiguration = Target.Configuration;

				GlobalDefinitions.Add("USE_ANDROID_INPUT=0");
				GlobalDefinitions.Add("USE_ANDROID_OPENGL=0");
				GlobalDefinitions.Add("USE_ANDROID_LAUNCH=0");
				GlobalDefinitions.Add("USE_ANDROID_JNI=0");
			}
			else if (Target.Platform == UnrealTargetPlatform.IOS) // TODO: this doesn't compile
			{
				GlobalDefinitions.Add("HAS_METAL=0");

				bIsBuildingConsoleApplication = false;
				// Required for IOS, but needs to fix compilation errors
				bCompileAgainstApplicationCore = true;

			}
		}
	}
}
