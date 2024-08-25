// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.IO;
using System.Threading;
using System.Linq;
using System.Security.Cryptography;
using AutomationTool;
using UnrealBuildTool;
using System.Text;
using System.Text.RegularExpressions;
using EpicGames.Core;

/*

General Device Notes (and areas for improvement):

1) We don't currently support parallel iOS tests, see https://jira.it.epicgames.net/browse/UEATM-219
2) Device Farm devices should be in airplane mode + wifi to avoid No Sim warning notification

*/

namespace Gauntlet
{

	class IOSAppInstance : IAppInstance
	{
		protected IOSAppInstall Install;
		public IOSAppInstance(IOSAppInstall InInstall, IProcessResult InProcess, string InCommandLine)			
		{
			Install = InInstall;
			this.CommandLine = InCommandLine;
			this.ProcessResult = InProcess;		
		}

		public string ArtifactPath
		{
			get
			{
				if (bHaveSavedArtifacts == false)
				{
					if (HasExited)
					{
						SaveArtifacts();
						bHaveSavedArtifacts = true;
					}
				}
				
				return Install.IOSDevice.LocalCachePath + "/" + Install.IOSDevice.DeviceArtifactPath;
			}
		}

		public ITargetDevice Device
		{
			get
			{
				return Install.Device;
			}
		}

		protected void SaveArtifacts()
		{
			TargetDeviceIOS Device = Install.IOSDevice;

			// copy remote artifacts to local		
			string CommandLine = String.Format("--bundle_id {0} --download={1} --to {2}", Install.PackageName, Device.DeviceArtifactPath, Device.LocalCachePath);

			IProcessResult DownloadCmd = Device.ExecuteIOSDeployCommand(CommandLine, 120);

			if (DownloadCmd.ExitCode != 0)
			{
				Log.Warning(KnownLogEvents.Gauntlet_DeviceEvent, "Failed to retrieve artifacts. {Output}", DownloadCmd.Output);
			}
			
		}

		public IProcessResult ProcessResult { get; private set; }

		public bool HasExited { get { return ProcessResult.HasExited; } }

		public bool WasKilled { get; protected set; }		

		public int ExitCode { get { return ProcessResult.ExitCode; } }

		public string CommandLine { get; private set; }

