// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.IO;
using AutomationTool;
using UnrealBuildTool;
using System.Linq;
using System.Net.NetworkInformation;
using System.Text.RegularExpressions;
using System.Net;
using System.Text.Json;
using System.Text.Json.Serialization;

namespace Gauntlet
{
	/// <summary>
	/// Defines roles that can be performed for Unreal
	/// 
	/// TODO - Editor variants should be a tag, not an enum?
	/// </summary>
	public enum UnrealTargetRole
	{
		Unknown,
		Editor,
		EditorGame,
		EditorServer,
		Client,
		Server,
		CookedEditor,
	};

    /// <summary>
    /// Base class of a file to copy as part of a test.
    /// </summary>
    public class UnrealFileToCopy
    {
        /// <summary>
        /// Base file to copy, including filename and extension.
        /// </summary>
        public string SourceFileLocation;
        /// <summary>
        /// The high-level base directory we want to copy to.
        /// </summary>
        public EIntendedBaseCopyDirectory TargetBaseDirectory;
        /// <summary>
        /// Where to copy the file to, including filename and extension, relative to the intended base directory.
        /// Should be any additional subdirectories, plus file name.
        /// </summary>
        public string TargetRelativeLocation;
        public UnrealFileToCopy(string SourceLoc, EIntendedBaseCopyDirectory TargetDirType, string TargetLoc)
        {
            SourceFileLocation = SourceLoc;
            TargetBaseDirectory = TargetDirType;
            TargetRelativeLocation = TargetLoc;
        }
    }

    /// <summary>
    /// Helper extensions for our enums
    /// </summary>
    public static class UnrealRoleTypeExtensions
	{
		public static bool UsesEditor(this UnrealTargetRole Type)
		{
			if (Globals.Params.ParseParam("cookededitor"))
			{
				return Type == UnrealTargetRole.EditorGame || Type == UnrealTargetRole.EditorServer;
			}
			return Type == UnrealTargetRole.Editor || Type == UnrealTargetRole.EditorGame || Type == UnrealTargetRole.EditorServer;
		}

		public static bool IsServer(this UnrealTargetRole Type)
		{
			return Type == UnrealTargetRole.EditorServer || Type == UnrealTargetRole.Server;
		}

		public static bool IsClient(this UnrealTargetRole Type)
		{
			return Type == UnrealTargetRole.EditorGame || Type == UnrealTargetRole.Client;
		}

		public static bool IsEditor(this UnrealTargetRole Type)
		{
			return Type == UnrealTargetRole.Editor || Type == UnrealTargetRole.CookedEditor;
		}
		public static bool IsCookedEditor(this UnrealTargetRole Type)
		{
			return Type == UnrealTargetRole.CookedEditor;
		}

		public static bool RunsLocally(this UnrealTargetRole Type)
		{
			return UsesEditor(Type) || IsServer(Type) || Type == UnrealTargetRole.CookedEditor;
		}
	}

	/// <summary>
	/// Utility functions for Unreal tests
	/// </summary>
	public static class UnrealHelpers
	{
		public static UnrealTargetPlatform GetPlatformFromString(string PlatformName)
		{
			UnrealTargetPlatform UnrealPlatform;
			if (UnrealTargetPlatform.TryParse(PlatformName, out UnrealPlatform))
            {
	            return UnrealPlatform;
            }
			
			throw new AutomationException("Unable convert platform {0} into a valid Unreal Platform", PlatformName);
		}

