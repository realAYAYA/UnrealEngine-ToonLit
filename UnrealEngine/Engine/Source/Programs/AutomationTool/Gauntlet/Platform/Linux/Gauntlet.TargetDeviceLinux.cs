// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Threading;
using AutomationTool;
using UnrealBuildTool;
using System.Text.RegularExpressions;
using EpicGames.Core;
using static AutomationTool.ProcessResult;

namespace Gauntlet
{
	/// <summary>
	/// Linux implementation of a device to run applications
	/// </summary>
	public class TargetDeviceLinux : TargetDeviceDesktopCommon
	{
		public bool IsArm64 { get; set; }

		public TargetDeviceLinux(string InName, string InCacheDir)
			: base(InName, InCacheDir)
		{
			Platform = UnrealTargetPlatform.Linux;
			RunOptions = CommandUtils.ERunOptions.NoWaitForExit | CommandUtils.ERunOptions.NoLoggingOfRunCommand;
		}

		public TargetDeviceLinux(string InName, string InCacheDir, bool InIsArm64)
			: base(InName, InCacheDir)
		{
			IsArm64 = InIsArm64;
			Platform = InIsArm64 ? UnrealTargetPlatform.LinuxArm64 : UnrealTargetPlatform.Linux;
			RunOptions = CommandUtils.ERunOptions.NoWaitForExit | CommandUtils.ERunOptions.NoLoggingOfRunCommand;
		}

		public override IAppInstance Run(IAppInstall App)
		{
			LinuxAppInstall LinuxApp = App as LinuxAppInstall;

			if (LinuxApp == null)
			{
				throw new DeviceException("AppInstance is of incorrect type!");
			}

			if (File.Exists(LinuxApp.ExecutablePath) == false)
			{
				throw new DeviceException("Specified path {0} not found!", LinuxApp.ExecutablePath);
			}

			IProcessResult Result = null;

			lock (Globals.MainLock)
			{
				string ExePath = Path.GetDirectoryName(LinuxApp.ExecutablePath);
				string NewWorkingDir = string.IsNullOrEmpty(LinuxApp.WorkingDirectory) ? ExePath : LinuxApp.WorkingDirectory;
				string OldWD = Environment.CurrentDirectory;
				Environment.CurrentDirectory = NewWorkingDir;

				Log.Info("Launching {0} on {1}", App.Name, ToString());

				string CmdLine = LinuxApp.CommandArguments;

				Log.Verbose("\t{0}", CmdLine);

				bool bAllowSpew = LinuxApp.RunOptions.HasFlag(CommandUtils.ERunOptions.AllowSpew);

				bool bAppContainerized = LinuxApp is IContainerized;

				// Forward command to Docker container if running containerized
				if (bAppContainerized)
				{
					ContainerInfo Container = ((IContainerized)LinuxApp).ContainerInfo;
					string ContainerApp = Container.WorkingDir + "/" + Path.GetRelativePath(Globals.UnrealRootDir, LinuxApp.ExecutablePath).Replace('\\', '/');
					CmdLine = $"run --name {Container.ContainerName} {Container.ImageName} {Container.RunCommandPrepend} {ContainerApp} {CmdLine}";
				}

				Result = CommandUtils.Run(bAppContainerized ? "docker" : LinuxApp.ExecutablePath,
					CmdLine,
					Options: LinuxApp.RunOptions,
					SpewFilterCallback: new SpewFilterCallbackType(delegate (string M) { return bAllowSpew ? M : null; }) /* make sure stderr does not spew in the stdout */,
					WorkingDir: LinuxApp.WorkingDirectory);

				if (Result.HasExited && Result.ExitCode != 0)
				{
					throw new AutomationException("Failed to launch {0}. Error {1}", LinuxApp.ExecutablePath, Result.ExitCode);
				}

				Environment.CurrentDirectory = OldWD;
			}

			return new LinuxAppInstance(LinuxApp, Result, LinuxApp.LogFile);
		}

		protected override IAppInstall InstallNativeStagedBuild(UnrealAppConfig AppConfig, NativeStagedBuild InBuild)
		{
			LinuxAppInstall LinuxApp;
			if (AppConfig.ContainerInfo != null)
			{
				LinuxApp = new LinuxAppContainerInstall(AppConfig.Name, AppConfig.ProjectName, AppConfig.ContainerInfo, this);
			}
			else
			{
				LinuxApp = new LinuxAppInstall(AppConfig.Name, AppConfig.ProjectName, this);
			}
			LinuxApp.SetDefaultCommandLineArguments(AppConfig, RunOptions, InBuild.BuildPath);

			CopyAdditionalFiles(AppConfig.FilesToCopy);

			LinuxApp.ExecutablePath = Path.Combine(InBuild.BuildPath, InBuild.ExecutablePath);
			LinuxApp.WorkingDirectory = InBuild.BuildPath;

			return LinuxApp;
		}