		public string StdOut 
		{  
			get
			{
				if (HasExited)
				{
					// The ios application is being run under lldb by ios-deploy
					// lldb catches crashes and we have it setup to dump thread callstacks
					// parse any crash dumps into Unreal crash format and append to output
					string CrashLog = LLDBCrashParser.GenerateCrashLog(ProcessResult.Output);

					if (!string.IsNullOrEmpty(CrashLog))
					{
						return String.Format("{0}\n{1}", ProcessResult.Output, CrashLog);
					}
				}

				return ProcessResult.Output;

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

		public void Kill()
		{
			if (!HasExited)
			{
				WasKilled = true;
				ProcessResult.ProcessObject.Kill(true);
			}
		}


		internal bool bHaveSavedArtifacts;		
	}


	class IOSAppInstall : IAppInstall
	{
		public string Name { get; protected set; }
			
		public string CommandLine { get; protected set; }

        public string PackageName { get; protected set; }

		public ITargetDevice Device { get { return IOSDevice; } }

		public TargetDeviceIOS IOSDevice;

		public IOSAppInstall(string InName, TargetDeviceIOS InDevice, string InPackageName, string InCommandLine)
		{
			Name = InName;			
			CommandLine = InCommandLine;
            PackageName = InPackageName;

			IOSDevice = InDevice;
		}

		public IAppInstance Run()
		{
			return Device.Run(this);
		}
	}

	public class IOSDeviceFactory : IDeviceFactory
	{
		public bool CanSupportPlatform(UnrealTargetPlatform? Platform)
		{
			return Platform == UnrealTargetPlatform.IOS;
		}

		public ITargetDevice CreateDevice(string InRef, string InCachePath, string InParam)
		{
			return new TargetDeviceIOS(InRef, InCachePath);
		}
	}

	/// <summary>
	/// iOS implementation of a device to run applications
	/// </summary>
	public class TargetDeviceIOS : ITargetDevice
	{
		public string Name { get; protected set; }

		/// <summary>
		/// Low-level device name (uuid)
		/// </summary>
		public string DeviceName { get; protected set; }
		
		protected Dictionary<EIntendedBaseCopyDirectory, string> LocalDirectoryMappings { get; set; }

		public TargetDeviceIOS(string InName, string InCachePath = null)
		{
			KillZombies();

			var DefaultDevices = GetConnectedDeviceUUID();			

			IsDefaultDevice = (String.IsNullOrEmpty(InName) || InName.Equals("default", StringComparison.OrdinalIgnoreCase));

			Name = InName;
			LocalDirectoryMappings = new Dictionary<EIntendedBaseCopyDirectory, string>();

			// If no device name or its 'default' then use the first default device
			if (IsDefaultDevice)
			{				
				if (DefaultDevices.Count() == 0)
				{
					throw new AutomationException("No default device available");
				}

				DeviceName = DefaultDevices.First();

				Log.Verbose("Selected device {0} as default", DeviceName);
			}
			else
			{
				DeviceName = InName.Trim();
				if (!DefaultDevices.Contains(DeviceName))
				{
					throw new AutomationException("Device with UUID {0} not found in device list", DeviceName);
				}
			}

			// setup local cache
			LocalCachePath = InCachePath ?? Path.Combine(GauntletAppCache, "Device_" + Name);
		}

		bool IsDefaultDevice = false;		

		#region IDisposable Support
		private bool disposedValue = false; // To detect redundant calls

		protected virtual void Dispose(bool disposing)
		{
			if (!disposedValue)
			{
				try
				{
					if (Directory.Exists(LocalCachePath))
					{
						Directory.Delete(LocalCachePath, true);
					}
				}
				catch (Exception Ex)
				{
					Log.Warning(KnownLogEvents.Gauntlet_DeviceEvent, "TargetDeviceIOS.Dispose() threw: {Exception}", Ex.Message);
				}
				finally
				{
					disposedValue = true;
				}

			}
		}

		// This code added to correctly implement the disposable pattern.
		public void Dispose()
		{
			// Do not change this code. Put cleanup code in Dispose(bool disposing) above.
			Dispose(true);
			GC.SuppressFinalize(this);
		}
		#endregion

		public CommandUtils.ERunOptions RunOptions { get; set; }

		public IAppInstance Run(IAppInstall App)
		{
			IOSAppInstall IOSApp = App as IOSAppInstall;

			if (IOSApp == null)
			{
				throw new DeviceException("AppInstance is of incorrect type!");
			}

			string CommandLine = IOSApp.CommandLine.Replace("\"", "\\\\\"");
			
			Log.Info("Launching {0} on {1}", App.Name, ToString());
			Log.Verbose("\t{0}", CommandLine);

			// ios-deploy notes: -L launches detached, -I non-interactive  (exits when app exits), -r uninstalls before install (removes app Documents folder)
			// -t <seconds> number of seconds to wait for device to be connected

			// setup symbols if available
			string DSymBundle = "";
			string DSymDir = Path.Combine(GauntletAppCache, "Symbols");

			if (Directory.Exists(DSymDir))
			{
				DSymBundle = Directory.GetDirectories(DSymDir).Where(D => Path.GetExtension(D).ToLower() == ".dsym").FirstOrDefault();
				DSymBundle = string.IsNullOrEmpty(DSymBundle) ? "" : DSymBundle = " -S \"" + DSymBundle + "\"";
			}

			string CL = "--noinstall -I" + DSymBundle + " -b \"" + LocalAppBundle + "\" --args '" + CommandLine.Trim() + "'";

			IProcessResult Result = ExecuteIOSDeployCommand(CL, 0);

			Thread.Sleep(5000);

			// Give ios-deploy a chance to throw out any errors...
			if (Result.HasExited)
			{
				Log.Warning(KnownLogEvents.Gauntlet_DeviceEvent, "ios-deploy exited early: " + Result.Output);
				throw new DeviceException("Failed to launch on {0}. {1}", Name, Result.Output);
			}

			return new IOSAppInstance(IOSApp, Result, IOSApp.CommandLine);
		}

		/// <summary>
		/// Remove the application entirely from the iOS device, this includes any persistent app data in /Documents
		/// </summary>
		private void RemoveApplication(IOSBuild Build)
		{
			string CommandLine = String.Format("--bundle_id {0} --uninstall_only", Build.PackageName);
			ExecuteIOSDeployCommand(CommandLine);
		}

		/// <summary>
		/// Remove artifacts from device
		/// </summary>
		private bool CleanDeviceArtifacts(IOSBuild Build)
		{			
			try
			{
				Log.Verbose("Cleaning device artifacts");

				string CleanCommand = String.Format("--bundle_id {0} --rmtree {1}", Build.PackageName, DeviceArtifactPath);
				IProcessResult Result = ExecuteIOSDeployCommand(CleanCommand, 120);

				if (Result.ExitCode != 0)
				{
					Log.Warning(KnownLogEvents.Gauntlet_DeviceEvent, "Failed to clean artifacts from device");
					return false;
				}

			}
			catch (Exception Ex)
			{
				Log.Verbose("Exception while cleaning artifacts from device: {0}", Ex.Message);
			}

			return true;

		}

		/// <summary>
		/// Checks whether version of deployed bundle matches local IPA
		/// </summary>
		bool CheckDeployedIPA(IOSBuild Build)
		{
			try
			{
				Log.Verbose("Checking deployed IPA hash");

				string CommandLine = String.Format("--bundle_id {0} --download={1} --to {2}", Build.PackageName, "/Documents/IPAHash.txt", LocalCachePath);
				IProcessResult Result = ExecuteIOSDeployCommand(CommandLine, 120);

				if (Result.ExitCode != 0)
				{
					return false;
				}

				string Hash = File.ReadAllText(LocalCachePath + "/Documents/IPAHash.txt").Trim();
				string StoredHash = File.ReadAllText(IPAHashFilename).Trim();

				if (Hash == StoredHash)
				{
					Log.Verbose("Deployed app hash matched cached IPA hash");
					return true;
				}

			}
			catch (Exception Ex)
			{
				if (!Ex.Message.Contains("is denied"))
				{
					Log.Verbose("Unable to pull cached IPA cache from device, cached file may not exist: {0}", Ex.Message);
				}
			}

			Log.Verbose("Deployed app hash doesn't match, IPA will be installed");
			return false;

		}

		/// <summary>
		/// Resign application using local executable and update debug symbols
		/// </summary>
		void ResignApplication(UnrealAppConfig AppConfig)
		{
			// check that we have the signing stuff we need
			string SignProvision = Globals.Params.ParseValue("signprovision", String.Empty);
			string SignEntitlements = Globals.Params.ParseValue("signentitlements", String.Empty);
			string SigningIdentity =  Globals.Params.ParseValue("signidentity", String.Empty);
			
			// handle signing provision
			if (string.IsNullOrEmpty(SignProvision) || !File.Exists(SignProvision))
			{
				throw new AutomationException("Absolute path to existing provision must be specified, example: -signprovision=/path/to/myapp.provision");
			}
			
			// handle entitlements
			// Note this extracts entitlements: which may be useful when using same provision/entitlements?: codesign -d --entitlements :entitlements.plist ~/.gauntletappcache/Payload/Example.app/

			if (string.IsNullOrEmpty(SignEntitlements) || !File.Exists(SignEntitlements))
			{
				throw new AutomationException("Absolute path to existing entitlements must be specified, example: -signprovision=/path/to/entitlements.plist");
			}

			// signing identity
			if (string.IsNullOrEmpty(SigningIdentity))
			{
				throw new AutomationException("Signing identity must be specified, example: -signidentity=\"iPhone Developer: John Smith\"");
			}

			string ProjectName = AppConfig.ProjectName;
			string BundleName = Path.GetFileNameWithoutExtension(LocalAppBundle);
			string ExecutableName = UnrealHelpers.GetExecutableName(ProjectName, UnrealTargetPlatform.IOS, AppConfig.Configuration, AppConfig.ProcessType, "");
			string CachedAppPath = Path.Combine(GauntletAppCache, "Payload", string.Format("{0}.app", BundleName));			

			string LocalExecutable = Path.Combine(Environment.CurrentDirectory, ProjectName, string.Format("Binaries/IOS/{0}", ExecutableName));
			if (!File.Exists(LocalExecutable))
			{
				throw new AutomationException("Local executable not found for -dev argument: {0}", LocalExecutable);
			}

			File.WriteAllText(CacheResignedFilename, "The application has been resigned");

			// copy local executable
			FileInfo SrcInfo = new FileInfo(LocalExecutable);			
			string DestPath = Path.Combine(CachedAppPath, BundleName);
			SrcInfo.CopyTo(DestPath, true);
			Log.Verbose("Copied local executable from {0} to {1}", LocalExecutable, DestPath);

			// copy provision
			SrcInfo = new FileInfo(SignProvision);
			DestPath = Path.Combine(CachedAppPath, "embedded.mobileprovision");
			SrcInfo.CopyTo(DestPath, true);
			Log.Verbose("Copied provision from {0} to {1}", SignProvision, DestPath);

			// handle symbols
			string LocalSymbolsDir = Path.Combine(Environment.CurrentDirectory, ProjectName, string.Format("Binaries/IOS/{0}.dSYM", ExecutableName));
			DestPath = Path.Combine(GauntletAppCache, string.Format("Symbols/{0}.dSYM", ExecutableName));

			if (Directory.Exists(DestPath))
			{
				Directory.Delete(DestPath, true);
			}

			if (Directory.Exists(LocalSymbolsDir))
			{				
				CommandUtils.CopyDirectory_NoExceptions(LocalSymbolsDir, DestPath, true);
			}
			else
			{
				Log.Warning(KnownLogEvents.Gauntlet_DeviceEvent, "No symbols found for local build at {Directory}, removing cached app symbols", LocalSymbolsDir);
			}

			// resign application
			// @todo: this asks for password unless "Always Allow" is selected, also for builders, document how to permanently grant codesign access to keychain
			string SignArgs = string.Format("-f -s \"{0}\" --entitlements \"{1}\" \"{2}\"", SigningIdentity, SignEntitlements, CachedAppPath);
			Log.Info("\nResigning app, please enter keychain password if prompted:\n\ncodesign {0}", SignArgs);
			var Result = IOSBuild.ExecuteCommand("codesign", SignArgs);
			if (Result.ExitCode != 0)
			{
				throw new AutomationException("Failed to resign application");
			}
		}

		// We need to lock around setting up the IPA
		static object IPALock = new object();
		public IAppInstall InstallApplication(UnrealAppConfig AppConfig)
		{
            IOSBuild Build = AppConfig.Build as IOSBuild;

			// Ensure Build exists
			if (Build == null)
			{
				throw new AutomationException("Invalid build for IOS!");
			}

			bool CacheResigned = false;
			bool UseLocalExecutable = Globals.Params.ParseParam("dev");

			lock(IPALock)
			{
				Log.Info("Installing using IPA {0}", Build.SourceIPAPath);

				// device artifact path
				DeviceArtifactPath = string.Format("/Documents/{0}/Saved", AppConfig.ProjectName);

				CacheResigned = File.Exists(CacheResignedFilename);

				if (CacheResigned && !UseLocalExecutable)
				{
					if (File.Exists(IPAHashFilename))
					{
						Log.Verbose("App was resigned, invalidating app cache");
						File.Delete(IPAHashFilename);
					}
				}

				PrepareIPA(Build);

				// local executable support
				if (UseLocalExecutable)
				{
					ResignApplication(AppConfig);
				}
			}

			if (CacheResigned || UseLocalExecutable || !CheckDeployedIPA(Build))
			{
				// uninstall will clean all device artifacts
				ExecuteIOSDeployCommand(String.Format("--uninstall -b \"{0}\"", LocalAppBundle), 20 * 60);
			}
			else
			{
				// remove device artifacts
				CleanDeviceArtifacts(Build);
			}

			// parallel iOS tests use same app install folder, so lock it as setup is quick
			lock (Globals.MainLock)
			{
				// local app install with additional files, this directory will be mirrored to device in a single operation
				string AppInstallPath;

				AppInstallPath = Path.Combine(Globals.TempDir, "iOSAppInstall");

				if (Directory.Exists(AppInstallPath))
				{
					Directory.Delete(AppInstallPath, true);
				}

				Directory.CreateDirectory(AppInstallPath);

				if (LocalDirectoryMappings.Count == 0)
				{
					PopulateDirectoryMappings(AppInstallPath);
				}

				//@todo: Combine Build and AppConfig files, this should be done in higher level code, not per device implementation

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
							Log.Verbose("Copying app install: {0} to {1}", FileToCopy, DirectoryToCopyTo);
						}
						else
						{
							Log.Warning(KnownLogEvents.Gauntlet_DeviceEvent, "File to copy {File} not found", FileToCopy);
						}
					}
				}

				// copy mapped files in a single pass
				string CopyCommand = String.Format("--bundle_id {0} --upload={1} --to {2}", Build.PackageName, AppInstallPath, DeviceArtifactPath);
				ExecuteIOSDeployCommand(CopyCommand, 120);

				// store the IPA hash to avoid redundant deployments
				CopyCommand = String.Format("--bundle_id {0} --upload={1} --to {2}", Build.PackageName, IPAHashFilename, "/Documents/IPAHash.txt");
				ExecuteIOSDeployCommand(CopyCommand, 120);
			}