		/// <summary>
		/// Given a platform and a client/server flag, returns the name Unreal refers to it as. E.g. "WindowsClient", "LinuxServer".
		/// </summary>
		/// <param name="InTargetPlatform"></param>
		/// <param name="InTargetType"></param>
		/// <returns></returns>
		public static string GetPlatformName(UnrealTargetPlatform TargetPlatform, UnrealTargetRole ProcessType, bool UsesSharedBuildType)
		{
			string Platform = "";

			bool IsDesktop = UnrealBuildTool.Utils.GetPlatformsInClass(UnrealPlatformClass.Desktop).Contains(TargetPlatform);

			// These platforms can be built as either game, server or client
			if (IsDesktop)
			{
				Platform = (TargetPlatform == UnrealTargetPlatform.Win64) ? "Windows" : TargetPlatform.ToString();

				if (ProcessType == UnrealTargetRole.Client)
				{
					if (!UsesSharedBuildType)
					{
						Platform += "Client";
					}
				}
				else if (ProcessType == UnrealTargetRole.Server)
				{
					if (!UsesSharedBuildType)
					{
						Platform += "Server";
					}
				}
			}
			else if (TargetPlatform == UnrealTargetPlatform.Android)
			{
				// TODO: hardcoded ETC2 for now, need to determine cook flavor used.
				// actual flavour required may depend on the target HW...
				if (UsesSharedBuildType)
				{
					Platform = TargetPlatform.ToString() + "_ETC2";
				}
				else
				{
					Platform = TargetPlatform.ToString() + "_ETC2Client";
				}
			}
			else
			{
				Platform = TargetPlatform.ToString();
			}

			return Platform;
		}

		/// <summary>
		/// Fallback to use when GetHostEntry throws an exception.
		/// </summary>
		/// <returns>
		/// An array of IP addresses associated with the network interface Type.
		/// </returns>
		public static System.Net.IPAddress[] GetAllLocalIPv4(NetworkInterfaceType Type)
		{
			List<System.Net.IPAddress> IpAddrList = new List<System.Net.IPAddress>();

			foreach (NetworkInterface Item in NetworkInterface.GetAllNetworkInterfaces())
			{
				if (Item.NetworkInterfaceType == Type && Item.OperationalStatus == OperationalStatus.Up)
				{
					foreach (UnicastIPAddressInformation IpAddr in Item.GetIPProperties().UnicastAddresses)
					{
						if (IpAddr.Address.AddressFamily == System.Net.Sockets.AddressFamily.InterNetwork)
						{
							IpAddrList.Add(IpAddr.Address);
						}
					}
				}
			}

			return IpAddrList.ToArray();
		}

		/// <summary>
		/// Gets the filehost IP to provide to devkits by examining our local adapters and
		/// returning the one that's active and on the local LAN (based on DNS assignment)
		/// </summary>
		/// <returns></returns>
		public static string GetHostIpAddress(string PreferredDomain="epicgames.net")
		{
			System.Net.IPAddress LocalAddress;

			try
			{
				// Default to the first address with a valid prefix
				LocalAddress = Dns.GetHostEntry(Dns.GetHostName()).AddressList
					.Where(o => o.AddressFamily == System.Net.Sockets.AddressFamily.InterNetwork
						&& o.GetAddressBytes()[0] != 169)
					.FirstOrDefault();
			}
			catch
			{
				//
				// When the above fails, attempt to extract the eth IP manually.
				//
				// TODO: Fallback to the wireless adapter if/when no device is
				//       available/active.
				//
				LocalAddress = GetAllLocalIPv4(NetworkInterfaceType.Ethernet)
					.Where(o => o.AddressFamily == System.Net.Sockets.AddressFamily.InterNetwork
						&& o.GetAddressBytes()[0] != 169)
					.FirstOrDefault();
			}

			var ActiveInterfaces = NetworkInterface.GetAllNetworkInterfaces()
				.Where(I => I.OperationalStatus == OperationalStatus.Up);

			bool MultipleInterfaces = ActiveInterfaces.Count() > 1;

			if (MultipleInterfaces)
			{
				// Now, lots of Epic PCs have virtual adapters etc, so see if there's one that's on our network and if so use that IP
				var PreferredInterface = ActiveInterfaces
					.Where(I => I.GetIPProperties().DnsSuffix.IndexOf(PreferredDomain, StringComparison.OrdinalIgnoreCase) >= 0)
					.SelectMany(I => I.GetIPProperties().UnicastAddresses)
					.Where(A => A.Address.AddressFamily == System.Net.Sockets.AddressFamily.InterNetwork)
					.FirstOrDefault();

				if (PreferredInterface != null)
				{
					LocalAddress = PreferredInterface.Address;
				}
			}
	
			string HostIP = Globals.Params.ParseValue("hostip", "");
			HostIP = string.IsNullOrEmpty(HostIP) ? LocalAddress.ToString() : HostIP;
			return HostIP;
		}

