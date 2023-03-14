// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.IO;
using AutomationTool;
using AutomationTool.DeviceReservation;
using UnrealBuildTool;
using System.Threading;
using System.Text;
using System.Text.RegularExpressions;
using System.Drawing;
using System.Linq;
using System.Collections;
using System.Diagnostics;

namespace Gauntlet
{
	public enum ERoleModifier
	{
		None,
		Dummy,
		Null
	};

	/// <summary>
	/// Represents a role that will be performed in an Unreal Session
	/// </summary>
	public class UnrealSessionRole
	{

		/// <summary>
		/// Type of role
		/// </summary>
		public UnrealTargetRole RoleType;

		/// <summary>
		/// Platform this role uses
		/// </summary>
		public UnrealTargetPlatform? Platform;

		/// <summary>
		/// Configuration this role runs in
		/// </summary>
		public UnrealTargetConfiguration Configuration;

		/// <summary>
		/// Constraints this role runs under
		/// </summary>
		public UnrealDeviceTargetConstraint Constraint;

		/// <summary>
		/// Options that this role needs
		/// </summary>
		public IConfigOption<UnrealAppConfig> Options;

		/// <summary>
		/// Command line that this role will use
		/// </summary>
		public string CommandLine
		{
			get
			{
				if (CommandLineParams == null)
				{
					CommandLineParams = new GauntletCommandLine();
				}
				return CommandLineParams.GenerateFullCommandLine();
			}
			set
			{
				if (CommandLineParams == null)
				{
					CommandLineParams = new GauntletCommandLine();
				}

				CommandLineParams.ClearCommandLine();

				CommandLineParams.AddRawCommandline(value, false);
			}
		}
		/// <summary>
		/// Dictionary of commandline arguments that are turned into a commandline at the end.
		/// For flags, leave the value set to null. Generated from the Test Role's commandline object
		/// and modified as needed. Then passed through to the AppConfig in UnrealBuildSource.cs
		/// </summary>
		public GauntletCommandLine CommandLineParams { get; set; }


		/// <summary>
		/// Map override to use on a server in case we don't want them all running the same map.
		/// </summary>
		public string MapOverride;

		/// <summary>
		/// List of files to copy to the device.
		/// </summary>
		public List<UnrealFileToCopy> FilesToCopy;

		/// <summary>
		/// 
		/// </summary>
		public List<EIntendedBaseCopyDirectory> AdditionalArtifactDirectories;
		/// <summary>
		/// Role device configuration 
		/// </summary>
		public ConfigureDeviceHandler ConfigureDevice;

		/// <summary>
		/// Properties we require our build to have
		/// </summary>
		public BuildFlags RequiredBuildFlags;

		/// <summary>
		/// Should be represented by a null device?
		/// </summary>
		public ERoleModifier RoleModifier;

		/// <summary>
		/// Is this a dummy executable? 
		/// </summary>
		public bool IsDummy() { return RoleModifier == ERoleModifier.Dummy; }

		/// <summary>
		/// Whether this role should be responsible only for installing the build and not monitoring a process.
		/// </summary>
		public bool InstallOnly { get; set; }

		/// <summary>
		/// Is this role Null? 
		/// </summary>
		public bool IsNullRole() { return RoleModifier == ERoleModifier.Null; }


		/// <summary>
		/// Constructor taking limited params
		/// </summary>
		/// <param name="InType"></param>
		/// <param name="InPlatform"></param>
		/// <param name="InConfiguration"></param>
		/// <param name="InOptions"></param>
		public UnrealSessionRole(UnrealTargetRole InType, UnrealTargetPlatform? InPlatform, UnrealTargetConfiguration InConfiguration, IConfigOption<UnrealAppConfig> InOptions)
			: this(InType, InPlatform, InConfiguration, null, InOptions)
		{
		}

		/// <summary>
		/// Constructor taking optional params
		/// </summary>
		/// <param name="InType"></param>
		/// <param name="InPlatform"></param>
		/// <param name="InConfiguration"></param>
		/// <param name="InCommandLine"></param>
		/// <param name="InOptions"></param>
		public UnrealSessionRole(UnrealTargetRole InType, UnrealTargetPlatform? InPlatform, UnrealTargetConfiguration InConfiguration, string InCommandLine = null, IConfigOption<UnrealAppConfig> InOptions = null)
		{
			RoleType = InType;

			Platform = InPlatform;
			Configuration = InConfiguration;
			MapOverride = string.Empty;

			if (string.IsNullOrEmpty(InCommandLine))
			{
				CommandLine = string.Empty;
			}
			else
			{
				CommandLine = InCommandLine;
			}

			RequiredBuildFlags = BuildFlags.None;

			if (Globals.Params.ParseParam("dev") && !RoleType.UsesEditor())
			{
				RequiredBuildFlags |= BuildFlags.CanReplaceExecutable;
			}

			// Enforce build flags for the platform build that support it 
			IDeviceBuildSupport TargetBuildSupport = Gauntlet.Utils.InterfaceHelpers.FindImplementations<IDeviceBuildSupport>().Where(B => B.CanSupportPlatform(InPlatform)).FirstOrDefault();
			if (TargetBuildSupport != null)
			{
				if (Globals.Params.ParseParam("bulk") && TargetBuildSupport.CanSupportBuildType(BuildFlags.Bulk))
				{
					RequiredBuildFlags |= BuildFlags.Bulk;
				}
				else if (TargetBuildSupport.CanSupportBuildType(BuildFlags.NotBulk))
				{
					RequiredBuildFlags |= BuildFlags.NotBulk;
				}

				if (Globals.Params.ParseParam("packaged") && TargetBuildSupport.CanSupportBuildType(BuildFlags.Packaged))
				{
					RequiredBuildFlags |= BuildFlags.Packaged;
				}
			}

			InstallOnly = false;
			Options = InOptions;
            FilesToCopy = new List<UnrealFileToCopy>();
			CommandLineParams = new GauntletCommandLine();
			RoleModifier = ERoleModifier.None;
        }