			IOSAppInstall IOSApp = new IOSAppInstall(AppConfig.Name, this, Build.PackageName, AppConfig.CommandLine);
			return IOSApp;
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


		public UnrealTargetPlatform? Platform { get { return UnrealTargetPlatform.IOS; } }

		/// <summary>
		/// Temp path we use to push/pull things from the device
		/// </summary>
		public string LocalCachePath { get; protected set; }


		/// <summary>
		/// Artifact (e.g. Saved) path on the device
		/// </summary>
		public string DeviceArtifactPath { get; protected set;  }


		#region Device State Management

		// NOTE: We check that a default device UUID or the one specifed is connected with 'ios-deploy --detect' at device creation time
		// otherwise, ios-deploy doesn't currently support this style of queries
		// It might be possible to add additional lldb queries/commands through the python interface (there are various solutions for reboot, though the ones I have found require jailbreaking)
		public bool IsAvailable { get { return true; } }
		public bool IsConnected { get { return Connected; } }
		public bool IsOn { get { return true; } }
		public bool PowerOn() { return true; }
		public bool PowerOff() { return true; }
		
		public bool Reboot() 
		{ 
			const string Cmd = "/usr/local/bin/idevicediagnostics";
			if (!File.Exists(Cmd))
			{
				Log.Verbose("Rebooting iOS device requires idevicediagnostics binary");
				return true;
			}

			var Result = IOSBuild.ExecuteCommand(Cmd, string.Format("restart -u {0}", DeviceName));
			if (Result.ExitCode != 0)
			{
				Log.Warning(string.Format("Failed to reboot iOS device {0}, restart command failed", DeviceName));
				return true;
			}

			// initial wait 20 seconds
			Thread.Sleep(20 * 1000);

			const int WaitPeriod = 10;
			int WaitTime = 120;
			bool rebooted = false;
			do
			{
				Result = IOSBuild.ExecuteCommand(Cmd, string.Format("diagnostics WiFi -u {0}", DeviceName));
				if (Result.ExitCode == 0)
				{
					rebooted = true;
					break;
				}
				
				Thread.Sleep(WaitPeriod * 1000);
				WaitTime -= WaitPeriod;

			} while (WaitTime > 0);

			if (!rebooted) 
			{
				Log.Warning(KnownLogEvents.Gauntlet_DeviceEvent, "Failed to reboot iOS device {Name}, device didn't come back after restart", DeviceName);
			}

			return true; 
		}

