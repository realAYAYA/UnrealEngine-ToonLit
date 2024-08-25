// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.Diagnostics.CodeAnalysis;
using System.IO;
using System.Linq;
using System.Text;
using System.Text.Json;
using Microsoft.Extensions.Logging;
using UnrealBuildBase;
using EpicGames.Core;
using System.Runtime.InteropServices;

namespace EpicGames.Core
{
	/// <summary>
	/// SDK installation status
	/// </summary>
	public enum SDKStatus
	{
		/// <summary>
		/// Desired SDK is installed and set up.
		/// </summary>
		Valid,

		/// <summary>
		/// Could not find the desired SDK, SDK setup failed, etc.		
		/// </summary>
		Invalid,
	};

	/// <summary>
	/// Range of SDK versions that are valid for a platform
	/// </summary>
	public class SDKDescriptor
	{
		public string Name;
		public string? Min;
		public string? Max;
		public string? Current;

		// for SDKs where you need 1 of a set of SDKs (for instance, multiple toolchains for one platform), give them all the same GroupName
		// and Turnkey will make sure that at least one of them is installed for the SDK setup to be valid. Leave null otherwise
		public string? GroupName = null;

		public UInt64 MinInt = 0;
		public UInt64 MaxInt = UInt64.MaxValue;
		public UInt64 CurrentInt = 0;

		public SDKStatus Validity = SDKStatus.Invalid;

		public SDKDescriptor()
		{
			Name = "Unknown";
		}

		public SDKDescriptor(string InName, string? InMin, string? InMax, string? InCurrent, string? InGroupName, SDKCollection? Collection)
		{
			Name = InName;
			Min = InMin;
			Max = InMax;
			Current = InCurrent;
			GroupName = InGroupName;

			if (Collection != null)
			{
				MinInt = Collection.PlatformSDK.ConvertVersionToInt(Min, Name);
				// null will convert to 0, but we want MaxValue on null
				MaxInt = Max == null ? UInt64.MaxValue : Collection.PlatformSDK.ConvertVersionToInt(Max, Name);
				CurrentInt = Collection.PlatformSDK.ConvertVersionToInt(Current, Name);
				UpdateValidity(Collection);
			}
		}


		public SDKDescriptor(string InName, string? InMin, string? InMax)
			: this(InName, InMin, InMax, null, null, null)
		{
		}

		public SDKDescriptor(string InName, string? InCurrent)
			: this(InName, null, null, InCurrent, null, null)
		{
		}

		public void UpdateCurrent(string InCurrent, string? Hint,  SDKCollection? Collection)
		{
			Current = InCurrent;
			if (Collection != null)
			{
				CurrentInt = Collection.PlatformSDK.ConvertVersionToInt(Current, Hint);
				UpdateValidity(Collection);
			}
		}

		private void UpdateValidity(SDKCollection Collection)
		{
			// instead of just checking the range numerically, we allow platforms to override the IsValid check, which makes this a little convoluted (calling back into the Collection 
			// so it can call back into the PlatformSDK with the proper SDK vs Software function)
			Validity = (Current != null && Collection.IsValid(Current, this)) ? SDKStatus.Valid : SDKStatus.Invalid;
		}

		public string ToString(bool bIncludeCurrent=true)
		{
			return $"MinVersion={Min ?? ""}, MaxVersion={Max ?? ""}" + (bIncludeCurrent ? $", Current={Current ?? ""}" : "");
		}
		public string ToString(string Descriptor, string? CurrentDescriptor, bool bIncludeName)
		{
			string DisplayName = bIncludeName ? "_" + Name : "";

			// if they are different, print something like:
			//   MinAllowed_Toolchain=1.0, MaxAllowed_Toolchain=2.0
			if (Min != Max)
			{
				return $"Min{Descriptor}{DisplayName}={Min ?? ""}, Max{Descriptor}{DisplayName}={Max ?? ""}" + (CurrentDescriptor != null ? $", Current{CurrentDescriptor}{DisplayName}={Current ?? ""}" : "");
			}
			// if they are the same, print something like:
			//   Allowed_Toolchain=1.0
			return $"{Descriptor}{DisplayName}={Min}" + (CurrentDescriptor != null ? $", Current{CurrentDescriptor}{DisplayName}={Current ?? ""}" : "");
		}
	}
	public class SDKCollection
	{
		public virtual string DefaultName { get { return "Sdk"; } }
		public virtual bool IsValid(string Version, SDKDescriptor Info)
		{
			return PlatformSDK.IsVersionValid(Version, Info);
		}



		public UEBuildPlatformSDK PlatformSDK;
		public List<SDKDescriptor> Sdks = new List<SDKDescriptor>();
		public List<SDKDescriptor> FullSdks
		{ get { return Sdks.Where(x => string.Compare(x.Name, "AutoSdk", true) != 0).ToList(); } }
		public SDKDescriptor? AutoSdk
		{ get { return Sdks.FirstOrDefault(x => string.Compare(x.Name, "AutoSdk", true) == 0); } }


		public SDKCollection(UEBuildPlatformSDK PlatformSDK)
		{
			this.PlatformSDK = PlatformSDK;
		}

		public SDKCollection(SDKCollection Other, UEBuildPlatformSDK PlatformSDK)
			: this(PlatformSDK)
		{
			foreach (SDKDescriptor Desc in Other.Sdks)
			{
				SetupSDK(Desc.Name, Desc.Min, Desc.Max, Desc.Current, Desc.GroupName);
			}
		}

		// convenience for single unnamed Sdk range
		public SDKCollection(string? Min, string? Max, UEBuildPlatformSDK PlatformSDK)
			: this(PlatformSDK)
		{
			Sdks.Add(new SDKDescriptor(DefaultName, Min, Max, null, null, this));
		}

		// convenience for single unnamed Sdk current value
		public SDKCollection(string? Current, UEBuildPlatformSDK PlatformSDK)
			: this(PlatformSDK)
		{
			if (Current != null)
			{
				Sdks.Add(new SDKDescriptor(DefaultName, null, null, Current, null, this));
			}
		}

		public bool AreAllManualSDKsValid()
		{
			IEnumerable<SDKDescriptor>? Sdks = FullSdks;

			if (Sdks == null)
			{
				return true;
			}

			// check if all the SDKs not in groups are valid (All returns true if empty set)
			bool bAllUngroupedAreValid = Sdks
				.Where(Desc => Desc.GroupName == null)
				.All(Desc => Desc.Validity == SDKStatus.Valid);

			// for each groupname (ToHashSet removes duplicates), check if at least 1 SDK with that group name is valid
			bool bAllGroupsHaveOneValid = Sdks
				.Where(Desc => Desc.GroupName != null)
				.Select(Desc => Desc.GroupName)
				.ToHashSet()
				.All(GroupName => Sdks.Any(Desc => Desc.GroupName == GroupName && Desc.Validity == SDKStatus.Valid));

			return bAllUngroupedAreValid && bAllGroupsHaveOneValid;
		}
	
		public bool IsAutoSDKValid()
		{
			return AutoSdk?.Validity == SDKStatus.Valid;
		}


		public string ToString(bool bIncludeCurrent=true)
		{
			// by default print something like "MinVersion=1.0, MaxVersion=2.0"
			return ToString("Version", "Version");
		}

		public string ToString(string Descriptor, string? CurrentDescriptor=null)
		{
			if (Sdks.Count == 1)
			{
				return Sdks[0].ToString(Descriptor, CurrentDescriptor, false);
			}
			return string.Join(", ", Sdks.Select(x => x.ToString(Descriptor, CurrentDescriptor, true)));
		}

		public string ToMultilineString(bool bIncludeCurrent = true)
		{
			return $"";// MinVersion={Min ?? ""}, MaxVersion={Max ?? ""}" + (bIncludeCurrent ? $", Current={Current ?? ""}" : "");
		}

		public void SetupSDK(string Name, string? Min, string? Max, string? Current, string? GroupName)
		{
			Sdks.Add(new SDKDescriptor(Name, Min, Max, Current, GroupName, this));
		}

		public void SetupCurrent(string Name, string Current)
		{
			Sdks.Add(new SDKDescriptor(Name, null, null, Current, null, this));
		}

		public bool UpdateCurrentForSingle(string Name, string CurrentVersion)
		{
			if (PlatformSDK == null)
			{
				throw new Exception("Cannot call UpdateCurrentForSingle in a SDKCollection without a PlatformSDK set");
			}

			SDKDescriptor? Info;

			// for ease, we assume that if there is only one "Sdk" or "Software" in this Collection, then any Name passed in will work with it
			// so just use that one entry
			if (Sdks.Count == 1 && (Sdks[0].Name == "Sdk" || Sdks[0].Name == "Software"))
			{
				Info = Sdks[0];
			}
			else
			{
				Info = Sdks.FirstOrDefault(x => string.Compare(x.Name, Name, true) == 0);
				if (Info == null)
				{
					return false;
				}
			}

			Info.UpdateCurrent(CurrentVersion, Name, this);
			return true;
		}
	}

	public class SoftwareCollection : SDKCollection
	{
		public SoftwareCollection(UEBuildPlatformSDK PlatformSDK)
			: base(PlatformSDK)
		{
		}
		public SoftwareCollection(string? Min, string? Max, UEBuildPlatformSDK PlatformSDK)
			: base(Min, Max, PlatformSDK)
		{
		}
		public SoftwareCollection(string? Current, UEBuildPlatformSDK PlatformSDK)
			: base(Current, PlatformSDK)
		{
		}


		public override string DefaultName { get { return "Software"; } }

		public override bool IsValid(string Version, SDKDescriptor Info)
		{
			return PlatformSDK.IsSoftwareVersionValid(Version, Info);
		}
	}


	/// <summary>
	/// SDK for a platform
	/// </summary>
	abstract public class UEBuildPlatformSDK
	{
		// Public SDK handling, not specific to AutoSDK

		protected readonly ILogger Logger;

		public UEBuildPlatformSDK(ILogger InLogger)
		{
			Logger = InLogger;
		}

		#region Global SDK Registration

