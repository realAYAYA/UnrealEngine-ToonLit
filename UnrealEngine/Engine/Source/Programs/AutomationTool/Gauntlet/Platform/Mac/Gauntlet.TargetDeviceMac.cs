// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.IO;
using AutomationTool;
using UnrealBuildTool;
using System.Text.RegularExpressions;
using EpicGames.Core;
using UnrealBuildBase;
using static AutomationTool.ProcessResult;

namespace Gauntlet
{
	public class TargetDeviceMac : TargetDeviceDesktopCommon
	{
		public TargetDeviceMac(string InName, string InCachePath)
			: base(InName, InCachePath)
		{
			Platform = UnrealTargetPlatform.Mac;
			RunOptions = CommandUtils.ERunOptions.NoWaitForExit;
		}

		public override IAppInstall InstallApplication(UnrealAppConfig AppConfig)
		{
			switch (AppConfig.Build)
			{
				case NativeStagedBuild:
					return InstallNativeStagedBuild(AppConfig, AppConfig.Build as NativeStagedBuild);

				case StagedBuild:
					return InstallStagedBuild(AppConfig, AppConfig.Build as StagedBuild);

				case MacPackagedBuild:
					return InstallPackagedBuild(AppConfig, AppConfig.Build as MacPackagedBuild);

				case EditorBuild:
					return InstallEditorBuild(AppConfig, AppConfig.Build as EditorBuild);

				default:
					throw new AutomationException("{0} is an invalid build type!", AppConfig.Build.ToString());
			}
		}

		public override IAppInstance Run(IAppInstall App)
		{
			MacAppInstall MacInstall = App as MacAppInstall;

			if (MacInstall == null)
			{
				throw new AutomationException("Invalid install type!");
			}

			IProcessResult Result = null;

			lock (Globals.MainLock)
			{
				string OldWD = Environment.CurrentDirectory;
				Environment.CurrentDirectory = MacInstall.WorkingDirectory;

				Log.Info("Launching {0} on {1}", App.Name, ToString());
				Log.Verbose("\t{0}", MacInstall.CommandArguments);

				bool bAllowSpew = MacInstall.RunOptions.HasFlag(CommandUtils.ERunOptions.AllowSpew);
				Result = CommandUtils.Run(GetExecutableIfBundle(MacInstall.ExecutablePath), MacInstall.CommandArguments, Options: MacInstall.RunOptions, SpewFilterCallback: new SpewFilterCallbackType(delegate (string M) { return bAllowSpew ? M : null; }) /* make sure stderr does not spew in the stdout */);

				if (Result.HasExited && Result.ExitCode != 0)
				{
					throw new AutomationException("Failed to launch {0}. Error {1}", MacInstall.ExecutablePath, Result.ExitCode);
				}

				Environment.CurrentDirectory = OldWD;
			}

			return new MacAppInstance(MacInstall, Result, MacInstall.LogFile);
		}

		protected override IAppInstall InstallNativeStagedBuild(UnrealAppConfig AppConfig, NativeStagedBuild InBuild)
		{
			MacAppInstall MacApp = new MacAppInstall(AppConfig.Name, AppConfig.ProjectName, this);
			MacApp.SetDefaultCommandLineArguments(AppConfig, RunOptions, InBuild.BuildPath);
			MacApp.WorkingDirectory = InBuild.BuildPath;
			MacApp.ExecutablePath = Path.Combine(InBuild.BuildPath, InBuild.ExecutablePath);

			CopyAdditionalFiles(AppConfig.FilesToCopy);

			return MacApp;
		}

		protected override IAppInstall InstallEditorBuild(UnrealAppConfig AppConfig, EditorBuild Build)
		{
			MacAppInstall MacApp = new MacAppInstall(AppConfig.Name, AppConfig.ProjectName, this);
			MacApp.SetDefaultCommandLineArguments(AppConfig, RunOptions, Build.ExecutablePath);
			MacApp.WorkingDirectory = Path.GetFullPath(Build.ExecutablePath);
			MacApp.ExecutablePath = GetExecutableIfBundle(Build.ExecutablePath);

			if (LocalDirectoryMappings.Count == 0)
			{
				PopulateDirectoryMappings(AppConfig.ProjectFile.Directory.FullName);
			}

			CopyAdditionalFiles(AppConfig.FilesToCopy);

			return MacApp;
		}

		protected override IAppInstall InstallStagedBuild(UnrealAppConfig AppConfig, StagedBuild Build)
		{
			string BuildPath = CopyBuildIfNecessary(AppConfig, Build.BuildPath);
			string BundlePath = Path.Combine(BuildPath, Build.ExecutablePath);
			return InstallBuild(AppConfig, BuildPath, BundlePath);
		}

		protected IAppInstall InstallPackagedBuild(UnrealAppConfig AppConfig, MacPackagedBuild Build)
		{
			string BuildPath = CopyBuildIfNecessary(AppConfig, Build.BuildPath);
			return InstallBuild(AppConfig, BuildPath, BuildPath);
		}

