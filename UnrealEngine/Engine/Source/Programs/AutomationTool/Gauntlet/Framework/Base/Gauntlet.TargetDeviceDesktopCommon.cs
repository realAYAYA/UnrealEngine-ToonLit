// Copyright Epic Games, Inc. All Rights Reserved.

using AutomationTool;
using EpicGames.Core;
using System;
using System.Collections.Generic;
using System.IO;
using System.Text;
using System.Threading;
using System.Linq;
using UnrealBuildTool;
using System.Text.RegularExpressions;
using static AutomationTool.ProcessResult;

namespace Gauntlet
{
	public abstract class TargetDeviceDesktopCommon : ITargetDevice
	{
		#region ITargetDevice
		public string Name { get; protected set; }
		public UnrealTargetPlatform? Platform { get; protected set; }
		public CommandUtils.ERunOptions RunOptions { get; set; }
		public bool IsAvailable => true;
		public bool IsConnected => true;
		public bool IsOn => true;
		public bool Connect() => true;
		public bool Disconnect(bool bForce = false) => true;
		public bool PowerOn() => true;
		public bool PowerOff() => true;
		public bool Reboot() => true;
		public Dictionary<EIntendedBaseCopyDirectory, string> GetPlatformDirectoryMappings() => LocalDirectoryMappings;
		public override string ToString() => Name;
		#endregion

		#region IDisposable Support
		private bool DisposedValue = false; // To detect redundant calls

		protected virtual void Dispose(bool Disposing)
		{
			if (!DisposedValue)
			{
				if (Disposing)
				{
					// TODO: dispose managed state (managed objects).
				}

				// TODO: free unmanaged resources (unmanaged objects) and override a finalizer below.
				// TODO: set large fields to null.

				DisposedValue = true;
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

		public string LocalCachePath { get; protected set; }

		public string UserDir { get; protected set; }

		/// <summary>
		/// Optional directory staged builds are installed to if the requested build is not already on the same volume
		/// </summary>
		[AutoParam]
		public string InstallRoot { get; protected set; }

		// TODO - move this be part of ITargetDevice
		protected Dictionary<EIntendedBaseCopyDirectory, string> LocalDirectoryMappings { get; set; }

		public TargetDeviceDesktopCommon(string InName, string InCacheDir)
		{
			AutoParam.ApplyParamsAndDefaults(this, Globals.Params.AllArguments);

			Name = InName;
			LocalCachePath = InCacheDir;
			UserDir = Path.Combine(InCacheDir, "UserDir");

			if(string.IsNullOrEmpty(InstallRoot))
			{
				InstallRoot = InCacheDir;
			}

			LocalDirectoryMappings = new Dictionary<EIntendedBaseCopyDirectory, string>();
		}

		public virtual IAppInstall InstallApplication(UnrealAppConfig AppConfig)
		{
			switch (AppConfig.Build)
			{
				case NativeStagedBuild:
					return InstallNativeStagedBuild(AppConfig, AppConfig.Build as NativeStagedBuild);

				case StagedBuild:
					return InstallStagedBuild(AppConfig, AppConfig.Build as StagedBuild);

				case EditorBuild:
					return InstallEditorBuild(AppConfig, AppConfig.Build as EditorBuild);

				default:
					throw new AutomationException("{0} is an invalid build type!", AppConfig.Build.ToString());
			}
		}

		public void FullClean()
		{

		}

		public void CleanArtifacts()
		{

		}

		public void InstallBuild(UnrealAppConfig AppConfiguration)
		{

		}

		public IAppInstall CreateAppInstall(UnrealAppConfig AppConfig)
		{
			return null;
		}

		public void CopyAdditionalFiles(IEnumerable<UnrealFileToCopy> FilesToCopy)
		{
			if (FilesToCopy == null || FilesToCopy.Any())
			{
				return;
			}

			foreach (UnrealFileToCopy FileToCopy in FilesToCopy)
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
					Log.Warning(KnownLogEvents.Gauntlet_DeviceEvent, "File to copy {File} not found", FileToCopy);
				}
			}
		}

		public abstract IAppInstance Run(IAppInstall Install);