		static public string GetExecutableName(string ProjectName, UnrealTargetPlatform Platform, UnrealTargetConfiguration Config, UnrealTargetRole Role, string Extension)
		{
			string ExeName = ProjectName;

			if (Role == UnrealTargetRole.Server)
			{
				ExeName = Regex.Replace(ExeName, "Game", "Server", RegexOptions.IgnoreCase);
			}
			else if (Role == UnrealTargetRole.Client)
			{
				ExeName = Regex.Replace(ExeName, "Game", "Client", RegexOptions.IgnoreCase);
			}

			if (Config != UnrealTargetConfiguration.Development)
			{
				ExeName += string.Format("-{0}-{1}", Platform, Config);
			}

			// todo , how to find this?
			if (Platform == UnrealTargetPlatform.Android)
			{
				ExeName += "-arm64";
			}

			// not all platforms use an extension
			if (!string.IsNullOrEmpty(Extension))
			{
				if (Extension.StartsWith(".") == false)
				{
					ExeName += ".";
				}

				ExeName += Extension;
			}

			return ExeName;
		}

		internal class ConfigInfo
		{
			public UnrealTargetRole 			RoleType;
			public UnrealTargetPlatform? 		Platform;
			public UnrealTargetConfiguration 	Configuration;
			public bool							SharedBuild;
			public string						Flavor;

			public ConfigInfo()
			{
				RoleType = UnrealTargetRole.Unknown;
				Configuration = UnrealTargetConfiguration.Unknown;
			}
		}

		public static Dictionary<string, UnrealTargetRole> CustomModuleToRoles = new Dictionary<string, UnrealTargetRole>();

		/// <summary>
		/// Adds an additional module name to search for when determining if an application path has a guantlet usable role.
		/// Traditionally all application paths must be in the form ProjectName(Game|Server).exe
		/// This method allows for applications with nontraditional names to have the role of Game, Server, Editor etc.
		/// 
		/// </summary>
		/// <param name="InModuleName"> The ModuleName to look for in the application path</param>
		/// <param name="InRole"> When applications are discovered applications with matching module name will have this role</param>
		public static void AddCustomModuleName(string InModuleName, UnrealTargetRole InRole)
		{
			CustomModuleToRoles.Add(InModuleName, InRole);
		}
		
		static ConfigInfo GetUnrealConfigFromFileName(string InProjectName, string InName)
		{
			ConfigInfo Config = new ConfigInfo();

			string ShortName = Regex.Replace(InProjectName, "Game", "", RegexOptions.IgnoreCase);

			if (InName.StartsWith("UnrealGame", StringComparison.OrdinalIgnoreCase))
			{
				ShortName = "Unreal";
			}

			string AppName = Path.GetFileNameWithoutExtension(InName);

			// A project name may be either something like EngineTest or FortniteGame.
			// The Game, Client, Server executables will be called
			// EngineTest, EngineTestClient, EngineTestServer
			// FortniteGame, FortniteClient, FortniteServer
			// Or EngineTest-WIn64-Shipping, FortniteClient-Win64-Shipping etc
			// So we need to search for the project name minus 'Game', with the form, build-type, and platform all optional :(
			// FortniteClient and EngineTest should match
			// FortniteCustomName should not match.
			string ProjectNameRegEx = string.Format("|{0}", InProjectName);
			foreach (KeyValuePair<string, UnrealTargetRole> ModuleAndRole in CustomModuleToRoles)
			{
				ProjectNameRegEx += string.Format("|{0}", ModuleAndRole.Key);
			}
			string RegExMatch = string.Format(@"^(?:.+[_-])?(({0}(Game|Client|Server|CookedEditor)){1})(?:-(.+?)-(Debug|Test|Shipping))?(?:[_-](.+))?$", ShortName, ProjectNameRegEx);

			// Format should be something like
			// FortniteClient
			// FortniteClient-Win64-Test
			// Match this and break it down to components
			var NameMatch = Regex.Match(AppName, RegExMatch, RegexOptions.IgnoreCase);

			if (NameMatch.Success)
			{
				string ModuleName = NameMatch.Groups[1].ToString();
				string ModuleType = NameMatch.Groups[3].ToString().ToLower();
				string PlatformName = NameMatch.Groups[4].ToString();
				string ConfigType = NameMatch.Groups[5].ToString();
				string BuildFlavor= NameMatch.Groups[6].ToString().ToLower();
				if (CustomModuleToRoles.ContainsKey(ModuleName))
				{
					Config.RoleType = CustomModuleToRoles[ModuleName];
				}
				else if (ModuleType == "cookededitor" || (ModuleType.Length == 0 && Globals.Params.ParseParam("cookededitor")))
				{
					Config.RoleType = UnrealTargetRole.CookedEditor;
				}
				else if (ModuleType.Length == 0 || ModuleType == "game")
				{
					// how to express client&server?
					Config.RoleType = UnrealTargetRole.Client;
					Config.SharedBuild = true;
				}
				else if (ModuleType == "client")
				{
					Config.RoleType = UnrealTargetRole.Client;
				}
				else if (ModuleType == "server")
				{
					Config.RoleType = UnrealTargetRole.Server;
				}

				if (ConfigType.Length > 0)
				{
					Enum.TryParse(ConfigType, true, out Config.Configuration);
				}
				else
				{
					Config.Configuration = UnrealTargetConfiguration.Development;   // Development has no string
				}
				Config.Flavor = BuildFlavor;

				UnrealTargetPlatform Platform;
				if (PlatformName.Length > 0 && UnrealTargetPlatform.TryParse(PlatformName, out Platform))
				{
					Config.Platform = Platform;
				}
			}

			return Config;
		}

