// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Text;
using System.Xml.Serialization;
using System.IO;
using EpicGames.Core;
using UnrealBuildTool;
using AutomationTool;
using System.Linq;
using System.Text.RegularExpressions;

namespace Turnkey
{
	public class CopySource
	{
		// @todo turnkey: for some reason the TypeConverter stuff setup in UnrealTargetPlatform isn't kicking in to convert from string to UTP, so do it a manual way
		[XmlAttribute("HostPlatform")]
		public string HostPlatformString = null;
		[XmlIgnore]
		public UnrealTargetPlatform? Platform = null;

		[XmlAttribute]
		// if this is set, then the actual copy operation will use this,m 
		public string CopyOverride = null;

		[XmlText]
		public string Operation;

		/// <summary>
		/// Needs a parameterless constructor for Xml deserialization
		/// </summary>
		public CopySource()
		{ }

		public CopySource(CopySource Other)
		{
			HostPlatformString = Other.HostPlatformString;
			Platform = Other.Platform;
			CopyOverride = Other.CopyOverride;
			Operation = Other.Operation;
		}

		public string GetOperation()
		{
			return TurnkeyUtils.ExpandVariables(CopyOverride != null ? CopyOverride : Operation);
		}

		public void PostDeserialize()
		{
			if (!string.IsNullOrEmpty(HostPlatformString))
			{
				Platform = UnrealTargetPlatform.Parse(HostPlatformString);
			}

			// perform early expansion, important for $(ThisManifestDir) which is valid only during deserialization
			// but don't use any other variables yet, because UAT could have bad values in Environment
			CopyOverride = TurnkeyUtils.ExpandVariables(CopyOverride, bUseOnlyTurnkeyVariables: true)?.Trim();
			Operation = TurnkeyUtils.ExpandVariables(Operation, bUseOnlyTurnkeyVariables: true)?.Trim();
		}

		public bool ShouldExecute()
		{
			return !Platform.HasValue || Platform == HostPlatform.Current.HostEditorPlatform;
		}
		public bool NeedsExpansion(out string OperationForExpansion)
		{
			const string Prefix = "fileexpansion:";
			bool bNeedsExpansion = ShouldExecute() && Operation.StartsWith(Prefix, StringComparison.InvariantCultureIgnoreCase);

			// get the bit after the expansion tag
			OperationForExpansion = bNeedsExpansion ? Operation.Substring(Prefix.Length) : null;
			return bNeedsExpansion;
		}
		public bool NeedsExpansion()
		{
			return NeedsExpansion(out _);
		}
	}

	public class FileSource
	{
		public enum SourceType
		{
			BuildOnly,
			AutoSdk,
			RunOnly,
			Full,
			Flash,
			Misc,
			Build,
		};

		#region Fields

		[XmlElement("Platform")]
		public string PlatformString = null;
		[XmlIgnore]
		private List<UnrealTargetPlatform> Platforms;

		[XmlElement("Type")]
		public string TypeString = null;
		[XmlIgnore]
		public SourceType Type = SourceType.Full;
		[XmlIgnore]
		public string SpecificSDKType = null;

		[XmlElement("Source")]
		public CopySource[] Sources = null;



		public string Version = null;
		public string Name = null;
		public string AllowedFlashDeviceTypes = null;
		
		// Project for Build types to filter on
		public string Project;
		public string BuildPlatformEnumerationSuffix;


		public static FileSource CreateCodeSpecifiedSource(string Name, string Version, UnrealTargetPlatform Platform)
		{
			FileSource NewSource = new FileSource();
			NewSource.PlatformString = Platform.ToString();
			NewSource.Platforms = new List<UnrealTargetPlatform>() { Platform };
			NewSource.Name = Name;
			NewSource.Version = Version;
			NewSource.Sources = new CopySource[] { };

			return NewSource;
		}

		public FileSource CloneForExpansion(string NewValue, Action<FileSource, string> Setter)
		{
			FileSource Clone = new FileSource();
			Clone.PlatformString = PlatformString;
			Clone.TypeString = TypeString;
			Clone.Version = Version;
			Clone.Name = Name;
			Clone.AllowedFlashDeviceTypes = AllowedFlashDeviceTypes;
			Clone.Project = Project;
			Clone.BuildPlatformEnumerationSuffix = BuildPlatformEnumerationSuffix;

			if (NewValue != null)
			{
				Setter(Clone, NewValue);
			}

			if (Sources != null)
			{
				List<CopySource> NewSources = new List<CopySource>();
				foreach (CopySource Source in Sources)
				{
					// if we want to execute the installer, then copy it over
					if (Source.ShouldExecute())
					{
						NewSources.Add(new CopySource(Source));
					}
				}
				Clone.Sources = NewSources.ToArray();
			}

			Clone.PostDeserialize();

			return Clone;
		}

