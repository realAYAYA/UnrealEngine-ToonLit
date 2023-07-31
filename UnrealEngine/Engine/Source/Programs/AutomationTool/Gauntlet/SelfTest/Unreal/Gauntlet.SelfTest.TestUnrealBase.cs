// Copyright Epic Games, Inc. All Rights Reserved.

using Gauntlet;
using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using EpicGames.Core;
using UnrealBuildTool;

namespace Gauntlet.SelfTest
{
	/// <summary>
	/// Base class for Unreal tests. Mostly provides Build / Environment info
	/// </summary>
	abstract class TestUnrealBase : BaseTestNode
	{
		/// <summary>
		/// Name of the project we're testing
		/// </summary>
		public string ProjectName { get; protected set; }

		/// <summary>
		/// Name of the project we're testing
		/// </summary>
		public FileReference ProjectFile { get; protected set; }

		/// <summary>
		/// Path to Unreal
		/// </summary>
		public DirectoryReference UnrealPath { get; protected set; }

		/// <summary>
		/// True if this project uses the Game/NoEditor shared build type instead of separate client/server ones
		/// </summary>
		public bool UsesSharedBuildType { get; protected set; }

		/// <summary>
		/// Name of devkit to use for tests
		/// </summary>
		public string DevkitName { get; protected set; }

		/// <summary>
		/// Path of the build to test against
		/// </summary>
		public string BuildPath { get; protected set; }

		/// <summary>
		/// Client platforms we support
		/// </summary>
		public UnrealTargetPlatform[] SupportedClientPlatforms { get; protected set; }

		/// <summary>
		/// Server platforms we support
		/// </summary>
		public UnrealTargetPlatform[] SupportedServerPlatforms { get; protected set; }

		/// <summary>
		/// Configurations we support
		/// </summary>
		public UnrealTargetConfiguration[] SupportedConfigurations { get; protected set; }

		public TestUnrealBase()
		{
			ProjectName = "Fortnite";
			ProjectFile = new FileReference("FortniteGame/FortniteGame.uproject");
			UsesSharedBuildType = false;
			DevkitName = "Default";

			string BuildPath = Gauntlet.Globals.Params.ParseValue("Build", null);

			if (BuildHostPlatform.Current.Platform == UnrealTargetPlatform.Win64)
			{
				IEnumerable<UnrealTargetPlatform> SupportedDevices = Gauntlet.Utils.InterfaceHelpers.FindImplementations<IDeviceBuildSupport>().Select(D => D.GetPlatform() ?? UnrealTargetPlatform.Win64);
				SupportedClientPlatforms = SupportedDevices.Append(UnrealTargetPlatform.Win64).Distinct().ToArray();
				SupportedServerPlatforms = new[] { UnrealTargetPlatform.Win64, UnrealTargetPlatform.Linux };
			}
			else
			{
				SupportedClientPlatforms = new[] { UnrealTargetPlatform.Mac };
				SupportedServerPlatforms = new[] { UnrealTargetPlatform.Mac };
			}

			SupportedConfigurations = new[] { UnrealTargetConfiguration.Development, UnrealTargetConfiguration.Test };
		}

		private string FindValidBuild(string GameName)
		{
			// if we found a valid build < 1 day ago try to reuse it to avoid needless syncing..
			string LastBuildFile = Path.Combine(Path.GetTempPath(), "GauntletTestBuild.txt");

			if (File.Exists(LastBuildFile) && (DateTime.Now - File.GetLastWriteTime(LastBuildFile)).TotalDays < 3)
			{
				string LastBuild = File.ReadAllText(LastBuildFile);

				if (Directory.Exists(LastBuild))
				{
					return LastBuild;
				}
			}

			string ProjectName = GameName.Replace("Game", "");

			DirectoryInfo Di = new DirectoryInfo(@"P:\Builds\" + ProjectName);
			string BranchName = string.Format("++{0}+Main", ProjectName);

			// find a build with one or more pak in WindowsClient/Cooked
			DirectoryInfo BuildDir = Di.GetDirectories()
				.Where(D =>
				{
					if (D.Name.Contains(BranchName))
					{
						string PakPath = string.Format(@"WindowsClient\Staged\{0}\Content\Paks", GameName);
						string WindowsClient = Path.Combine(D.FullName, PakPath);

						if (Directory.Exists(WindowsClient) && Directory.GetFiles(WindowsClient, "*.pak").Length >= 1)
						{
							return true;
						}
					}
					return false;
				})
				.OrderByDescending(D => D.CreationTimeUtc)
				.FirstOrDefault();

			// save this build
			File.WriteAllText(LastBuildFile, BuildDir.FullName);
			
			return BuildDir.FullName;
		}
	}
}