		/// <summary>
		/// Registers the SDK for a given platform (as a string, but equivalent to UnrealTargetPlatform)
		/// </summary>
		/// <param name="SDK">SDK object</param>
		/// <param name="PlatformName">Platform name for this SDK</param>
		/// <param name="bIsSdkAllowedOnHost"></param>
		public static void RegisterSDKForPlatform(UEBuildPlatformSDK SDK, string PlatformName, bool bIsSdkAllowedOnHost)
		{
			// verify that neither platform or sdk were added before
			if (SDKRegistry.Count(x => x.Key == PlatformName || x.Value == SDK) > 0)
			{
				throw new Exception(string.Format("Re-registering SDK for {0}. All Platforms must have a unique SDK object", PlatformName));
			}
			
			SDKRegistry.Add(PlatformName, SDK);

			SDK.Init(PlatformName, bIsSdkAllowedOnHost);
		}

		private void Init(string InPlatformName, bool bInIsSdkAllowedOnHost)
		{
			PlatformName = InPlatformName;
			bIsSdkAllowedOnHost = bInIsSdkAllowedOnHost;

			// load the SDK config file
			if (bIsSdkAllowedOnHost)
			{
				LoadJsonFile(PlatformName);
			}

			// if the parent set up autosdk, the env vars will be wrong, but we can still get the manual SDK version from before it was setup
			string? ParentManualSDKVersions = Environment.GetEnvironmentVariable(GetPlatformManualSDKSetupEnvVar());
			if (!string.IsNullOrEmpty(ParentManualSDKVersions))
			{

				// we pass along __None to indicate the parent didn't have a manual sdk installed
				if (ParentManualSDKVersions == "__None")
				{
					// empty it out
					CachedManualSDKVersions = new Dictionary<string, string>();
				}
				else
				{
					Console.WriteLine("Processing parent manual sdk: {0}", ParentManualSDKVersions);
					CachedManualSDKVersions = JsonSerializer.Deserialize<Dictionary<string, string>>(ParentManualSDKVersions) ?? new Dictionary<string, string>();
				}
			}
			else
			{
				CachedManualSDKVersions = new Dictionary<string, string>();

				// if there was no parent, get the SDK version before we run AutoSDK to get the manual version
				SDKCollection InstalledVersions = GetInstalledSDKVersions();
				foreach (SDKDescriptor Desc in InstalledVersions.FullSdks)
				{
					CachedManualSDKVersions[Desc.Name] = Desc.Current!;
				}
			}
		}

		#endregion

		#region Main Public Interface/Utilties

		/// <summary>
		/// Retrieves a previously registered SDK for a given platform
		/// </summary>
		/// <param name="PlatformName">String name of the platform (equivalent to UnrealTargetPlatform)</param>
		/// <returns></returns>
		public static UEBuildPlatformSDK? GetSDKForPlatform(string PlatformName)
		{
			UEBuildPlatformSDK? SDK;
			SDKRegistry.TryGetValue(PlatformName, out SDK);

			return SDK;
		}

		public static T? GetSDKForPlatformOrMakeTemp<T>(string PlatformName) where T : UEBuildPlatformSDK
		{
			UEBuildPlatformSDK? SDK;
			SDKRegistry.TryGetValue(PlatformName, out SDK);

			// make a temp one if needed, this is not expected to happen often at all
			if (SDK == null)
			{
				if (!TempSDKRegistry.TryGetValue(PlatformName, out SDK))
				{
					object[] parameter = new object[1];
					parameter[0] = Log.Logger;
					SDK = (UEBuildPlatformSDK)Activator.CreateInstance(typeof(T), parameter)!;
					// by setting this to false, we don't require any of the RequiredVersions to exist, but if they do, they will be read
					// this is useful on other platforms that 
					SDK.bIsSdkAllowedOnHost = false;
					SDK.LoadJsonFile(PlatformName);
					TempSDKRegistry.Add(PlatformName, SDK);
				}
			}
			return (T?)SDK;
		}


		/// <summary>
		/// Gets the set of all known SDKs
		/// </summary>
		public static UEBuildPlatformSDK[] AllPlatformSDKObjects
		{
			get { return SDKRegistry.Values.ToArray(); }
		}

		// True if at least one platform has had its version changed from the standard SDK version, which means it may not be compatible with other projects
		public static bool bHasAnySDKOverride = false;

		// True if this SDK has had its version changed from the standard SDK version, which means it may not be compatible with other projects
		public bool bHasSDKOverride = false;

		// Contains a list of projects that had per-project SDK overrides, used for validating conflicting SDKs
		public List<FileReference> ProjectsThatOverrodeSDK = new();
	
		// String name of the platform (will match an UnrealTargetPlatform)
		public string? PlatformName;

		// True if this Sdk is allowed to be used by this host - if not, we can skip a lot 
		public bool bIsSdkAllowedOnHost;

		public SDKCollection GetAllSDKInfo()
		{
			SDKCollection AllSdks = new SDKCollection(this);

			if (bIsSdkAllowedOnHost)
			{
				// walk over each one version the platform supports, and get it's current installed info
				foreach (SDKDescriptor Desc in GetValidVersions().Sdks)
				{
					AllSdks.SetupSDK(Desc.Name, Desc.Min, Desc.Max, CachedManualSDKVersions.GetValueOrDefault(Desc.Name), Desc.GroupName);
				}

				// now get current AutoSDK version (if autosdk is set up, then the GetInstalledSDKVersion will return AutoSDK version)
				bool bIsAutoSDK = false;
				string? CurrentAutoSDKVersion = (PlatformSupportsAutoSDKs() && HasRequiredAutoSDKInstalled() == SDKStatus.Valid && HasSetupAutoSDK()) ? GetInstalledVersion(out bIsAutoSDK) : null;
				AllSdks.SetupSDK("AutoSdk", GetMainVersion(), GetMainVersion(), CurrentAutoSDKVersion, null);

				// verify some assumptions
				if (CurrentAutoSDKVersion != null && bIsAutoSDK == false)
				{
					throw new Exception($"AutoSDK was indicated to be setup ({CurrentAutoSDKVersion}), but GetInstalledSDKVersion returned false for bIsAutoSDK");
				}
				if (CurrentAutoSDKVersion != null && CurrentAutoSDKVersion != GetMainVersion())
				{
					throw new Exception($"AutoSDK was indicated to be setup, but the version if returned ({CurrentAutoSDKVersion}) doesn't equal the MainVersion ({GetMainVersion()}");
				}
			}

			return AllSdks;
		}

		public SDKDescriptor? GetSDKInfo(string SDKName)
		{
			return GetAllSDKInfo().Sdks.FirstOrDefault(x => string.Compare(x.Name, SDKName, true) == 0);
		}

		public SDKDescriptor? GetSoftwareInfo(string? SDKName=null)
		{
			if (SDKName == null)
			{
				// todo: make this queryable?
				SDKName = "Software";
			}
			return GetAllSoftwareInfo().Sdks.FirstOrDefault(x => string.Compare(x.Name, SDKName, true) == 0);
		}

		public SDKCollection GetAllSoftwareInfo(string? DeviceType=null, string? Current=null)
		{
			SDKCollection AllSoftwares = new SDKCollection(this);

			// walk over the valid software ranges, potentially setting the current verison if one was passed in
			foreach (SDKDescriptor Desc in GetValidSoftwareVersions().Sdks)
			{
				// are we restricting this to one type?
				if (DeviceType == null || string.Compare(Desc.Name, DeviceType, true) == 0)
				{
					AllSoftwares.SetupSDK(Desc.Name, Desc.Min, Desc.Max, null, Desc.GroupName);
				}

				if (Current != null)
				{
					// set the current for this one (likely it will only be for DeviceType, but support passing in null DeviceType, and non-null Current
					AllSoftwares.UpdateCurrentForSingle(Desc.Name, Current);
				}
			}

			return AllSoftwares;
		}

		//public SDKCollection GetAllSoftwareInfo(string DeviceId)
		//{
		//	SDKCollection AllSoftwares =
		//}


		public string? GetInstalledVersion(out bool bIsAutoSDK)
		{
			bIsAutoSDK = HasSetupAutoSDK();
			return GetInstalledSDKVersion();
		}

		public string? GetInstalledVersion()
		{
			return GetInstalledSDKVersion();
		}
		public string? GetInstalledVersion(string SDKName)
		{
			return GetSDKInfo(SDKName)?.Current;
		}

		//		public void GetInstalledVersions(out string? ManualSDKVersion, out string? AutoSDKVersion)
		//		{
		//			// if we support AutoSDKs, then return both versions
		//			if (PlatformSupportsAutoSDKs())
		//			{
		//				AutoSDKVersion = (HasRequiredAutoSDKInstalled() == SDKStatus.Valid) ? GetInstalledSDKVersion() : null;
		////				AutoSDKVersion = GetInstalledSDKVersion();
		//			}
		//			else
		//			{
		//				AutoSDKVersion = null;
		//				if (CachedManualSDKVersion != GetInstalledSDKVersion())
		//				{
		//					throw new Exception("Manual SDK version changed, this is not supported yet");
		//				}
		//			}

		//			ManualSDKVersion = CachedManualSDKVersion;
		//		}


		public virtual bool IsVersionValid(string? Version, string SDKType)
		{
			return IsVersionValidInternal(Version, SDKType, null);
		}
		public virtual bool IsVersionValid(string? Version, SDKDescriptor Info)
		{
			return IsVersionValidInternal(Version, null, Info);
		}
		public virtual bool IsSoftwareVersionValid(string Version, string DeviceType)
		{
			return IsSoftwareVersionValidInternal(Version, DeviceType, null);
		}
		public virtual bool IsSoftwareVersionValid(string Version, SDKDescriptor Info)
		{
			return IsSoftwareVersionValidInternal(Version, null, Info);
		}

		public void ReactivateAutoSDK()
		{
			// @todo turnkey: this needs to force re-doing it, as it is likely a no-op, need to investigate what to clear out
			ManageAndValidateSDK();
		}

		#endregion

		#region Platform Overrides

		/// <summary>
		/// Return the SDK version that the platform wants to use (AutoSDK dir must match this, full SDKs can be in a valid range)
		/// </summary>
		/// <returns></returns>
		public virtual string GetMainVersion()
		{
			// by default, look up in SDK config value
			return GetRequiredVersionFromConfig("MainVersion");
		}