		static public UnrealTargetConfiguration GetConfigurationFromExecutableName(string InProjectName, string InName)
		{
			return GetUnrealConfigFromFileName(InProjectName, InName).Configuration;
		}

		static public UnrealTargetRole GetRoleFromExecutableName(string InProjectName, string InName)
		{
			return GetUnrealConfigFromFileName(InProjectName, InName).RoleType;
		}
		static public string GetBuildFlavorFromExecutableName(string InProjectName, string InName)
		{
			return GetUnrealConfigFromFileName(InProjectName, InName).Flavor;
		}
	}

	/// <summary>
	/// Automatically maps root drive to proper platform specific path
	/// </summary>
	public class EpicRoot
	{
		string PlatformPath;

		public EpicRoot(string Path)
		{
			PlatformPath = Path;

			if (BuildHostPlatform.Current.Platform == UnrealTargetPlatform.Mac || BuildHostPlatform.Current.Platform == UnrealTargetPlatform.Linux)
			{
				string PosixMountPath = CommandUtils.IsBuildMachine ? "/Volumes/epicgames.net/root" : "/Volumes/root";

				if (!Path.Contains("P:"))
				{
					PlatformPath = Regex.Replace(Path, @"\\\\epicgames.net\\root", PosixMountPath, RegexOptions.IgnoreCase);
				}
				else
				{
					PlatformPath = Regex.Replace(Path, "P:", PosixMountPath, RegexOptions.IgnoreCase);
				}
				
				PlatformPath = PlatformPath.Replace(@"\", "/");
			}

		}

		public static implicit operator string(EpicRoot Path)
		{
			return Path.PlatformPath;
		}
		public static implicit operator EpicRoot(string Path)
		{
			return new EpicRoot(Path);
		}

	}

	/// <summary>
	///  Converts between json and UnrealTargetPlatform
	/// </summary>
	public class UnrealTargetPlatformConvertor : JsonConverter<UnrealTargetPlatform>
	{
		public override bool CanConvert(Type ObjectType)
		{
			return ObjectType == typeof(UnrealTargetPlatform);
		}

		public override UnrealTargetPlatform Read(ref Utf8JsonReader Reader, Type TypeToConvert, JsonSerializerOptions Options)
		{
			if(Reader.TokenType == JsonTokenType.Null)
			{
				return BuildHostPlatform.Current.Platform;
			}

			UnrealTargetPlatform StructValue;
			if (UnrealTargetPlatform.TryParse(Reader.GetString(), out StructValue))
			{
				return StructValue;
			}
			throw new JsonException();
		}

		public override void Write(Utf8JsonWriter Writer, UnrealTargetPlatform StructValue, JsonSerializerOptions Options)
		{
			var Value = StructValue.ToString();
			Writer.WriteStringValue(Options.PropertyNamingPolicy?.ConvertName(Value) ?? Value);
		}
	}


}