		static Dictionary<string, bool> ConnectedDevices = new Dictionary<string, bool>();
		bool Connected = false;

		public bool Connect() 
		{ 
			lock (Globals.MainLock)
			{
				if (Connected)
				{
					return true;								
				}

				bool ExistingConnection = false;
				if (ConnectedDevices.TryGetValue(DeviceName, out ExistingConnection))
				{
					if (ExistingConnection)
					{
						throw new AutomationException("Connected to already connected device");
					}
				}

				ConnectedDevices[DeviceName] = true;

				Connected = true;
			}

			return true; 
		}
		public bool Disconnect(bool bForce=false)
		{ 
			lock (Globals.MainLock)
			{
				if (!Connected)
				{
					return true;								
				}

				Connected = false;

				if (ConnectedDevices.ContainsKey(DeviceName))
				{
					ConnectedDevices.Remove(DeviceName);
				}

			}

			return true; 
		}

		#endregion

		public override string ToString()
		{
			return Name;
		}


		/// <summary>
		/// Get UUID of all connected iOS devices
		/// </summary>
		List<string> GetConnectedDeviceUUID()
		{
			var Result = ExecuteIOSDeployCommand("--detect", 60, true, false);

			if (Result.ExitCode != 0)
			{
				return new List<string>();
			}

			MatchCollection DeviceMatches = Regex.Matches(Result.Output, @"(.?)Found\ ([a-z0-9]{40}|[A-Z0-9]{8}-[A-Z0-9]{16})");

			return DeviceMatches.Cast<Match>().Select<Match, string>(
				M => M.Groups[2].ToString()
			).ToList();
		}