		/// <summary>
		/// Gets the valid string range of Sdk versions. TryConvertVersionToInt() will need to succeed to make this usable for range checks
		/// </summary>
		/// <param name="MinVersion">Smallest version allowed</param>
		/// <param name="MaxVersion">Largest version allowed (inclusive)</param>
		protected virtual void GetValidVersionRange(out string? MinVersion, out string? MaxVersion)
		{
			// by default, use SDK config file values
			MinVersion = GetVersionFromConfig("MinVersion");
			MaxVersion = GetVersionFromConfig("MaxVersion");
		}

		protected virtual SDKCollection GetValidVersions()
		{
			// if the platform doesn't override this, then it only has one sdk, so put it into the collection as the single sdk (this is very much the standard behavior)
			string? MinVersion, MaxVersion;
			GetValidVersionRange(out MinVersion, out MaxVersion);
			return new SDKCollection(MinVersion, MaxVersion, this);
		}

		/// <summary>
		/// Gets the valid string range of software/flash versions. TryConvertVersionToInt() will need to succeed to make this usable for range checks
		/// </summary>
		/// <param name="MinVersion">Smallest version allowed, or null if no minmum (in other words, 0 - MaxVersion)</param>
		/// <param name="MaxVersion">Largest version allowed (inclusive), or null if no maximum (in other words, MinVersion - infinity)y</param>
		protected virtual void GetValidSoftwareVersionRange(out string? MinVersion, out string? MaxVersion)
		{
			MinVersion = GetVersionFromConfig("MinSoftwareVersion");
			MaxVersion = GetVersionFromConfig("MaxSoftwareVersion");
		}

		protected virtual SoftwareCollection GetValidSoftwareVersions()
		{
			// if the platform doesn't override this, then it only has one sdk, so put it into the collection as the single software (this is very much the standard behavior)
			string? MinVersion, MaxVersion;
			GetValidSoftwareVersionRange(out MinVersion, out MaxVersion);
			SoftwareCollection SoftwareVersions = new SoftwareCollection(MinVersion, MaxVersion, this);
			return SoftwareVersions;
		}

		/// <summary>
		/// Returns the installed SDK version, used to determine if up to date or not (
		/// </summary>
		/// <returns></returns>
		protected virtual string? GetInstalledSDKVersion()
		{
			if (GetInstalledSDKVersions().FullSdks.Count() == 1)
			{
				return GetInstalledSDKVersions().FullSdks[0].Current;
			}
			throw new Exception($"This platform's SDK class {GetType()} did not implement GetInstalledSDKVersion(). That means it has multiple SDKs and you will need use GetAllSDKInfo() or GetInstalledVersion(SDKName)");
		}

		protected virtual SDKCollection GetInstalledSDKVersions()
		{
			if (bIsSdkAllowedOnHost)
			{
				// if the platform doesn't override this, then it only has one sdk, so put it into the collection as the single sdk (this is very much the standard behavior)
				string? Version = GetInstalledSDKVersion();
				if (Version != null)
				{
					return new SDKCollection(Version, this);
				}
			}

			// if no manual version, then return an empty list of current sdks
			return new SDKCollection(this);
		}

		/// <summary>
		/// Returns a platform-specific version string that can be retrieved from anywhere without needing to typecast the SDK object
		/// </summary>
		/// <param name="VersionType">Descriptor of the version you want</param>
		/// <returns>Version string, or empty string if not known/supported</returns>
		public virtual string GetPlatformSpecificVersion(string VersionType)
		{
			return GetRequiredVersionFromConfig(VersionType);
		}

		/// <summary>
		/// Return a list of full SDK versions that are already installed and can be quickly switched to without running any installers.
		/// </summary>
		/// <returns></returns>
		public virtual string[] GetAllInstalledSDKVersions()
		{
			return new string[] { };
		}

		/// <summary>
		/// Switch to another version of the SDK than what GetInstalledSDKVersion() returns. This will be one of the versions returned from GetAllInstalledSDKVersions()
		/// </summary>
		/// <param name="Version">String name of the version to switch to</param>
		/// <param name="bSwitchForThisProcessOnly">If true, only switch for this process (usually via process environment variable). If false, switch permanently (usually via system-wide environment variable)</param>
		/// <returns>True if successful</returns>
		public virtual bool SwitchToAlternateSDK(string Version, bool bSwitchForThisProcessOnly)
		{
			return false;
		}


		/// <summary>
		/// For a platform that doesn't use properly named AutoSDK directories, the directory name may not be convertible to an integer,
		/// and IsVersionValid checks could fail when checking AutoSDK version for an exact match. GetMainVersion() would return the 
		/// proper, integer-convertible version number of the SDK inside of the directory returned by GetAutoSDKDirectoryForMainVersion()
		/// </summary>
		/// <returns></returns>
		public virtual string GetAutoSDKDirectoryForMainVersion()
		{
			// if there is an AutoSDKDirectory property, use it, otherwise, use the MainVersion string
			return GetVersionFromConfig("AutoSDKDirectory") ?? GetMainVersion();
		}


		///// <summary>
		///// Gets the valid (integer) range of Sdk versions. Must be an integer to easily check a range vs a particular version
		///// </summary>
		///// <param name="MinVersion">Smallest version allowed</param>
		///// <param name="MaxVersion">Largest version allowed (inclusive)</param>
		///// <returns>True if the versions are valid, false if the platform is unable to convert its versions into an integer</returns>
		//public virtual void GetValidVersionRange(out UInt64 MinVersion, out UInt64 MaxVersion)
		//{
		//	string MinVersionString, MaxVersionString;
		//	GetValidVersionRange(out MinVersionString, out MaxVersionString);

		//	// failures to convert here are bad
		//	if (!TryConvertVersionToInt(MinVersionString, out MinVersion) || !TryConvertVersionToInt(MaxVersionString, out MaxVersion))
		//	{
		//		throw new Exception(string.Format("Unable to convert Min and Max valid versions to integers in {0} (Versions are {1} - {2})", GetType().Name, MinVersionString, MaxVersionString));
		//	}
		//}
		//public virtual void GetValidSoftwareVersionRange(out UInt64 MinVersion, out UInt64 MaxVersion)
		//{
		//	string? MinVersionString, MaxVersionString;
		//	GetValidSoftwareVersionRange(out MinVersionString, out MaxVersionString);

		//	MinVersion = UInt64.MinValue;
		//	MaxVersion = UInt64.MaxValue - 1; // MaxValue is always bad

		//	// failures to convert here are bad
		//	if ((MinVersionString != null && !TryConvertVersionToInt(MinVersionString, out MinVersion)) ||
		//		(MaxVersionString != null && !TryConvertVersionToInt(MaxVersionString, out MaxVersion)))
		//	{
		//		throw new Exception(string.Format("Unable to convert Min and Max valid Software versions to integers in {0} (Versions are {1} - {2})", GetType().Name, MinVersionString, MaxVersionString));
		//	}
		//}


		// Let platform override behavior to determine if a version is a valid (useful for non-numeric versions)
		protected virtual bool IsVersionValidInternal(string? Version, string? SDKType, SDKDescriptor? VersionInfo)
		{
			// we could have null if no SDK is installed at all, etc, which is always a failure
			if (Version == null)
			{
				return false;
			}

			// AutoSDK must match the desired version exactly, since that is the only one we will use
			if (string.Compare(SDKType, "AutoSdk", true) == 0)
			{
				// if integer version checking failed, then we can detect valid autosdk if the version matches the autosdk directory by name
				return string.Compare(Version, GetAutoSDKDirectoryForMainVersion(), true) == 0;
			}

			// look for a range for this hinted type of SDK
			VersionInfo = VersionInfo ?? GetSDKVersionForHint(GetValidVersions(), SDKType);

			// if we couldn't find the Info, we can't do anything, so fail
			if (VersionInfo == null)
			{
				return false;
			}

			// convert it to an integer
			UInt64 IntVersion;
			if (!TryConvertVersionToInt(Version, out IntVersion))
			{
				return false;
			}

			// short circuit range check if the Version is the desired version already
			if (IntVersion == ConvertVersionToInt(GetMainVersion()))
			{
				return true;
			}

			// finally do the numeric comparison
			return IntVersion >= VersionInfo.MinInt && IntVersion <= VersionInfo.MaxInt;
		}

		protected virtual bool IsSoftwareVersionValidInternal(string? Version, string? DeviceType, SDKDescriptor? VersionInfo)
		{
			// we could have null if no SDK is installed at all, etc, which is always a failure
			if (Version == null)
			{
				return false;
			}

			// convert it to an integer
			UInt64 IntVersion;
			if (!TryConvertVersionToInt(Version, out IntVersion))
			{
				return false;
			}

			// look for a range for this hinted type of SDK
			VersionInfo = VersionInfo ?? GetSDKVersionForHint(GetValidSoftwareVersions(), DeviceType);

			// if we couldn't find the Info, we can't do anything, so fail
			if (VersionInfo == null)
			{
				return false;
			}

			// finally do the numeric comparison
			return IntVersion >= VersionInfo.MinInt && IntVersion <= VersionInfo.MaxInt;
		}


		/// <summary>
		/// Only the platform can convert a version string into an integer that is usable for comparison
		/// </summary>
		/// <param name="StringValue">Version that comes from the installed SDK or a Turnkey manifest or the like</param>
		/// <param name="OutValue"></param>
		/// <param name="Hint">A platform specific hint that can help guide conversion (usually SDKName or device type)</param>
		/// <returns>If the StringValue was able to be be converted to an integer</returns>
		public abstract bool TryConvertVersionToInt(string? StringValue, out UInt64 OutValue, string? Hint = null);

		/// <summary>
		/// Like TryConvertVersionToInt, but will throw an exception on failure
		/// </summary>
		/// <param name="StringValue">Version that comes from the PlatformSDK class (should not be used with Manifest or other user supplied versions)</param>
		/// <param name="Hint">A platform specific hint that can help guide conversion (usually SDKName or device type)</param>
		/// <returns>The integer version of StringValue, can be used to compare against a valid range</returns>
		public UInt64 ConvertVersionToInt(string? StringValue, string? Hint = null)
		{
			// quickly handle null input, which is not necessarily an error case
			if (StringValue == null)
			{
				return 0;
			}

			UInt64 Result;
			if (!TryConvertVersionToInt(StringValue, out Result, Hint))
			{
				throw new Exception($"Unable to convert {GetType()} version '{StringValue}' to an integer. Likely this version was supplied by code, and is expected to be valid.");
			}
			return Result;
		}

