// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.IO;
using AutomationTool;
using UnrealBuildTool;
using EpicGames.Core;
using static AutomationTool.ProcessResult;

namespace Gauntlet
{
	/// <summary>
	/// Win32/64 implementation of a device to run applications
	/// </summary>
	public class TargetDeviceWindows : TargetDeviceDesktopCommon
	{
		public TargetDeviceWindows(string InName, string InCacheDir)
			: base(InName, InCacheDir)
		{
			Platform = UnrealTargetPlatform.Win64;
			RunOptions = CommandUtils.ERunOptions.NoWaitForExit | CommandUtils.ERunOptions.NoLoggingOfRunCommand;
		}

		public override IAppInstall InstallApplication(UnrealAppConfig AppConfig)
		{
			switch (AppConfig.Build)
			{
				case NativeStagedBuild:
					return InstallNativeStagedBuild(AppConfig, AppConfig.Build as NativeStagedBuild);

				case StagedBuild:
					return InstallStagedBuild(AppConfig, AppConfig.Build as StagedBuild);

				case EditorBuild:
					return InstallEditorBuild(AppConfig, AppConfig.Build as EditorBuild);

				case IWindowsSelfInstallingBuild:
					return InstallSelfInstallingBuild(AppConfig, AppConfig.Build as IWindowsSelfInstallingBuild);

				default:
					throw new AutomationException("{0} is an invalid build type!", AppConfig.Build.ToString());
			}
		}

		public override IAppInstance Run(IAppInstall App)
		{
			WindowsAppInstall WinApp = App as WindowsAppInstall;

			if (WinApp == null)
			{
				throw new DeviceException("AppInstance is of incorrect type!");
			}

			if (File.Exists(WinApp.ExecutablePath) == false)
			{
				throw new DeviceException("Specified path {0} not found!", WinApp.ExecutablePath);
			}

			IProcessResult Result = null;

			lock (Globals.MainLock)
			{
				string ExePath = Path.GetDirectoryName(WinApp.ExecutablePath);
				string NewWorkingDir = string.IsNullOrEmpty(WinApp.WorkingDirectory) ? ExePath : WinApp.WorkingDirectory;
				string OldWD = Environment.CurrentDirectory;
				Environment.CurrentDirectory = NewWorkingDir;

				Log.Info("Launching {0} on {1}", App.Name, ToString());

				string CmdLine = WinApp.CommandArguments;
				CommandUtils.ERunOptions FinalRunOptions = WinApp.RunOptions;
				bool bAllowSpew = WinApp.RunOptions.HasFlag(CommandUtils.ERunOptions.AllowSpew);

				Log.Verbose("\t{0}", CmdLine);

				Result = CommandUtils.Run(WinApp.ExecutablePath,
					CmdLine,
					Options: FinalRunOptions,
					SpewFilterCallback: new SpewFilterCallbackType(delegate (string M) { return bAllowSpew ? M : null; }) /* make sure stderr does not spew in the stdout */,
					WorkingDir: WinApp.WorkingDirectory);

				if (Result.HasExited && Result.ExitCode != 0)
				{
					throw new AutomationException("Failed to launch {0}. Error {1}", WinApp.ExecutablePath, Result.ExitCode);
				}

				Environment.CurrentDirectory = OldWD;
			}

			return new WindowsAppInstance(WinApp, Result, WinApp.LogFile);
		}

		protected override IAppInstall InstallNativeStagedBuild(UnrealAppConfig AppConfig, NativeStagedBuild InBuild)
		{
			WindowsAppInstall WinApp = new WindowsAppInstall(AppConfig.Name, AppConfig.ProjectName, this);
			WinApp.WorkingDirectory = InBuild.BuildPath;
			WinApp.SetDefaultCommandLineArguments(AppConfig, RunOptions, InBuild.BuildPath);
			WinApp.ExecutablePath = Path.Combine(InBuild.BuildPath, InBuild.ExecutablePath);

			CopyAdditionalFiles(AppConfig.FilesToCopy);

			return WinApp;
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

			WindowsAppInstall WinApp = new WindowsAppInstall(AppConfig.Name, AppConfig.ProjectName, this);
			WinApp.SetDefaultCommandLineArguments(AppConfig, RunOptions, BuildDir);

			if (LocalDirectoryMappings.Count == 0)
			{
				PopulateDirectoryMappings(Path.Combine(BuildDir, AppConfig.ProjectName));
			}

			CopyAdditionalFiles(AppConfig.FilesToCopy);

			if (Path.IsPathRooted(InBuild.ExecutablePath))
			{
				WinApp.ExecutablePath = InBuild.ExecutablePath;
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
						WinApp.CommandArguments += $" -basedir=\"{Path.GetDirectoryName(BinaryPath)}\"";

						BinaryPath = LocalBinary;
					}
				}

				WinApp.ExecutablePath = BinaryPath;
			}

			return WinApp;
		}

		protected override IAppInstall InstallEditorBuild(UnrealAppConfig AppConfig, EditorBuild Build)
		{
			WindowsAppInstall WinApp = new WindowsAppInstall(AppConfig.Name, AppConfig.ProjectName, this);

			WinApp.WorkingDirectory = Path.GetDirectoryName(Build.ExecutablePath);
			WinApp.SetDefaultCommandLineArguments(AppConfig, RunOptions, WinApp.WorkingDirectory);
			WinApp.ExecutablePath = Build.ExecutablePath;

			if (LocalDirectoryMappings.Count == 0)
			{
				PopulateDirectoryMappings(AppConfig.ProjectFile.Directory.FullName);
			}

			CopyAdditionalFiles(AppConfig.FilesToCopy);

			return WinApp;
		}

		protected IAppInstall InstallSelfInstallingBuild(UnrealAppConfig AppConfig, IWindowsSelfInstallingBuild Build)
		{
			WindowsAppInstall WinApp = Build.Install(this, AppConfig, out string BasePath);
			WinApp.SetDefaultCommandLineArguments(AppConfig, RunOptions, BasePath);

			if (LocalDirectoryMappings.Count == 0)
			{
				PopulateDirectoryMappings(Path.Combine(BasePath, AppConfig.ProjectName));
			}

			CopyAdditionalFiles(AppConfig.FilesToCopy);
			return WinApp;
		}
	}

	public class WindowsAppInstall : DesktopCommonAppInstall<TargetDeviceWindows>, IAppInstall.IDynamicCommandLine
	{
		[Obsolete("Will be removed in a future release. Use 'DesktopDevice' instead.")]
		public TargetDeviceWindows WinDevice => DesktopDevice;

		public WindowsAppInstall(string InName, string InProjectName, TargetDeviceWindows InDevice)
			: base(InName, InProjectName, InDevice)
		{ }
	}

	public class WindowsAppInstance : DesktopCommonAppInstance<WindowsAppInstall, TargetDeviceWindows>
	{
		public WindowsAppInstance(WindowsAppInstall InInstall, IProcessResult InProcess, string InProcessLogFile = null)
			: base(InInstall, InProcess, InProcessLogFile)
		{ }
	}

	public class Win64DeviceFactory : IDeviceFactory
	{
		public bool CanSupportPlatform(UnrealTargetPlatform? Platform)
		{
			return Platform == UnrealTargetPlatform.Win64;
		}

		public ITargetDevice CreateDevice(string InRef, string InCachePath, string InParam = null)
		{
			return new TargetDeviceWindows(InRef, InCachePath);
		}
	}
}