		#endregion
		// 
		// 
		// 		static string[] ExtendedVariables =
		// 		{
		// 			"Project",
		// 		};
		// 
		// 
		// 		[Flags]
		// 		public enum LocalAvailability
		// 		{
		// 			None = 0,
		// 			AutoSdk_VariableExists = 1,
		// 			AutoSdk_ValidVersionExists = 2,
		// 			AutoSdk_InvalidVersionExists = 4,
		// 			// InstalledSdk_BuildOnlyWasInstalled = 8,
		// 			InstalledSdk_ValidVersionExists = 16,
		// 			InstalledSdk_InvalidVersionExists = 32,
		// 			Platform_ValidHostPrerequisites = 64,
		// 			Platform_InvalidHostPrerequisites = 128,
		// 		}
		// 
		// 
		// 

		public bool NeedsFileExpansion()
		{
			return Sources.Any(x => x.NeedsExpansion());
		}

		public bool SupportsPlatform(UnrealTargetPlatform Platform)
		{
			return Platforms.Contains(Platform);
		}

		public bool IsSdkType()
		{
			return Type == SourceType.BuildOnly || Type == SourceType.AutoSdk || Type == SourceType.RunOnly || Type == SourceType.Full || Type == SourceType.Flash;
		}

		public bool IsVersionValid(UnrealTargetPlatform Platform, DeviceInfo Device=null)
		{
			// Non-Sdk types are always valid, although this isn't meant for them
			if (!IsSdkType())
			{
				return true;
			}

			if (!SupportsPlatform(Platform))
			{
				return false;
			}

// 			if (!CheckForExtendedVariables(false))
// 			{
// 				// user canceled a choice
// 				return false;
// 			}
// 
			if (Type == SourceType.Flash)
			{
				UEBuildPlatformSDK SDK = UEBuildPlatformSDK.GetSDKForPlatform(Platform.ToString());
				// if we are specific to any particular device type, use one of them to pass to the validation function (different device types may have different SDK versions)
				string DeviceTypeHint = string.IsNullOrEmpty(AllowedFlashDeviceTypes) ? null : AllowedFlashDeviceTypes.Split(",").First();
				bool bIsValid = SDK.IsSoftwareVersionValid(Version, DeviceTypeHint);
				// if we were passed a device, also check if this Sdk is valid for that device
				if (Device != null)
				{
					bIsValid &= TurnkeyUtils.IsValueValid(Device.Type, AllowedFlashDeviceTypes, AutomationTool.Platform.GetPlatform(Platform));
				}
				return bIsValid;
			}
			else
			{
				string SDKTypeHint = SpecificSDKType ?? Type.ToString();
				return UEBuildPlatformSDK.GetSDKForPlatform(Platform.ToString()).IsVersionValid(Version, SDKTypeHint);
			}
		}

		public UnrealTargetPlatform[] GetPlatforms()
		{
			return Platforms.ToArray();
		}


		public string GetCopySourceOperation()
		{
			// use the one matching copy source for this host platform, or null if there isn't one
			return Sources.FirstOrDefault(x => x.ShouldExecute())?.GetOperation();
		}

		static public FileSource ChooseBest(List<FileSource> Sdks, UEBuildPlatformSDK PlatformSDK)
		{
			if (Sdks == null)
			{
				return null;
			}

			FileSource Best = null;
			UInt64 MainVersionInt;
			PlatformSDK.TryConvertVersionToInt(PlatformSDK.GetMainVersion(), out MainVersionInt);

			foreach (FileSource Sdk in Sdks)
			{
				if (Best == null)
				{
					Best = Sdk;
				}
				else
				{
					// bigger version is better
					UInt64 ThisVersion, BestVersion;
					if (PlatformSDK.TryConvertVersionToInt(Sdk.Version, out ThisVersion) && PlatformSDK.TryConvertVersionToInt(Best.Version, out BestVersion))
					{
						// there is no MainVersion for flash (yet), so don't take it into account
						if (Sdk.Type == SourceType.Flash)
						{
							if (ThisVersion > BestVersion)
							{
								Best = Sdk;
							}
						}
						// always use MainVersion, otherwise, use largest version if MainVersion hasn't been found
						else if (ThisVersion == MainVersionInt || (BestVersion != MainVersionInt && ThisVersion > BestVersion))
						{
							Best = Sdk;
						}
					}
				}
			}

			return Best;
		}