		/// <summary>
		/// Compare two sdk versions, for Sort() purposes
		/// e.g. SdkVersions.Sort( (A,B) => PlatformSDK.SdkVersionsCompare(A,B) );
		/// </summary>
		/// <param name="StringValueA">First Version to compare</param>
		/// <param name="StringValueB">Second Version to compare</param>
		/// <param name="Hint">A platform specific hint that can help guide conversion (usually SDKName or device type)</param>
		/// <returns>Comparison integer</returns>
		public int SdkVersionsCompare( string? StringValueA, string? StringValueB, string? Hint = null )
		{
			UInt64 ValueA, ValueB;
			TryConvertVersionToInt(StringValueA, out ValueA, Hint);
			TryConvertVersionToInt(StringValueB, out ValueB, Hint);
			if (ValueA == ValueB)
			{
				return 0;
			}
			else if (ValueA < ValueB)
			{
				return -1;
			}
			else
			{
				return 1;
			}
		}


		/// <summary>
		/// Allow the platform SDK to override the name it will use in AutoSDK, but default to the platform name
		/// </summary>
		/// <returns>The name of the directory to use inside the AutoSDK system</returns>
		public virtual string GetAutoSDKPlatformName()
		{
			return GetVersionFromConfig("AutoSDKPlatform") ?? PlatformName!;
		}

		#endregion

		#region Per-project SDK support

		private static List<FileReference> ProjectsToCheckForSDKOverrides = new();
		public static void InitializePerProjectSDKVersions(IEnumerable<FileReference> ProjectsToCheckForOverrides)
		{
			ProjectsToCheckForSDKOverrides = ProjectsToCheckForOverrides.ToList();
		}

		#endregion

		#region Print SDK Info
		private static bool bHasShownTurnkey = false;
		private SDKStatus? SDKInfoValidity = null;
		public static bool bSuppressSDKWarnings = false;

		public virtual SDKStatus PrintSDKInfoAndReturnValidity(LogEventType Verbosity = LogEventType.Console, LogFormatOptions Options = LogFormatOptions.None,
			LogEventType ErrorVerbosity = LogEventType.Error, LogFormatOptions ErrorOptions = LogFormatOptions.None)
		{
			if (SDKInfoValidity != null)
			{
				return SDKInfoValidity.Value;
			}

			SDKCollection SDKInfo = GetAllSDKInfo();

			// will mark invalid below if needed
			SDKInfoValidity = SDKStatus.Valid;

			SDKDescriptor? AutoSDKInfo = SDKInfo.AutoSdk;
			if (AutoSDKInfo != null && AutoSDKInfo.Validity == SDKStatus.Valid)
			{
				string PlatformSDKRoot = GetPathToPlatformAutoSDKs();
				Log.WriteLine(Verbosity, Options, "{0} using Auto SDK {1} from: {2} 0x{3:X}", PlatformName, AutoSDKInfo.Current, Path.Combine(PlatformSDKRoot, GetAutoSDKDirectoryForMainVersion()), AutoSDKInfo.CurrentInt);
			}
			else
			{
				if (SDKInfo.AreAllManualSDKsValid())
				{
					Log.WriteLine(Verbosity, Options, "{0} Installed SDK(s): {1}", PlatformName, SDKInfo.ToString(true));
				}
				else
				{
					SDKInfoValidity = SDKStatus.Invalid;

					StringBuilder Msg = new StringBuilder();
					Msg.AppendFormat("Unable to find valid SDK(s) for {0}:", PlatformName);

					foreach (SDKDescriptor Desc in SDKInfo.Sdks)
					{
						if (Desc.Validity == SDKStatus.Valid)
						{
							Msg.Append($"  {Desc.Name} is valid ({Desc}");
						}
						else
						{
							if (Desc.Current != null)
							{
								Msg.AppendFormat($" Found {Desc.Name} Version: {Desc.Current}.");
							}

							Msg.AppendLine($"   {Desc.ToString("Required", null, false)}");
						}
					}


					if (!bHasShownTurnkey)
					{
						Msg.AppendLine("  If your Studio has it set up, you can run this command to find the SDK to install:");
						Msg.AppendLine("    RunUAT Turnkey -command=InstallSdk -platform={0} -BestAvailable", PlatformName!);

						if ((ErrorOptions & LogFormatOptions.NoConsoleOutput) == LogFormatOptions.None)
						{
							bHasShownTurnkey = true;
						}
					}

					// Reducing warnings to log to help prevent warnings locally or in Horde about SDKs we might not currently be concerned about
					if (bSuppressSDKWarnings)
					{
						ErrorVerbosity = LogEventType.Log;
					}

					// always print errors to the screen
					Log.WriteLine(ErrorVerbosity, ErrorOptions, Msg.ToString());
				}
			}

			return SDKInfoValidity.Value;
		}

		#endregion





		#region Private/Protected general functionality

		// this is the SDK version that was set before activating AutoSDK, since AutoSDK may remove ability to retrieve the Manual SDK version
		protected Dictionary<string, string> CachedManualSDKVersions = new Dictionary<string, string>();
		private static Dictionary<string, UEBuildPlatformSDK> SDKRegistry = new Dictionary<string, UEBuildPlatformSDK>();

		// this map holds on to some temporary SDK objects that are generally used once and don't want to stick around, but they could be used multiple times, 
		// like in MicrosoftPlatofrmSDK.Version.cs, if one of the versions is needed, C# will go construct every version, needing the SDK multiple times
		private static Dictionary<string, UEBuildPlatformSDK> TempSDKRegistry = new Dictionary<string, UEBuildPlatformSDK>();

		private SDKDescriptor? GetSDKVersionForHint(SDKCollection Collection, string? Hint)
		{
			// if the hint is found, use it always
			SDKDescriptor? SDKDesc = Collection.Sdks.FirstOrDefault(x => string.Compare(x.Name, Hint, true) == 0);
			if (SDKDesc != null)
			{
				return SDKDesc;
			}

			// only use AUtoSDK if explicitly asked for above, otherwise, remove it and look at what's left
			IEnumerable<SDKDescriptor> FullSdks = Collection.FullSdks;

			// if there's one with the special name, then use it as it's generic (common case here)
			if (FullSdks.Count() == 1 && (FullSdks.First().Name == "Sdk" || FullSdks.First().Name == "Software"))
			{
				return FullSdks.First();
			}

			// finally we have to give up
			return null;
		}

		// cached SDK info from the SDK.json file
		private Dictionary<string, string> ConfigSDKVersions = new(StringComparer.OrdinalIgnoreCase);
		private Dictionary<string, string[]> ConfigSDKVersionArrays = new(StringComparer.OrdinalIgnoreCase);

		private void LoadJsonFile(string Platform)
		{
			// fixup for Windows
			if (Platform == "Win64")
			{
				Platform = "Windows";
			}

			Func<DirectoryReference, bool, FileReference?> MakeConfigFilename = (RootDir, bIsRequired) =>
			{
				FileReference PlatformExtensionLocation = FileReference.Combine(RootDir, "Platforms", Platform, "Config", $"{Platform}_SDK.json");
				if (FileReference.Exists(PlatformExtensionLocation))
				{
					return PlatformExtensionLocation;
				}
				FileReference StandardLocation = FileReference.Combine(RootDir, "Config", Platform, $"{Platform}_SDK.json");
				if (FileReference.Exists(StandardLocation))
				{
					return StandardLocation;
				}
				if (bIsRequired)
				{
					throw new Exception($"Failed to find required SDK.json for {Platform}. Looked in '{StandardLocation}' and '{PlatformExtensionLocation}'.");
				}
				return null;
			};

			// if the SDK isn't allowed on the host, then allow it to not exist
			FileReference EngineSDKConfigFile = MakeConfigFilename(Unreal.EngineDirectory, bIsSdkAllowedOnHost)!;

			// load the file, along with any chained group file
			ProcessJsonFile(EngineSDKConfigFile, ConfigSDKVersions, ConfigSDKVersionArrays);

			// copy off the versions to the defaults, so we can check if overridden by the project
			Dictionary<string, string> DefaultConfigSDKVersions = new(ConfigSDKVersions);
			foreach (string Key in ConfigSDKVersions.Keys)
			{
				DefaultConfigSDKVersions[Key] = ConfigSDKVersions[Key];
			}

			// now read overrides into the array
			foreach (FileReference ProjectFile in ProjectsToCheckForSDKOverrides)
			{
				FileReference? ProjectSDKConfigFile = MakeConfigFilename(ProjectFile.Directory, false);
				if (ProjectSDKConfigFile != null)
				{
					// load a project's SDK file if thre is one, into a temp dictionary
					Dictionary<string, string> OverrideConfigSDKVersions = new(StringComparer.OrdinalIgnoreCase);
					Dictionary<string, string[]> OverrideConfigSDKVersionArrays = new(StringComparer.OrdinalIgnoreCase);
					ProcessJsonFile(ProjectSDKConfigFile, OverrideConfigSDKVersions, OverrideConfigSDKVersionArrays);
					if (OverrideConfigSDKVersionArrays.Count > 0)
					{
						throw new Exception($"Overriding version arrays, in project '{ProjectFile.GetFileNameWithoutExtension()}', platform {Platform} is not currently supported");
					}

					// currently only care about MainVersion in the overrides
					string? OverrideMainVersion;
					const string MainVersionKey = "MainVersion";
					if (OverrideConfigSDKVersions.TryGetValue(MainVersionKey, out OverrideMainVersion))
					{
						// if different from default, then remmber it, and mark that it's been overridden
						if (!OverrideMainVersion.Equals(DefaultConfigSDKVersions[MainVersionKey], StringComparison.OrdinalIgnoreCase))
						{
							// now check if it was already overridden, in which case we have a conflict we can't resolve, so error
							if (ProjectsThatOverrodeSDK.Count > 0 && !OverrideMainVersion.Equals(ConfigSDKVersions[MainVersionKey], StringComparison.OrdinalIgnoreCase))
							{
								throw new Exception($"Project {ProjectFile.GetFileNameWithoutAnyExtensions()} wants to override {Platform} SDK to {OverrideMainVersion}, but it was already overridden to version {ConfigSDKVersions[MainVersionKey]}");							
							}

							Logger.LogWarning("Project {Project} is overriding {Platform} SDK to {OverrideMainVersion}", ProjectFile.GetFileNameWithoutAnyExtensions(), Platform, OverrideMainVersion);

							ConfigSDKVersions[MainVersionKey] = OverrideMainVersion;
							UEBuildPlatformSDK.bHasAnySDKOverride = true;
							bHasSDKOverride = true;
							ProjectsThatOverrodeSDK.Add(ProjectFile);
						}
					}
				}
			}
		}