		public virtual void PopulateDirectoryMappings(string BaseDirectory)
		{
			LocalDirectoryMappings.Clear();

			LocalDirectoryMappings.Add(EIntendedBaseCopyDirectory.Build, Path.Combine(BaseDirectory, "Build"));
			LocalDirectoryMappings.Add(EIntendedBaseCopyDirectory.Binaries, Path.Combine(BaseDirectory, "Binaries"));
			LocalDirectoryMappings.Add(EIntendedBaseCopyDirectory.Config, Path.Combine(BaseDirectory, "Saved", "Config"));
            LocalDirectoryMappings.Add(EIntendedBaseCopyDirectory.Content, Path.Combine(BaseDirectory, "Content"));
            LocalDirectoryMappings.Add(EIntendedBaseCopyDirectory.Demos, Path.Combine(BaseDirectory, "Saved", "Demos"));
			LocalDirectoryMappings.Add(EIntendedBaseCopyDirectory.PersistentDownloadDir, Path.Combine(BaseDirectory, "Saved", "PersistentDownloadDir"));
			LocalDirectoryMappings.Add(EIntendedBaseCopyDirectory.Profiling, Path.Combine(BaseDirectory, "Saved", "Profiling"));
            LocalDirectoryMappings.Add(EIntendedBaseCopyDirectory.Saved, Path.Combine(BaseDirectory, "Saved"));
		}

		// TODO - b.lienau: implement these at desktop level and remove implementations from each desktop
		protected abstract IAppInstall InstallNativeStagedBuild(UnrealAppConfig AppConfig, NativeStagedBuild Build);
		protected abstract IAppInstall InstallStagedBuild(UnrealAppConfig AppConfig, StagedBuild Build);
		protected abstract IAppInstall InstallEditorBuild(UnrealAppConfig AppConfig, EditorBuild Build);
	}

	public abstract class DesktopCommonAppInstall<DesktopTargetDevice> : IAppInstall where DesktopTargetDevice : TargetDeviceDesktopCommon
	{
		public string Name { get; }

		public ITargetDevice Device => DesktopDevice;

		public DesktopTargetDevice DesktopDevice { get; protected set; }

		public string ArtifactPath;

		public string ExecutablePath;

		public string ProjectName;

		public string WorkingDirectory;

		public string LogFile;

		public bool CanAlterCommandArgs;

		public string CommandArguments
		{
			get { return CommandArgumentsPrivate; }
			set
			{
				if (CanAlterCommandArgs || string.IsNullOrEmpty(CommandArgumentsPrivate))
				{
					CommandArgumentsPrivate = value;
				}
				else
				{
					Log.Info("Skipped setting command AppInstall line when CanAlterCommandArgs = false");
				}
			}
		}

		private string CommandArgumentsPrivate;

		public void AppendCommandline(string AdditionalCommandline)
		{
			CommandArguments += AdditionalCommandline;
		}

		public CommandUtils.ERunOptions RunOptions { get; set; }

		public DesktopCommonAppInstall(string InName, string InProjectName, DesktopTargetDevice InDevice)
		{
			Name = InName;
			ProjectName = InProjectName;
			DesktopDevice = InDevice;
			CommandArguments = string.Empty;
			RunOptions = CommandUtils.ERunOptions.NoWaitForExit;
			CanAlterCommandArgs = true;
		}

		public virtual IAppInstance Run()
		{
			return Device.Run(this);
		}

		public virtual void CleanDeviceArtifacts()
		{
			// log file
			try
			{
				if (LogFile != null && File.Exists(LogFile))
				{
					EpicGames.Core.FileUtils.ForceDeleteFile(LogFile);
				}
			}
			catch (Exception Ex)
			{
				Log.Warning(KnownLogEvents.Gauntlet_DeviceEvent, "Unable to delete existing log file {File}. {Exception}", LogFile, Ex.Message);
			}
			// all other artifacts
			if (!string.IsNullOrEmpty(ArtifactPath) && Directory.Exists(ArtifactPath))
			{
				try
				{
					Log.Info("Clearing device artifacts path {0} for {1}", ArtifactPath, Device.Name);
					Directory.Delete(ArtifactPath, true);
				}
				catch (Exception Ex)
				{
					Log.Info(KnownLogEvents.Gauntlet_DeviceEvent, "First attempt at clearing artifact path {0} failed - trying again", ArtifactPath);
					if (!ForceCleanDeviceArtifacts())
					{
						Log.Warning(KnownLogEvents.Gauntlet_DeviceEvent, "Failed to delete {File}. {Exception}", ArtifactPath, Ex.Message);
					}
				}
			}
		}

