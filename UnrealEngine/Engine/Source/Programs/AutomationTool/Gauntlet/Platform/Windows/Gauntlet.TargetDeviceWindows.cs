// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Threading;
using AutomationTool;
using UnrealBuildTool;
using System.Text.RegularExpressions;

namespace Gauntlet
{

	public abstract class LocalAppProcess : IAppInstance
	{
		public IProcessResult ProcessResult { get; private set; }

		public bool HasExited { get { return ProcessResult.HasExited; } }

		public bool WasKilled { get; protected set; }

		public string StdOut { get { return string.IsNullOrEmpty(ProcessLogFile) ? ProcessResult.Output : ProcessLogOutput; } }

		public int ExitCode { get { return ProcessResult.ExitCode; } }

		public string CommandLine { get; private set; }

		public LocalAppProcess(IProcessResult InProcess, string InCommandLine, string InProcessLogFile = null)
		{
			this.CommandLine = InCommandLine;
			this.ProcessResult = InProcess;
			this.ProcessLogFile = InProcessLogFile;

			// start reader thread if logging to a file
			if (!string.IsNullOrEmpty(InProcessLogFile))
			{
				new System.Threading.Thread(LogFileReaderThread).Start();
			}
		}

		public int WaitForExit()
		{
			if (!HasExited)
			{
				ProcessResult.WaitForExit();
			}

			return ExitCode;
		}

		virtual public void Kill()
		{
			if (!HasExited)
			{
				WasKilled = true;
				ProcessResult.ProcessObject.Kill();
			}
		}

		/// <summary>
		/// Reader thread when logging to file
		/// </summary>
		void LogFileReaderThread()
		{
			// Wait for the processes log file to be created
			while (!File.Exists(ProcessLogFile) && !HasExited)
			{
				Thread.Sleep(2000);
			}

			// Check whether the process exited before log file was created (this can happen for example if a server role exits and forces client to shutdown)
			if (!File.Exists(ProcessLogFile))
			{
				ProcessLogOutput += "Process exited before log file created";
				return;
			}

			Thread.Sleep(1000);

			using (FileStream ProcessLog = File.Open(ProcessLogFile, FileMode.Open, FileAccess.Read, FileShare.ReadWrite))
			{
				StreamReader LogReader = new StreamReader(ProcessLog);
				// Read until the process has exited
				do
				{
					Thread.Sleep(250);

					while (!LogReader.EndOfStream)
					{
						string Output = LogReader.ReadToEnd();

						if (Output != null)
						{
							ProcessLogOutput += Output;
						}
					}
				}
				while (!HasExited);

				LogReader.Close();
				ProcessLog.Close();
				ProcessLog.Dispose();
			}
		}


		public abstract string ArtifactPath { get; }

		public abstract ITargetDevice Device { get; }

		string ProcessLogFile;
		string ProcessLogOutput = "";		
	}

	class WindowsAppInstance : LocalAppProcess
	{
		protected WindowsAppInstall Install;

		public WindowsAppInstance(WindowsAppInstall InInstall, IProcessResult InProcess, string ProcessLogFile = null)
			: base(InProcess, InInstall.CommandArguments, ProcessLogFile)
		{
			Install = InInstall;
		}

		public override string ArtifactPath
		{
			get
			{
				return Install.ArtifactPath;
			}
		}

		public override ITargetDevice Device
		{
			get
			{
				return Install.Device;
			}
		}
	}


	class WindowsAppInstall : IAppInstall
	{
		public string Name { get; private set; }

		public string WorkingDirectory;

		public string ExecutablePath;

		public string CommandArguments;

		public string ArtifactPath;

		public string ProjectName;

		public TargetDeviceWindows WinDevice { get; private set; }

		public ITargetDevice Device { get { return WinDevice; } }

		public CommandUtils.ERunOptions RunOptions { get; set; }

		public WindowsAppInstall(string InName, string InProjectName, TargetDeviceWindows InDevice)
		{
			Name = InName;
			ProjectName = InProjectName;
			WinDevice = InDevice;
			CommandArguments = "";
			this.RunOptions = CommandUtils.ERunOptions.NoWaitForExit;
		}

		public IAppInstance Run()
		{
			return Device.Run(this);
		}