		private void ProcessJsonFile(FileReference SDKConfigFile, Dictionary<string, string> VersionMap, Dictionary<string, string[]> VersionArrayMap)
		{
			string Contents = FileReference.ReadAllText(SDKConfigFile);

			// load the json into a dictionary
			JsonSerializerOptions Options = new()
			{
				AllowTrailingCommas = true,
				ReadCommentHandling = JsonCommentHandling.Skip,
			};
			Dictionary<string, JsonElement>? LoadedDictionary = JsonSerializer.Deserialize<Dictionary<string, JsonElement>>(Contents, Options);
			if (LoadedDictionary == null)
			{
				throw new Exception($"Failed to parse SDK version file '{SDKConfigFile}'");
			}

			// look for special key to chain to other - after processing everything else (we only add from the "parent" settings not already in the map)
			List<string> Parents = new();
			foreach (string Key in LoadedDictionary.Keys)
			{
				JsonElement Obj = LoadedDictionary[Key];
				string? StringValue = Obj.ValueKind == JsonValueKind.String ? Obj.GetString() : null;
				string[]? ArrayValue = Obj.ValueKind == JsonValueKind.Array ? Obj.EnumerateArray().Select(x => x.GetString()!).ToArray() : null;

				if (StringValue != null)
				{
					if (Key.Equals("ParentSDKFile", StringComparison.OrdinalIgnoreCase))
					{
						Parents.Add(StringValue);
					}
					// add this key if it's not already in the (case-insensitive) dictionary 
					else if (!VersionMap.ContainsKey(Key))
					{
						VersionMap.Add(Key, StringValue);
					}
				}
				else if (ArrayValue != null)
				{
					VersionArrayMap.Add(Key, ArrayValue);
				}
			}

			// now load parents, filling in unset properties
			foreach (string Parent in Parents)
			{
				FileReference ParentConfigFile = FileReference.Combine(SDKConfigFile.Directory, Parent);
				ProcessJsonFile(ParentConfigFile, VersionMap, VersionArrayMap);
			}
		}

		private string GetHostSpecificVersionName(string VersionName)
		{
			string HostPlatform = "Win64";
			if (OperatingSystem.IsMacOS())
			{
				HostPlatform = "Mac";
			}
			else if (OperatingSystem.IsLinux())
			{
				HostPlatform = "Linux";
			}
			return $"{VersionName}_{HostPlatform}";
		}
		public string GetRequiredVersionFromConfig(string VersionName)
		{
			// when bIsRequired is true, then we know it will return non-null
			return GetVersionFromConfig(VersionName, bIsRequired:true)!;
		}

		public string? GetVersionFromConfig(string VersionName, bool bIsRequired=false)
		{
			string? Version;
			// look up both Version_Host and Version (Host specific version wins)
			if (ConfigSDKVersions!.TryGetValue(GetHostSpecificVersionName(VersionName), out Version) || ConfigSDKVersions.TryGetValue(VersionName, out Version))
			{
				return Version;
			}

			if (bIsRequired)
			{
				throw new Exception($"Unable to find required SDK version '{VersionName}' for platform {PlatformName}. Check your SDK.json files");
			}
			return null;
		}

		protected VersionNumber GetRequiredVersionNumberFromConfig(string VersionName)
		{
			// required won't ever return null
			return GetVersionNumberFromConfig(VersionName, true)!;
		}

		public VersionNumber? GetVersionNumberFromConfig(string VersionName, bool bIsRequired=false)
		{
			string? VersionString = GetVersionFromConfig(VersionName, bIsRequired);

			if (VersionString == null)
			{
				return null;
			}

			return VersionNumber.Parse(VersionString);
		}


		private VersionNumberRange? ParseVersionNumberRange(string Range)
		{
			string[] Versions = Range.Split("-");
			if (Versions.Length != 2)
			{
				return null;
			}

			return VersionNumberRange.Parse(Versions[0], Versions[1]);
		}

		public VersionNumberRange? GetRequiredVersionNumberRangeFromConfig(string VersionName, bool bIsRequired = false)
		{
			// required won't ever return null
			return GetVersionNumberRangeFromConfig(VersionName, true)!;
		}

		public VersionNumberRange? GetVersionNumberRangeFromConfig(string VersionName, bool bIsRequired = false)
		{
			string? VersionRange;
			if (!ConfigSDKVersions!.TryGetValue(GetHostSpecificVersionName(VersionName), out VersionRange) && !ConfigSDKVersions.TryGetValue(VersionName, out VersionRange))
			{
				if (bIsRequired)
				{
					throw new Exception($"Unable to find required SDK version range '{VersionName}' for platform {PlatformName}. Check your SDK.json files");
				}
				return null;
			}

			VersionNumberRange? Range = ParseVersionNumberRange(VersionRange);
			if (Range == null && bIsRequired)
			{
				throw new Exception($"Unable to parse the version number range for required version '{VersionName}' for platform {PlatformName}. Check your SDK.json files");
			}
			return Range;
		}

		public VersionNumberRange[] GetVersionNumberRangeArrayFromConfig(string VersionName)
		{
			string[]? VersionRanges;
			List<VersionNumberRange> Ranges = new();
			if (ConfigSDKVersionArrays.TryGetValue(GetHostSpecificVersionName(VersionName), out VersionRanges) || ConfigSDKVersionArrays.TryGetValue(VersionName, out VersionRanges))
			{
				foreach (string VersionRange in VersionRanges) 
				{
					VersionNumberRange? Range = ParseVersionNumberRange(VersionRange);
					if (Range != null)
					{
						Ranges.Add(Range);
					}
				}
			}

			return Ranges.ToArray();
		}

		public string[] GetStringArrayFromConfig(string Name)
		{
			string[]? Results;
			if (ConfigSDKVersionArrays.TryGetValue(GetHostSpecificVersionName(Name), out Results) || ConfigSDKVersionArrays.TryGetValue(Name, out Results))
			{
				return Results;
			}

			return Array.Empty<string>();
		}

		#endregion

		// AutoSDKs handling portion

		#region protected AutoSDKs Utility

		/// <summary>
		/// Name of the file that holds currently install SDK version string
		/// </summary>
		protected const string CurrentlyInstalledSDKStringManifest = "CurrentlyInstalled.txt";

		/// <summary>
		/// name of the file that holds the last succesfully run SDK setup script version
		/// </summary>
		protected const string LastRunScriptVersionManifest = "CurrentlyInstalled.Version.txt";

		/// <summary>
		/// Filename of the script version file in each AutoSDK directory. Changing the contents of this file will force reinstallation of an AutoSDK.
		/// </summary>
		private const string ScriptVersionFilename = "Version.txt";

		/// <summary>
		/// Name of the file that holds environment variables of current SDK
		/// </summary>
		protected const string SDKEnvironmentVarsFile = "OutputEnvVars.txt";

		protected const string SDKRootEnvVar = "UE_SDKS_ROOT";

		protected const string AutoSetupEnvVar = "AutoSDKSetup";
		protected const string ManualSetupEnvVar = "ManualSDKSetup";


		private static string GetAutoSDKHostPlatform()
		{
			if (RuntimePlatform.IsWindows)
			{
				return "Win64";
			}
			else if (RuntimePlatform.IsMac)
			{
				return "Mac";
			}
			else if (RuntimePlatform.IsLinux)
			{
				return "Linux";
			}
			throw new Exception("Unknown host platform!");
		}

		/// <summary>
		/// Whether platform supports switching SDKs during runtime
		/// </summary>
		/// <returns>true if supports</returns>
		protected virtual bool PlatformSupportsAutoSDKs()
		{
			return false;
		}

		static private bool bCheckedAutoSDKRootEnvVar = false;
		static private bool bAutoSDKSystemEnabled = false;
		static private bool HasAutoSDKSystemEnabled()
		{
			if (!bCheckedAutoSDKRootEnvVar)
			{
				string? SDKRoot = Environment.GetEnvironmentVariable(SDKRootEnvVar);
				if (SDKRoot != null)
				{
					bAutoSDKSystemEnabled = true;
				}
				bCheckedAutoSDKRootEnvVar = true;
			}
			return bAutoSDKSystemEnabled;
		}

		// Whether AutoSDK setup is safe. AutoSDKs will damage manual installs on some platforms.
		protected bool IsAutoSDKSafe()
		{
			return !IsAutoSDKDestructive() || !HasAnyManualInstall();
		}

		/// <summary>
		/// Gets the version number of the SDK setup script itself.  The version in the base should ALWAYS be the primary revision from the last refactor.
		/// If you need to force a rebuild for a given platform, modify the version file.
		/// </summary>
		/// <returns>Setup script version</returns>
		private string GetRequiredScriptVersionString()
		{
			const string UnspecifiedVersion = "UnspecifiedScriptVersion";

			string VersionFilename = Path.Combine(GetPathToPlatformAutoSDKs(), GetAutoSDKDirectoryForMainVersion(), ScriptVersionFilename);
			if (File.Exists(VersionFilename))
			{
				return File.ReadAllLines(VersionFilename).FirstOrDefault() ?? UnspecifiedVersion;
			}
			else
			{
				return UnspecifiedVersion;
			}
		}

		/// <summary>
		/// Returns path to platform SDKs
		/// </summary>
		/// <returns>Valid SDK string</returns>
		protected string GetPathToPlatformAutoSDKs()
		{
			string SDKPath = "";
			string? SDKRoot = Environment.GetEnvironmentVariable(SDKRootEnvVar);
			if (SDKRoot != null)
			{
				if (SDKRoot != "")
				{
					SDKPath = Path.Combine(SDKRoot, "Host" + GetAutoSDKHostPlatform(), GetAutoSDKPlatformName());
				}
			}
			return SDKPath;
		}