		static bool ZombiesKilled = false;

		// Get rid of any zombie lldb/iosdeploy processes, this needs to be reworked to use tracked process id's when running parallel tests across multiple AutomationTool.exe processes on test workers
		void KillZombies()
		{

			if (ZombiesKilled)
			{
				return;
			}

			ZombiesKilled = true;

			IOSBuild.ExecuteCommand("killall", "ios-deploy");
			Thread.Sleep(2500);
			IOSBuild.ExecuteCommand("killall", "lldb");			
			Thread.Sleep(2500);
		}
		
		// Gauntlet cache folder for tracking device/ipa state
		string GauntletAppCache
		{
			get
			{	
				return Path.Combine(Globals.TempDir, "IOSAppCache");
			}
		}

		// path to locally extracted (and possibly resigned) app bundle
		// Note: ios-deploy works with app bundles, which requires the IPA be unzipped for deployment (this will allow us to resign in the future as well)
		string LocalAppBundle = null;

		// the current IPA MD5 hash, which is tracked to avoid unneccessary deployments and unzip operations
		string IPAHashFilename { get { return Path.Combine(GauntletAppCache, "IPAHash.txt"); } }

		// file whose presence signals that cache was resigned
		string CacheResignedFilename { get { return Path.Combine(GauntletAppCache, "Resigned.txt"); } }


