// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.IO;
using AutomationTool;
using UnrealBuildTool;
using System.Text.RegularExpressions;
using System.Linq;
using System.Text;
using System.Threading;
using EpicGames.Core;
using System.Diagnostics;

namespace Gauntlet
{
	// device data from json
	public sealed class AndroidDeviceData
	{
		// remote device settings (wifi)

		// host of PC which is tethered 
		public string hostIP { get; set; }

		// public key 
		public string publicKey { get; set; }

		// private key
		public string privateKey { get; set; }
	}

	// become IAppInstance when implemented enough
	class AndroidAppInstance : IAppInstance
	{
		protected TargetDeviceAndroid AndroidDevice;

		protected AndroidAppInstall Install;

		internal IProcessResult LaunchProcess;

		internal bool bHaveSavedArtifacts;

		public string CommandLine { get { return Install.CommandLine; } }

		public AndroidAppInstance(TargetDeviceAndroid InDevice, AndroidAppInstall InInstall, IProcessResult InProcess)
		{
			AndroidDevice = InDevice;
			Install = InInstall;
			LaunchProcess = InProcess;
		}

		public string ArtifactPath
		{
			get
			{
				if (HasExited)
				{
					SaveArtifacts();
				}
				
				return Path.Combine(AndroidDevice.LocalCachePath, "Saved");
			}
		}

		public ITargetDevice Device
		{
			get
			{
				return AndroidDevice;
			}
		}

		public bool HasExited
		{
			get
			{
				try
				{
					if (!LaunchProcess.HasExited)
					{
						return false;
					}
				}
				catch (System.InvalidOperationException)
				{
					return true;
				}

				return IsActivityRunning();
			}
		}

		/// <summary>
		/// Checks on device whether the activity is running, this is an expensive shell with output operation
		/// the result is cached, with checks at ActivityCheckDelta seconds
		/// </summary>
		private bool IsActivityRunning()
		{
			if (ActivityExited)
			{
				return true;
			}

			if ((DateTime.UtcNow - ActivityCheckTime) < ActivityCheckDelta)
			{
				return false;
			}

			if(AndroidDevice.Disposed)
			{
				return false;
			}

			ActivityCheckTime = DateTime.UtcNow;

			// get activities filtered by our package name
			IProcessResult ActivityQuery = AndroidDevice.RunAdbDeviceCommand("shell dumpsys activity -p " + Install.AndroidPackageName + " a");

			// We have exited if our activity doesn't appear in the activity query or is not the focused activity.
			bool bActivityPresent = ActivityQuery.Output.Contains(Install.AndroidPackageName);
			bool bActivityInForeground = ActivityQuery.Output.Contains("ResumedActivity");
			bool bHasExited = !bActivityPresent || !bActivityInForeground;
			if (bHasExited)
			{
				ActivityExited = true;
				// The activity has exited, make sure entire activity log has been captured, sleep to allow time for the log to flush
				Thread.Sleep(5000);
				UpdateCachedLog(true);
				Log.VeryVerbose("{0}: process exited, Activity running={1}, Activity in foreground={2} ", ToString(), bActivityPresent.ToString(), bActivityInForeground.ToString());
			}

			return bHasExited;

		}

		private static readonly TimeSpan ActivityCheckDelta = TimeSpan.FromSeconds(10);
		private DateTime ActivityCheckTime = DateTime.UtcNow;
		private bool ActivityExited = false;

		public bool WasKilled { get; protected set; }

		/// <summary>
		/// The output of the test activity
		/// </summary>
		public string StdOut
		{
			get
			{
				UpdateCachedLog();
				return String.IsNullOrEmpty(ActivityLogCached) ? String.Empty : ActivityLogCached;
			}
		}
		/// <summary>
		/// Updates cached activity log by running a shell command returning the full log from device (possibly over wifi)
		/// The result is cached and updated at ActivityLogDelta frequency
		/// </summary>
		private void UpdateCachedLog(bool ForceUpdate = false)
		{
			if (!ForceUpdate && (ActivityLogTime == DateTime.MinValue || ((DateTime.UtcNow - ActivityLogTime) < ActivityLogDelta)))
			{
				return;
			}

			if (Install.AndroidDevice != null && Install.AndroidDevice.Disposed)
			{
				Log.Warning(KnownLogEvents.Gauntlet_DeviceEvent, "Attempting to cache log using disposed Android device");
				return;
			}

			// Note: DeviceLogPath has the correct location for the current app. Apps can be configured to override the default log storage path. 
			string GetLogCommand = string.Format("shell cat {0}", Install.AndroidDevice.DeviceLogPath);
			IProcessResult LogQuery = Install.AndroidDevice.RunAdbDeviceCommand(GetLogCommand, true);

			if (LogQuery.ExitCode != 0)
			{
				Log.VeryVerbose("Unable to query activity stdout on device {0}", Install.AndroidDevice.Name);
			}
			else
			{
				ActivityLogCached = LogQuery.Output;
			}

			ActivityLogTime = DateTime.UtcNow;

			// the activity has exited, mark final log sentinel 
			if (ActivityExited)
			{
				ActivityLogTime = DateTime.MinValue;
			}

		}

		private static readonly TimeSpan ActivityLogDelta = TimeSpan.FromSeconds(15);
		private DateTime ActivityLogTime = DateTime.UtcNow - ActivityLogDelta;
		private string ActivityLogCached = string.Empty;
		

		public int WaitForExit()
		{
			if (!HasExited)
			{
				LaunchProcess.WaitForExit();
			}

			return ExitCode;
		}

		public void Kill()
		{
			if (!HasExited && !AndroidDevice.Disposed)
			{
				WasKilled = true;
				Install.AndroidDevice.KillRunningProcess(Install.AndroidPackageName);
			}
		}
		public int ExitCode { get { return LaunchProcess.ExitCode; } }

