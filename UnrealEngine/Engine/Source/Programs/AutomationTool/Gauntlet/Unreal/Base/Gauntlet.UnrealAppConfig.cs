// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using AutomationTool;
using EpicGames.Core;
using UnrealBuildTool;

namespace Gauntlet
{ 
	/// <summary>
	/// Represents the configuration needed to run an instance of an Unreal app
	/// </summary>
	public class UnrealAppConfig : IAppConfig
    {
		/// <summary>
		/// Reference name
		/// </summary>
		public string Name { get; set; }

		/// <summary>
		/// Name of this unreal project
		/// </summary>
		public string ProjectName { get; set; }

		// <summary>
		/// Path to the file. Can be null if the project isn't on disk
		/// </summary>
		public FileReference ProjectFile { get; set; }

		/// <summary>
		/// Type of role this instance performs
		/// </summary>
		public UnrealTargetRole ProcessType { get; set; }

		/// <summary>
		/// Platform this role runs on
		/// </summary>
		public UnrealTargetPlatform? Platform { get; set; }

		/// <summary>
		/// Configuration for this role
		/// </summary>
		public UnrealTargetConfiguration Configuration { get; set; }

        /// <summary>
        /// Files to copy over to the device, plus what type of files they are.
        /// </summary>
        public List<UnrealFileToCopy> FilesToCopy { get; set; }

		/// <summary>
		/// Some IAppInstall instances can alter command line after it has been copied from AppConfig.CommandLine
		/// Use this to restrict this behavior if necessary.
		/// </summary>
		public bool CanAlterCommandArgs {
			get { return CanAlterCommandArgsPrivate; }
			set { CanAlterCommandArgsPrivate = value; }
		}
		private bool CanAlterCommandArgsPrivate = true;

		/// <summary>
		/// Set this property when the application is executed through a Docker container.
		/// </summary>
		public ContainerInfo ContainerInfo { get; set; }

		/// <summary>
		/// Arguments for this instance
		/// </summary>
		public string CommandLine
		{
			get
			{
				if (CommandLineParams == null)
				{
					CommandLineParams = new GauntletCommandLine();
				}
				return CommandLineParams.GenerateFullCommandLine();
			}
			set
			{
				if (CommandLineParams == null)
				{
					CommandLineParams = new GauntletCommandLine();
				}

				CommandLineParams.ClearCommandLine();

				CommandLineParams.AddRawCommandline(value, false);
			}
		}
		/// <summary>
		/// Dictionary of commandline arguments that are turned into a commandline at the end.
		/// For flags, leave the value set to null.
		/// </summary>
		public GauntletCommandLine CommandLineParams;

		/// <summary>
		/// Sandbox that we'd like to install this instance in
		/// </summary>
		public string Sandbox { get; set; }

		public IBuild Build { get; set; }

		// Prevents installing a build on device
		public bool SkipInstall => ForceSkipInstall.HasValue ? ForceSkipInstall.Value : _SkipInstall;

		// Performs a full clean on the device before installing
		public bool FullClean => ForceFullClean.HasValue ? ForceFullClean.Value : _FullClean;

		// Force a full clean
		public static bool? ForceFullClean = null;

		// Force a skip install
		public static bool? ForceSkipInstall = null;
		
		[AutoParamWithNames(false, "SkipInstall", "SkipDeploy", "SkipCopy")]
		private bool _SkipInstall { get; set; }
		
		[AutoParamWithNames(false, "FullClean")]
		private bool _FullClean { get; set; }

		/// <summary>
		/// Constructor that sets some required values to defaults
		/// </summary>
		public UnrealAppConfig()
		{
			Name = "UnrealApp";
			ProjectName = "UnknownProject";
			CommandLine = "";
			Configuration = UnrealTargetConfiguration.Development;
			Sandbox = "Gauntlet";
			AutoParam.ApplyParamsAndDefaults(this, Globals.Params.AllArguments);
		}
	}
}