		/// <summary>
		/// Generate MD5 and cache IPA bundle files
		/// </summary>
		private bool PrepareIPA(IOSBuild Build)
		{	
			Log.Info("Preparing IPA {0}", Build.SourceIPAPath);

			try
			{	
				// cache the unzipped app using a MD5 checksum, avoiding needing to unzip		
				string Hash = null;
				string StoredHash = null;
				using (var MD5Hash = MD5.Create())
				{
					using (var Stream = File.OpenRead(Build.SourceIPAPath))
					{
						Hash = BitConverter.ToString(MD5Hash.ComputeHash(Stream)).Replace("-", "").ToLowerInvariant();
					}
				}
				string PayloadDir = Path.Combine(GauntletAppCache, "Payload");
				string SymbolsDir = Path.Combine(GauntletAppCache, "Symbols");
				
				if (File.Exists(IPAHashFilename) && Directory.Exists(PayloadDir))
				{
					StoredHash = File.ReadAllText(IPAHashFilename).Trim();
					if (Hash != StoredHash)
					{
						Log.Verbose("IPA hash out of date, clearing cache");
						StoredHash = null;
					}
				}

				if (String.IsNullOrEmpty(StoredHash) || Hash != StoredHash)
				{
					if (Directory.Exists(PayloadDir))
					{
						Directory.Delete(PayloadDir, true);
					}

					if (Directory.Exists(SymbolsDir))
					{
						Directory.Delete(SymbolsDir, true);
					}

					if (File.Exists(CacheResignedFilename))
					{
						File.Delete(CacheResignedFilename);
					}

					Log.Verbose("Unzipping IPA {0} to cache at: {1}", Build.SourceIPAPath, GauntletAppCache);

					string Output;
					if (!IOSBuild.ExecuteIPADittoCommand(String.Format("-x -k {0} {1}", Build.SourceIPAPath, GauntletAppCache), out Output, PayloadDir))
					{
						throw new Exception(String.Format("Unable to extract IPA {0}", Build.SourceIPAPath));
					}

					// Cache symbols for symbolicated callstacks
					string SymbolsZipFile = string.Format("{0}/../../Symbols/{1}.dSYM.zip", Path.GetDirectoryName(Build.SourceIPAPath), Path.GetFileNameWithoutExtension(Build.SourceIPAPath));

					Log.Verbose("Checking Symbols at {0}", SymbolsZipFile);

					if (File.Exists(SymbolsZipFile))
					{						
						Log.Verbose("Unzipping Symbols {0} to cache at: {1}", SymbolsZipFile, SymbolsDir);

						if (!IOSBuild.ExecuteIPAZipCommand(String.Format("{0} -d {1}", SymbolsZipFile, SymbolsDir), out Output, SymbolsDir))
						{
							throw new Exception(String.Format("Unable to extract build symbols {0} -> {1}", SymbolsZipFile, SymbolsDir));
						}
					}					

					// store hash
					File.WriteAllText(IPAHashFilename, Hash);

					Log.Verbose("IPA cached");
				}
				else
				{
					Log.Verbose("Using cached IPA");
				}

				LocalAppBundle = Directory.GetDirectories(PayloadDir).Where(D => Path.GetExtension(D) == ".app").FirstOrDefault();

				if (String.IsNullOrEmpty(LocalAppBundle))
				{
					throw new Exception(String.Format("Unable to find app in local app bundle {0}", PayloadDir));
				}

			}
			catch (Exception Ex)
			{
				throw new AutomationException("Unable to prepare {0} : {1}", Build.SourceIPAPath, Ex.Message);
			}		

			return true;
		}

		public Dictionary<EIntendedBaseCopyDirectory, string> GetPlatformDirectoryMappings()
		{
			if (LocalDirectoryMappings.Count == 0)
			{
				Log.Warning("Platform directory mappings have not been populated for this platform! This should be done within InstallApplication()");
			}
			return LocalDirectoryMappings;
		}

		public IProcessResult ExecuteIOSDeployCommand(String CommandLine, int WaitTime = 60, bool WarnOnTimeout = true, bool UseDeviceID = true)
		{
			if (UseDeviceID && !IsDefaultDevice)
			{
				CommandLine = String.Format("--id {0} {1}", DeviceName, CommandLine);
			}

			String IOSDeployPath = Path.Combine(Globals.UnrealRootDir, "Engine/Extras/ThirdPartyNotUE/ios-deploy/bin/ios-deploy");

			if (!File.Exists(IOSDeployPath))
			{
				throw new AutomationException("Unable to run ios-deploy binary at {0}", IOSDeployPath);
			}

			CommandUtils.ERunOptions RunOptions = CommandUtils.ERunOptions.NoWaitForExit;

			if (Log.IsVeryVerbose)
			{
				RunOptions |= CommandUtils.ERunOptions.AllowSpew;
			}
			else
			{
				RunOptions |= CommandUtils.ERunOptions.NoLoggingOfRunCommand;
			}

			Log.Verbose("ios-deploy executing '{0}'", CommandLine);

			IProcessResult Result = CommandUtils.Run(IOSDeployPath, CommandLine, Options: RunOptions);

			if (WaitTime > 0)
			{
				DateTime StartTime = DateTime.Now;

				Result.ProcessObject.WaitForExit(WaitTime * 1000);

				if (Result.HasExited == false)
				{
					if ((DateTime.Now - StartTime).TotalSeconds >= WaitTime)
					{
						string Message = String.Format("IOSDeployPath timeout after {0} secs: {1}, killing process", WaitTime, CommandLine);

						if (WarnOnTimeout)
						{ 
							Log.Warning(Message);
						}
						else
						{
							Log.Info(Message);
						}
						
						Result.ProcessObject.Kill();
						// wait up to 15 seconds for process exit
						Result.ProcessObject.WaitForExit(15000);
					}
				}
			}

			return Result;
		}
	}