        /// <summary>
        /// Debugging aid
        /// </summary>
        /// <returns></returns>
        public override string ToString()
		{
			return string.Format("{0} {1} {2}", Platform, Configuration, RoleType);
		}
	}

	/// <summary>
	/// Represents an instance of a running an Unreal session. Basically an aggregate of all processes for
	/// all roles (clients, server, etc
	/// 
	/// TODO - combine this into UnrealSession
	/// </summary>
	public class UnrealSessionInstance : IDisposable
	{
		/// <summary>
		/// Represents an running role in our session
		/// </summary>
		public class RoleInstance
		{
			public RoleInstance(UnrealSessionRole InRole, IAppInstance InInstance)
			{
				Role = InRole;
				AppInstance = InInstance;
			}

			/// <summary>
			/// Role that is being performed in this session
			/// </summary>
			public UnrealSessionRole Role { get; protected set; }

			/// <summary>
			/// Underlying AppInstance that us running the role
			/// </summary>
			public IAppInstance AppInstance { get; protected set; }

			/// <summary>
			/// Debugging aid
			/// </summary>
			/// <returns></returns>
			public override string ToString()
			{
				return Role.ToString();
			}
		};

		/// <summary>
		/// All running roles
		/// </summary>
		public RoleInstance[] RunningRoles { get; protected set; }

		/// <summary>
		/// Helper for accessing all client processes. May return an empty array if no clients are involved
		/// </summary>
		public IAppInstance[] ClientApps
		{
			get
			{
				return RunningRoles.Where(R => R.Role.RoleType.IsClient()).Select(R => R.AppInstance).ToArray();
			}
		}

		/// <summary>
		/// Helper for accessing server process. May return null if no serer is invlved
		/// </summary>
		public IAppInstance ServerApp
		{
			get
			{
				return RunningRoles.Where(R => R.Role.RoleType.IsServer()).Select(R => R.AppInstance).FirstOrDefault();
			}
		}

		/// <summary>
		/// Helper for accessing server process. May return null if no serer is invlved
		/// </summary>
		public IAppInstance EditorApp
		{
			get
			{
				return RunningRoles.Where(R => R.Role.RoleType.IsEditor()).Select(R => R.AppInstance).FirstOrDefault();
			}
		}

		/// <summary>
		/// Helper that returns true if clients are currently running
		/// </summary>
		public bool ClientsRunning
		{
			get
			{
				return ClientApps != null && ClientApps.Where(C => C.HasExited).Count() == 0;
			}
		}

		/// <summary>
		/// Helper that returns true if there's a running server
		/// </summary>
		public bool ServerRunning
		{
			get
			{
				return ServerApp != null && ServerApp.HasExited == false;
			}
		}

		/// <summary>
		/// Returns true if any of our roles are still running
		/// </summary>
		public bool IsRunningRoles
		{
			get
			{
				return RunningRoles.Any(R => R.AppInstance.HasExited == false);
			}
		}

		/// <summary>
		/// COnstructor. Roles must be passed in
		/// </summary>
		/// <param name="InRunningRoles"></param>
		public UnrealSessionInstance(RoleInstance[] InRunningRoles)
		{
			RunningRoles = InRunningRoles;
		}

		~UnrealSessionInstance()
		{
			Dispose(false);
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

				Shutdown();
				disposedValue = true;
			}
		}

		// This code added to correctly implement the disposable pattern.
		public void Dispose()
		{
			// Do not change this code. Put cleanup code in Dispose(bool disposing) above.
			Dispose(true);
		}
		#endregion

