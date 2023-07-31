// Copyright Epic Games, Inc. All Rights Reserved.

using AutomationTool;
using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
//using Tools.DotNETCommon;
using UnrealBuildTool;

namespace Gauntlet 
{
	public class HoloLensBuildSource : StagedBuildSource<StagedBuild>
	{
		public override string BuildName { get { return "HoloLensStagedBuild"; } }

		public override UnrealTargetPlatform Platform { get { return UnrealTargetPlatform.HoloLens; } }

		public override string PlatformFolderPrefix { get { return "HoloLens"; } }

		HoloLensBuildSource()
		{

		}
	}

	/// <summary>
	/// HoloLens packaged build from a NSP
	/// </summary>
	public class HoloLensPackageBuild : IBuild
	{
		public BuildFlags Flags { get; protected set; }

		public UnrealTargetConfiguration Configuration { get; protected set; }

		public UnrealTargetPlatform Platform { get { return UnrealTargetPlatform.HoloLens; } }

		public string ProgramId = null;

		public string SourceAppxBundlePath = null;
		public string SourceCertPath = null;

		public HoloLensPackageBuild(UnrealTargetConfiguration InConfig, string InAppxBundlePath, BuildFlags InFlags, string InSourceCertPath)
		{
			Configuration = InConfig;
			SourceAppxBundlePath = InAppxBundlePath;
			Flags = InFlags;
			SourceCertPath = InSourceCertPath;
		}

		public bool CanSupportRole(UnrealTargetRole Role)
		{
			return Role.IsClient();
		}

		public static IEnumerable<HoloLensPackageBuild> CreateFromPath(string InProjectName, string InPath)
		{
			string BuildPath = InPath;

			List<HoloLensPackageBuild> DiscoveredBuilds = new List<HoloLensPackageBuild>();

			DirectoryInfo Di = new DirectoryInfo(BuildPath);

			// find all appxbundle packages
			FileInfo[] InstallFiles = Di.GetFiles("*.appxbundle");

			foreach (FileInfo Fi in InstallFiles)
			{
				FileInfo[] CertFiles = Di.GetFiles("*.cer");

				var UnrealConfig = UnrealHelpers.GetConfigurationFromExecutableName(InProjectName, Fi.Name);
				BuildFlags Flags = BuildFlags.Packaged | BuildFlags.CanReplaceCommandLine | BuildFlags.Bulk;
				string AbsPath = Fi.Directory.FullName;
				string CertPath = null;

				if (CertFiles.Count() > 0)
				{
					CertPath = CertFiles[0].FullName;
				}
				else
				{
					Log.Warning("Found {0} {1} build at {2}, but no cert file present.", UnrealConfig, ((Flags & BuildFlags.Bulk) == BuildFlags.Bulk) ? "(bulk)" : "(not bulk)", AbsPath);
				}

				Log.Verbose("Pulling package data from {0}", Fi.FullName);

				HoloLensPackageBuild NewBuild = new HoloLensPackageBuild(UnrealConfig, Fi.FullName, Flags, CertPath);

				DiscoveredBuilds.Add(NewBuild);

				Log.Verbose("Found {0} {1} build at {2}", UnrealConfig, ((Flags & BuildFlags.Bulk) == BuildFlags.Bulk) ? "(bulk)" : "(not bulk)", AbsPath);

			}

			return DiscoveredBuilds;
		}
	}

	public class HoloLensPackagedBuildSource : IFolderBuildSource
	{
		public string BuildName { get { return "HoloLensPackagedBuildSource"; } }

		public bool CanSupportPlatform(UnrealTargetPlatform InPlatform)
		{
			return InPlatform == UnrealTargetPlatform.HoloLens;
		}

		public string ProjectName { get; protected set; }

		public HoloLensPackagedBuildSource()
		{

		}

		public List<IBuild> GetBuildsAtPath(string InProjectName, string InPath, int MaxRecursion = 3)
		{
			List<DirectoryInfo> AllDirs = new List<DirectoryInfo>();

			// c:\path\to\build
			DirectoryInfo PathDI = new DirectoryInfo(InPath);

			if (PathDI.Name.IndexOf("HoloLens", StringComparison.OrdinalIgnoreCase) >= 0)
			{
				AllDirs.Add(PathDI);
			}

			// find all directories that begin with HoloLens
			DirectoryInfo[] HoloLensDirs = PathDI.GetDirectories("HoloLens*", SearchOption.TopDirectoryOnly);

			AllDirs.AddRange(HoloLensDirs);

			List<DirectoryInfo> DirsToRecurse = AllDirs;

			// now get subdirs
			while (MaxRecursion-- > 0)
			{
				List<DirectoryInfo> DiscoveredDirs = new List<DirectoryInfo>();

				DirsToRecurse.ToList().ForEach((D) =>
				{
					DiscoveredDirs.AddRange(D.GetDirectories("*", SearchOption.TopDirectoryOnly));
				});

				AllDirs.AddRange(DiscoveredDirs);
				DirsToRecurse = DiscoveredDirs;
			}

			List<IBuild> Builds = new List<IBuild>();

			foreach (DirectoryInfo Di in AllDirs)
			{
				IEnumerable<HoloLensPackageBuild> FoundBuilds = HoloLensPackageBuild.CreateFromPath(InProjectName, Di.FullName);

				if (FoundBuilds != null)
				{
					Builds.AddRange(FoundBuilds);
				}
			}

			return Builds;
		}

	}

}