	/// <summary>
	/// Helper class to parses LLDB crash threads and generate Unreal compatible log callstack
	/// </summary>
	static class LLDBCrashParser
	{
		// Frame in callstack
		class FrameInfo
		{
			public string Module;
			public string Symbol = String.Empty;
			public string Address;
			public string Offset;
			public string Source;
			public string Line;

			public override string ToString()
			{
				// symbolicated
				if (!String.IsNullOrEmpty(Source))
				{
					return string.Format("Error: [Callstack] 0x{0} {1}!{2} [{3}{4}]", Address, Module, Symbol.Replace(" ", "^"), Source, String.IsNullOrEmpty(Line) ? "" : ":" + Line);
				}

				// unsymbolicated
				return string.Format("Error: [Callstack] 0x{0} {1}!{2} [???]", Address, Module, Symbol.Replace(" ", "^"));
			}

		}

		// Parsed thread callstack
		class ThreadInfo
		{
			public int Num;
			public string Status;
			public bool Current;
			public List<FrameInfo> Frames = new List<FrameInfo>();

			public override string ToString()
			{
				return string.Format("{0}{1}{2}\n{3}", Num, string.IsNullOrEmpty(Status) ? "" : " " + Status + " ", Current ? " (Current)" : "", string.Join("\n", Frames));
			}
		}

		/// <summary>
		/// Parse lldb thread crash dump to Unreal log format 
		/// </summary>		
		public static string GenerateCrashLog(string LogOutput)
		{
			try
			{
				DateTime TimeStamp;
				int Frame;
				ThreadInfo Thread = ParseCallstack(LogOutput, out TimeStamp, out Frame);
				if (Thread == null)
				{
					return null;
				}

				StringBuilder CrashLog = new StringBuilder();
				CrashLog.Append(string.Format("[{0}:000][{1}]LogCore: === Fatal Error: ===\n", TimeStamp.ToString("yyyy.mm.dd - H.mm.ss"), Frame));
				CrashLog.Append(string.Format("Error: Thread #{0} {1}\n", Thread.Num, Thread.Status));
				CrashLog.Append(string.Join("\n", Thread.Frames));

				return CrashLog.ToString();
			} 
			catch (Exception Ex)
			{
				Log.Warning(KnownLogEvents.Gauntlet_DeviceEvent, "Exception parsing LLDB callstack {Exception}", Ex.Message);
			}

			return null;

		}