		/// <summary>
		/// Returns path to platform SDKs
		/// </summary>
		/// <returns>Valid SDK string</returns>
		public static bool TryGetHostPlatformAutoSDKDir([NotNullWhen(true)] out DirectoryReference? OutPlatformDir)
		{
			string? SDKRoot = Environment.GetEnvironmentVariable(SDKRootEnvVar);
			if (String.IsNullOrEmpty(SDKRoot))
			{
				OutPlatformDir = null;
				return false;
			}
			else
			{
				OutPlatformDir = DirectoryReference.Combine(new DirectoryReference(SDKRoot), "Host" + GetAutoSDKHostPlatform());
				return true;
			}
		}

		/// <summary>
		/// Because most ManualSDK determination depends on reading env vars, if this process is spawned by a process that ALREADY set up
		/// AutoSDKs then all the SDK env vars will exist, and we will spuriously detect a Manual SDK. (children inherit the environment of the parent process).
		/// Therefore we write out an env variable to set in the command file (OutputEnvVars.txt) such that child processes can determine if their manual SDK detection
		/// is bogus.  Make it platform specific so that platforms can be in different states.
		/// </summary>
		protected string GetPlatformAutoSDKSetupEnvVar()
		{
			return GetAutoSDKPlatformName() + AutoSetupEnvVar;
		}
		protected string GetPlatformManualSDKSetupEnvVar()
		{
			return GetAutoSDKPlatformName() + ManualSetupEnvVar;
		}

		/// <summary>
		/// Gets currently installed version
		/// </summary>
		/// <param name="PlatformSDKRoot">absolute path to platform SDK root</param>
		/// <param name="OutInstalledSDKVersionString">version string as currently installed</param>
		/// <param name="OutInstalledSDKLevel"></param>
		/// <returns>true if was able to read it</returns>
		protected bool GetCurrentlyInstalledSDKString(string PlatformSDKRoot, out string OutInstalledSDKVersionString, out string OutInstalledSDKLevel)
		{
			if (Directory.Exists(PlatformSDKRoot))
			{
				string VersionFilename = Path.Combine(PlatformSDKRoot, CurrentlyInstalledSDKStringManifest);
				if (File.Exists(VersionFilename))
				{
					using (StreamReader Reader = new StreamReader(VersionFilename))
					{
						string? Version = Reader.ReadLine();
						string? Type = Reader.ReadLine();
						string? Level = Reader.ReadLine();
						if (string.IsNullOrEmpty(Level))
						{
							Level = "FULL";
						}

						// don't allow ManualSDK installs to count as an AutoSDK install version.
						if (Type != null && Type == "AutoSDK")
						{
							if (Version != null)
							{
								OutInstalledSDKVersionString = Version;
								OutInstalledSDKLevel = Level;
								return true;
							}
						}
					}
				}
			}

			OutInstalledSDKVersionString = "";
			OutInstalledSDKLevel = "";
			return false;
		}

		/// <summary>
		/// Gets the version of the last successfully run setup script.
		/// </summary>
		/// <param name="PlatformSDKRoot">absolute path to platform SDK root</param>
		/// <param name="OutLastRunScriptVersion">version string</param>
		/// <returns>true if was able to read it</returns>
		protected bool GetLastRunScriptVersionString(string PlatformSDKRoot, out string OutLastRunScriptVersion)
		{
			if (Directory.Exists(PlatformSDKRoot))
			{
				string VersionFilename = Path.Combine(PlatformSDKRoot, LastRunScriptVersionManifest);
				if (File.Exists(VersionFilename))
				{
					using (StreamReader Reader = new StreamReader(VersionFilename))
					{
						string? Version = Reader.ReadLine();
						if (Version != null)
						{
							OutLastRunScriptVersion = Version;
							return true;
						}
					}
				}
			}

			OutLastRunScriptVersion = "";
			return false;
		}

		/// <summary>
		/// Sets currently installed version
		/// </summary>
		/// <param name="InstalledSDKVersionString">SDK version string to set</param>
		/// <param name="InstalledSDKLevelString"></param>
		/// <returns>true if was able to set it</returns>
		protected bool SetCurrentlyInstalledAutoSDKString(String InstalledSDKVersionString, String InstalledSDKLevelString)
		{
			String PlatformSDKRoot = GetPathToPlatformAutoSDKs();
			if (Directory.Exists(PlatformSDKRoot))
			{
				string VersionFilename = Path.Combine(PlatformSDKRoot, CurrentlyInstalledSDKStringManifest);
				if (File.Exists(VersionFilename))
				{
					File.Delete(VersionFilename);
				}

				using (StreamWriter Writer = File.CreateText(VersionFilename))
				{
					Writer.WriteLine(InstalledSDKVersionString);
					Writer.WriteLine("AutoSDK");
					Writer.WriteLine(InstalledSDKLevelString);
					return true;
				}
			}

			return false;
		}

		protected void SetupManualSDK()
		{
			if (PlatformSupportsAutoSDKs() && HasAutoSDKSystemEnabled())
			{
				String InstalledSDKVersionString = GetAutoSDKDirectoryForMainVersion();
				String PlatformSDKRoot = GetPathToPlatformAutoSDKs();
                if (!Directory.Exists(PlatformSDKRoot))
                {
                    Directory.CreateDirectory(PlatformSDKRoot);
                }

				{
					string VersionFilename = Path.Combine(PlatformSDKRoot, CurrentlyInstalledSDKStringManifest);
					if (File.Exists(VersionFilename))
					{
						File.Delete(VersionFilename);
					}

					string EnvVarFile = Path.Combine(PlatformSDKRoot, SDKEnvironmentVarsFile);
					if (File.Exists(EnvVarFile))
					{
						File.Delete(EnvVarFile);
					}

					using (StreamWriter Writer = File.CreateText(VersionFilename))
					{
						Writer.WriteLine(InstalledSDKVersionString);
						Writer.WriteLine("ManualSDK");
						Writer.WriteLine("FULL");
					}
				}
			}
		}

		protected bool SetLastRunAutoSDKScriptVersion(string LastRunScriptVersion)
		{
			String PlatformSDKRoot = GetPathToPlatformAutoSDKs();
			if (Directory.Exists(PlatformSDKRoot))
			{
				string VersionFilename = Path.Combine(PlatformSDKRoot, LastRunScriptVersionManifest);
				if (File.Exists(VersionFilename))
				{
					File.Delete(VersionFilename);
				}

				using (StreamWriter Writer = File.CreateText(VersionFilename))
				{
					Writer.WriteLine(LastRunScriptVersion);
					return true;
				}
			}
			return false;
		}

		/// <summary>
		/// Returns Hook names as needed by the platform
		/// (e.g. can be overridden with custom executables or scripts)
		/// </summary>
		/// <param name="Hook">Hook type</param>
		protected virtual string GetHookExecutableName(SDKHookType Hook)
		{
			if (RuntimePlatform.IsWindows)
			{
				if (Hook == SDKHookType.Uninstall)
				{
					return "unsetup.bat";
				}
				else
				{
					return "setup.bat";
				}
			}
			else
			{
				if (Hook == SDKHookType.Uninstall)
				{
					return "unsetup.sh";
				}
				else
				{
					return "setup.sh";
				}
			}
		}

		/// <summary>
		/// Whether the hook must be run with administrator privileges.
		/// </summary>
		/// <param name="Hook">Hook for which to check the required privileges.</param>
		/// <returns>true if the hook must be run with administrator privileges.</returns>
		protected virtual bool DoesHookRequireAdmin(SDKHookType Hook)
		{
			return true;
		}

		private void LogAutoSDKHook(object Sender, DataReceivedEventArgs Args)
		{
			if (Args.Data != null)
			{
				LogFormatOptions Options = Log.OutputLevel >= LogEventType.Verbose ? LogFormatOptions.None : LogFormatOptions.NoConsoleOutput;
				Log.WriteLine(LogEventType.Log, Options, Args.Data);
			}
		}

		static private string[] AutoSDKLevels = new string[]
		{
			"NONE",
			"BUILD",
			"PACKAGE",
			"RUN",
			"FULL",
		};

		private bool IsSDKLevelAtLeast(string Value, string ComparedTo)
		{
			int ValueIndex = AutoSDKLevels.FindIndex(x => x == Value.ToUpper());
			int ComparedIndex = AutoSDKLevels.FindIndex(x => x == ComparedTo.ToUpper());
			if (ValueIndex < 0 || ComparedIndex < 0)
			{
				throw new Exception($"Passed in a bad value to IsSDKLevelAtLeast: {Value}, {ComparedTo}");
			}
			return ValueIndex >= ComparedIndex;
		}

		private string GetAutoSDKLevelForPlatform(string PlatformSDKRoot)
		{
			// the last component is the platform name
			string PlatformName = Path.GetFileName(PlatformSDKRoot).ToUpper();

			// parse the envvar
			Dictionary<string, string> PlatformSpecificLevels = new Dictionary<string, string>();
			string? DetailedSettings = Environment.GetEnvironmentVariable("UE_AUTOSDK_SPECIFIC_LEVELS"); // "Android=PACKAGE;GDk=RuN;PS5=BUILD";
			if (!string.IsNullOrEmpty(DetailedSettings) && DetailedSettings.Contains(PlatformName, StringComparison.InvariantCultureIgnoreCase))
			{
				foreach (string Detail in DetailedSettings.ToUpper().Split(';'))
				{
					string[] Tokens = Detail.Split('=');
					// validate the level string
					if (AutoSDKLevels.Contains(Tokens[1]))
					{
						PlatformSpecificLevels.Add(Tokens[0], Tokens[1]);
					}
				}
			}

			string FinalAutoSDKLevel = "FULL";
			if (PlatformSpecificLevels.ContainsKey(PlatformName))
			{
				FinalAutoSDKLevel = PlatformSpecificLevels[PlatformName];
			}
			else
			{
				string? DefaultLevel = Environment.GetEnvironmentVariable("UE_AUTOSDK_DEFAULT_LEVEL");
				if (!string.IsNullOrEmpty(DefaultLevel) && AutoSDKLevels.Contains(DefaultLevel, StringComparer.InvariantCultureIgnoreCase))
				{
					FinalAutoSDKLevel = DefaultLevel.ToUpper();
				}
			}

			return FinalAutoSDKLevel;
		}

