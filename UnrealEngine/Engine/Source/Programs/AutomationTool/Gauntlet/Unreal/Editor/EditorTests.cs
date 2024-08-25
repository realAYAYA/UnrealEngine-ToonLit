// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Linq;
using Gauntlet;
using Gauntlet.UnrealTest;


namespace UnrealEditor
{
	/// <summary>
	/// Default set of options for testing Editor. Options that tests can configure
	/// should be public, external command-line driven options should be protected/private
	/// </summary>
	public class EditorTestConfig : UE.AutomationTestConfig
	{
		/// <summary>
		/// Use Simple Horde Report instead of Unreal Automated Tests
		/// </summary>
		public override bool SimpleHordeReport { get; set; } = true;

		/// <summary>
		/// Force some specific plugins to load, comma delimited (at time of writing)
		/// </summary>
		[AutoParam]
		public string EnablePlugins = string.Empty;

		/// <summary>
		/// The file to trace profile data to
		/// </summary>
		[AutoParam]
		public string TraceFile = string.Empty;

		/// <summary>
		/// Control for interpretation of log warnings as test failures
		/// </summary>
		[AutoParam]
		public bool SuppressLogWarnings = false;

		/// <summary>
		/// Control for interpretation of log errors as test failures
		/// </summary>
		[AutoParam]
		public bool SuppressLogErrors = false;

		/// <summary>
		/// Modify the game instance lost timeout interval
		/// </summary>
		[AutoParam]
		public string GameInstanceLostTimerSeconds = string.Empty;

		/// <summary>
		/// Disable loading a level at startup (for profiling the map load)
		/// </summary>
		[AutoParam]
		public bool NoLoadLevelAtStartup = false;

		/// <summary>
		/// Disable distribution of shader builds (but use worker processes still)
		/// </summary>
		[AutoParam]
		public bool NoShaderDistrib = false;

		/// <summary>
		/// Enable Verbose Shader logging so we don't time out compiling lots of shaders
		/// </summary>
		[AutoParam]
		public bool VerboseShaderLogging = false;

		/// <summary>
		/// Enable benchmarking features in the engine
		/// </summary>
		[AutoParam]
		public bool Benchmarking = false;

		/// <summary>
		/// Applies these options to the provided app config
		/// </summary>
		/// <param name="AppConfig"></param>
		public override void ApplyToConfig(UnrealAppConfig AppConfig, UnrealSessionRole ConfigRole, IEnumerable<UnrealSessionRole> OtherRoles)
		{
			base.ApplyToConfig(AppConfig, ConfigRole, OtherRoles);

			if (EnablePlugins != string.Empty)
			{
				AppConfig.CommandLineParams.Add(string.Format("EnablePlugins={0}", EnablePlugins));
			}

			if (TraceFile != string.Empty)
			{
				AppConfig.CommandLineParams.Add(string.Format("tracefile={0}", TraceFile));
				AppConfig.CommandLineParams.Add("tracefiletrunc"); // replace existing
				AppConfig.CommandLineParams.Add("trace=default,counters,stats,loadtime,savetime,assetloadtime");
				AppConfig.CommandLineParams.Add("statnamedevents");
			}

			if (SuppressLogWarnings)
			{
				AppConfig.CommandLineParams.Add("ini:Engine:[/Script/AutomationController.AutomationControllerSettings]:bSuppressLogWarnings=true");
			}

			if (SuppressLogErrors)
			{
				AppConfig.CommandLineParams.Add("ini:Engine:[/Script/AutomationController.AutomationControllerSettings]:bSuppressLogErrors=true");
			}

			if (GameInstanceLostTimerSeconds != string.Empty)
			{
				AppConfig.CommandLineParams.Add(string.Format("ini:Engine:[/Script/AutomationController.AutomationControllerSettings]:GameInstanceLostTimerSeconds={0}", GameInstanceLostTimerSeconds));
			}

			if (NoLoadLevelAtStartup)
			{
				AppConfig.CommandLineParams.Add("ini:EditorPerProjectUserSettings:[/Script/UnrealEd.EditorLoadingSavingSettings]:LoadLevelAtStartup=None");
			}

			if (NoShaderDistrib)
			{
				AppConfig.CommandLineParams.Add("noxgeshadercompile");
			}

			if (VerboseShaderLogging)
			{
				AppConfig.CommandLineParams.Add("ini:Engine:[Core.Log]:LogShaderCompilers=Verbose");
			}

			if (Benchmarking)
			{
				AppConfig.CommandLineParams.Add("BENCHMARK");
				AppConfig.CommandLineParams.Add("Deterministic");
			}
		}
	}

	public class EditorTests : UE.AutomationNodeBase<EditorTestConfig>
	{
		public EditorTests(UnrealTestContext InContext) : base(InContext)
		{
		}

		public override EditorTestConfig GetConfiguration()
		{
			if (CachedConfig != null)
			{
				return CachedConfig;
			}
			// just need a single client
			EditorTestConfig Config = base.GetConfiguration();
			Config.RequireRole(UnrealTargetRole.Editor);	
			return Config;
		}

		protected override string HordeReportTestName
		{
			get
			{
				return GetConfiguration().RunTest.Replace(".", " ");
			}
		}
	}
}