		static public FileSource FindMatchingSdk(AutomationTool.Platform Platform, SourceType[] TypePriority, bool bSelectBest, string DeviceType = null, string CurrentSdk = null, string SpecificType = null)
		{
			UEBuildPlatformSDK SDK = UEBuildPlatformSDK.GetSDKForPlatform(Platform.PlatformType.ToString());

			foreach (SourceType Type in TypePriority)
			{
				List<FileSource> Sdks = TurnkeyManifest.FilterDiscoveredFileSources(Platform.IniPlatformType, Type);

				// check valid versions/device types
				//				Sdks = Sdks.FindAll(x => x.Version == null || (Type == SourceType.Flash ? TurnkeyUtils.IsValueValid(x.Version, Platform.GetAllowedSoftwareVersions(), Platform) : SDK.IsVersionValid(x.Version, bForAutoSDK: x.Type == SourceType.AutoSdk)));
				Sdks = Sdks.FindAll(x => x.Version == null ||
					(Type == SourceType.Flash ?
						SDK.IsSoftwareVersionValid(x.Version, DeviceType) :
						SDK.IsVersionValid(x.Version, (bSelectBest && x.Type == SourceType.AutoSdk) ? "AutoSDK" : SpecificType ?? "Sdk")
					));

				if (DeviceType != null)
				{
					Sdks = Sdks.FindAll(x => TurnkeyUtils.IsValueValid(DeviceType, x.AllowedFlashDeviceTypes, Platform));
				}

				if (SpecificType != null)
				{
					Sdks = Sdks.FindAll(x => TurnkeyUtils.IsValueValid(SpecificType, x.SpecificSDKType, Platform));
				}

				// if none were found try next type
				if (Sdks.Count == 0)
				{
					continue;
				}

				// if one was found return it!
				if (Sdks.Count == 1)
				{
					return Sdks[0];
				}

				// the best for a AutoSdk is the one that matches the desired version exactly
				FileSource Best = ChooseBest(Sdks, SDK);

				if (bSelectBest)
				{
					// select best one if requested
					return Best;
				}

				// find the current sdk, if any
				FileSource Current = null;
				if (CurrentSdk != null)
				{
					Current = Sdks.Find( x => x.Version == CurrentSdk);
				}

				// helper for getting the display name for the sdks as they are presented to the user
				string GetDisplayName( FileSource Sdk )
				{
					string Result = Sdk.Name;
					if (Current != null && Sdk == Current) 
					{
						Result += " (current)";
					}
					if (Sdk == Best)
					{
						Result += " [Best Choice]";
					}
					return Result;
				}


				// if unable to pick one automatically, ask the user
				int BestIndex = Sdks.IndexOf(Best);
				int Choice = TurnkeyUtils.ReadInputInt("Multiple Sdks found that could be installed. Please select one:", Sdks.Select(x => GetDisplayName(x)).ToList(), true, BestIndex >= 0 ? BestIndex + 1 : -1);

				// we take canceling to mean to try the next type
				if (Choice == 0)
				{
					continue;
				}

				return Sdks[Choice - 1];
			}


			// if we never found anything, fail
			return null;
		}

		public bool DownloadOrInstall(UnrealTargetPlatform Platform, ITurnkeyContext TurnkeyContext, DeviceInfo Device, bool bUnattended, bool bSdkAlreadyInstalled)
		{
			// standard variables
			TurnkeyUtils.SetVariable("Platform", Platform.ToString());
			TurnkeyUtils.SetVariable("Version", Version);

			if (Type == SourceType.AutoSdk)
			{
				// AutoSdk has some extra setup needed
				return SdkUtils.SetupAutoSdk(this, TurnkeyContext, Platform, bUnattended);
			}

			// if we have sources, make sure we can download something. if we have a null Sources, that indicates
			// it was code generated, and we don't need to download anything
			if (Sources.Length != 0)
			{
				// this will set the $(CopyOutputPath) variable which the Sdks will generally need to use
				string DownloadedSDK = TurnkeyContext.RetrieveFileSource(this);

				if (string.IsNullOrEmpty(DownloadedSDK))
				{
//					TurnkeyContext.ReportError($"Unable to download anInstaller for {Platform}. Your Studio's TurnkeyManifest.xml file(s) may need to be fixed.");
					return false;
				}
			}

			// let the platform decide how to install
			return AutomationTool.Platform.GetPlatform(Platform).InstallSDK(TurnkeyUtils.CommandUtilHelper, TurnkeyContext, Device, bUnattended, bSdkAlreadyInstalled);
		}







		// 
		// 		// for all platforms this supports, get all devices
		// 		public DeviceInfo[] GetAllPossibleDevices()
		// 		{
		// 			List<DeviceInfo> AllDevices = new List<DeviceInfo>();
		// 			foreach (var Pair in AutomationPlatforms)
		// 			{
		// 				DeviceInfo[] Devices = Pair.Value.GetDevices();
		// 				if (Devices != null)
		// 				{
		// 					AllDevices.AddRange(Devices);
		// 				}
		// 			}
		// 			return AllDevices.ToArray();
		// 		}
		// 
		// 		public DeviceInfo GetDevice(UnrealTargetPlatform Platform, string DeviceName)
		// 		{
		// 			DeviceInfo[] Devices = AutomationPlatforms[Platform].GetDevices();
		// 			if (Devices != null)
		// 			{
		// 				return Array.Find(AutomationPlatforms[Platform].GetDevices(), y => (DeviceName == null && y.bIsDefault) || (DeviceName != null && string.Compare(y.Name, DeviceName, true) == 0));
		// 			}
		// 			return null;
		// 		}