		static ThreadInfo ParseCallstack(string LogOutput, out DateTime Timestamp, out int FrameNum)
		{
			Timestamp = DateTime.UtcNow;
			FrameNum = 0;

			Regex LogLineRegex = new Regex(@"(?<timestamp>\s\[\d.+\]\[\s*\d+\])(?<log>.*)");
			Regex TimeRegex = new Regex(@"\[(?<year>\d+)\.(?<month>\d+)\.(?<day>\d+)-(?<hour>\d+)\.(?<minute>\d+)\.(?<second>\d+):(?<millisecond>\d+)\]\[(?<frame>\s*\d+)\]", RegexOptions.IgnoreCase);
			Regex ThreadRegex = new Regex(@"(thread\s#)(?<threadnum>\d+),?(?<status>.+)");
			Regex SymbolicatedFrameRegex = new Regex(@"\*?\s#(?<framenum>\d+):\s0x(?<address>[\da-f]+)\s(?<module>.+)\`(?<symbol>.+)(\sat\s)(?<source>.+)\s\[opt\]");
			Regex UnsymbolicatedFrameRegex = new Regex(@"\*?frame\s#(?<framenum>\d+):\s0x(?<address>[\da-f]+)\s(?<module>.+)\`(?<symbol>.+)(\s\+\s(?<offset>\d+))?");

			LinkedList<string> CrashLog = new LinkedList<string>(Regex.Split(LogOutput, "\r\n|\r|\n"));

			List<ThreadInfo> Threads = new List<ThreadInfo>();
			ThreadInfo Thread = null;

			var LineNode = CrashLog.First;
			while (LineNode != null)
			{
				string Line = LineNode.Value.Trim();

				// If Gauntlet marks the test as complete, ignore any thread dumps from forcing process to exit
				if (Line.Contains("**** TEST COMPLETE. EXIT CODE: 0 ****"))
				{
					return null;
				}

				// Parse log timestamps
				if (LogLineRegex.IsMatch(Line))
				{
					GroupCollection LogGroups = LogLineRegex.Match(Line).Groups;
					if (TimeRegex.IsMatch(LogGroups["timestamp"].Value))
					{
						GroupCollection TimeGroups = TimeRegex.Match(LogGroups["timestamp"].Value).Groups;
						int Year = int.Parse(TimeGroups["year"].Value);
						int Month = int.Parse(TimeGroups["month"].Value);
						int Day = int.Parse(TimeGroups["day"].Value);
						int Hour = int.Parse(TimeGroups["hour"].Value);
						int Minute = int.Parse(TimeGroups["minute"].Value);
						int Second = int.Parse(TimeGroups["second"].Value);
						FrameNum = int.Parse(TimeGroups["frame"].Value);
						Timestamp = new DateTime(Year, Month, Day, Hour, Minute, Second);
					}

					LineNode = LineNode.Next;
					continue;
				}

				if (Thread != null)
				{
					FrameInfo Frame = null;
					GroupCollection FrameGroups = null;

					// Parse symbolicated frame
					if (SymbolicatedFrameRegex.IsMatch(Line))
					{
						FrameGroups = SymbolicatedFrameRegex.Match(Line).Groups;

						Frame = new FrameInfo()
						{
							Address = FrameGroups["address"].Value,
							Module = FrameGroups["module"].Value,
							Symbol = FrameGroups["symbol"].Value,
						};

						Frame.Source = FrameGroups["source"].Value;
						if (Frame.Source.Contains(":"))
						{
							Frame.Source = FrameGroups["source"].Value.Split(':')[0];
							Frame.Line = FrameGroups["source"].Value.Split(':')[1];
						}
					}

					// Parse unsymbolicated frame
					if (UnsymbolicatedFrameRegex.IsMatch(Line))
					{
						FrameGroups = UnsymbolicatedFrameRegex.Match(Line).Groups;

						Frame = new FrameInfo()
						{
							Address = FrameGroups["address"].Value,
							Offset = FrameGroups["offset"].Value,
							Module = FrameGroups["module"].Value,
							Symbol = FrameGroups["symbol"].Value
						};
					}

					if (Frame != null)
					{
						Thread.Frames.Add(Frame);
					}
					else
					{
						Thread = null;
					}

				}

				// Parse thread
				if (ThreadRegex.IsMatch(Line))
				{

					GroupCollection ThreadGroups = ThreadRegex.Match(Line).Groups;
					int Num = int.Parse(ThreadGroups["threadnum"].Value);
					string Status = ThreadGroups["status"].Value.Trim();

					Thread = Threads.SingleOrDefault(T => T.Num == Num);

					if (Thread == null)
					{
						Thread = new ThreadInfo()
						{
							Num = Num,
							Status = Status
						};

						if (Line.Trim().StartsWith("*"))
						{
							Thread.Current = true;
						}

						Threads.Add(Thread);

					}
				}

				LineNode = LineNode.Next;
			}

			if (Threads.Count(T => T.Current == true) > 1)
			{
				Log.Warning(KnownLogEvents.Gauntlet_DeviceEvent, "LLDB debug parsed more than one current thread");
			}

			Thread = Threads.FirstOrDefault(T => T.Current == true);

			if (Threads.Count > 0 && Thread == null)
			{
				Log.Warning(KnownLogEvents.Gauntlet_DeviceEvent, "Unable to parse full crash callstack");
			}


			// Do not want to surface crashes which happen as a result of requesting exit
			if (Thread != null && Thread.Frames.FirstOrDefault(F => F.Symbol.Contains("::RequestExit")) != null)
			{
				Thread = null;
			}

			return Thread;

		}

	}

	public class IOSBuildSupport : BaseBuildSupport
	{
		protected override BuildFlags SupportedBuildTypes => BuildFlags.Packaged | BuildFlags.CanReplaceCommandLine | BuildFlags.CanReplaceExecutable | BuildFlags.Bulk | BuildFlags.NotBulk;
		protected override UnrealTargetPlatform? Platform => UnrealTargetPlatform.IOS;
	}
}