		protected void SaveArtifacts()
		{
			if (bHaveSavedArtifacts)
				return;

			// copy remote artifacts to local
			if (Directory.Exists(Install.AndroidDevice.LocalCachePath))
			{

				Log.Verbose("Deleting {0}", Install.AndroidDevice.LocalCachePath);

				try
				{
					// don't consider this fatal, people often have the directory or a file open
					Directory.Delete(Install.AndroidDevice.LocalCachePath, true);
				}
				catch
				{
					Log.Warning(KnownLogEvents.Gauntlet_DeviceEvent, "Failed to remove old cache folder {Folder}", Install.AndroidDevice.LocalCachePath);
				}
			}

			// mark it as a temp dir (will also create it)
			try
			{
				Utils.SystemHelpers.MarkDirectoryForCleanup(Install.AndroidDevice.LocalCachePath);
			}
			catch (Exception Ex)
			{
				Log.Warning(KnownLogEvents.Gauntlet_DeviceEvent, "Exception marking directory for cleanup {Exception}", Ex.Message);
			}			

			string LocalSaved = Path.Combine(Install.AndroidDevice.LocalCachePath, "Saved");
			Log.Verbose("Creating {0}", LocalSaved);
			Directory.CreateDirectory(LocalSaved);


			


			// pull all the artifacts
			string ArtifactPullCommand = string.Format("pull {0} \"{1}\"", Install.AndroidDevice.DeviceArtifactPath, Install.AndroidDevice.LocalCachePath);
			IProcessResult PullCmd = Install.AndroidDevice.RunAdbDeviceCommand(ArtifactPullCommand, bShouldLogCommand: Log.IsVerbose);

			if (PullCmd.ExitCode != 0)
			{
				Log.Warning(KnownLogEvents.Gauntlet_DeviceEvent, "Failed to retrieve artifacts. {Output}", PullCmd.Output);
			}
			else
			{
				// update final cached stdout property
				string LogFilename = Install.AndroidDevice.DeviceLogPath;
				if (File.Exists(LogFilename))
				{
					ActivityLogCached = File.ReadAllText(LogFilename);
					ActivityLogTime = DateTime.MinValue;
				}

			}

			// pull the logcat over from device.
			IProcessResult LogcatResult = Install.AndroidDevice.RunAdbDeviceCommand("logcat -d", bShouldLogCommand: Log.IsVerbose);

			string LogcatFilename = "Logcat.log";
			// Save logcat dump to local artifact path.
			File.WriteAllText(Path.Combine(LocalSaved, LogcatFilename), LogcatResult.Output);

			Install.AndroidDevice.PostRunCleanup();

			bHaveSavedArtifacts = true;
		}
	}

	class AndroidAppInstall : IAppInstall
	{
		public string Name { get; protected set; }

		public string AndroidPackageName { get; protected set; }

		public TargetDeviceAndroid AndroidDevice { get; protected set; }

		public ITargetDevice Device { get { return AndroidDevice; } }

		public string CommandLine { get; protected set; }

		public IAppInstance Run()
		{
			return AndroidDevice.Run(this);
		}

		public AndroidAppInstall(TargetDeviceAndroid InDevice, string InName, string InAndroidPackageName, string InCommandLine)
		{
			AndroidDevice = InDevice;
			Name = InName;
			AndroidPackageName = InAndroidPackageName;
			CommandLine = InCommandLine;
		}
	}

	public class DefaultAndroidDevices : IDefaultDeviceSource
	{
		public bool CanSupportPlatform(UnrealTargetPlatform? Platform)
		{
			return Platform == UnrealTargetPlatform.Android;
		}

		public ITargetDevice[] GetDefaultDevices()
		{
			return TargetDeviceAndroid.GetDefaultDevices();
		}
	}

	public class AndroidBuildSupport : BaseBuildSupport
	{
		protected override BuildFlags SupportedBuildTypes => BuildFlags.Packaged | BuildFlags.CanReplaceCommandLine | BuildFlags.CanReplaceExecutable | BuildFlags.Bulk | BuildFlags.NotBulk;
		protected override UnrealTargetPlatform? Platform => UnrealTargetPlatform.Android;
	}

	public class AndroidDeviceFactory : IDeviceFactory
	{
		public bool CanSupportPlatform(UnrealTargetPlatform? Platform)
		{
			return Platform == UnrealTargetPlatform.Android;
		}

		public ITargetDevice CreateDevice(string InRef, string InCachePath, string InParam = null)
		{
			AndroidDeviceData DeviceData = null;

			if (!String.IsNullOrEmpty(InParam))
			{
				DeviceData = fastJSON.JSON.Instance.ToObject<AndroidDeviceData>(InParam);
			}

			return new TargetDeviceAndroid(InRef, DeviceData, InCachePath);
		}
	}

	/// <summary>
	/// Android implementation of a device that can run applications
	/// </summary>
	public class TargetDeviceAndroid : ITargetDevice
	{ 
		/// <summary>
		/// Friendly name for this target
		/// </summary>
		public string Name { get; protected set; }
	
		/// <summary>
		/// Low-level device name
		/// </summary>
		public string DeviceName { get; protected set; }

		/// <summary>
		/// Platform type.
		/// </summary>
		public UnrealTargetPlatform? Platform { get { return UnrealTargetPlatform.Android; } }

		/// <summary>
		/// Options for executing commands
		/// </summary>
		public CommandUtils.ERunOptions RunOptions { get; set; }

		/// <summary>
		/// Temp path we use to push/pull things from the device
		/// </summary>
		public string LocalCachePath { get; protected set; }

		/// <summary>
		/// Artifact (e.g. Saved) path on the device
		/// </summary>
		public string DeviceArtifactPath { get; protected set;  }

		/// <summary>
		/// External storage path on the device, e.g. /sdcard/UEGame/ etc..
		/// </summary>
		public string DeviceExternalStorageSavedPath { get; protected set; }

		/// <summary>
		/// External files path on the device, this is the app's own publically accessible data directory e.g. /sdcard/Android/data/[com.package.name]/files/UnrealGame etc..
		/// </summary>
		public string DeviceExternalFilesSavedPath { get; protected set; }

		/// <summary>
		/// Path to the log file. (This takes public logs setting of the package in to account.)
		/// </summary>
		public string DeviceLogPath { get; protected set; }		

		/// <summary>
		/// Path to a command line if installed
		/// </summary>
		protected string CommandLineFilePath { get; set; }

		public bool IsAvailable
		{
			get
			{
                // ensure our device is present in 'adb devices' output.
				var AllDevices = GetAllConnectedDevices();

				if (AllDevices.Keys.Contains(DeviceName) == false)
				{
					return false;
				}

				if (AllDevices[DeviceName] != "device")
				{
					Log.Warning(KnownLogEvents.Gauntlet_DeviceEvent, "Device {Name} is '{State}'", DeviceName, AllDevices[DeviceName]);
					return false;
				}

				// any device will do, but only one at a time.
				return true;
            }
		}