		// 		public bool IsValid(UnrealTargetPlatform Platform, string DeviceName = null)
		// 		{
		// 			if (!SupportsPlatform(Platform))
		// 			{
		// 				return false;
		// 			}
		// 
		// 			if (!CheckForExtendedVariables(false))
		// 			{
		// 				// user canceled a choice
		// 				return false;
		// 			}
		// 
		// 			if (Type == SourceType.Flash)
		// 			{
		// 				bool bIsValid = TurnkeyUtils.IsValueValid(Version, AutomationPlatforms[Platform].GetAllowedSoftwareVersions(), AutomationPlatforms[Platform]);
		// 				// if we were passed a device, also check if this Sdk is valid for that device
		// 				//				if (DeviceName != null)
		// 				{
		// 					DeviceInfo Device = GetDevice(Platform, DeviceName);
		// 					bIsValid = bIsValid && Device != null && TurnkeyUtils.IsValueValid(Device.Type, AllowedFlashDeviceTypes, AutomationPlatforms[Platform]);
		// 				}
		// 				return bIsValid;
		// 			}
		// 			else
		// 			{
		// 				return UEBuildPlatformSDK.GetSDKForPlatform(Platform.ToString()).IsVersionValid(Version, bForAutoSDK:(Type == SourceType.AutoSdk));
		// 			}
		// 		}
		// 
		// 		public bool Install(UnrealTargetPlatform Platform, DeviceInfo Device=null, bool bUnattended=false, bool bSkipAutoSdkSetup=false)
		// 		{
		// 			if (Type ==SourceType.AutoSdk && !bSkipAutoSdkSetup)
		// 			{
		// 				// AutoSdk has some extra setup needed
		// 				if (SdkInfo.ConditionalSetupAutoSdk(this, Platform, bUnattended))
		// 				{
		// 					return true;
		// 				}
		// 			}
		// 
		// 			if (!CheckForExtendedVariables(true))
		// 			{
		// 				// user canceled a choice
		// 				return false;
		// 			}
		// 
		// 			// standard variables
		// 			TurnkeyUtils.SetVariable("Platform", Platform.ToString());
		// 			TurnkeyUtils.SetVariable("Version", Version);
		// 
		// 			if (Device != null)
		// 			{
		// 				TurnkeyUtils.SetVariable("DeviceName", Device.Name);
		// 				TurnkeyUtils.SetVariable("DeviceId", Device.Id);
		// 				TurnkeyUtils.SetVariable("DeviceType", Device.Type);
		// 			}
		// 
		// 			// custom sdk installation is enabled if ChosenSdk.CustomVersionId is !null
		// 			if (CustomSdkId != null)
		// 			{
		// 				// copy files down, which are needed to check if whatever is installed is up to date (only do it once)
		// 				if (CustomVersionLocalFiles == null && CustomSdkInputFiles != null)
		// 				{
		// 					foreach (CopyAndRun CustomCopy in CustomSdkInputFiles)
		// 					{
		// 						if (CustomCopy.ShouldExecute())
		// 						{
		// 							if (CustomVersionLocalFiles != null)
		// 							{
		// 								throw new AutomationTool.AutomationException("CustomSdkInputFiles specified multiple locations to be copied for this platform, which is not supported (only one value of $(CustomVersionLocalFiles) allowed)");
		// 							}
		// 
		// 							CustomCopy.Execute();
		// 							CustomVersionLocalFiles = TurnkeyUtils.GetVariableValue("CopyOutputPath");
		// 						}
		// 					}
		// 				}
		// 
		// 				// in case the custom sdk modifies global env vars, make sure we capture them
		// 				TurnkeyUtils.StartTrackingExternalEnvVarChanges();
		// 
		// 				// re-set the path to local files
		// 				TurnkeyUtils.SetVariable("CustomVersionLocalFiles", CustomVersionLocalFiles);
		// 				AutomationPlatforms[Platform].CustomVersionUpdate(CustomSdkId, TurnkeyUtils.ExpandVariables(CustomSdkParams), new CopyProviderRetriever());
		// 
		// 				TurnkeyUtils.EndTrackingExternalEnvVarChanges();
		// 			}
		// 
		// 			// now run any installers
		// 			if (Installers == null)
		// 			{
		// 				// if there were no installers, then we succeeded
		// 				return true;
		// 			}
		// 
		// 			bool bSucceeded = true;
		// 			foreach (CopyAndRun Install in Installers)
		// 			{
		// 				// build only types we keep in a nice directory structure in the PermanentStorage (if the copy provider allows for choosing destination)
		// 				if (Type ==SourceType.BuildOnly)
		// 				{
		// 					// download subdir is based on platform 
		// 					string SubDir = string.Format("{0}/{1}", Platform.ToString(), Version);
		// 					bSucceeded = bSucceeded && Install.Execute(CopyExecuteSpecialMode.UsePermanentStorage, SubDir);
		// 				}
		// 				else
		// 				{
		// 					// !@todo turnkey : verify it worked
		// 					bSucceeded = bSucceeded && Install.Execute();
		// 				}
		// 			}
		// 
		// 			return bSucceeded;
		// 		}
		// 
		// 		public static bool ConditionalSetupAutoSdk(SdkInfo Sdk, UnrealTargetPlatform Platform, bool bUnattended)
		// 		{
		// 			bool bAttemptAutoSdkSetup = false;
		// 			bool bSetupEnvVarAfterInstall = false;
		// 			if (Environment.GetEnvironmentVariable("UE_SDKS_ROOT") != null)
		// 			{
		// 				bAttemptAutoSdkSetup = true;
		// 			}
		// 			else
		// 			{
		// 				if (!bUnattended)
		// 				{
		// 					// @todo turnkey - have studio settings 
		// 					bool bResponse = TurnkeyUtils.GetUserConfirmation("AutoSdks are not setup, but your studio has support. Would you like to set it up now?", true);
		// 					if (bResponse)
		// 					{
		// 						bAttemptAutoSdkSetup = true;
		// 						bSetupEnvVarAfterInstall = true;
		// 					}
		// 				}
		// 			}
		// 
		// 			if (bAttemptAutoSdkSetup)
		// 			{
		// 				AutomationTool.Platform AutomationPlatform = AutomationTool.Platform.GetPlatform(Platform);
		// 
		// 				TurnkeyUtils.Log("{0}: AutoSdk is setup on this computer, will look for available AutoSdk to download", Platform);
		// 
		// 				// make sure this is unset so that we can know if it worked or not after install
		// 				TurnkeyUtils.ClearVariable("CopyOutputPath");
		// 
		// 				// now download it (AutoSdks don't "install") on download
		// 				// @todo turnkey: handle errors, handle p4 going to wrong location, handle one Sdk for multiple platforms
		// 				Sdk.Install(Platform, null, bUnattended, bSkipAutoSdkSetup:true);
		// 
		// 				if (bSetupEnvVarAfterInstall)
		// 				{
		// 
		// 					// this is where we synced the Sdk to
		// 					string InstalledRoot = TurnkeyUtils.GetVariableValue("CopyOutputPath");
		// 
		// 					// failed to install, nothing we can do
		// 					if (string.IsNullOrEmpty(InstalledRoot))
		// 					{
		// 						TurnkeyUtils.ExitCode = ExitCode.Error_SDKNotFound;
		// 						return false;
		// 					}
		// 
		// 					// walk up to one above Host* directory
		// 					DirectoryInfo AutoSdkSearch;
		// 					if (Directory.Exists(InstalledRoot))
		// 					{
		// 						AutoSdkSearch = new DirectoryInfo(InstalledRoot);
		// 					}
		// 					else
		// 					{
		// 						AutoSdkSearch = new FileInfo(InstalledRoot).Directory;
		// 					}
		// 					while (AutoSdkSearch.Name != "Host" + HostPlatform.Current.HostEditorPlatform.ToString())
		// 					{
		// 						AutoSdkSearch = AutoSdkSearch.Parent;
		// 					}
		// 
		// 					// now go one up to the parent of Host
		// 					AutoSdkSearch = AutoSdkSearch.Parent;
		// 
		// 					string AutoSdkDir = AutoSdkSearch.FullName;
		// 					if (!bUnattended)
		// 					{
		// 						string Response = TurnkeyUtils.ReadInput("Enter directory for root of AutoSdks. Use detected value, or enter another:", AutoSdkSearch.FullName);
		// 						if (string.IsNullOrEmpty(Response))
		// 						{
		// 							return false;
		// 						}
		// 					}
		// 
		// 					// set the env var, globally
		// 					TurnkeyUtils.StartTrackingExternalEnvVarChanges();
		// 					Environment.SetEnvironmentVariable("UE_SDKS_ROOT", AutoSdkDir);
		// 					Environment.SetEnvironmentVariable("UE_SDKS_ROOT", AutoSdkDir, EnvironmentVariableTarget.User);
		// 					TurnkeyUtils.EndTrackingExternalEnvVarChanges();
		// 
		// 				}
		// 
		// 				// and now activate it in case we need it this run
		// 				TurnkeyUtils.Log("Re-activating AutoSDK '{0}'...", Sdk.DisplayName);
		// 
		// 				EpicGames.Core.UEBuildPlatformSDK.GetSDKForPlatform(Platform.ToString()).ReactivateAutoSDK();
		// 
		// 				return true;
		// 			}
		// 
		// 			return false;
		// 		}
		// 
		// 
		// 
		// 
		// 
		// 
		// 
		// 
		// 		private bool CheckForExtendedVariables(bool bIncludeCustomSdk)
		// 		{
		// 			foreach (string Var in ExtendedVariables)
		// 			{
		// 				if (!TurnkeyUtils.HasVariable(Var))
		// 				{
		// 					if (NeedsVariableToBeSet(Var, bIncludeCustomSdk))
		// 					{
		// 						// ask for it!
		// 						TurnkeyUtils.Log("An Sdk ({0}) needs a extended variable ({1}) to be set that isn't set. Asking now...", DisplayName, Var);
		// 						
		// 						if (Var == "Project")
		// 						{
		// 							// we need a project, so choose one now
		// 							List<string> Options = new List<string>();
		// 							Options.Add("Engine");
		// 							Options.Add("FortniteGame");
		// 
		// 							// we force the user to select something
		// 							int Choice = TurnkeyUtils.ReadInputInt("Select a Project:", Options, true);
		// 
		// 							if (Choice == 0)
		// 							{
		// 								return false;
		// 							}
		// 
		// 							// set the projectname
		// 							TurnkeyUtils.SetVariable(Var, Options[Choice - 1]);
		// 						}
		// 					}
		// 				}
		// 			}
		// 
		// 			return true;
		// 		}
		// 		 
		// 		private bool NeedsVariableToBeSet(string Variable, bool bIncludeCustomSdk)
		// 		{
		// 			string Format = "$(" + Variable + ")";
		// 			bool bContainsVar = false;
		// 			if (bIncludeCustomSdk && CustomSdkId != null)
		// 			{
		// 				bContainsVar = bContainsVar || (CustomSdkParams != null && CustomSdkParams.Contains(Format));
		// 				if (CustomSdkInputFiles != null)
		// 				{
		// 					foreach (CopyAndRun CustomInput in CustomSdkInputFiles)
		// 					{
		// 						bContainsVar = bContainsVar || (CustomInput.Copy != null && CustomInput.Copy.Contains(Format));
		// 						bContainsVar = bContainsVar || (CustomInput.CommandPath != null && CustomInput.CommandPath.Contains(Format));
		// 						bContainsVar = bContainsVar || (CustomInput.CommandLine != null && CustomInput.CommandLine.Contains(Format));
		// 					}
		// 				}
		// 			}
		// 			if (Installers != null)
		// 			{
		// 				foreach (CopyAndRun Installer in Installers)
		// 				{
		// 					bContainsVar = bContainsVar || (Installer.Copy != null && Installer.Copy.Contains(Format));
		// 					bContainsVar = bContainsVar || (Installer.CommandPath != null && Installer.CommandPath.Contains(Format));
		// 					bContainsVar = bContainsVar || (Installer.CommandLine != null && Installer.CommandLine.Contains(Format));
		// 				}
		// 			}
		// 
		// 			return bContainsVar;
		// 		}
		// 
		// 
		// 
		// 