		/// <summary>
		/// Shutdown the session by killing any remaining processes.
		/// </summary>
		/// <returns></returns>
		public void Shutdown()
		{
			// Kill any remaining client processes
			if (ClientApps != null)
			{
				List<IAppInstance> RunningApps = ClientApps.Where(App =>( App != null && App.HasExited == false)).ToList();

				if (RunningApps.Count > 0)
				{
					Log.Info("Shutting down {0} clients", RunningApps.Count);
					RunningApps.ForEach(App =>
					{
						App.Kill();
						// Apps that are still running have timed out => fail
						IDeviceUsageReporter.RecordEnd(App.Device.Name, App.Device.Platform, IDeviceUsageReporter.EventType.Test, IDeviceUsageReporter.EventState.Success);
					});
				}

				List<IAppInstance> ClosedApps = ClientApps.Where(App => (App != null && App.HasExited == true)).ToList();
				if(ClosedApps.Count > 0)
				{
					ClosedApps.ForEach(App =>
					{
						// Apps that have already exited have 'succeeded'
						IDeviceUsageReporter.RecordEnd(App.Device.Name, App.Device.Platform, IDeviceUsageReporter.EventType.Test, IDeviceUsageReporter.EventState.Failure);
					});
				}
			}

			if (ServerApp != null)
			{
				if (ServerApp.HasExited == false)
				{
					Log.Info("Shutting down server");
					ServerApp.Kill();
				}
			}

			// kill anything that's left
			RunningRoles.Where(R => !R.Role.InstallOnly && R.AppInstance.HasExited == false).ToList().ForEach(R => R.AppInstance.Kill());

			// Wait for it all to end
			RunningRoles.Where(R => !R.Role.InstallOnly).ToList().ForEach(R => R.AppInstance.WaitForExit());

			Thread.Sleep(3000);
		}
	}

	/// <summary>
	/// Represents the set of available artifacts available after an UnrealSessionRole has completed
	/// </summary>
	public class UnrealRoleArtifacts
	{
		/// <summary>
		/// Session role info that created these artifacts
		/// </summary>
		public UnrealSessionRole SessionRole { get; protected set; }

		/// <summary>
		/// AppInstance that was used to run this role
		/// </summary>
		public IAppInstance AppInstance { get; protected set; }

		/// <summary>
		/// Path to artifacts from this role (these are local and were retried from the device).
		/// </summary>
		public string ArtifactPath { get; protected set; }

		/// <summary>
		/// Path to Log from this role
		/// </summary>
		public string LogPath { get; protected set; }

		/// <summary>
		/// Constructor, all values must be provided
		/// </summary>
		/// <param name="InSessionRole"></param>
		/// <param name="InAppInstance"></param>
		/// <param name="InArtifactPath"></param>
		/// <param name="InLogSummary"></param>
		public UnrealRoleArtifacts(UnrealSessionRole InSessionRole, IAppInstance InAppInstance, string InArtifactPath, string InLogPath, UnrealLogParser InLog)
		{
			SessionRole = InSessionRole;
			AppInstance = InAppInstance;
			ArtifactPath = InArtifactPath;
			LogPath = InLogPath;
			//LogParser = InLog;
		}
	}


	/// <summary>
	/// Helper class that understands how to launch/monitor/stop an an Unreal test (clients + server) based on params contained in the test context and config
	/// </summary>
	public class UnrealSession : IDisposable
	{
		/// <summary>
		/// Device reservation instance of this session
		/// </summary>
		public UnrealDeviceReservation UnrealDeviceReservation { get; private set; }

		/// <summary>
		/// Source of the build that will be launched
		/// </summary>
		protected UnrealBuildSource BuildSource { get; set; }

		/// <summary>
		/// Roles that will be performed by this session
		/// </summary>
		protected UnrealSessionRole[] SessionRoles { get; set; }

		/// <summary>
		/// Running instance of this session
		/// </summary>
		public UnrealSessionInstance SessionInstance { get; protected set; }

		/// <summary>
		/// Sandbox for installed apps
		/// </summary>
		public string Sandbox { get; set; }

		/// <summary>
		/// Whether or not we should retain our devices this pass
		/// </summary>
		public bool ShouldRetainDevices { get; set; }

		/// <summary>
		/// Record of our installations in case we want to re-use them in a later pass
		/// </summary>
		public Dictionary<UnrealSessionRole, IAppInstall> RolesToInstalls;

		/// <summary>
		/// Constructor that takes a build source and a number of roles
		/// </summary>
		/// <param name="InSource"></param>
		/// <param name="InSessionRoles"></param>
		public UnrealSession(UnrealBuildSource InSource, IEnumerable<UnrealSessionRole> InSessionRoles)
		{
			BuildSource = InSource;
			SessionRoles = InSessionRoles.ToArray();

			if (SessionRoles.Length == 0)
			{
				throw new AutomationException("No roles specified for Unreal session");
			}

			List<string> ValidationIssues = new List<string>();

			if (!CheckRolesArePossible(ref ValidationIssues))
			{
				ValidationIssues.ForEach(S => Log.Error("{0}", S));
				throw new AutomationException("One or more issues occurred when validating build {0} against requested roles", InSource.BuildName);
			}

			UnrealDeviceReservation = new UnrealDeviceReservation();
		}

		/// <summary>
		/// Destructor, terminates any running session
		/// </summary>
		~UnrealSession()
		{
			Dispose(false);
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

				ShutdownSession();
				disposedValue = true;
			}
		}

		// This code added to correctly implement the disposable pattern.
		public void Dispose()
		{
			// Do not change this code. Put cleanup code in Dispose(bool disposing) above.
			Dispose(true);
		}
		#endregion

		/// <summary>
		/// Helper that reserves and returns a list of available devices based on the passed in roles
		/// </summary>
		/// <param name="Configs"></param>
		/// <returns></returns>
		public bool TryReserveDevices()
		{
			// figure out how many of each device we need
			Dictionary<UnrealDeviceTargetConstraint, int> RequiredDeviceTypes = new Dictionary<UnrealDeviceTargetConstraint, int>();
			IEnumerable<UnrealSessionRole> RolesNeedingDevices = SessionRoles.Where(R => !R.IsNullRole());

			// Get a count of the number of devices required for each platform
			RolesNeedingDevices.ToList().ForEach(C =>
			{
				if (!RequiredDeviceTypes.ContainsKey(C.Constraint))
				{
					RequiredDeviceTypes[C.Constraint] = 0;
				}
				RequiredDeviceTypes[C.Constraint]++;
			});


			if (UnrealDeviceReservation.ReservedDevices != null && UnrealDeviceReservation.ReservedDevices.Count > 0
				&& ShouldRetainDevices)
			{
				return true;
			}
			else
			{
				return UnrealDeviceReservation.TryReserveDevices(RequiredDeviceTypes, RolesNeedingDevices.Count());
			}
		}

		/// <summary>
		/// Check that all the current roles can be performed by our build source
		/// </summary>
		/// <param name="Issues"></param>
		/// <returns></returns>
		bool CheckRolesArePossible(ref List<string> Issues)
		{
			bool Success = true;
			foreach (var Role in SessionRoles)
			{
				if (!BuildSource.CanSupportRole(Role, ref Issues))
				{
					Success = false;
				}
			}

			return Success;
		}

		/// <summary>
		/// Installs and launches all of our roles and returns an UnrealSessonInstance that represents the aggregate
		/// of all of these processes. Will perform retries if errors with devices are encountered so this is a "best
		/// attempt" at running with the devices provided
		/// </summary>
		/// <returns></returns>
		public UnrealSessionInstance LaunchSession()
		{
			SessionInstance = null;

			// The number of retries when launching session to avoid an endless loop if package can't be installed, network timeouts, etc
			int SessionRetries = 2;

			// tries to find devices and launch our session. Will loop until we succeed, we run out of devices/retries, or
			// something fatal occurs..
			while (SessionInstance == null && Globals.CancelSignalled == false)
			{
				int ReservationRetries = 5;
				int ReservationRetryWait = 120;

				IEnumerable<UnrealSessionRole> RolesNeedingDevices = SessionRoles.Where(R => R.IsNullRole() == false);

				while (UnrealDeviceReservation.ReservedDevices.Count() < RolesNeedingDevices.Count())
				{
					// get devices
					TryReserveDevices();

					if (Globals.CancelSignalled)
					{
						break;
					}

					// if we failed to get enough devices, show a message and wait
					if (UnrealDeviceReservation.ReservedDevices.Count() != SessionRoles.Count())
					{
						if (ReservationRetries == 0)
						{
							throw new AutomationException("Unable to acquire all devices for test.");
						}
						Log.Info("\nUnable to find enough device(s). Waiting {0} secs (retries left={1})\n", ReservationRetryWait, --ReservationRetries);
						Thread.Sleep(ReservationRetryWait * 1000);
					}
				}

				if (Globals.CancelSignalled)
				{
					return null;
				}

				Dictionary<IAppInstall, UnrealSessionRole> InstallsToRoles = new Dictionary<IAppInstall, UnrealSessionRole>();

				// create a copy of our list
				IEnumerable<ITargetDevice> DevicesToInstallOn = UnrealDeviceReservation.ReservedDevices.ToArray();

				bool InstallSuccess = true;

				// sort by constraints, so that we pick constrained devices first
				List<UnrealSessionRole> SortedRoles = SessionRoles.OrderBy(R => R.Constraint.IsIdentity() ? 1 : 0).ToList();				

				// first install all roles on these devices
				foreach (var Role in SortedRoles)
				{
					ITargetDevice Device = null;

					if (Role.IsNullRole() == false)
					{
						Device = DevicesToInstallOn.Where(D => D.IsConnected && D.Platform == Role.Platform
															&& ( Role.Constraint.IsIdentity() || DevicePool.Instance.GetConstraint(D) == Role.Constraint)).First();

						DevicesToInstallOn = DevicesToInstallOn.Where(D => D != Device);
					}
					else
					{
						Device = new TargetDeviceNull(string.Format("Null{0}", Role.RoleType));
					}

					var OtherRoles = SortedRoles.Where(R => R != Role);

					// create a config from the build source (this also applies the role options)
					UnrealAppConfig AppConfig = BuildSource.CreateConfiguration(Role, OtherRoles);

					//Verify the device's OS version, and update if necessary
					if(Globals.Params.ParseParam("TryFirmwareUpdate") && AppConfig.Platform != null)
					{
						List<IPlatformFirmwareHandler> PlatformFirmwareHandlers = Gauntlet.Utils.InterfaceHelpers.FindImplementations<IPlatformFirmwareHandler>(true).ToList();

						PlatformFirmwareHandlers = PlatformFirmwareHandlers.Where(FC => FC.CanSupportPlatform((UnrealTargetPlatform)AppConfig.Platform)
							&& FC.CanSupportProject(AppConfig.ProjectName)).ToList();
						if (PlatformFirmwareHandlers.Count > 0)
						{
							IPlatformFirmwareHandler SelectedFirmwareHandler = PlatformFirmwareHandlers.First();
							Log.Verbose("Found IPlatformFirmwareHandler {0}", SelectedFirmwareHandler.GetType().Name);
							string DesiredFirmware = string.Empty;
							if (!SelectedFirmwareHandler.GetDesiredVersion((UnrealTargetPlatform)AppConfig.Platform, AppConfig.ProjectName, out DesiredFirmware))
							{
								Log.Info("Failed to get desired os version for project {0} for platform {1}, skipping firmware check",
									AppConfig.ProjectName, AppConfig.Platform);
							}
							else
							{
								Log.Info("Desired Firmware for project {0} and platform {1}: {2}", AppConfig.ProjectName, AppConfig.Platform, DesiredFirmware);

								string CurrentFirmware = string.Empty;
								if (!SelectedFirmwareHandler.GetCurrentVersion(Device, out CurrentFirmware))
								{
									Log.Info("Failed to get current os version for device {0} for role {1}, skipping firmware check", Device, Role);
								}
								else
								{
									Log.Info("Current Firmware for device {0}: {1}", Device, CurrentFirmware);

									if(CurrentFirmware.Equals(DesiredFirmware))
									{
										Log.Info("Device {0} os version match!  No need to update", Device);
									}
									else
									{
										Log.Info("Device {0} os version out of date!  Updating to version {1}", Device, DesiredFirmware);

										if(SelectedFirmwareHandler.UpdateDeviceFirmware(Device, DesiredFirmware))
										{
											Log.Info("Successfully updated device {0} to os version {1}", Device, DesiredFirmware);
										}
										else
										{
											Log.Info("Failed to update os of device {0} for role {1}.  Will retry with new device", Device, Role);
											UnrealDeviceReservation.MarkProblemDevice(Device);
											InstallSuccess = false;
											break;
										}
									}
								}
							}
						}
						else
						{
							Log.Info("Unable to locate any IPlatformFirmwareCheckers that support Project {0} and platform {1}, skipping firmware check",
								AppConfig.ProjectName, AppConfig.Platform);
						}
					}

					// todo - should this be elsewhere?
					AppConfig.Sandbox = Sandbox;
					
					IAppInstall Install = null;
					bool bReinstallPerPass = Globals.Params.ParseParam("ReinstallPerPass");
					if (RolesToInstalls == null || !RolesToInstalls.ContainsKey(Role) || bReinstallPerPass)
					{
						// Tag the device for report result
						if(BuildHostPlatform.Current.Platform != Device.Platform)
						{
							AppConfig.CommandLineParams.Add("DeviceTag", Device.Name);
						}

						IDeviceUsageReporter.RecordStart(Device.Name, Device.Platform, IDeviceUsageReporter.EventType.Device, IDeviceUsageReporter.EventState.Success);
						IDeviceUsageReporter.RecordStart(Device.Name, Device.Platform, IDeviceUsageReporter.EventType.Install, IDeviceUsageReporter.EventState.Success, BuildSource.BuildName);
						try
						{
							Install = Device.InstallApplication(AppConfig);
							IDeviceUsageReporter.RecordEnd(Device.Name, Device.Platform, IDeviceUsageReporter.EventType.Install, IDeviceUsageReporter.EventState.Success);
						}
						catch (System.Exception Ex)
						{
							// Warn, ignore the device, and do not continue
							string ErrorMessage = string.Format("Failed to install app onto device {0} for role {1}. {2}. Will retry with new device", Device, Role, Ex);
							if (ErrorMessage.Contains("not enough space"))
							{
								Log.Warning(ErrorMessage);
								if (Device.Platform == BuildHostPlatform.Current.Platform)
								{
									// If on desktop platform, we are not retrying.
									// It is unlikely that space is going to be made and InstallBuildParallel has marked the build path as problematic.
									SessionRetries = 0;
								}
							}
							else
							{
								Log.Info(ErrorMessage);
							}
							UnrealDeviceReservation.MarkProblemDevice(Device);
							InstallSuccess = false;
							IDeviceUsageReporter.RecordEnd(Device.Name, Device.Platform, IDeviceUsageReporter.EventType.Install, IDeviceUsageReporter.EventState.Failure);
							break;
						}


						if (Globals.CancelSignalled)
						{
							break;
						}

						// Device has app installed, give role a chance to configure device
						Role.ConfigureDevice?.Invoke(Device);

						InstallsToRoles[Install] = Role;

						if(RolesToInstalls == null)
						{
							RolesToInstalls = new Dictionary<UnrealSessionRole, IAppInstall>();
						}
						if (!bReinstallPerPass)
						{
							RolesToInstalls[Role] = Install;
						}
					}
					else
					{
						Install = RolesToInstalls[Role];
						InstallsToRoles[Install] = Role;
						Log.Info("Using previous install of {0} on {1}", Install.Name, Install.Device.Name);
					}
				}

				if (InstallSuccess == false)
				{
					ReleaseSessionDevices();

					if (SessionRetries == 0)
					{
						throw new AutomationException("Unable to install application for session.");
					}

					Log.Info("\nUnable to install application for session (retries left={0})\n", --SessionRetries);
				}
				

				if (InstallSuccess && Globals.CancelSignalled == false)
				{
					List<UnrealSessionInstance.RoleInstance> RunningRoles = new List<UnrealSessionInstance.RoleInstance>();

					// Now try to run all installs on their devices
					foreach (var InstallRoleKV in InstallsToRoles)
					{
						IAppInstall CurrentInstall = InstallRoleKV.Key;
						if (InstallRoleKV.Value.InstallOnly)
						{	RunningRoles.Add(new UnrealSessionInstance.RoleInstance(InstallRoleKV.Value, null));
							continue;
						}
						bool Success = false;

						try
						{
							Log.Info("Starting {0} on {1}", InstallRoleKV.Value, CurrentInstall.Device);
							IAppInstance Instance = CurrentInstall.Run();
							IDeviceUsageReporter.RecordStart(Instance.Device.Name, Instance.Device.Platform, IDeviceUsageReporter.EventType.Test);

							if (Instance != null || Globals.CancelSignalled)
							{
								RunningRoles.Add(new UnrealSessionInstance.RoleInstance(InstallRoleKV.Value, Instance));
							}

							Success = true;
						}
						catch (DeviceException Ex)
						{
							// shutdown all 
							Log.Warning("Device {0} threw an exception during launch. \nException={1}", CurrentInstall.Device, Ex.Message);
							Success = false;
						}

						if (Success == false)
						{
							Log.Warning("Failed to start build on {0}. Marking as problem device and retrying with new set", CurrentInstall.Device);

							// terminate anything that's running
							foreach (UnrealSessionInstance.RoleInstance RunningRole in RunningRoles)
							{
								Log.Info("Shutting down {0}", RunningRole.AppInstance.Device);
								RunningRole.AppInstance.Kill();
								RunningRole.AppInstance.Device.Disconnect();
							}

							// mark that device as a problem
							UnrealDeviceReservation.MarkProblemDevice(CurrentInstall.Device);

							ReleaseSessionDevices();

							if (SessionRetries == 0)
							{
								throw new AutomationException("Unable to start application for session, see warnings for details.");
							}

							Log.Info("\nUnable to start application for session (retries left={0})\n", --SessionRetries);

							break; // do not continue loop
						}
					}

					if (RunningRoles.Count() == SessionRoles.Count())
					{
						SessionInstance = new UnrealSessionInstance(RunningRoles.ToArray());
					}
				}
			}

			return SessionInstance;
		}

		/// <summary>
		/// Restarts the current session (if any)
		/// </summary>
		/// <returns></returns>
		public UnrealSessionInstance RestartSession()
		{
			ShutdownSession();

			// AG-TODO - want to preserve device reservations here...

			return LaunchSession();
		}

		///<summary>
		/// Shuts down any running apps
		/// </summary>
		public void ShutdownInstance()
		{
			if (SessionInstance != null)
			{
				SessionInstance.Dispose();
				SessionInstance = null;
			}
		}

		/// <summary>
		/// Shuts down the current session (if any)
		/// </summary>
		/// <returns></returns>
		public void ShutdownSession()
		{
			ShutdownInstance();

			if (!ShouldRetainDevices)
			{
				ReleaseSessionDevices();
			}
		}

		public void ReleaseSessionDevices()
		{
			UnrealDeviceReservation.ReleaseDevices();
			if (RolesToInstalls != null)
			{
				RolesToInstalls.Clear();
			}
		}

		/// <summary>
		/// Retrieves and saves all artifacts from the provided session role. Artifacts are saved to the destination path 
		/// </summary>
		/// <param name="InContext"></param>
		/// <param name="InRunningRole"></param>
		/// <param name="InDestArtifactPath"></param>
		/// <returns></returns>
		public UnrealRoleArtifacts SaveRoleArtifacts(UnrealTestContext InContext, UnrealSessionInstance.RoleInstance InRunningRole, string InDestArtifactPath)
		{

			bool IsServer = InRunningRole.Role.RoleType.IsServer();
			string RoleName = (InRunningRole.Role.IsDummy() ? "Dummy" : "") + InRunningRole.Role.RoleType.ToString();
			UnrealTargetPlatform? Platform = InRunningRole.Role.Platform;
			string RoleConfig = InRunningRole.Role.Configuration.ToString();

			Directory.CreateDirectory(InDestArtifactPath);

			bool IsDevBuild = InContext.TestParams.ParseParam("dev");
			bool IsEditorBuild = InRunningRole.Role.RoleType.UsesEditor();

			// Unless there was a crash on a builder don't archive editor data (there can be a *lot* of stuff in their).
			bool SkipArchivingAssets = IsDevBuild ||
										(IsEditorBuild && 
											(CommandUtils.IsBuildMachine == false || InRunningRole.AppInstance.ExitCode == 0)
										);


			string DestSavedDir = InDestArtifactPath;
			string SourceSavedDir = "";

			// save the contents of the saved directory
			SourceSavedDir = InRunningRole.AppInstance.ArtifactPath;

			// save the output from TTY
			string ArtifactLogPath = Path.Combine(InDestArtifactPath, RoleName + "Output.log");

			// Write a brief Gauntlet header to aid debugging
			StringBuilder LogOut = new StringBuilder();
			LogOut.AppendLine("------ Gauntlet Test ------");
			LogOut.AppendFormat("Role: {0}\r\n", InRunningRole.Role);
			LogOut.AppendFormat("Automation Command: {0}\r\n", Environment.CommandLine);
			LogOut.AppendLine("---------------------------");

			// Write instance stdout stream
			LogOut.Append(InRunningRole.AppInstance.StdOut);

			File.WriteAllText(ArtifactLogPath, LogOut.ToString());

			// On build machines, copy all role logs to Horde.
			if (CommandUtils.IsBuildMachine)
			{
				string HordeLogFileName = Path.Combine(CommandUtils.CmdEnv.LogFolder, RoleName + "Output.log");
				File.WriteAllText(HordeLogFileName, LogOut.ToString());
			}

			Log.Info("Wrote Log to {0}", ArtifactLogPath);

			if (IsServer == false)
			{
				// gif-ify and jpeg-ify any screenshots
				try
				{

					string ScreenshotPath = Path.Combine(SourceSavedDir, "Screenshots", Platform.ToString()).ToLower();
					string TempScreenshotPath = Path.GetTempPath();
					if (!Directory.Exists(TempScreenshotPath))
					{
						Log.Info("Creating temp directory {0}", TempScreenshotPath);
						Directory.CreateDirectory(TempScreenshotPath);
					}

					if (Directory.Exists(ScreenshotPath) && Directory.GetFiles(ScreenshotPath).Length > 0)
					{
						Log.Info("Downsizing and gifying session images at {0}", ScreenshotPath);

						// downsize first so gif-step is quicker and takes less resoruces.
						Utils.Image.ConvertImages(ScreenshotPath, TempScreenshotPath, "jpg", true);

						string GifPath = Path.Combine(InDestArtifactPath, RoleName + "Test.gif");
						if (Utils.Image.SaveImagesAsGif(TempScreenshotPath, GifPath))
						{
							Log.Info("Saved gif to {0}", GifPath);
						}
					}
				}
				catch (Exception Ex)
				{
					Log.Info("Failed to downsize and gif-ify images! {0}", Ex.Message);
				}
			}

			Dictionary<string, string> LongCrashReporterStringToIndex = new Dictionary<string, string>();

			// Filter that truncates some overly long file paths like CrashReporter files. These are particularly problematic with 
			// testflights which append a long random name to the destination folder, easily pushing past 260 chars.
			Func<string, string> TruncateLongPathFilter= (X =>
			{
				// /UECC-Windows-F0DD9BB04C3C9250FAF39D8AB4A88556/
				Match M = Regex.Match(X, @"UECC-.+-([\dA-Fa-f]+)");

				if (M.Success)
				{
					string LongString = M.Groups[1].ToString();

					if (!LongCrashReporterStringToIndex.ContainsKey(LongString))
					{
						LongCrashReporterStringToIndex[LongString] = LongCrashReporterStringToIndex.Keys.Count.ToString("D2");
					}

					string ShortSring = LongCrashReporterStringToIndex[LongString];
					X = X.Replace(LongString, ShortSring);
				}				

				return X;
			});

			// don't archive data in dev mode, because peoples saved data could be huuuuuuuge!
			if (SkipArchivingAssets)
			{
				if (IsEditorBuild)
				{
					Log.Info("Skipping archival of assets for editor {0}", RoleName);
				}
				else if (IsDevBuild)
				{
					Log.Info("Skipping archival of assets for dev build");
				}
			}
			else
			{ 
				LogLevel OldLevel = Log.Level;
				Log.Level = LogLevel.Normal;

				if (Directory.Exists(SourceSavedDir))
				{
					Utils.SystemHelpers.CopyDirectory(SourceSavedDir, Utils.SystemHelpers.GetFullyQualifiedPath(DestSavedDir), Utils.SystemHelpers.CopyOptions.Default, TruncateLongPathFilter);
					Log.Info("Archived artifacts to to {0}", DestSavedDir);
				}
				else
				{
					Log.Info("Archive path '{0}' was not found!", SourceSavedDir);
				}

				Log.Level = OldLevel;
			}

			foreach (EIntendedBaseCopyDirectory ArtifactDir in InRunningRole.Role.AdditionalArtifactDirectories)
			{
				if (InRunningRole.AppInstance.Device.GetPlatformDirectoryMappings().ContainsKey(ArtifactDir))
				{
					string SourcePath = InRunningRole.AppInstance.Device.GetPlatformDirectoryMappings()[ArtifactDir];
					var DirToCopy = new DirectoryInfo(SourcePath);
					if (DirToCopy.Exists)
					{
						// Grab the final dir name to copy everything into so everything's not just going into root artifact dir.
						string IntendedCopyLocation = Path.Combine(InDestArtifactPath, DirToCopy.Name);
						Utils.SystemHelpers.CopyDirectory(SourcePath, Utils.SystemHelpers.GetFullyQualifiedPath(IntendedCopyLocation), Utils.SystemHelpers.CopyOptions.Default, TruncateLongPathFilter);
					}
				}
			}
			// TODO REMOVEME- this should go elsewhere, likely a util that can be called or inserted by relevant test nodes.
			if (IsServer == false)
			{
				// Copy over PSOs
				try
				{
					if (InContext.Options.LogPSO)
					{
						foreach (var ThisFile in CommandUtils.FindFiles_NoExceptions(true, "*.rec.upipelinecache", true, DestSavedDir))
						{
							bool Copied = false;
							var JustFile = Path.GetFileName(ThisFile);
							if (JustFile.StartsWith("++"))
							{

								var Parts = JustFile.Split(new Char[] { '+', '-' }).Where(A => A != "").ToArray();
								if (Parts.Count() >= 2)
								{
									string ProjectName = Parts[0].ToString();
									string BuildRoot = CommandUtils.CombinePaths(CommandUtils.RootBuildStorageDirectory());

									string SrcBuildPath = CommandUtils.CombinePaths(BuildRoot, ProjectName);
									string SrcBuildPath2 = CommandUtils.CombinePaths(BuildRoot, ProjectName.Replace("Game", "").Replace("game", ""));

									if (!CommandUtils.DirectoryExists(SrcBuildPath))
									{
										SrcBuildPath = SrcBuildPath2;
									}
									if (CommandUtils.DirectoryExists(SrcBuildPath))
									{
										var JustBuildFolder = JustFile.Replace("-" + Parts.Last(), "");

										string PlatformStr = Platform.ToString();
										string SrcCLMetaPath = CommandUtils.CombinePaths(SrcBuildPath, JustBuildFolder, PlatformStr, "MetaData");
										if (CommandUtils.DirectoryExists(SrcCLMetaPath))
										{
											string SrcCLMetaPathCollected = CommandUtils.CombinePaths(SrcCLMetaPath, "CollectedPSOs");
											if (!CommandUtils.DirectoryExists(SrcCLMetaPathCollected))
											{
												Log.Info("Creating Directory {0}", SrcCLMetaPathCollected);
												CommandUtils.CreateDirectory(SrcCLMetaPathCollected);
											}
											if (CommandUtils.DirectoryExists(SrcCLMetaPathCollected))
											{
												string DestFile = CommandUtils.CombinePaths(SrcCLMetaPathCollected, JustFile);
												CommandUtils.CopyFile_NoExceptions(ThisFile, DestFile, true);
												if (CommandUtils.FileExists(true, DestFile))
												{
													Log.Info("Deleting local file, copied to {0}", DestFile);
													CommandUtils.DeleteFile_NoExceptions(ThisFile, true);
													Copied = true;
												}
											}
										}
									}
								}
							}
							if (!Copied)
							{
								Log.Warning("Could not find anywhere to put this file {0}", JustFile);
							}
						}
					}
				}
				catch (Exception Ex)
				{
					Log.Info("Failed to copy upipelinecaches to the network {0}", Ex);
				}

			}
			// END REMOVEME


			UnrealLogParser LogParser = new UnrealLogParser(InRunningRole.AppInstance.StdOut);

			UnrealRoleArtifacts Artifacts = new UnrealRoleArtifacts(InRunningRole.Role, InRunningRole.AppInstance, InDestArtifactPath, ArtifactLogPath, LogParser);

			return Artifacts;
		}

		/// <summary>
		/// Saves all artifacts from the provided session to the specified output path. 
		/// </summary>
		/// <param name="Context"></param>
		/// <param name="TestInstance"></param>
		/// <param name="OutputPath"></param>
		/// <returns></returns>
		public IEnumerable<UnrealRoleArtifacts> SaveRoleArtifacts(UnrealTestContext Context, UnrealSessionInstance TestInstance, string OutputPath)
		{
			Dictionary<UnrealTargetRole, int> RoleCounts = new Dictionary<UnrealTargetRole, int>();
			int DummyClientCount = 0;

			List<UnrealRoleArtifacts> AllArtifacts = new List<UnrealRoleArtifacts>();

			foreach (UnrealSessionInstance.RoleInstance App in TestInstance.RunningRoles)
			{
				string RoleName = (App.Role.IsDummy() ? "Dummy" : "") + App.Role.RoleType.ToString();
				string FolderName = RoleName;

				int RoleCount = 1;

				if (App.Role.IsDummy())
				{
					DummyClientCount++;
					RoleCount = DummyClientCount;
				}
				else
				{
					if (!RoleCounts.ContainsKey(App.Role.RoleType))
					{
						RoleCounts.Add(App.Role.RoleType, 1);
					}
					else
					{
						RoleCounts[App.Role.RoleType]++;
					}

					RoleCount = RoleCounts[App.Role.RoleType];
				}

				
				if (RoleCount > 1)
				{
					FolderName += string.Format("_{0:00}", RoleCount);
				}

				string DestPath = Path.Combine(OutputPath, FolderName);

				if (!App.Role.IsNullRole() && !App.Role.InstallOnly )
				{
					Log.VeryVerbose("Calling SaveRoleArtifacts, Role: {0}  Artifact Path: {1}", App.ToString(), App.AppInstance.ArtifactPath);
					UnrealRoleArtifacts Artifacts = null;
					ITargetDevice device = App.AppInstance.Device;
					IDeviceUsageReporter.RecordStart(device.Name, device.Platform, IDeviceUsageReporter.EventType.SavingArtifacts);
					try
					{
						Artifacts = SaveRoleArtifacts(Context, App, DestPath);
					}
					catch (Exception SaveArtifactsException)
					{
						// Caught an exception -> report failure
						IDeviceUsageReporter.RecordEnd(device.Name, device.Platform, IDeviceUsageReporter.EventType.SavingArtifacts, IDeviceUsageReporter.EventState.Failure);

						// Retry once only, after rebooting device, if artifacts couldn't be saved.
						if (SaveArtifactsException.Message.Contains("A retry should be performed"))
						{
							Log.Info("Rebooting device and retrying save role artifacts once.");
							App.AppInstance.Device.Reboot();
							Artifacts = SaveRoleArtifacts(Context, App, DestPath);
						}
						else
						{
							// Pass exception to the surrounding try/catch in UnrealTestNode while preserving the original callstack
							throw;
						}
					}
					// Did not catch -> successful reporting
					IDeviceUsageReporter.RecordEnd(device.Name, device.Platform, IDeviceUsageReporter.EventType.SavingArtifacts, IDeviceUsageReporter.EventState.Success);

					if (Artifacts != null)
					{
						AllArtifacts.Add(Artifacts);
					}
				}
				else 
				{
					Log.Verbose("Skipping SaveRoleArtifacts for Null Role: {0}", App.ToString());
				}
			}

			return AllArtifacts;
		}
	}
}