		public virtual void CleanDeviceArtifacts()
		{
			if (!string.IsNullOrEmpty(ArtifactPath) && Directory.Exists(ArtifactPath))
			{
				try
				{
					Log.Info("Clearing actifact path {0} for {1}", ArtifactPath, Device.Name);
					Directory.Delete(ArtifactPath, true);
				}
				catch (Exception Ex)
				{
					Log.Warning("Failed to delete {0}. {1}", ArtifactPath, Ex.Message);
				}
			}
		}
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

	/// <summary>
	/// Win32/64 implementation of a device to run applications
	/// </summary>
	public class TargetDeviceWindows : ITargetDevice
	{
		public string Name { get; protected set; }

		protected string UserDir { get; set; }

		/// <summary>
		/// Our mappings of Intended directories to where they actually represent on this platform.
		/// </summary>
		protected Dictionary<EIntendedBaseCopyDirectory, string> LocalDirectoryMappings { get; set; }

		public TargetDeviceWindows(string InName, string InCacheDir)
		{
			Name = InName;
			LocalCachePath = InCacheDir;
			RunOptions = CommandUtils.ERunOptions.NoWaitForExit | CommandUtils.ERunOptions.NoLoggingOfRunCommand;

			UserDir = Path.Combine(LocalCachePath, "UserDir");
            LocalDirectoryMappings = new Dictionary<EIntendedBaseCopyDirectory, string>();
		}

		#region IDisposable Support
		private bool disposedValue = false; // To detect redundant calls

		protected virtual void Dispose(bool disposing)
		{
			if (!disposedValue)
			{
				if (disposing)
				{
					// TODO: dispose managed state (managed objects).
				}

				// TODO: free unmanaged resources (unmanaged objects) and override a finalizer below.
				// TODO: set large fields to null.

				disposedValue = true;
			}
		}

		// This code added to correctly implement the disposable pattern.
		public void Dispose()
		{
			// Do not change this code. Put cleanup code in Dispose(bool disposing) above.
			Dispose(true);
			// TODO: uncomment the following line if the finalizer is overridden above.
			// GC.SuppressFinalize(this);
		}
		#endregion

		public CommandUtils.ERunOptions RunOptions { get; set; }

        // We care about UserDir in windows as some of the roles may require files going into user instead of build dir.
        public void PopulateDirectoryMappings(string BasePath, string UserDir)
		{
			LocalDirectoryMappings.Add(EIntendedBaseCopyDirectory.Build, Path.Combine(BasePath, "Build"));
			LocalDirectoryMappings.Add(EIntendedBaseCopyDirectory.Binaries, Path.Combine(BasePath, "Binaries"));
			LocalDirectoryMappings.Add(EIntendedBaseCopyDirectory.Config, Path.Combine(BasePath, "Saved", "Config"));
            LocalDirectoryMappings.Add(EIntendedBaseCopyDirectory.Content, Path.Combine(BasePath, "Content"));
            LocalDirectoryMappings.Add(EIntendedBaseCopyDirectory.Demos, Path.Combine(UserDir, "Saved", "Demos"));
			LocalDirectoryMappings.Add(EIntendedBaseCopyDirectory.PersistentDownloadDir, Path.Combine(BasePath, "Saved", "PersistentDownloadDir"));
			LocalDirectoryMappings.Add(EIntendedBaseCopyDirectory.Profiling, Path.Combine(BasePath, "Saved", "Profiling"));
            LocalDirectoryMappings.Add(EIntendedBaseCopyDirectory.Saved, Path.Combine(BasePath, "Saved"));
		}

        public IAppInstance Run(IAppInstall App)
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
			string ProcessLogFile = null;