		static private Regex ExpansionRegex = new Regex(@"^listexpansion:(\w*)=(.*)$");
		private List<FileSource> CheckForListExpansions(List<FileSource> Sources, Func<FileSource, string> Getter, Action<FileSource, string> Setter)
		{
			List<FileSource> NewSources = new List<FileSource>();
			foreach (FileSource Source in Sources)
			{
				string Value = Getter(Source);
				if (Value == null)
				{
					NewSources.Add(Source);
					continue;
				}

				// get the value in question from the lambda, and see if it's what we are looking for
				Match Result = ExpansionRegex.Match(Value);// Getter(Source));

				if (Result.Success)
				{
					// get the variable we are going to replace in the expansions
					string ExpansionVar = Result.Groups[1].Value;
					// remember the old value, in case it had one
					string OldVarValue = TurnkeyUtils.GetVariableValue(ExpansionVar);

					string[] NewValues = TurnkeyUtils.ExpandVariables(Result.Groups[2].Value).Split(",".ToCharArray());

					foreach(string NewValue in NewValues)
					{
						TurnkeyUtils.SetVariable(ExpansionVar, NewValue);

						// now when we make a clone, it will have the ExpansionVar set, and will be used
						FileSource NewSource = Source.CloneForExpansion(NewValue, Setter);
						NewSources.Add(NewSource);
					}

					// and retore it
					TurnkeyUtils.SetVariable(ExpansionVar, OldVarValue);
				}
				else
				{
					NewSources.Add(Source);
				}
			}

			return NewSources;
		}