		protected IAppInstall InstallBuild(UnrealAppConfig AppConfig, string BuildPath, string BundlePath)
		{
			MacAppInstall MacApp = new MacAppInstall(AppConfig.Name, AppConfig.ProjectName, this);
			MacApp.SetDefaultCommandLineArguments(AppConfig, RunOptions, BuildPath);
			MacApp.WorkingDirectory = BuildPath;
			MacApp.ExecutablePath = GetExecutableIfBundle(BundlePath);

			PopulateDirectoryMappings(BuildPath);

			CopyAdditionalFiles(AppConfig.FilesToCopy);

			// check for a local newer executable
			if (Globals.Params.ParseParam("dev")
				&& AppConfig.ProcessType.UsesEditor() == false
				&& AppConfig.ProjectFile != null)
			{
				// Get project properties
				ProjectProperties Props = ProjectUtils.GetProjectProperties(AppConfig.ProjectFile,
					new List<UnrealTargetPlatform>(new[] { AppConfig.Platform.Value }),
					new List<UnrealTargetConfiguration>(new[] { AppConfig.Configuration }));

				// Would this executable be built under Engine or a Project?
				DirectoryReference WorkingDir = Props.bIsCodeBasedProject ? AppConfig.ProjectFile.Directory : Unreal.EngineDirectory;

				// The bundlepath may be under Binaries/Mac for a staged build, or it could be in any folder for a packaged build so just use the name and
				// build the path ourselves
				string LocalProjectBundle = FileReference.Combine(WorkingDir, "Binaries", "Mac", Path.GetFileName(BundlePath)).FullName;

				string LocalProjectBinary = GetExecutableIfBundle(LocalProjectBundle);

				bool LocalFileExists = File.Exists(LocalProjectBinary);
				bool LocalFileNewer = LocalFileExists && File.GetLastWriteTime(LocalProjectBinary) > File.GetLastWriteTime(MacApp.ExecutablePath);

				Log.Verbose("Checking for newer binary at {0}", LocalProjectBinary);
				Log.Verbose("LocalFile exists: {0}. Newer: {1}", LocalFileExists, LocalFileNewer);

				if (LocalFileExists && LocalFileNewer)
				{
					// need to -basedir to have our exe load content from the path that the bundle sits in
					MacApp.CommandArguments += string.Format(" -basedir={0}", Path.GetDirectoryName(BundlePath));
					MacApp.ExecutablePath = LocalProjectBinary;
				}
			}

			return MacApp;
		}

		protected string GetVolumeName(string InPath)
		{
			Match M = Regex.Match(InPath, @"/Volumes/(.+?)/");

			if (M.Success)
			{
				return M.Groups[1].ToString();
			}

			return "";
		}

		/// <summary>
		/// If the path is to Foo.app this returns the actual executable to use (e.g. Foo.app/Contents/MacOS/Foo).
		/// </summary>
		/// <param name="InBundlePath"></param>
		/// <returns></returns>
		protected string GetExecutableIfBundle(string InBundlePath)
		{
			if (Path.GetExtension(InBundlePath).Equals(".app", StringComparison.OrdinalIgnoreCase))
			{
				// Technically we should look at the plist, but for now...
				string BaseName = Path.GetFileNameWithoutExtension(InBundlePath);
				return Path.Combine(InBundlePath, "Contents", "MacOS", BaseName);
			}

			return InBundlePath;
		}


		/// <summary>
		/// Copies a build folder (either a package.app or a folder with a staged built) to a local path if necessary.
		/// Necessary is defined as not being on locally attached storage
		/// </summary>
		/// <param name="AppConfig"></param>
		/// <param name="InBuildPath"></param>
		/// <returns></returns>
		protected string CopyBuildIfNecessary(UnrealAppConfig AppConfig, string InBuildPath)
		{
			string BuildDir = InBuildPath;

			string BuildVolume = GetVolumeName(BuildDir);
			string LocalRoot = GetVolumeName(Environment.CurrentDirectory);

			// Must be on our volume to run
			if (BuildVolume.Equals(LocalRoot, StringComparison.OrdinalIgnoreCase) == false)
			{
				string SubDir = string.IsNullOrEmpty(AppConfig.Sandbox) ? AppConfig.ProjectName : AppConfig.Sandbox;
				string InstallDir = Path.Combine(InstallRoot, SubDir, AppConfig.ProcessType.ToString());

				if (!AppConfig.SkipInstall)
				{
					Utils.SystemHelpers.CopyDirectory(BuildDir, InstallDir, Utils.SystemHelpers.CopyOptions.Mirror);
				}
				else
				{
					Log.Info("Skipping install of {0} (-SkipInstall)", BuildDir);
				}

				BuildDir = InstallDir;
				Utils.SystemHelpers.MarkDirectoryForCleanup(InstallDir);
			}

			return BuildDir;
		}
	}

	public class MacAppInstall : DesktopCommonAppInstall<TargetDeviceMac>
	{
		[Obsolete("Will be removed in a future release. Use WorkingDirectory instead.")]
		public string LocalPath
		{
			get => WorkingDirectory;
			set { } // intentional nop
		}

		public MacAppInstall(string InName, string InProjectName, TargetDeviceMac InDevice)
			: base(InName, InProjectName, InDevice)
		{ }
	}

	public class MacAppInstance : DesktopCommonAppInstance<MacAppInstall, TargetDeviceMac>
	{
		public MacAppInstance(MacAppInstall InInstall, IProcessResult InProcess, string ProcessLogFile = null)
			: base(InInstall, InProcess, ProcessLogFile)
		{ }
	}

	public class MacDeviceFactory : IDeviceFactory
	{
		public bool CanSupportPlatform(UnrealTargetPlatform? Platform)
		{
			return Platform == UnrealTargetPlatform.Mac;
		}

		public ITargetDevice CreateDevice(string InRef, string InCachePath, string InParam = null)
		{
			return new TargetDeviceMac(InRef, InCachePath);
		}
	}
}