		/// <summary>
		/// Runs install/uninstall hooks for SDK
		/// </summary>
		/// <param name="PlatformSDKRoot">absolute path to platform SDK root</param>
		/// <param name="SDKVersionString">version string to run for (can be empty!)</param>
		/// <param name="AutoSDKLevel"></param>
		/// <param name="Hook">which one of hooks to run</param>
		/// <param name="bHookCanBeNonExistent">whether a non-existing hook means failure</param>
		/// <returns>true if succeeded</returns>
		protected virtual bool RunAutoSDKHooks(string PlatformSDKRoot, string SDKVersionString, string AutoSDKLevel, SDKHookType Hook, bool bHookCanBeNonExistent = true)
		{
			if (!IsAutoSDKSafe())
			{
				Logger.LogDebug("{Platform} attempted to run SDK hook which could have damaged manual SDK install!", GetAutoSDKPlatformName());
				return false;
			}
			if (SDKVersionString != "")
			{
				string SDKDirectory = Path.Combine(PlatformSDKRoot, SDKVersionString);
				string HookExe = Path.Combine(SDKDirectory, GetHookExecutableName(Hook));

				if (File.Exists(HookExe))
				{
					Logger.LogDebug("Running {Hook} hook {HookExe}", Hook, HookExe);

					// run it
					Process HookProcess = new Process();
					HookProcess.StartInfo.WorkingDirectory = SDKDirectory;
					HookProcess.StartInfo.FileName = HookExe;
					HookProcess.StartInfo.Arguments = AutoSDKLevel;
					HookProcess.StartInfo.WindowStyle = ProcessWindowStyle.Hidden;

					bool bHookRequiresAdmin = DoesHookRequireAdmin(Hook);
					if (bHookRequiresAdmin)
					{
						// installers may require administrator access to succeed. so run as an admin.
						HookProcess.StartInfo.Verb = "runas";

						//Forcing the old .Net Framework default to prevent processes from failing
						HookProcess.StartInfo.UseShellExecute = true;
					}
					else
					{
						HookProcess.StartInfo.UseShellExecute = false;
						HookProcess.StartInfo.RedirectStandardOutput = true;
						HookProcess.StartInfo.RedirectStandardError = true;
						HookProcess.OutputDataReceived += LogAutoSDKHook;
						HookProcess.ErrorDataReceived += LogAutoSDKHook;
					}

					//using (ScopedTimer HookTimer = new ScopedTimer("Time to run hook: ", LogEventType.Log))
					{
						HookProcess.Start();
						if (!bHookRequiresAdmin)
						{
							HookProcess.BeginOutputReadLine();
							HookProcess.BeginErrorReadLine();
						}
						HookProcess.WaitForExit();
					}

					if (HookProcess.ExitCode != 0)
					{
						Logger.LogDebug("Hook exited uncleanly (returned {ExitCode}), considering it failed.", HookProcess.ExitCode);
						return false;
					}

					return true;
				}
				else
				{
					Logger.LogDebug("File {HookExe} does not exist", HookExe);
				}
			}
			else
			{
				Logger.LogDebug("Version string is blank for {SdkRoot}. Can't determine {Hook} hook.", PlatformSDKRoot, Hook.ToString());
			}

			return bHookCanBeNonExistent;
		}

		/// <summary>
		/// Loads environment variables from SDK
		/// If any commands are added or removed the handling needs to be duplicated in
		/// TargetPlatformManagerModule.cpp
		/// </summary>
		/// <param name="PlatformSDKRoot">absolute path to platform SDK</param>
		/// <returns>true if succeeded</returns>
		protected bool SetupEnvironmentFromAutoSDK(string PlatformSDKRoot)
		{
			string EnvVarFile = Path.Combine(PlatformSDKRoot, SDKEnvironmentVarsFile);
			if (File.Exists(EnvVarFile))
			{
				using (StreamReader Reader = new StreamReader(EnvVarFile))
				{
					List<string> PathAdds = new List<string>();
					List<string> PathRemoves = new List<string>();

					List<string> EnvVarNames = new List<string>();
					List<string> EnvVarValues = new List<string>();

					bool bNeedsToWriteAutoSetupEnvVar = true;
					String PlatformSetupEnvVar = GetPlatformAutoSDKSetupEnvVar();
					for (; ; )
					{
						string? VariableString = Reader.ReadLine();
						if (VariableString == null)
						{
							break;
						}

						string[] Parts = VariableString.Split('=');
						if (Parts.Length != 2)
						{
							Logger.LogDebug("Incorrect environment variable declaration:");
							Logger.LogDebug("{VariableString}", VariableString);
							return false;
						}

						if (String.Compare(Parts[0], "strippath", true) == 0)
						{
							PathRemoves.Add(Parts[1]);
						}
						else if (String.Compare(Parts[0], "addpath", true) == 0)
						{
							PathAdds.Add(Parts[1]);
						}
						else
						{
							if (String.Compare(Parts[0], PlatformSetupEnvVar) == 0)
							{
								bNeedsToWriteAutoSetupEnvVar = false;
							}
							// convenience for setup.bat writers.  Trim any accidental whitespace from variable names/values.
							EnvVarNames.Add(Parts[0].Trim());
							EnvVarValues.Add(Parts[1].Trim());
						}
					}

					// don't actually set anything until we successfully validate and read all values in.
					// we don't want to set a few vars, return a failure, and then have a platform try to
					// build against a manually installed SDK with half-set env vars.
					for (int i = 0; i < EnvVarNames.Count; ++i)
					{
						string EnvVarName = EnvVarNames[i];
						string EnvVarValue = EnvVarValues[i];
						Logger.LogDebug("Setting variable '{Name}' to '{Value}'", EnvVarName, EnvVarValue);
						Environment.SetEnvironmentVariable(EnvVarName, EnvVarValue);
					}


                    // actually perform the PATH stripping / adding.
                    String? OrigPathVar = Environment.GetEnvironmentVariable("PATH");
                    String PathDelimiter = RuntimePlatform.IsWindows ? ";" : ":";
                    String[] PathVars = { };
                    if (!String.IsNullOrEmpty(OrigPathVar))
                    {
                        PathVars = OrigPathVar.Split(PathDelimiter.ToCharArray());
                    }
                    else
                    {
                        Logger.LogDebug("Path environment variable is null during AutoSDK");
                    }

					List<String> ModifiedPathVars = new List<string>();
					ModifiedPathVars.AddRange(PathVars);

					// perform removes first, in case they overlap with any adds.
					foreach (String PathRemove in PathRemoves)
					{
						foreach (String PathVar in PathVars)
						{
							if (PathVar.IndexOf(PathRemove, StringComparison.OrdinalIgnoreCase) >= 0)
							{
								Logger.LogDebug("Removing Path: '{Path}'", PathVar);
								ModifiedPathVars.Remove(PathVar);
							}
						}
					}

					// remove all the of ADDs so that if this function is executed multiple times, the paths will be guaranteed to be in the same order after each run.
					// If we did not do this, a 'remove' that matched some, but not all, of our 'adds' would cause the order to change.
					foreach (String PathAdd in PathAdds)
					{
						foreach (String PathVar in PathVars)
						{
							if (String.Compare(PathAdd, PathVar, true) == 0)
							{
								Logger.LogDebug("Removing Path: '{Path}'", PathVar);
								ModifiedPathVars.Remove(PathVar);
							}
						}
					}

					// perform adds, but don't add duplicates
					foreach (String PathAdd in PathAdds)
					{
						if (!ModifiedPathVars.Contains(PathAdd))
						{
							Logger.LogDebug("Adding Path: '{Path}'", PathAdd);
							ModifiedPathVars.Add(PathAdd);
						}
					}

					String ModifiedPath = String.Join(PathDelimiter, ModifiedPathVars);
					Environment.SetEnvironmentVariable("PATH", ModifiedPath);

					Reader.Close();

					// write out environment variable command so any process using this commandfile will mark itself as having had autosdks set up.
					// avoids child processes spuriously detecting manualsdks.
					if (bNeedsToWriteAutoSetupEnvVar)
					{
						// write out the manual sdk version since child processes won't be able to detect manual with AutoSDK messing up env vars
						using (StreamWriter Writer = File.AppendText(EnvVarFile))
						{
							Writer.WriteLine("{0}={1}", PlatformSetupEnvVar, "1");
						}
						// set the variable in the local environment in case this process spawns any others.
						Environment.SetEnvironmentVariable(PlatformSetupEnvVar, "1");
					}

					// make sure we know that we've modified the local environment, invalidating manual installs for this run.
					bLocalProcessSetupAutoSDK = true;

					// tell any child processes what our manual versions were before setting up autosdk
					string ValueToWrite = CachedManualSDKVersions.Count > 0 ? JsonSerializer.Serialize(CachedManualSDKVersions) : "__None";
					Environment.SetEnvironmentVariable(GetPlatformManualSDKSetupEnvVar(), ValueToWrite);

					return true;
				}
			}
			else
			{
				Logger.LogDebug("Cannot set up environment for {SdkRoot} because command file {EnvVarFile} does not exist.", PlatformSDKRoot, EnvVarFile);
			}

			return false;
		}

		protected void InvalidateCurrentlyInstalledAutoSDK()
		{
			String PlatformSDKRoot = GetPathToPlatformAutoSDKs();
			if (Directory.Exists(PlatformSDKRoot))
			{
				string SDKFilename = Path.Combine(PlatformSDKRoot, CurrentlyInstalledSDKStringManifest);
				if (File.Exists(SDKFilename))
				{
					File.Delete(SDKFilename);
				}

				string VersionFilename = Path.Combine(PlatformSDKRoot, LastRunScriptVersionManifest);
				if (File.Exists(VersionFilename))
				{
					File.Delete(VersionFilename);
				}

				string EnvVarFile = Path.Combine(PlatformSDKRoot, SDKEnvironmentVarsFile);
				if (File.Exists(EnvVarFile))
				{
					File.Delete(EnvVarFile);
				}
			}
		}