		internal List<FileSource> ConditionalExpandLists()
		{
			List<FileSource> Expansions = new List<FileSource>() { this };

			Expansions = CheckForListExpansions(Expansions, x => x.PlatformString, (x,y) => x.PlatformString = y);
			Expansions = CheckForListExpansions(Expansions, x => x.Version, (x, y) => x.Version = y);
			Expansions = CheckForListExpansions(Expansions, x => x.AllowedFlashDeviceTypes, (x, y) => x.AllowedFlashDeviceTypes = y);
			Expansions = CheckForListExpansions(Expansions, x => x.Project, (x, y) => x.Project = y);

			// return null if nothing was actually expanded!
			if (Expansions.Count == 1 && Expansions[0] == this)
			{
				return null;
			}

			return Expansions;
		}
		internal List<FileSource> ExpandCopySource()
		{
			List<FileSource> ResultExpansions = new List<FileSource>();

			// get Expansions for this host platform
			CopySource ExpandingSource = null;
			string ExpansionOperation = null;
			foreach (CopySource Source in Sources)
			{
				if (Source.NeedsExpansion(out ExpansionOperation))
				{
					if (ExpandingSource != null)
					{
						throw new AutomationTool.AutomationException("FileSource {0} had multiple expansions active on this platform. This is not allowed", Name);
					}
					ExpandingSource = Source;
				}
			}

			// something wasn't right, leave
			if (ExpansionOperation == null)
			{
				return ResultExpansions;
			}

			// fixup the Operation like so:
			//    fileexpansion:perforce://depot/CarefullyRedist/HostWin64/SomePlatform/$[ExpVersion]/Setup.* 
			//  becomes:
			//    perforce://depot/CarefullyRedist/HostWin64/SomePlatform/*/Setup.* 
			string FixedSourceOperation = ExpansionOperation;
			int CaptureLocation;
			Dictionary<int, string> LocationToVariableMap = new Dictionary<int, string>();
			while ((CaptureLocation = ExpansionOperation.IndexOf("$[")) != -1)
			{
				int CaptureEnd = ExpansionOperation.IndexOf("]", CaptureLocation);
				if (CaptureEnd == -1)
				{
					throw new AutomationTool.AutomationException("FileSource operation {0} had malformed capture variables", ExpandingSource.Operation);
				}
				string Variable = ExpansionOperation.Substring(CaptureLocation + 2, (CaptureEnd - CaptureLocation) - 2);

				// track the location to figure out the index of the *  (the index will be index of the *)
				LocationToVariableMap.Add(CaptureLocation, Variable);
				string CaptureVar = string.Format("$[{0}]", Variable);
				ExpansionOperation = ExpansionOperation.Replace(CaptureVar, "*");
				FixedSourceOperation = FixedSourceOperation.Replace(CaptureVar, string.Format("$({0})", Variable));
			};
			ExpandingSource.Operation = FixedSourceOperation;

			// now go back and figure out the index of the variables
			int StarLocation = -1;
			int StarIndex = 0;
			Dictionary<int, string> StarIndexToVariableMap = new Dictionary<int, string>();
			while ((StarLocation = ExpansionOperation.IndexOf("*", StarLocation + 1)) != -1)
			{
				string Variable;
				if (LocationToVariableMap.TryGetValue(StarLocation, out Variable))
				{
					StarIndexToVariableMap.Add(StarIndex, Variable);
				}
				StarIndex++;
			}

			// now enumerate and get the values
			List<List<string>> Expansions = new List<List<string>>();
			string[] ExpandedInstallerResults = CopyProvider.ExecuteEnumerate(ExpansionOperation, Expansions);

			// expansion may not work, expand it to nothing
			if (ExpandedInstallerResults == null)
			{
				return ResultExpansions;
			}

			if (Expansions.Count != ExpandedInstallerResults.Length)
			{
				throw new AutomationException(string.Format("Bad expansions output from CopyProvider ({0} returned {1} count, expected {2}, from {3}",
					ExpansionOperation, Expansions.Count, ExpandedInstallerResults.Length, string.Join(", ", ExpandedInstallerResults)));
			}

			// @todo turnkey: this will be used in Builds also, make it a function with a lambda
			// make a new SdkInfo for each expansion
			int MaxIndex = 0;
			for (int ResultIndex = 0; ResultIndex < ExpandedInstallerResults.Length; ResultIndex++)
			{
				// set captured variables to the values returned from the enumeration
				TurnkeyUtils.SetVariable("Expansion", ExpandedInstallerResults[ResultIndex]);
				// @todo turnkey: if there were multiple captures for the same variable name, make sure they are the same value: googledrive:/Foo/[$Ver]/Bar_[$Ver].zip 
				for (int ExpansionIndex = 0; ExpansionIndex < Expansions[ResultIndex].Count; ExpansionIndex++)
				{
					string Variable;
					if (StarIndexToVariableMap.TryGetValue(ExpansionIndex, out Variable))
					{
						TurnkeyUtils.SetVariable(Variable, Expansions[ResultIndex][ExpansionIndex]);
					}
				}

				// remember how many we beed to unset
				MaxIndex = Math.Max(MaxIndex, Expansions[ResultIndex].Count);

				// make a new Sdk for each result in the expansion
				ResultExpansions.Add(CloneForExpansion(null, null));
			}

			// clear temp variables
			TurnkeyUtils.ClearVariable("Expansion");
			StarIndexToVariableMap.Values.ToList().ForEach(x => TurnkeyUtils.ClearVariable(x));

			return ResultExpansions;

		}