		protected override IAppInstall InstallStagedBuild(UnrealAppConfig AppConfig, StagedBuild InBuild)
		{
			string BuildDir = InBuild.BuildPath;

			if (Utils.SystemHelpers.IsNetworkPath(BuildDir))
			{
				string SubDir = string.IsNullOrEmpty(AppConfig.Sandbox) ? AppConfig.ProjectName : AppConfig.Sandbox;
				string InstallDir = Path.Combine(InstallRoot, SubDir, AppConfig.ProcessType.ToString());

				if (!AppConfig.SkipInstall)
				{
					InstallDir = StagedBuild.InstallBuildParallel(AppConfig, InBuild, BuildDir, InstallDir, ToString());
				}
				else
				{
					Log.Info("Skipping install of {0} (-SkipInstall)", BuildDir);
				}

				BuildDir = InstallDir;
				Utils.SystemHelpers.MarkDirectoryForCleanup(InstallDir);
			}

			LinuxAppInstall LinuxApp = new LinuxAppInstall(AppConfig.Name, AppConfig.ProjectName, this);
			LinuxApp.SetDefaultCommandLineArguments(AppConfig, RunOptions, BuildDir);

			if (LocalDirectoryMappings.Count == 0)
			{
				PopulateDirectoryMappings(Path.Combine(BuildDir, AppConfig.ProjectName));
			}

			CopyAdditionalFiles(AppConfig.FilesToCopy);

			if (Path.IsPathRooted(InBuild.ExecutablePath))
			{
				LinuxApp.ExecutablePath = InBuild.ExecutablePath;
			}
			else
			{
				// TODO - this check should be at a higher level....
				string BinaryPath = Path.Combine(BuildDir, InBuild.ExecutablePath);

				// check for a local newer executable
				if (Globals.Params.ParseParam("dev") && AppConfig.ProcessType.UsesEditor() == false)
				{
					string LocalBinary = Path.Combine(Environment.CurrentDirectory, InBuild.ExecutablePath);

					bool LocalFileExists = File.Exists(LocalBinary);
					bool LocalFileNewer = LocalFileExists && File.GetLastWriteTime(LocalBinary) > File.GetLastWriteTime(BinaryPath);

					Log.Verbose("Checking for newer binary at {0}", LocalBinary);
					Log.Verbose("LocalFile exists: {0}. Newer: {1}", LocalFileExists, LocalFileNewer);

					if (LocalFileExists && LocalFileNewer)
					{
						// need to -basedir to have our exe load content from the path
						LinuxApp.CommandArguments += string.Format(" -basedir={0}", Path.GetDirectoryName(BinaryPath));

						BinaryPath = LocalBinary;
					}
				}

				LinuxApp.ExecutablePath = BinaryPath;
			}

			return LinuxApp;
		}

		protected override IAppInstall InstallEditorBuild(UnrealAppConfig AppConfig, EditorBuild Build)
		{
			LinuxAppInstall LinuxApp = new LinuxAppInstall(AppConfig.Name, AppConfig.ProjectName, this);

			LinuxApp.WorkingDirectory = Path.GetDirectoryName(Build.ExecutablePath);
			LinuxApp.SetDefaultCommandLineArguments(AppConfig, RunOptions, LinuxApp.WorkingDirectory);
			LinuxApp.ExecutablePath = Build.ExecutablePath;

			if (LocalDirectoryMappings.Count == 0)
			{
				PopulateDirectoryMappings(AppConfig.ProjectFile.Directory.FullName);
			}

			CopyAdditionalFiles(AppConfig.FilesToCopy);

			return LinuxApp;
		}
	}

	public class LinuxAppInstall : DesktopCommonAppInstall<TargetDeviceLinux>
	{
		[Obsolete("Will be removed in a future release. Use 'DesktopDevice' instead.")]
		public TargetDeviceLinux LinuxDevice { get; private set; }

		public LinuxAppInstall(string InName, string InProjectName, TargetDeviceLinux InDevice)
			: base(InName, InProjectName, InDevice)
		{ }
	}

	public class LinuxAppContainerInstall : LinuxAppInstall, IContainerized
	{
		public ContainerInfo ContainerInfo { get; set; }

		public LinuxAppContainerInstall(string InName, string InProjectName, ContainerInfo InContainerInfo, TargetDeviceLinux InDevice)
			: base(InName, InProjectName, InDevice)
		{
			ContainerInfo = InContainerInfo;
		}
	}

	public class LinuxAppInstance : DesktopCommonAppInstance<LinuxAppInstall, TargetDeviceLinux>
	{
		public LinuxAppInstance(LinuxAppInstall InInstall, IProcessResult InProcess, string InProcessLogFile = null)
			: base(InInstall, InProcess, InProcessLogFile)
		{ }
	}

	public class LinuxDeviceFactory : IDeviceFactory
	{
		public bool CanSupportPlatform(UnrealTargetPlatform? Platform)
		{
			return Platform == UnrealTargetPlatform.Linux;
		}

		public ITargetDevice CreateDevice(string InRef, string InCachePath, string InParam = null)
		{
			return new TargetDeviceLinux(InRef, InCachePath);
		}
	}

	public class LinuxArm64DeviceFactory : IDeviceFactory
	{
		public bool CanSupportPlatform(UnrealTargetPlatform? Platform)
		{
			return Platform == UnrealTargetPlatform.LinuxArm64;
		}

		public ITargetDevice CreateDevice(string InRef, string InCachePath, string InParam = null)
		{
			return new TargetDeviceLinux(InRef, InCachePath, true);
		}
	}
}