		/// <summary>
		/// Currently installed AutoSDK is written out to a text file in a known location.
		/// This function just compares the file's contents with the current requirements.
		/// </summary>
		public SDKStatus HasRequiredAutoSDKInstalled()
		{
			if (PlatformSupportsAutoSDKs() && HasAutoSDKSystemEnabled())
			{
				string AutoSDKRoot = GetPathToPlatformAutoSDKs();
				if (AutoSDKRoot != "")
				{
					string DesiredSDKLevel = GetAutoSDKLevelForPlatform(AutoSDKRoot);
					// if the user doesn't want AutoSDK for this platform, then return that it is not installed, even if it actually is
					if (string.Compare(DesiredSDKLevel, "NONE", true) == 0)
					{
						return SDKStatus.Invalid;
					}

					// check script version so script fixes can be propagated without touching every build machine's CurrentlyInstalled file manually.
					string CurrentScriptVersionString;
					if (GetLastRunScriptVersionString(AutoSDKRoot, out CurrentScriptVersionString) && CurrentScriptVersionString == GetRequiredScriptVersionString())
					{
						// check to make sure OutputEnvVars doesn't need regenerating
						string EnvVarFile = Path.Combine(AutoSDKRoot, SDKEnvironmentVarsFile);
						bool bEnvVarFileExists = File.Exists(EnvVarFile);

						string CurrentSDKString;
						string CurrentSDKLevel;
						if (bEnvVarFileExists && GetCurrentlyInstalledSDKString(AutoSDKRoot, out CurrentSDKString, out CurrentSDKLevel))
						{
							// match version
							if (CurrentSDKString == GetAutoSDKDirectoryForMainVersion())
							{
								if (IsSDKLevelAtLeast(CurrentSDKLevel, DesiredSDKLevel))
								{
									return SDKStatus.Valid;
								}
							}
						}
					}
				}
			}
			return SDKStatus.Invalid;
		}

		// This tracks if we have already checked the sdk installation.
		private Int32 SDKCheckStatus = -1;

		// true if we've ever overridden the process's environment with AutoSDK data.  After that, manual installs cannot be considered valid ever again.
		private bool bLocalProcessSetupAutoSDK = false;

		protected bool HasSetupAutoSDK()
		{
			return bLocalProcessSetupAutoSDK || HasParentProcessSetupAutoSDK(out _);
		}

		protected bool HasParentProcessSetupAutoSDK([NotNullWhen(true)] out string? OutAutoSDKSetupValue)
		{
			String AutoSDKSetupVarName = GetPlatformAutoSDKSetupEnvVar();
			OutAutoSDKSetupValue = Environment.GetEnvironmentVariable(AutoSDKSetupVarName);
			
			if (!String.IsNullOrEmpty(OutAutoSDKSetupValue))
			{
				return true;
			}
			return false;
		}

		public SDKStatus HasRequiredManualSDK()
		{
// 			if (HasSetupAutoSDK())
// 			{
// 				return SDKStatus.Invalid;
// 			}
//
//			// manual installs are always invalid if we have modified the process's environment for AutoSDKs
			return HasRequiredManualSDKInternal();
		}

		// for platforms with destructive AutoSDK.  Report if any manual sdk is installed that may be damaged by an autosdk.
		protected virtual bool HasAnyManualInstall()
		{
			return false;
		}

		// tells us if the user has a valid manual install.
		protected virtual SDKStatus HasRequiredManualSDKInternal()
		{
			return GetAllSDKInfo().AreAllManualSDKsValid() ? SDKStatus.Valid : SDKStatus.Invalid;
			//string? ManualSDKVersion;
			//GetInstalledVersions(out ManualSDKVersion, out _);

			//return IsVersionValid(ManualSDKVersion, bForAutoSDK:false) ? SDKStatus.Valid : SDKStatus.Invalid;
		}

		// some platforms will fail if there is a manual install that is the WRONG manual install.
		protected virtual bool AllowInvalidManualInstall()
		{
			return true;
		}

		// platforms can choose if they prefer a correct the the AutoSDK install over the manual install.
		protected virtual bool PreferAutoSDK()
		{
			return true;
		}

		// some platforms don't support parallel SDK installs.  AutoSDK on these platforms will
		// actively damage an existing manual install by overwriting files in it.  AutoSDK must NOT
		// run any setup if a manual install exists in this case.
		protected virtual bool IsAutoSDKDestructive()
		{
			return false;
		}

		/// <summary>
		/// Runs batch files if necessary to set up required AutoSDK.
		/// AutoSDKs are SDKs that have not been setup through a formal installer, but rather come from
		/// a source control directory, or other local copy.
		/// </summary>
		private void SetupAutoSDK()
		{
			if (IsAutoSDKSafe() && PlatformSupportsAutoSDKs() && HasAutoSDKSystemEnabled())
			{
				// run installation for autosdk if necessary.
				if (HasRequiredAutoSDKInstalled() == SDKStatus.Invalid)
				{
					string AutoSDKRoot = GetPathToPlatformAutoSDKs();

					string DesiredSDKLevel = GetAutoSDKLevelForPlatform(AutoSDKRoot);
					// if the user doesn't want AutoSDK for this platform, then do nothing
					if (string.Compare(DesiredSDKLevel, "NONE", true) == 0)
					{
						Logger.LogDebug("Skipping AutoSDK for {PlatformName} because NONE was specified as the desired AutoSDK level", PlatformName);
						return;
					}

					//reset check status so any checking sdk status after the attempted setup will do a real check again.
					SDKCheckStatus = -1;

					string CurrentSDKString;
					string CurrentSDKLevel;
					GetCurrentlyInstalledSDKString(AutoSDKRoot, out CurrentSDKString, out CurrentSDKLevel);

					// switch over (note that version string can be empty)
					if (!RunAutoSDKHooks(AutoSDKRoot, CurrentSDKString, CurrentSDKLevel, SDKHookType.Uninstall))
					{
						Logger.LogDebug("Failed to uninstall currently installed SDK {SdkVersion}", CurrentSDKString);
						InvalidateCurrentlyInstalledAutoSDK();
						return;
					}
					// delete Manifest file to avoid multiple uninstalls
					InvalidateCurrentlyInstalledAutoSDK();

					if (!RunAutoSDKHooks(AutoSDKRoot, GetAutoSDKDirectoryForMainVersion(), DesiredSDKLevel, SDKHookType.Install, false))
					{
						Logger.LogDebug("Failed to install required SDK {SdkVersion}.  Attemping to uninstall", GetAutoSDKDirectoryForMainVersion());
						RunAutoSDKHooks(AutoSDKRoot, GetAutoSDKDirectoryForMainVersion(), DesiredSDKLevel, SDKHookType.Uninstall, false);
						return;
					}

					string EnvVarFile = Path.Combine(AutoSDKRoot, SDKEnvironmentVarsFile);
					if (!File.Exists(EnvVarFile))
					{
						Logger.LogDebug("Installation of required SDK {SdkVersion}.  Did not generate Environment file {EnvVarFile}", GetAutoSDKDirectoryForMainVersion(), EnvVarFile);
						RunAutoSDKHooks(AutoSDKRoot, GetAutoSDKDirectoryForMainVersion(), DesiredSDKLevel, SDKHookType.Uninstall, false);
						return;
					}

					SetCurrentlyInstalledAutoSDKString(GetAutoSDKDirectoryForMainVersion(), DesiredSDKLevel);
					SetLastRunAutoSDKScriptVersion(GetRequiredScriptVersionString());
				}

				// fixup process environment to match autosdk
				SetupEnvironmentFromAutoSDK();
			}
		}

		/// <summary>
		/// Allows the platform to optionally returns a path to the internal SDK
		/// </summary>
		/// <returns>Valid path to the internal SDK, null otherwise</returns>
		public virtual string? GetInternalSDKPath()
		{
			return null;
		}

		#endregion

		#region public AutoSDKs Utility

		/// <summary>
		/// Enum describing types of hooks a platform SDK can have
		/// </summary>
		public enum SDKHookType
		{
			Install,
			Uninstall
		};

		/* Whether or not we should try to automatically switch SDKs when asked to validate the platform's SDK state. */
		public static bool bAllowAutoSDKSwitching = true;

		public SDKStatus SetupEnvironmentFromAutoSDK()
		{
			string PlatformSDKRoot = GetPathToPlatformAutoSDKs();

			// load environment variables from current SDK
			if (!SetupEnvironmentFromAutoSDK(PlatformSDKRoot))
			{
				Logger.LogDebug("Failed to load environment from required SDK {SdkRoot}", GetAutoSDKDirectoryForMainVersion());
				InvalidateCurrentlyInstalledAutoSDK();
				return SDKStatus.Invalid;
			}
			return SDKStatus.Valid;
		}

		/// <summary>
		/// Whether the required external SDKs are installed for this platform.
		/// Could be either a manual install or an AutoSDK.
		/// </summary>
		public SDKStatus HasRequiredSDKsInstalled()
		{
			// avoid redundant potentially expensive SDK checks.
			if (SDKCheckStatus == -1)
			{
				bool bHasManualSDK = HasRequiredManualSDK() == SDKStatus.Valid;
				bool bHasAutoSDK = HasRequiredAutoSDKInstalled() == SDKStatus.Valid;

				// Per-Platform implementations can choose how to handle non-Auto SDK detection / handling.
				SDKCheckStatus = (bHasManualSDK || bHasAutoSDK) ? 1 : 0;
			}
			return SDKCheckStatus == 1 ? SDKStatus.Valid : SDKStatus.Invalid;
		}

		// Arbitrates between manual SDKs and setting up AutoSDK based on program options and platform preferences.
		public void ManageAndValidateSDK()
		{
			// do not modify installed manifests if parent process has already set everything up.
			// this avoids problems with determining IsAutoSDKSafe and doing an incorrect invalidate.
			if (bAllowAutoSDKSwitching && !HasParentProcessSetupAutoSDK(out _))
			{
				bool bSetSomeSDK = false;
				bool bHasRequiredManualSDK = HasRequiredManualSDK() == SDKStatus.Valid;
				if (IsAutoSDKSafe() && (PreferAutoSDK() || !bHasRequiredManualSDK))
				{
					SetupAutoSDK();
					bSetSomeSDK = true;
				}

				//Setup manual SDK if autoSDK setup was skipped or failed for whatever reason.
				if (bHasRequiredManualSDK && (HasRequiredAutoSDKInstalled() != SDKStatus.Valid))
				{
					SetupManualSDK();
					bSetSomeSDK = true;
				}

				if (!bSetSomeSDK)
				{
					InvalidateCurrentlyInstalledAutoSDK();
				}
			}

			// print all SDKs to log file (errors will print out later for builds and generateprojectfiles)
			PrintSDKInfoAndReturnValidity(LogEventType.Log, LogFormatOptions.NoConsoleOutput, LogEventType.Verbose, LogFormatOptions.NoConsoleOutput);
		}
		#endregion

	}
}