		internal void PostDeserialize()
		{
			// if the Type is a platform specific type, then 
			if (!Enum.TryParse<SourceType>(TypeString, out Type))
			{
				Type = SourceType.Full;
				SpecificSDKType = TypeString;
			}

			// validate
			if (Version == null && IsSdkType())
			{
				throw new AutomationTool.AutomationException("FileSource {0} needs to have a version specified, since it's an Sdk type", Name);
			}
			if (Sources == null)
			{
				throw new AutomationTool.AutomationException("FileSource {0} has no acutal Source operations specified! This is a setup error.", Name);
			}

			PlatformString = TurnkeyUtils.ExpandVariables(PlatformString, true);
			Platforms = new List<UnrealTargetPlatform>();

			if (PlatformString != null)
			{
				string[] PlatformStrings = PlatformString.ToLower().Split(",".ToCharArray());
				// parse into runtime usable values
				foreach (string Plat in PlatformStrings)
				{
					if (UnrealTargetPlatform.IsValidName(Plat))
					{
						UnrealTargetPlatform TargetPlat = UnrealTargetPlatform.Parse(Plat);
						Platforms.Add(TargetPlat);
					}
					else
					{
						// allow for shared SDK "platform" types (we use the AutoSDK platform name in the SDK object to determine the shared SDK "platform"
						foreach (UEBuildPlatformSDK SDK in UEBuildPlatformSDK.AllPlatformSDKObjects)
						{
							// if the platforms contains a AutoSDK platform that doesn't match a real platform name, add the platform to the set
							if (PlatformStrings.Contains(SDK.GetAutoSDKPlatformName().ToLower()))
							{
								Platforms.Add(UnrealTargetPlatform.Parse(SDK.PlatformName));
							}
						}
					}
				}
			}


			Version = TurnkeyUtils.ExpandVariables(Version, true);
			Name = TurnkeyUtils.ExpandVariables(Name, true);

			if (string.IsNullOrEmpty(Name))
			{
				Name = string.Format("{0} {1}", PlatformString, Version);
			}

			AllowedFlashDeviceTypes = TurnkeyUtils.ExpandVariables(AllowedFlashDeviceTypes, true);

			Array.ForEach(Sources, x => x.PostDeserialize());
		}