		protected Dictionary<EIntendedBaseCopyDirectory, string> LocalDirectoryMappings { get; set; }
        void SetUpDirectoryMappings()
        {
            LocalDirectoryMappings = new Dictionary<EIntendedBaseCopyDirectory, string>();
        }

        public void PopulateDirectoryMappings(string ProjectDir)
		{
			LocalDirectoryMappings.Add(EIntendedBaseCopyDirectory.Build, Path.Combine(ProjectDir, "Build"));
			LocalDirectoryMappings.Add(EIntendedBaseCopyDirectory.Binaries, Path.Combine(ProjectDir, "Binaries"));
			LocalDirectoryMappings.Add(EIntendedBaseCopyDirectory.Config, Path.Combine(ProjectDir, "Config"));
            LocalDirectoryMappings.Add(EIntendedBaseCopyDirectory.Content, Path.Combine(ProjectDir, "Content"));
            LocalDirectoryMappings.Add(EIntendedBaseCopyDirectory.Demos, Path.Combine(ProjectDir, "Demos"));
			LocalDirectoryMappings.Add(EIntendedBaseCopyDirectory.PersistentDownloadDir, Path.Combine(ProjectDir, "Saved", "PersistentDownloadDir"));
			LocalDirectoryMappings.Add(EIntendedBaseCopyDirectory.Profiling, Path.Combine(ProjectDir, "Profiling"));
            LocalDirectoryMappings.Add(EIntendedBaseCopyDirectory.Saved, ProjectDir);
        }
        public bool IsConnected { get	{ return IsAvailable; }	}

		protected bool IsExistingDevice = false;

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="InDeviceName"></param>
		/// <param name="DeviceData"></param>
		/// <param name="InCachePath"></param>
		public TargetDeviceAndroid(string InDeviceName = "", AndroidDeviceData DeviceData = null, string InCachePath = null)
		{
			DeviceName = InDeviceName;

			AdbCredentialCache.AddInstance(DeviceData);

			// If no device name or its 'default' then use the first default device
			if (string.IsNullOrEmpty(DeviceName) || DeviceName.Equals("default", StringComparison.OrdinalIgnoreCase))
			{
				var DefaultDevices = GetAllAvailableDevices();	

				if (DefaultDevices.Count() == 0)
				{
					if (GetAllConnectedDevices().Count > 0)
					{
						throw new AutomationException("No default device available. One or more devices are connected but unauthorized or offline. See 'adb devices'");
					}
					else
					{
						throw new AutomationException("No default device available. See 'adb devices'");
					}
				}

				DeviceName = DefaultDevices.First();
			}

			if (Log.IsVerbose)
			{
				RunOptions = CommandUtils.ERunOptions.NoWaitForExit;
			}
			else
			{
				RunOptions = CommandUtils.ERunOptions.NoWaitForExit | CommandUtils.ERunOptions.NoLoggingOfRunCommand;
			}

			// if this is not a connected device then remove when done
			var ConnectedDevices = GetAllConnectedDevices();

			IsExistingDevice = ConnectedDevices.Keys.Contains(DeviceName);

			if (!IsExistingDevice)
			{
				// adb uses 5555 by default
				if (DeviceName.Contains(":") == false)
				{
					DeviceName = DeviceName + ":5555";
				}

				lock (Globals.MainLock)
				{
					using (var PauseEC = new ScopedSuspendECErrorParsing())
					{
						IProcessResult AdbResult = RunAdbGlobalCommand(string.Format("connect {0}", DeviceName));

						if (AdbResult.ExitCode != 0)
						{
							throw new AutomationException("adb failed to connect to {0}. {1}", DeviceName, AdbResult.Output);
						}
					}

					Log.Info("Connected to {0}", DeviceName);

					// Need to sleep for adb service process to register, otherwise get an unauthorized (especially on parallel device use)
					Thread.Sleep(5000);
				}
			}

			LocalDirectoryMappings = new Dictionary<EIntendedBaseCopyDirectory, string>();

			// for IP devices need to sanitize this
			Name = DeviceName.Replace(":", "_");

			// Path we use for artifacts, we'll create it later when we need it
			LocalCachePath = InCachePath ?? Path.Combine(Globals.TempDir, "AndroidDevice_" + Name);

			ConnectedDevices = GetAllConnectedDevices();

            SetUpDirectoryMappings();

            // sanity check that it was now dound
            if (ConnectedDevices.Keys.Contains(DeviceName) == false)
			{
				throw new AutomationException("Failed to find new device {0} in connection list", DeviceName);
			}

			if (ConnectedDevices[DeviceName] != "device")
			{
				Dispose();
				throw new AutomationException("Device {0} is '{1}'.", DeviceName, ConnectedDevices[DeviceName]);
			}
		}

		~TargetDeviceAndroid()
		{
			Dispose(false);
		}

		#region IDisposable Support
		private bool disposedValue = false; // To detect redundant calls

