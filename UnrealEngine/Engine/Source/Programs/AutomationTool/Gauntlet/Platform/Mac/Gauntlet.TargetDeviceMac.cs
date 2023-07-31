// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.IO;
using AutomationTool;
using UnrealBuildTool;
using System.Text.RegularExpressions;
using EpicGames.Core;
using UnrealBuildBase;

namespace Gauntlet
{
	
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

	class MacAppInstall : IAppInstall
	{
		public string Name { get; private set; }

		public string LocalPath;

		public string WorkingDirectory;

		public string ExecutablePath;

		public string CommandArguments;

		public string ArtifactPath;

		public ITargetDevice Device { get; protected set; }

		public CommandUtils.ERunOptions RunOptions { get; set; }

		public MacAppInstall(string InName, TargetDeviceMac InDevice)
		{
			Name = InName;
			Device = InDevice;
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

	class MacAppInstance : LocalAppProcess
	{
		protected MacAppInstall Install;

		public MacAppInstance(MacAppInstall InInstall, IProcessResult InProcess)
			: base(InProcess, InInstall.CommandArguments)
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

	public class TargetDeviceMac : ITargetDevice
	{
		public string Name { get; protected set; }

		public UnrealTargetPlatform? Platform { get { return UnrealTargetPlatform.Mac; } }

		public bool IsAvailable { get { return true; } }
		public bool IsConnected { get { return true; } }
		public bool IsOn { get { return true; } }
		public bool PowerOn() { return true; }
		public bool PowerOff() { return true; }
		public bool Reboot() { return true; }
		public bool Connect() { return true; }
		public bool Disconnect(bool bForce = false) { return true; }

		protected string UserDir { get; set; }

		protected string LocalCachePath { get; set; }

		public CommandUtils.ERunOptions RunOptions { get; set; }

		protected Dictionary<EIntendedBaseCopyDirectory, string> LocalDirectoryMappings { get; set; }

		public TargetDeviceMac(string InName, string InCachePath)
		{
			Name = InName;
			LocalCachePath = InCachePath;
			UserDir = Path.Combine(LocalCachePath, "UserDir");
			this.RunOptions = CommandUtils.ERunOptions.NoWaitForExit;
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

		public override string ToString()
		{
			return Name;
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

		public void PopulateDirectoryMappings(string ProjectDir)
		{
			LocalDirectoryMappings.Add(EIntendedBaseCopyDirectory.Build, Path.Combine(ProjectDir, "Build"));
			LocalDirectoryMappings.Add(EIntendedBaseCopyDirectory.Binaries, Path.Combine(ProjectDir, "Binaries"));
			LocalDirectoryMappings.Add(EIntendedBaseCopyDirectory.Config, Path.Combine(ProjectDir, "Saved", "Config"));
			LocalDirectoryMappings.Add(EIntendedBaseCopyDirectory.Content, Path.Combine(ProjectDir, "Content"));
			LocalDirectoryMappings.Add(EIntendedBaseCopyDirectory.Demos, Path.Combine(ProjectDir, "Saved", "Demos"));
			LocalDirectoryMappings.Add(EIntendedBaseCopyDirectory.PersistentDownloadDir, Path.Combine(ProjectDir, "Saved", "PersistentDownloadDir"));
			LocalDirectoryMappings.Add(EIntendedBaseCopyDirectory.Profiling, Path.Combine(ProjectDir, "Saved", "Profiling"));
			LocalDirectoryMappings.Add(EIntendedBaseCopyDirectory.Saved, Path.Combine(ProjectDir, "Saved"));
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
			bool SkipDeploy = Globals.Params.ParseParam("SkipDeploy");

			string OutBuildPath = InBuildPath;

			string BuildVolume = GetVolumeName(OutBuildPath);
			string LocalRoot = GetVolumeName(Environment.CurrentDirectory);

			// Must be on our volume to run
			if (BuildVolume.Equals(LocalRoot, StringComparison.OrdinalIgnoreCase) == false)
			{
				string SubDir = string.IsNullOrEmpty(AppConfig.Sandbox) ? AppConfig.ProjectName : AppConfig.Sandbox;
				string DestPath = Path.Combine(this.LocalCachePath, SubDir, AppConfig.ProcessType.ToString());

				if (!SkipDeploy)
				{
					Utils.SystemHelpers.CopyDirectory(InBuildPath, DestPath, Utils.SystemHelpers.CopyOptions.Mirror);
				}

				else
				{
					Log.Info("Skipping install of {0} (-skipdeploy)", InBuildPath);
				}

				Utils.SystemHelpers.MarkDirectoryForCleanup(DestPath);

				OutBuildPath = DestPath;
			}

			return OutBuildPath;
		}

		protected IAppInstall InstallBuild(UnrealAppConfig AppConfig, IBuild InBuild)
		{
			// Full path to the build
			string BuildPath;

			//  Full path to the build.app to use. This will be under BuildPath for staged builds, and the build path itself for packaged builds
			string BundlePath;

			if (InBuild is StagedBuild)
			{
				StagedBuild InStagedBuild = InBuild as StagedBuild;
				BuildPath = CopyBuildIfNecessary(AppConfig, InStagedBuild.BuildPath);
				BundlePath = Path.Combine(BuildPath, InStagedBuild.ExecutablePath);
			}
			else
			{
				MacPackagedBuild InPackagedBuild = InBuild as MacPackagedBuild;
				BuildPath = CopyBuildIfNecessary(AppConfig, InPackagedBuild.BuildPath);
				BundlePath = BuildPath;
			}

			MacAppInstall MacApp = new MacAppInstall(AppConfig.Name, this);
			MacApp.LocalPath = BuildPath;
			MacApp.WorkingDirectory = MacApp.LocalPath;
			MacApp.RunOptions = RunOptions;

			MacApp.ExecutablePath = GetExecutableIfBundle(BundlePath);

			// Set commandline replace any InstallPath arguments with the path we use
			MacApp.CommandArguments = Regex.Replace(AppConfig.CommandLine, @"\$\(InstallPath\)", BuildPath, RegexOptions.IgnoreCase);

			// Mac always forces this to stop logs and other artifacts going to different places
			// Mac always forces this to stop logs and other artifacts going to different places
			MacApp.CommandArguments += string.Format(" -userdir=\"{0}\"", UserDir);
			MacApp.ArtifactPath = Path.Combine(UserDir, @"Saved");

			if (LocalDirectoryMappings.Count == 0)
			{
				PopulateDirectoryMappings(BuildPath);
			}

			// clear artifact path
			MacApp.CleanDeviceArtifacts();
			
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
				DirectoryReference  WorkingDir = Props.bIsCodeBasedProject ? AppConfig.ProjectFile.Directory : Unreal.EngineDirectory;

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


		public IAppInstall InstallApplication(UnrealAppConfig AppConfig)
		{
			if (AppConfig.Build is StagedBuild || AppConfig.Build is MacPackagedBuild)
			{
				return InstallBuild(AppConfig, AppConfig.Build);
			}

			EditorBuild EditorBuild = AppConfig.Build as EditorBuild;

			if (EditorBuild == null)
			{
				throw new AutomationException("Invalid build type!");
			}

			MacAppInstall MacApp = new MacAppInstall(AppConfig.Name, this);

			MacApp.WorkingDirectory = Path.GetFullPath(EditorBuild.ExecutablePath);
			MacApp.CommandArguments = AppConfig.CommandLine;
			MacApp.RunOptions = RunOptions;

			// Mac always forces this to stop logs and other artifacts going to different places
			MacApp.CommandArguments += string.Format(" -userdir=\"{0}\"", UserDir);
			MacApp.ArtifactPath = Path.Combine(UserDir, @"Saved");

			// now turn the Foo.app into Foo/Content/MacOS/Foo
			string AppPath = Path.GetDirectoryName(EditorBuild.ExecutablePath);

			MacApp.ExecutablePath = GetExecutableIfBundle(EditorBuild.ExecutablePath);

			if (LocalDirectoryMappings.Count == 0)
			{
				PopulateDirectoryMappings(AppPath);
			}
			return MacApp;
		}

		public IAppInstance Run(IAppInstall App)
		{
			MacAppInstall MacInstall = App as MacAppInstall;

			if (MacInstall == null)
			{
				throw new AutomationException("Invalid install type!");
			}

			IProcessResult Result = null;

			lock (Globals.MainLock)
			{
				string NewWorkingDir = string.IsNullOrEmpty(MacInstall.WorkingDirectory) ? MacInstall.LocalPath : MacInstall.WorkingDirectory;
				string OldWD = Environment.CurrentDirectory;
				Environment.CurrentDirectory = NewWorkingDir;

				Log.Info("Launching {0} on {1}", App.Name, ToString());
				Log.Verbose("\t{0}", MacInstall.CommandArguments);

				Result = CommandUtils.Run(MacInstall.ExecutablePath, MacInstall.CommandArguments, Options: MacInstall.RunOptions);

				if (Result.HasExited && Result.ExitCode != 0)
				{
					throw new AutomationException("Failed to launch {0}. Error {1}", MacInstall.ExecutablePath, Result.ExitCode);
				}

				Environment.CurrentDirectory = OldWD;
			}

			return new MacAppInstance(MacInstall, Result);
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