			lock (Globals.MainLock)
			{
				string ExePath = Path.GetDirectoryName(WinApp.ExecutablePath);
				string NewWorkingDir = string.IsNullOrEmpty(WinApp.WorkingDirectory) ? ExePath : WinApp.WorkingDirectory;
				string OldWD = Environment.CurrentDirectory;
				Environment.CurrentDirectory = NewWorkingDir;

				Log.Info("Launching {0} on {1}", App.Name, ToString());

				string CmdLine = WinApp.CommandArguments;

				// Look in app parameters if abslog is specified, if so use it
				Regex CLRegex = new Regex(@"(--?[a-zA-Z]+)[:\s=]?([A-Z]:(?:\\[\w\s-]+)+\\?(?=\s-)|\""[^\""]*\""|[^-][^\s]*)?");
				foreach (Match M in CLRegex.Matches(CmdLine))
				{
					if (M.Groups.Count == 3 && M.Groups[1].Value == "-abslog")
					{
						ProcessLogFile = M.Groups[2].Value;
					}
				}

				// explicitly set log file when not already defined
				if (string.IsNullOrEmpty(ProcessLogFile))
				{
					string LogFolder = string.Format(@"{0}\Logs", WinApp.ArtifactPath);

					if (!Directory.Exists(LogFolder))
					{
						Directory.CreateDirectory(LogFolder);
					}

					ProcessLogFile = string.Format("{0}\\{1}.log", LogFolder, WinApp.ProjectName);
					CmdLine = string.Format("{0} -abslog=\"{1}\"", CmdLine, ProcessLogFile);
				}

				// cleanup any existing log file
				try
				{
					if (File.Exists(ProcessLogFile))
					{
						EpicGames.Core.FileUtils.ForceDeleteFile(ProcessLogFile);
					}
				}
				catch (Exception Ex)
				{
					//throw new AutomationException("Unable to delete existing log file {0} {1}", ProcessLogFile, Ex.Message);
					Log.Warning("Unable to delete existing log file {0}. {1}", ProcessLogFile, Ex.Message);
				}

				Log.Verbose("\t{0}", CmdLine);

				Result = CommandUtils.Run(WinApp.ExecutablePath, CmdLine, Options: WinApp.RunOptions | (ProcessLogFile != null ? CommandUtils.ERunOptions.NoStdOutRedirect : 0 ));

				if (Result.HasExited && Result.ExitCode != 0)
				{
					throw new AutomationException("Failed to launch {0}. Error {1}", WinApp.ExecutablePath, Result.ExitCode);
				}

				Environment.CurrentDirectory = OldWD;
			}

			return new WindowsAppInstance(WinApp, Result, ProcessLogFile);
		}

		private void CopyAdditionalFiles(UnrealAppConfig AppConfig)
		{
			if (AppConfig.FilesToCopy != null)
			{
				foreach (UnrealFileToCopy FileToCopy in AppConfig.FilesToCopy)
				{
					string PathToCopyTo = Path.Combine(LocalDirectoryMappings[FileToCopy.TargetBaseDirectory], FileToCopy.TargetRelativeLocation);
					if (File.Exists(FileToCopy.SourceFileLocation))
					{
						FileInfo SrcInfo = new FileInfo(FileToCopy.SourceFileLocation);
						SrcInfo.IsReadOnly = false;
						string DirectoryToCopyTo = Path.GetDirectoryName(PathToCopyTo);
						if (!Directory.Exists(DirectoryToCopyTo))
						{
							Directory.CreateDirectory(DirectoryToCopyTo);
						}
						if (File.Exists(PathToCopyTo))
						{
							FileInfo ExistingFile = new FileInfo(PathToCopyTo);
							ExistingFile.IsReadOnly = false;
						}
						SrcInfo.CopyTo(PathToCopyTo, true);
						Log.Info("Copying {0} to {1}", FileToCopy.SourceFileLocation, PathToCopyTo);
					}
					else
					{
						Log.Warning("File to copy {0} not found", FileToCopy);
					}
				}
			}
		}