		public virtual bool ForceCleanDeviceArtifacts()
		{
			DirectoryInfo ClientTempDirInfo = new DirectoryInfo(ArtifactPath) { Attributes = FileAttributes.Normal };
			Log.Info(KnownLogEvents.Gauntlet_DeviceEvent, "Setting files in device artifacts {0} to have normal attributes (no longer read-only).", ArtifactPath);
			foreach (FileSystemInfo info in ClientTempDirInfo.GetFileSystemInfos("*", SearchOption.AllDirectories))
			{
				info.Attributes = FileAttributes.Normal;
			}
			try
			{
				Log.Info(KnownLogEvents.Gauntlet_DeviceEvent, "Clearing device artifact path {0} (force)", ArtifactPath);
				Directory.Delete(ArtifactPath, true);
			}
			catch (Exception Ex)
			{
				Log.Warning(KnownLogEvents.Gauntlet_DeviceEvent, "Failed to force delete artifact path {File}. {Exception}", ArtifactPath, Ex.Message);
				return false;
			}
			return true;
		}

		public virtual void SetDefaultCommandLineArguments(UnrealAppConfig AppConfig, CommandUtils.ERunOptions InRunOptions, string BuildDir)
		{
			RunOptions = InRunOptions;
			if (Log.IsVeryVerbose)
			{
				RunOptions |= CommandUtils.ERunOptions.AllowSpew;
			}

			string UserDir = DesktopDevice.UserDir;
			// Set commandline replace any InstallPath arguments with the path we use
			CommandArguments = Regex.Replace(AppConfig.CommandLine, @"\$\(InstallPath\)", BuildDir, RegexOptions.IgnoreCase);
			CanAlterCommandArgs = AppConfig.CanAlterCommandArgs;

			if (CanAlterCommandArgs)
			{
				// Unreal userdir
				if (string.IsNullOrEmpty(UserDir) == false)
				{
					CommandArguments += $" -userdir=\"{UserDir}\"";
					ArtifactPath = Path.Combine(UserDir, "Saved");

					Utils.SystemHelpers.MarkDirectoryForCleanup(UserDir);
				}
				else
				{
					// e.g d:\Unreal\GameName\Saved
					ArtifactPath = Path.Combine(BuildDir, ProjectName, "Saved");
				}

				// Unreal abslog
				// Look in app parameters if abslog is specified, if so use it
				Regex LogRegex = new Regex(@"-abslog[:\s=](?:""([^""]*)""|([^-][^\s]*))?");
				Match M = LogRegex.Match(CommandArguments);
				if (M.Success)
				{
					LogFile = M.Groups[2].Value;
				}

				// Explicitly set log file when not already defined if not build machine
				// -abslog makes sure Unreal dynamically update the log window when using -log
				if (!CommandUtils.IsBuildMachine && (!string.IsNullOrEmpty(LogFile) || AppConfig.CommandLine.Contains("-log")))
				{
					string LogFolder = string.IsNullOrEmpty(LogFile) ? Path.Combine(ArtifactPath, "Logs") : Path.GetDirectoryName(LogFile);

					if (!Directory.Exists(LogFolder))
					{
						Directory.CreateDirectory(LogFolder);
					}

					if (string.IsNullOrEmpty(LogFile))
					{
						LogFile = Path.Combine(LogFolder, $"{ProjectName}.log");
						CommandArguments += string.Format(" -abslog=\"{0}\"", LogFile);
					}

					RunOptions |= CommandUtils.ERunOptions.NoStdOutRedirect;
				}

				// clear artifact path
				CleanDeviceArtifacts();
			}
		}
	}

	public abstract class DesktopCommonAppInstance<DesktopAppInstall, DesktopTargetDevice> : LocalAppProcess
		where DesktopAppInstall : DesktopCommonAppInstall<DesktopTargetDevice>
		where DesktopTargetDevice : TargetDeviceDesktopCommon
	{
		public override string ArtifactPath => Install.ArtifactPath;

		public override ITargetDevice Device => Install.Device;

		protected DesktopAppInstall Install;

		public DesktopCommonAppInstance(DesktopAppInstall InInstall, IProcessResult InProcess, string InProcessLogFile = null)
			: base(InProcess, InInstall.CommandArguments, InProcessLogFile)
		{
			Install = InInstall;
		}
	}

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
				new Thread(LogFileReaderThread).Start();
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
				ProcessResult.ProcessObject.Kill(true);
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

						if (!string.IsNullOrEmpty(Output))
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
}