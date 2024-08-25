// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.IO;
using System.Text.RegularExpressions;

namespace UnrealBuildTool.Rules
{
	public class GameInputBase : ModuleRules
	{
		private const int GameInputMinimumVersion_Win64 = 220604;
		
		/// <summary>
		/// True if this platform has support for the Game Input library.
		/// 
		/// Overriden per-platform implement of the Game Input Base module.
		/// </summary>
		protected virtual bool HasGameInputSupport(ReadOnlyTargetRules Target)
		{
			// Console platforms will override this function and determine if they have GameInput support on their own.
			// For the base functionality, we can only support Game Input on windows platforms.
			if (!Target.Platform.IsInGroup(UnrealPlatformGroup.Windows))
			{
				return false;
			}

			// The Game Input SDK is installed when you install the GRDK, which will set the environment variable "GRDKLatest"
			// We can then use that to look up the path to the GameInput.h and GameInput.lib files needed to use Game Input.
			//
			// https://learn.microsoft.com/en-us/gaming/gdk/_content/gc/input/overviews/input-overview
			string GRDKLatestPath = Environment.GetEnvironmentVariable("GRDKLatest");

			// We want to have the version number available to us in C++
			// so that we can validate it at runtime
			string GDRKVersion = GetGameInputVersionString(GRDKLatestPath);
			int GRDKIntVersion = -1;

			bool bParseSuccessful = int.TryParse(GDRKVersion, out GRDKIntVersion);

			// We have Game Input support if we could successfully get the version and it is equal to or greater than our min version
			return bParseSuccessful && GRDKIntVersion >= GameInputMinimumVersion_Win64;
		}

		/// <summary>
		/// An extension point for subclasses of this module to add any additional required include/library files
		/// that may be necessary to add the GameInput SDK to their platform. This will only be called if
		/// HasGameInputSupport returns true.
		/// </summary>
		protected virtual void AddRequiredDeps(ReadOnlyTargetRules Target)
		{
			// Console platforms will override this function and determine if they have GameInput support on their own.
			// For the base functionality, we can only support Game Input on windows platforms.
			if (!Target.Platform.IsInGroup(UnrealPlatformGroup.Windows))
			{
				return;
			}
			
			string GRDKLatestPath = Environment.GetEnvironmentVariable("GRDKLatest");
			
			// GameInput.h is from    <GRDKLatest>\GameKit\Include\GameInput.h
			
			string IncludePath = Path.Combine(GRDKLatestPath, "GameKit", "Include");
			
			PublicSystemIncludePaths.Add(IncludePath);
		}

		public GameInputBase(ReadOnlyTargetRules Target) : base(Target)
		{
			// Enable truncation warnings in this plugin
			UnsafeTypeCastWarningLevel = WarningLevel.Error;

			// Uncomment this line to make for easier debugging
			//OptimizeCode = CodeOptimization.Never;

			PublicDependencyModuleNames.AddRange(
				new string[]
				{
					"Core",
					"ApplicationCore",
					"Slate",
					"Engine",
					"InputCore",
					"InputDevice",				
					"CoreUObject",
					"DeveloperSettings",
				}
			);

			bool bHasGameInputSupport = HasGameInputSupport(Target);
			// Define this as 0 in the base module to avoid compilation errors when building
			// without any Game Input support. It is up to the platform-specific submodules to define 
			PublicDefinitions.Add("GAME_INPUT_SUPPORT=" + (bHasGameInputSupport ? "1" : "0"));

			// Give platforms extensions a chance to add any required dependencies that may be necessary to compile game input
			if (bHasGameInputSupport)
			{
				AddRequiredDeps(Target);
			}
		}
						
		/**
		 * Returns the SDK version number based on the installation path.
		 * This will first attempt to read the version from the grdk.ini file.
		 * If that fails, then we will try and parse the version from the file path of the installation.
		 *
		 * Returns a string of the version number, or "-1" if it couldn't be found. 
		 */
		private string GetGameInputVersionString(in string GRDKPath)
		{
			string GRDKVersionString = GetGameInputVersionStringFromIni(GRDKPath);
			if (GRDKVersionString == "-1")
			{
				GRDKVersionString = GetGameInputVersionStringFromPath(GRDKPath);
			}

			return GRDKVersionString;
		}
		
		/**
		 * Looks for the grdk.ini file in the given GRDK path.
		 * If the file exists, then attempt to find the version in the ini
		 *
		 * Returns a string of the version from the ini if successful, but "-1" if it fails. 
		 */
		private string GetGameInputVersionStringFromIni(in string GRDKPath)
		{
			if (!string.IsNullOrEmpty(GRDKPath) && Directory.Exists(GRDKPath))
			{
				string IniFileName = Path.Combine(GRDKPath, "grdk.ini");
				FileInfo Info = new FileInfo(IniFileName);
				if (Info.Exists)
				{
					// Parse the file for the _xbld_edition value
					// it will be in the format of 
					//  _xbld_edition=<version>
					// i.e.
					//  _xbld_edition=230306
					foreach (string Line in File.ReadAllLines(IniFileName))
					{
						if (!Line.Contains("_xbld_edition="))
						{
							continue;
						}

						string[] keyVal = Line.Split('=');
						if (keyVal.Length >= 2)
						{
							return keyVal[1];
						}
					}
				}
			}

			return "-1";
		}

		/**
		 * Returns the 6 digit version number based on the installation path of the SDK.
		 * -1 if the path is invalid or a valid version couldn't be found.
		 */
		private string GetGameInputVersionStringFromPath(in string GRDKPath)
		{
			if (GRDKPath is null)
			{
				return "-1";
			}

			// regex for finding a 6 digit number before "GRDK", which should always give us the version number of the GRDK that is installed
			string Pattern = "(\\d{6})(\\\\GRDK)";
			Regex Rg = new Regex(Pattern);

			MatchCollection Matched = Rg.Matches(GRDKPath);

			// The result we care about will be in the first match, second group
			return
				Matched.Count > 0 && Matched[0].Groups.Count > 1 ?
					Matched[0].Groups[1].ToString()
					: "-1";
		}
	}
}