		protected virtual void Dispose(bool disposing)
		{
			if (!disposedValue)
			{
				try
				{
					if (!IsExistingDevice)
					{
						// disconnect
						RunAdbGlobalCommand(string.Format("disconnect {0}", DeviceName), true, false, true);

						Log.Info("Disconnected {0}", DeviceName);
					}					
				}
				catch (Exception Ex)
				{
					Log.Warning(KnownLogEvents.Gauntlet_DeviceEvent, "TargetDeviceAndroid.Dispose() threw: {Exception}", Ex.Message);
				}
				finally
				{
					disposedValue = true;
					AdbCredentialCache.RemoveInstance();
				}

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

		public bool Disposed
		{
			get
			{
				return disposedValue;
			}
			
		}

		#endregion

		/// <summary>
		/// Returns a list of locally connected devices (e.g. 'adb devices'). 
		/// </summary>
		/// <returns></returns>
		static private Dictionary<string, string> GetAllConnectedDevices()
		{
           var Result = RunAdbGlobalCommand("devices");

            MatchCollection DeviceMatches = Regex.Matches(Result.Output, @"^([\d\w\.\:\-]{6,32})\s+(\w+)", RegexOptions.Multiline);

            var DeviceList = DeviceMatches.Cast<Match>().ToDictionary(
                M => M.Groups[1].ToString(),
                M => M.Groups[2].ToString().ToLower()
            );

            return DeviceList;
		}

		static private IEnumerable<string> GetAllAvailableDevices()
		{
			var AllDevices = GetAllConnectedDevices();
			return AllDevices.Keys.Where(D => AllDevices[D] == "device");
		}

		static public ITargetDevice[] GetDefaultDevices()
		{
			var Result = RunAdbGlobalCommand("devices");

			MatchCollection DeviceMatches = Regex.Matches(Result.Output, @"([\d\w\.\:]{8,32})\s+device");

			List<ITargetDevice> Devices = new List<ITargetDevice>();

			foreach (string Device in GetAllAvailableDevices())
			{
				ITargetDevice NewDevice = new TargetDeviceAndroid(Device);
				Devices.Add(NewDevice);
			}

			return Devices.ToArray();
		}

		internal void PostRunCleanup()
		{
			// Delete the commandline file, if someone installs an APK on top of ours
			// they will get very confusing behavior...
			if (string.IsNullOrEmpty(CommandLineFilePath) == false)
			{
				Log.Verbose("Removing {0}", CommandLineFilePath);
				DeleteFileFromDevice(CommandLineFilePath);
				CommandLineFilePath = null;
			}
		}

		public bool IsOn
		{
			get
			{
				string CommandLine = "shell dumpsys power";
				IProcessResult OnAndUnlockedQuery = RunAdbDeviceCommand(CommandLine, bPauseErrorParsing: true);
				return OnAndUnlockedQuery.Output.Contains("mHoldingDisplaySuspendBlocker=true")
					&& OnAndUnlockedQuery.Output.Contains("mHoldingWakeLockSuspendBlocker=true");
			}
		}

		public bool PowerOn()
		{
			Log.Verbose("{0}: Powering on", ToString());
			string CommandLine = "shell \"input keyevent KEYCODE_WAKEUP && input keyevent KEYCODE_MENU\"";
			IProcessResult PowerOnQuery = RunAdbDeviceCommand(CommandLine, bPauseErrorParsing: true);
			return !PowerOnQuery.Output.Contains("error");
		}
		public bool PowerOff()
		{
			Log.Verbose("{0}: Powering off", ToString());
			string CommandLine = "shell \"input keyevent KEYCODE_SLEEP\"";
			IProcessResult PowerOffQuery = RunAdbDeviceCommand(CommandLine, bPauseErrorParsing: true);
			return !PowerOffQuery.Output.Contains("error");
		}

		public bool Reboot()
		{
			return true;
		}

		public bool Connect()
		{
			AllowDeviceSleepState(true);
			return true;
		}

		public bool Disconnect(bool bForce=false)
		{
			AllowDeviceSleepState(false);
			return true;
		}

		public override string ToString()
		{
            // TODO: device id
			if (Name == DeviceName)
			{
				return Name;
			}
			return string.Format("{0} ({1})", Name, DeviceName);
		}

		protected bool DeleteFileFromDevice(string DestPath)
		{
			var AdbResult = RunAdbDeviceCommand(string.Format("shell rm -f {0}", DestPath));
			return AdbResult.ExitCode == 0;
		}

		public bool CopyFileToDevice(string PackageName, string SourcePath, string DestPath, bool IgnoreDependencies = false)
		{
			bool IsAPK = string.Equals(Path.GetExtension(SourcePath), ".apk", StringComparison.OrdinalIgnoreCase);

            // for the APK there's no easy/reliable way to get the date of the version installed, so 
            // we write this out to a dependency file in the demote dir and check it each time.
            // current file time
            DateTime LocalModifiedTime = File.GetLastWriteTime(SourcePath);

            string QuotedSourcePath = SourcePath;
            if (SourcePath.Contains(" "))
            {
                QuotedSourcePath = '"' + SourcePath + '"';
            }

            // dependency info is a hash of the destination name, saved under a folder on /sdcard
            string DestHash = ContentHash.MD5(DestPath).ToString();
			string DependencyCacheDir = "/sdcard/gdeps";
			string DepFile = string.Format("{0}/{1}", DependencyCacheDir, DestHash);

			IProcessResult AdbResult = null;

	
			// get info from the device about this file
			string CurrentFileInfo = null;

			if (IsAPK)
			{
				// for APK query the package info and get the update time
				AdbResult = RunAdbDeviceCommand(string.Format("shell dumpsys package {0} | grep lastUpdateTime", PackageName));

				if (AdbResult.ExitCode == 0)
				{
					CurrentFileInfo = AdbResult.Output.ToString().Trim();
				}
			}
			else
			{
				// for other files get the file info
				AdbResult = RunAdbDeviceCommand(string.Format("shell ls -l {0}", DestPath));

				if (AdbResult.ExitCode == 0)
				{
					CurrentFileInfo = AdbResult.Output.ToString().Trim();
				}
			}

			bool SkipInstall = false;

			// If this is valid then there is some form of that file on the device, now figure out if it matches the 
			if (string.IsNullOrEmpty(CurrentFileInfo) == false)
			{
				// read the dep file
				AdbResult = RunAdbDeviceCommand(string.Format("shell cat {0}", DepFile));

				if (AdbResult.ExitCode == 0)
				{
					// Dependency info is the modified time of the source, and the post-copy file stats of the installed file, separated by ###
					string[] DepLines = AdbResult.Output.ToString().Split(new[] { "###" }, StringSplitOptions.RemoveEmptyEntries).Select(S => S.Trim()).ToArray();

					if (DepLines.Length >= 2)
					{
						string InstalledSourceModifiedTime = DepLines[0];
						string InstalledFileInfo = DepLines[1];

						if (InstalledSourceModifiedTime == LocalModifiedTime.ToString()
							&& CurrentFileInfo == InstalledFileInfo)
						{
							SkipInstall = true;
						}
					}
				}
			}

			if (SkipInstall && IgnoreDependencies == false)
			{
				Log.Info("Skipping install of {0} - remote file up to date", Path.GetFileName(SourcePath));
			}
			else
			{
				if (IsAPK)
				{
					// we need to ununstall then install the apk - don't care if it fails, may have been deleted
					string AdbCommand = string.Format("uninstall {0}", PackageName);
					AdbResult = RunAdbDeviceCommand(AdbCommand);

					Log.Info("Installing {0} to {1}", SourcePath, Name);

					AdbCommand = string.Format("install {0}", QuotedSourcePath);
					AdbResult = RunAdbDeviceCommand(AdbCommand);

					if (AdbResult.ExitCode != 0)
					{
						throw new AutomationException("Failed to install {0}. Error {1}", SourcePath, AdbResult.Output);
					}

					// for APK query the package info and get the update time
					AdbResult = RunAdbDeviceCommand(string.Format("shell dumpsys package {0} | grep lastUpdateTime", PackageName));
					CurrentFileInfo = AdbResult.Output.ToString().Trim();
				}
				else
				{
					Log.Info("Copying {0} to {1} via adb push", QuotedSourcePath, DestPath);
					string AdbCommand = string.Format("push {0} {1}", QuotedSourcePath, DestPath);
					AdbResult = RunAdbDeviceCommand(AdbCommand);

					if (AdbResult.ExitCode != 0)
					{
						if ((AdbResult.Output.Contains("couldn't read from device") || AdbResult.Output.Contains("offline")) && !IsConnected)
						{
							Log.Info("Lost connection with device '{Name}'.", Name);
							// Disconnection occurred. Let's retry a second time before given up
							// Try to reconnect
							if (Connect())
							{
								Log.Info("Retrying to copy via adb push...");
								AdbResult = RunAdbDeviceCommand(AdbCommand);
								if (AdbResult.ExitCode != 0)
								{
									throw new AutomationException("Failed to push {0} to device. Error {1}", SourcePath, AdbResult.Output);
								}
							}
							else
							{
								throw new AutomationException("Failed to reconnect {0}", Name);
							}
						}
						else
						{
							throw new AutomationException("Failed to push {0} to device. Error {1}", SourcePath, AdbResult.Output);
						}
					}

					// Now pull info about the file which we'll write as a dep
					AdbResult = RunAdbDeviceCommand(string.Format("shell ls -l {0}", DestPath));
					CurrentFileInfo = AdbResult.Output.ToString().Trim();
				}

				// write the actual dependency info
				string DepContents = LocalModifiedTime + "###" + CurrentFileInfo;

				// save last modified time to remote deps after success
				RunAdbDeviceCommand(string.Format("shell mkdir -p {0}", DependencyCacheDir));

				string Cmd = string.Format("shell echo \"{0}\" > {1}", DepContents, DepFile);
				AdbResult = RunAdbDeviceCommand(Cmd);

				if (AdbResult.ExitCode != 0)
				{
					Log.Warning(KnownLogEvents.Gauntlet_DeviceEvent, "Failed to write dependency file {File}", DepFile);
				}
			}

			return true;
		}

		public IAppInstall InstallApplication(UnrealAppConfig AppConfig)
		{
			// todo - pass this through
			AndroidBuild Build = AppConfig.Build as AndroidBuild;

			// Ensure APK exists
			if (Build == null)
			{
				throw new AutomationException("Invalid build for Android!");
			}

			// kill any currently running instance:
			KillRunningProcess(Build.AndroidPackageName);

			string DeviceStorageQueryCommand = AndroidPlatform.GetStorageQueryCommand();
			IProcessResult StorageQueryResult = RunAdbDeviceCommand(DeviceStorageQueryCommand);
			string StorageLocation = StorageQueryResult.Output.Trim(); // "/mnt/sdcard";
																	   // remote dir used to save things
			string ExternalStoragePath = StorageLocation + "/UnrealGame/" + AppConfig.ProjectName;
			string ExternalFilesPath = StorageLocation + "/Android/data/" + Build.AndroidPackageName + "/files/UnrealGame/" + AppConfig.ProjectName;
			string DeviceBaseDir = Build.UsesExternalFilesDir ? ExternalFilesPath : ExternalStoragePath;

			// get the device's external file paths, always clear between runs
			DeviceExternalStorageSavedPath = string.Format("{0}/{1}/Saved", ExternalStoragePath, AppConfig.ProjectName);
			DeviceExternalFilesSavedPath = string.Format("{0}/{1}/Saved", ExternalFilesPath, AppConfig.ProjectName);
			DeviceLogPath = string.Format("{0}/Logs/{1}.log", Build.UsesPublicLogs ? DeviceExternalFilesSavedPath : DeviceExternalStorageSavedPath, AppConfig.ProjectName);
			DeviceArtifactPath = Build.UsesExternalFilesDir ? DeviceExternalFilesSavedPath : DeviceExternalStorageSavedPath;

			// path for OBB files
			string OBBRemoteDestination = string.Format("{0}/obb/{1}", StorageLocation, Build.AndroidPackageName);

			Log.Info("DeviceBaseDir: " + DeviceBaseDir);
			Log.Info("DeviceExternalStorageSavedPath: " + DeviceExternalStorageSavedPath);
			Log.Info("DeviceExternalFilesSavedPath: " + DeviceExternalFilesSavedPath);
			Log.Info("DeviceLogPath: " + DeviceLogPath);
			Log.Info("DeviceArtifactPath: " + DeviceArtifactPath);

			// clear all file store paths between installs:
			RunAdbDeviceCommand(string.Format("shell rm -r {0}", DeviceExternalStorageSavedPath));
			RunAdbDeviceCommand(string.Format("shell rm -r {0}", DeviceExternalFilesSavedPath));
		
			if (AppConfig.FullClean)
			{
				Log.Info("Fully cleaning console before install...");
				RunAdbDeviceCommand(string.Format("shell rm -r {0}/UnrealGame/*", StorageLocation));
				RunAdbDeviceCommand(string.Format("shell rm -r {0}/Android/data/{1}/*", StorageLocation, Build.AndroidPackageName));
				RunAdbDeviceCommand(string.Format("shell rm -r {0}/Android/obb/{1}/*", StorageLocation, Build.AndroidPackageName));
				RunAdbDeviceCommand(string.Format("shell rm -r {0}/Download/*", StorageLocation));
			}			

			if (!AppConfig.SkipInstall)
			{
				if (Globals.Params.ParseParam("cleandevice")
					|| AppConfig.FullClean)
				{
					Log.Info("Cleaning previous builds due to presence of -cleandevice");

					// we need to ununstall then install the apk - don't care if it fails, may have been deleted
					Log.Info("Uninstalling {0}", Build.AndroidPackageName);
					RunAdbDeviceCommand(string.Format("uninstall {0}", Build.AndroidPackageName));

					// delete DeviceExternalStorageSavedPath, note: DeviceExternalFilesSavedPath is removed with package uninstall.
					Log.Info("Removing {0}", DeviceExternalStorageSavedPath);
					RunAdbDeviceCommand(string.Format("shell rm -r {0}", DeviceExternalStorageSavedPath));

					Log.Info("Removing {0}", OBBRemoteDestination);
					RunAdbDeviceCommand(string.Format("shell rm -r {0}", OBBRemoteDestination));
				}

				// remote dir on the device, create it if it doesn't exist
				RunAdbDeviceCommand(string.Format("shell mkdir -p {0}/", DeviceExternalStorageSavedPath));
				RunAdbDeviceCommand(string.Format("shell mkdir -p {0}/", DeviceExternalFilesSavedPath));

				// path to the APK to install.
				string ApkPath = Build.SourceApkPath;

				// check for a local newer executable
				if (Globals.Params.ParseParam("dev"))
				{
					//string ApkFileName = Path.GetFileName(ApkPath);

					string ApkFileName2 = UnrealHelpers.GetExecutableName(AppConfig.ProjectName, UnrealTargetPlatform.Android, AppConfig.Configuration, AppConfig.ProcessType, "apk");

					string LocalAPK = Path.Combine(Environment.CurrentDirectory, AppConfig.ProjectName, "Binaries/Android", ApkFileName2);

					bool LocalFileExists = File.Exists(LocalAPK);
					bool LocalFileNewer = LocalFileExists && File.GetLastWriteTime(LocalAPK) > File.GetLastWriteTime(ApkPath);

					Log.Verbose("Checking for newer binary at {0}", LocalAPK);
					Log.Verbose("LocalFile exists: {0}. Newer: {1}", LocalFileExists, LocalFileNewer);

					if (LocalFileExists && LocalFileNewer)
					{
						ApkPath = LocalAPK;
					}
				}

				// first install the APK
				CopyFileToDevice(Build.AndroidPackageName, ApkPath, "");
			}

			// Convert the files from the source to final destination names
			Dictionary<string, string> FilesToInstall = new Dictionary<string, string>();

			Console.WriteLine("trying to copy files over.");
            if (AppConfig.FilesToCopy != null)
            {
                if (LocalDirectoryMappings.Count == 0)
                {
                    Console.WriteLine("Populating Directory");
                    PopulateDirectoryMappings(DeviceArtifactPath);
                }
                Console.WriteLine("trying to copy files over.");
                foreach (UnrealFileToCopy FileToCopy in AppConfig.FilesToCopy)
                {
                    string PathToCopyTo = Path.Combine(LocalDirectoryMappings[FileToCopy.TargetBaseDirectory], FileToCopy.TargetRelativeLocation);
                    if (File.Exists(FileToCopy.SourceFileLocation))
                    {
                        FileInfo SrcInfo = new FileInfo(FileToCopy.SourceFileLocation);
                        SrcInfo.IsReadOnly = false;
                        FilesToInstall.Add(FileToCopy.SourceFileLocation, PathToCopyTo.Replace("\\", "/"));
                    }

                    else
                    {
                        Log.Warning(KnownLogEvents.Gauntlet_DeviceEvent, "File to copy {File} not found", FileToCopy);
                    }
                }
            }

			if (!AppConfig.SkipInstall)
			{
				// obb files need to be named based on APK version (grrr), so find that out. This should return something like
				// versionCode=2 minSdk=21 targetSdk=21
				string PackageInfo = RunAdbDeviceCommand(string.Format("shell dumpsys package {0} | grep versionCode", Build.AndroidPackageName)).Output;
				var Match = Regex.Match(PackageInfo, @"versionCode=([\d\.]+)\s");
				if (Match.Success == false)
				{
					throw new AutomationException("Failed to find version info for APK!");
				}
				string PackageVersion = Match.Groups[1].ToString();

				Build.FilesToInstall.Keys.ToList().ForEach(K =>
				{
					string SrcPath = K;
					string SrcFile = Path.GetFileName(SrcPath);

					string DestPath = Build.FilesToInstall[K];
					string DestFile = Path.GetFileName(DestPath);

					// If we installed a new APK we need to change the package version
					Match OBBMatch = Regex.Match(SrcFile, @"\.(\d+)\.com.*\.obb");
					if (OBBMatch.Success)
					{
						DestPath = StorageLocation + "/obb/" + Build.AndroidPackageName + "/" + SrcFile.Replace(".Client.obb", ".obb").Replace(OBBMatch.Groups[1].ToString(), PackageVersion);
					}

					DestPath = Regex.Replace(DestPath, "%STORAGE%", StorageLocation, RegexOptions.IgnoreCase);

					FilesToInstall.Add(SrcPath, DestPath);
				});

				// get a list of files in the destination OBB directory
				IProcessResult AdbResult;
				AdbResult = RunAdbDeviceCommand(string.Format("shell ls {0}", OBBRemoteDestination));

				// if != 0 then no folder exists
				if (AdbResult.ExitCode == 0)
				{
					string[] Delimiters = { "\r\n", "\n" };
					string[] CurrentRemoteFileList = AdbResult.Output.Split(Delimiters, StringSplitOptions.RemoveEmptyEntries);
					for (int i = 0; i < CurrentRemoteFileList.Length; ++i)
					{
						CurrentRemoteFileList[i] = CurrentRemoteFileList[i].Trim();
					}

					IEnumerable<string> NewRemoteFileList = FilesToInstall.Values.Select(F => Path.GetFileName(F));

					// delete any files that should not be there
					foreach (string FileName in CurrentRemoteFileList)
					{
						if (FileName.StartsWith(".") || FileName.Length == 0)
						{
							continue;
						}

						if (NewRemoteFileList.Contains(FileName) == false)
						{
							RunAdbDeviceCommand(string.Format("shell rm {0}/{1}", OBBRemoteDestination, FileName));
						}
					}
				}

				EnablePermissions(Build.AndroidPackageName);
			}
			else
			{
				Log.Info("Skipping install of {0} (-skipdeploy)", Build.AndroidPackageName);
			}

			// create a tempfile, insert the command line, and push it over
			string CommandLineTmpFile = Path.GetTempFileName();
			CommandLineFilePath = string.Format("{0}/UECommandLine.txt", DeviceBaseDir);
			// I've seen a weird thing where adb push truncates by a byte, so add some padding...
			File.WriteAllText(CommandLineTmpFile, AppConfig.CommandLine + "    ");
			FilesToInstall.Add(CommandLineTmpFile, CommandLineFilePath);

			foreach (var KV in FilesToInstall)
			{
				string LocalFile = KV.Key;
				string RemoteFile = KV.Value;

				Console.WriteLine("Copying {0} to {1}", LocalFile, RemoteFile);
				CopyFileToDevice(Build.AndroidPackageName, LocalFile, RemoteFile);
			}

			File.Delete(CommandLineTmpFile);

			AndroidAppInstall AppInstall = new AndroidAppInstall(this, AppConfig.ProjectName, Build.AndroidPackageName, AppConfig.CommandLine);

			return AppInstall;
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

		}

		public IAppInstance Run(IAppInstall App)
		{
			AndroidAppInstall DroidAppInstall = App as AndroidAppInstall;

			if (DroidAppInstall == null)
			{
				throw new Exception("AppInstance is of incorrect type!");
			}

			// wake the device - we can install while its asleep but not run
			PowerOn();

			// kill any currently running instance:
			KillRunningProcess(DroidAppInstall.AndroidPackageName);

			string LaunchActivity = AndroidPlatform.GetLaunchableActivityName();

			Log.Info("Launching {0} on '{1}' ", DroidAppInstall.AndroidPackageName + "/" + LaunchActivity, ToString());
			Log.Verbose("\t{0}", DroidAppInstall.CommandLine);

			// Clear the device's logcat in preparation for the test..
			RunAdbDeviceCommand("logcat --clear");

			// start the app on device!
			string CommandLine = "shell am start -W -S -n " + DroidAppInstall.AndroidPackageName + "/" + LaunchActivity;
			IProcessResult Process = RunAdbDeviceCommand(CommandLine, false, true);

			return new AndroidAppInstance(this, DroidAppInstall, Process);
		}

		/// <summary>
		/// Runs an ADB command, automatically adding the name of the current device to
		/// the arguments sent to adb
		/// </summary>
		/// <param name="Args"></param>
		/// <param name="Wait"></param>
		/// <param name="Input"></param>
		/// <returns></returns>
		public IProcessResult RunAdbDeviceCommand(string Args, bool Wait=true, bool bShouldLogCommand = false, bool bPauseErrorParsing = false)
		{
			if (string.IsNullOrEmpty(DeviceName) == false)
			{
				Args = string.Format("-s {0} {1}", DeviceName, Args);
			}

			return RunAdbGlobalCommand(Args, Wait, bShouldLogCommand, bPauseErrorParsing);
		}

		/// <summary>
		/// Runs an ADB command, automatically adding the name of the current device to
		/// the arguments sent to adb
		/// </summary>
		/// <param name="Args"></param>
		/// <param name="Wait"></param>
		/// <param name="Input"></param>
		/// <returns></returns>
		public string RunAdbDeviceCommandAndGetOutput(string Args)
		{
			if (string.IsNullOrEmpty(DeviceName) == false)
			{
				Args = string.Format("-s {0} {1}", DeviceName, Args);
			}

			IProcessResult Result = RunAdbGlobalCommand(Args);

			if (Result.ExitCode != 0)
			{
				throw new AutomationException("adb command {0} failed. {1}", Args, Result.Output);
			}

			return Result.Output;
		}

		/// <summary>
		/// Runs an ADB command at the global scope
		/// </summary>
		/// <param name="Args"></param>
		/// <param name="Wait"></param>
		/// <returns></returns>
		public static IProcessResult RunAdbGlobalCommand(string Args, bool Wait = true, bool bShouldLogCommand = false, bool bPauseErrorParsing = false)
		{
			CommandUtils.ERunOptions RunOptions = CommandUtils.ERunOptions.AppMustExist | CommandUtils.ERunOptions.NoWaitForExit | CommandUtils.ERunOptions.SpewIsVerbose;

			if (Log.IsVeryVerbose)
			{
				RunOptions |= CommandUtils.ERunOptions.AllowSpew;
			}
			else
			{
				RunOptions |= CommandUtils.ERunOptions.NoLoggingOfRunCommand;
			}
	
			if (bShouldLogCommand)
			{
				Log.Verbose("Running ADB Command: adb {0}", Args);
			}

			IProcessResult Process;

			using (bPauseErrorParsing ? new ScopedSuspendECErrorParsing() : null)
			{
				Process = AndroidPlatform.RunAdbCommand(null, null, Args, null, RunOptions);

				if (Wait)
				{
					Process.WaitForExit();
				}
			}
			
			return Process;
		}

		public void AllowDeviceSleepState(bool bAllowSleep)
		{
			string CommandLine = "shell svc power stayon " + (bAllowSleep ? "false" : "usb");
			RunAdbDeviceCommand(CommandLine, true, false, true);
		}
		/// <summary>
		/// Enable Android permissions which would otherwise block automation with permimssion requests
		/// </summary>
		public void EnablePermissions(string AndroidPackageName)
		{
			List<string> Permissions = new List<string>{ "WRITE_EXTERNAL_STORAGE", "READ_EXTERNAL_STORAGE", "GET_ACCOUNTS", "RECORD_AUDIO" };
			Permissions.ForEach(Permission => {
				string CommandLine = string.Format("shell pm grant {0} android.permission.{1}", AndroidPackageName, Permission);
				Log.Verbose(string.Format("Enabling permission: {0} {1}", AndroidPackageName, Permission));
				RunAdbDeviceCommand(CommandLine, true, false, true);
			});
		}

		public void KillRunningProcess(string AndroidPackageName)
		{
			Log.Verbose("{0}: Killing process '{1}' ", ToString(), AndroidPackageName);
			string KillProcessCommand = string.Format("shell am force-stop {0}", AndroidPackageName);
			RunAdbDeviceCommand(KillProcessCommand);
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

	/// <summary>
	/// ADB key credentials, running adb-server commands (must) use same pub/private key store
	/// </summary>
	internal static class AdbCredentialCache
	{

		private static int InstanceCount = 0;
		private static bool bUsingCustomKeys = false;

		private static string PrivateKey;
		private static string PublicKey;

		private const string KeyBackupExt = ".gauntlet.bak";

		private static void Reset()
		{
			if (InstanceCount != 0)
			{
				throw new AutomationException("AdbCredentialCache.Reset() called with outstanding instances");
			}

			PrivateKey = PublicKey = String.Empty;
			bUsingCustomKeys = false;

			RestoreBackupKeys();
		}

		public static void AddInstance(AndroidDeviceData DeviceData = null)
		{
			lock (Globals.MainLock)
			{
				string KeyPath = Globals.Params.ParseValue("adbkeys", null);

				// setup key store from device data
				if (String.IsNullOrEmpty(KeyPath) && DeviceData != null)
				{
					// checked that cached keys are the same
					if (!String.IsNullOrEmpty(PrivateKey))
					{
						if (PrivateKey != DeviceData.privateKey)
						{
							throw new AutomationException("ADB device private keys must match");
						}
					}

					if (!String.IsNullOrEmpty(PublicKey))
					{
						if (PublicKey != DeviceData.publicKey)
						{
							throw new AutomationException("ADB device public keys must match");
						}
					}

					PrivateKey = DeviceData.privateKey;
					PublicKey = DeviceData.publicKey;

					if (String.IsNullOrEmpty(PublicKey) || String.IsNullOrEmpty(PrivateKey))
					{
						throw new AutomationException("Invalid key in device data");
					}

					KeyPath = Path.Combine(Globals.TempDir, "AndroidADBKeys");

					if (!Directory.Exists(KeyPath))
					{
						Directory.CreateDirectory(KeyPath);
					}

					if (InstanceCount == 0)
					{
						byte[] data = Convert.FromBase64String(PrivateKey);
						File.WriteAllText(KeyPath + "/adbkey", Encoding.UTF8.GetString(data));

						data = Convert.FromBase64String(PublicKey);
						File.WriteAllText(KeyPath + "/adbkey.pub", Encoding.UTF8.GetString(data));
					}

				}

				if (InstanceCount == 0 && !String.IsNullOrEmpty(KeyPath))
				{

					Log.Info("Using adb keys at {0}", KeyPath);

					string LocalKeyPath = Path.Combine(Environment.GetEnvironmentVariable("USERPROFILE"), ".android");
					if(!Directory.Exists(LocalKeyPath))
					{
						Directory.CreateDirectory(LocalKeyPath);
					}

					string RemoteKeyFile = Path.Combine(KeyPath, "adbkey");
					string RemotePubKeyFile = Path.Combine(KeyPath, "adbkey.pub");
					string LocalKeyFile = Path.Combine(LocalKeyPath, "adbkey");
					string LocalPubKeyFile = Path.Combine(LocalKeyPath, "adbkey.pub");
					string BackupSentry = Path.Combine(LocalKeyPath, "gauntlet.inuse");

					if (File.Exists(RemoteKeyFile) == false)
					{
						throw new AutomationException("adbkey at {0} does not exist", KeyPath);
					}

					if (File.Exists(RemotePubKeyFile) == false)
					{
						throw new AutomationException("adbkey.pub at {0} does not exist", KeyPath);
					}

					if (File.Exists(BackupSentry) == false)
					{
						if (File.Exists(LocalKeyFile))
						{
							File.Copy(LocalKeyFile, LocalKeyFile + KeyBackupExt, true);
						}

						if (File.Exists(LocalPubKeyFile))
						{
							File.Copy(LocalPubKeyFile, LocalPubKeyFile + KeyBackupExt, true);
						}
						File.WriteAllText(BackupSentry, "placeholder");
					}

					File.Copy(RemoteKeyFile, LocalKeyFile, true);
					File.Copy(RemotePubKeyFile, LocalPubKeyFile, true);

					bUsingCustomKeys = true;

					KillAdbServer();
				}

				InstanceCount++;
			}

		}

		private static void KillAdbServer()
		{			
			using (new ScopedSuspendECErrorParsing())
			{
				Log.Info("Running adb kill-server to refresh credentials");
				TargetDeviceAndroid.RunAdbGlobalCommand("kill-server");
				// killing the adb server restarts it and can surface superfluous device errors
				int SleepTime = CommandUtils.IsBuildMachine ? 15000 : 5000;
				Thread.Sleep(SleepTime);
			}

		}

		public static void RemoveInstance()
		{
			lock (Globals.MainLock)
			{

				InstanceCount--;

				if (InstanceCount == 0 && bUsingCustomKeys)
				{
					Reset();
					KillAdbServer();
					// Kill ADB server, just as a safety measure to ensure it closes
					IEnumerable<Process> ADBProcesses = Process.GetProcesses().Where(p => p.ProcessName.Equals("adb"));
					if (ADBProcesses.Count() > 0)
					{
						Log.Info("Terminating {0} ADB Process(es)", ADBProcesses.Count());
						foreach (Process ADBProcess in ADBProcesses)
						{
							Log.Info("Killing ADB process {0}", ADBProcess.Id);
							ADBProcess.Kill();
						}
					}
				}
			}
		}

		public static void RestoreBackupKeys()
		{			
			string LocalKeyPath = Path.Combine(Environment.GetEnvironmentVariable("USERPROFILE"), ".android");
			string LocalKeyFile = Path.Combine(LocalKeyPath, "adbkey");
			string LocalPubKeyFile = Path.Combine(LocalKeyPath, "adbkey.pub");
			string BackupSentry = Path.Combine(LocalKeyPath, "gauntlet.inuse");

			if (File.Exists(BackupSentry))
			{
				Log.Info("Restoring original adb keys");

				if (File.Exists(LocalKeyFile + KeyBackupExt))
				{
					File.Copy(LocalKeyFile + KeyBackupExt, LocalKeyFile, true);
					File.Delete(LocalKeyFile + KeyBackupExt);
				}
				else
				{
					File.Delete(LocalKeyFile);
				}

				if (File.Exists(LocalPubKeyFile + KeyBackupExt))
				{
					File.Copy(LocalPubKeyFile + KeyBackupExt, LocalPubKeyFile, true);
					File.Delete(LocalPubKeyFile + KeyBackupExt);
				}
				else
				{
					File.Delete(LocalPubKeyFile);
				}

				File.Delete(BackupSentry);
			}

		}

		static AdbCredentialCache()
		{
			Reset();
		}

	}


}