		protected IAppInstall InstallStagedBuild(UnrealAppConfig AppConfig, StagedBuild InBuild)
		{
			bool SkipDeploy = Globals.Params.ParseParam("SkipDeploy");

			string BuildPath = InBuild.BuildPath;

			if (CanRunFromPath(BuildPath) == false)
			{
				string SubDir = string.IsNullOrEmpty(AppConfig.Sandbox) ? AppConfig.ProjectName : AppConfig.Sandbox;
				string DestPath = Path.Combine(this.LocalCachePath, SubDir, AppConfig.ProcessType.ToString());

				if (!SkipDeploy)
				{
					DestPath = StagedBuild.InstallBuildParallel(AppConfig, InBuild, BuildPath, DestPath, ToString());
				}
				else
				{
					Log.Info("Skipping install of {0} (-skipdeploy)", BuildPath);
				}

				Utils.SystemHelpers.MarkDirectoryForCleanup(DestPath);

				BuildPath = DestPath;
			}

			WindowsAppInstall WinApp = new WindowsAppInstall(AppConfig.Name, AppConfig.ProjectName, this);
			WinApp.RunOptions = RunOptions;

			// Set commandline replace any InstallPath arguments with the path we use
			WinApp.CommandArguments = Regex.Replace(AppConfig.CommandLine, @"\$\(InstallPath\)", BuildPath, RegexOptions.IgnoreCase);

			if (string.IsNullOrEmpty(UserDir) == false)
			{
				WinApp.CommandArguments += string.Format(" -userdir=\"{0}\"", UserDir);
				WinApp.ArtifactPath = Path.Combine(UserDir, @"Saved");

				Utils.SystemHelpers.MarkDirectoryForCleanup(UserDir);
			}
			else
			{
				// e.g d:\Unreal\GameName\Saved
				WinApp.ArtifactPath = Path.Combine(BuildPath, AppConfig.ProjectName, @"Saved");

			}

			// clear artifact path
			WinApp.CleanDeviceArtifacts();

            if (LocalDirectoryMappings.Count == 0)
            {
                PopulateDirectoryMappings(Path.Combine(BuildPath, AppConfig.ProjectName), UserDir);
            }

			CopyAdditionalFiles(AppConfig);

            if (Path.IsPathRooted(InBuild.ExecutablePath))
			{
				WinApp.ExecutablePath = InBuild.ExecutablePath;
			}
			else
			{
				// TODO - this check should be at a higher level....
				string BinaryPath = Path.Combine(BuildPath, InBuild.ExecutablePath);

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
						WinApp.CommandArguments += string.Format(" -basedir={0}", Path.GetDirectoryName(BinaryPath));

						BinaryPath = LocalBinary;
					}
				}

				WinApp.ExecutablePath = BinaryPath;
			}

			return WinApp;
		}


		public IAppInstall InstallApplication(UnrealAppConfig AppConfig)
		{
			if (AppConfig.Build is StagedBuild)
			{
				return InstallStagedBuild(AppConfig, AppConfig.Build as StagedBuild);
			}

			EditorBuild EditorBuild = AppConfig.Build as EditorBuild;

			if (EditorBuild == null)
			{
				throw new AutomationException("Invalid build type!");
			}

			WindowsAppInstall WinApp = new WindowsAppInstall(AppConfig.Name, AppConfig.ProjectName, this);

			WinApp.WorkingDirectory = Path.GetDirectoryName(EditorBuild.ExecutablePath);
			WinApp.RunOptions = RunOptions;
	
			// Force this to stop logs and other artifacts going to different places
			WinApp.CommandArguments = AppConfig.CommandLine + string.Format(" -userdir=\"{0}\"", UserDir);
			WinApp.ArtifactPath = Path.Combine(UserDir, @"Saved");
			WinApp.ExecutablePath = EditorBuild.ExecutablePath;

			if (LocalDirectoryMappings.Count == 0)
			{
				PopulateDirectoryMappings(AppConfig.ProjectFile.Directory.FullName, AppConfig.ProjectFile.Directory.FullName);
			}

			CopyAdditionalFiles(AppConfig);

			return WinApp;
		}
		
		public bool CanRunFromPath(string InPath)
		{
			return !Utils.SystemHelpers.IsNetworkPath(InPath);
		}

		public UnrealTargetPlatform? Platform { get { return UnrealTargetPlatform.Win64; } }

		public string LocalCachePath { get; private set; }
		public bool IsAvailable { get { return true; } }
		public bool IsConnected { get { return true; } }
		public bool IsOn { get { return true; } }
		public bool PowerOn() { return true; }
		public bool PowerOff() { return true; }
		public bool Reboot() { return true; }
		public bool Connect() { return true; }
		public bool Disconnect(bool bForce = false) { return true; }

		public override string ToString()
		{
			return Name;
		}

		public Dictionary<EIntendedBaseCopyDirectory, string> GetPlatformDirectoryMappings()
		{
			if (LocalDirectoryMappings.Count == 0)
			{
				Log.Warning("Platform directory mappings have not been populated for this platform! This should be done within InstallApplication()");
			}
			return LocalDirectoryMappings;
		}
	}
}