		private string Indent(int Num)
		{
			return new string(' ', Num);
		}

		public override string ToString()
		{
			return ToString(0);
		}
		public string ToString(int BaseIndent)
		{
			StringBuilder Builder = new StringBuilder();
			Builder.AppendLine("{1}Name: {0}", Name, Indent(BaseIndent));
			Builder.AppendLine("{1}Version: {0}", Version == null ? "<Any>" : Version, Indent(BaseIndent + 2));
			Builder.AppendLine("{1}Platform: {0}", Platforms == null ? "" : string.Join(",", Platforms), Indent(BaseIndent + 2));
			Builder.AppendLine("{1}Type: {0}", Type, Indent(BaseIndent + 2));
			if (Type == SourceType.Flash)
			{
				Builder.AppendLine("{1}AllowedFlashDeviceTypes: {0}", AllowedFlashDeviceTypes, Indent(BaseIndent + 2));
			}
			Builder.AppendLine("{0}Installers:", Indent(BaseIndent + 2));
			foreach (CopySource Copy in Sources)
			{
				Builder.AppendLine("{1}HostPlatform: {0}", Copy.Platform, Indent(BaseIndent + 4));
				Builder.AppendLine("{1}CopyOperation: {0}", Copy.Operation, Indent(BaseIndent + 6));
				if (Copy.CopyOverride != null)
				{
					Builder.AppendLine("{1}CopyOverride: {0}", Copy.CopyOverride, Indent(BaseIndent + 6));
				}
			}
			return Builder.ToString().TrimEnd();

		}
	}
}
