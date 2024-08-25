// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using EpicGames.Core;
using EpicGames.Horde.Compute;

namespace UnrealBuildTool
{
	/// <summary>
	/// Configuration for Unreal Build Accelerator Horde session
	/// </summary>
	[System.Diagnostics.CodeAnalysis.SuppressMessage("Style", "IDE1006:Naming Styles", Justification = "UnrealBuildTool naming style")]
	class UnrealBuildAcceleratorHordeConfig
	{
		/// <summary>
		/// Uri of the Horde server
		/// </summary>
		[XmlConfigFile(Category = "Horde", Name = "Server")]
		[CommandLine("-UBAHorde=")]
		public string? HordeServer { get; set; }

		/// <summary>
		/// Uri of the Horde server
		/// </summary>
		[XmlConfigFile(Category = "Horde", Name = "Token")]
		[CommandLine("-UBAHordeToken=")]
		public string? HordeToken { get; set; }

		/// <summary>
		/// OIDC id for the login to use
		/// </summary>
		[XmlConfigFile(Category = "Horde", Name = "OidcProvider")]
		[CommandLine("-UBAHordeOidc=")]
		public string? HordeOidcProvider { get; set; }

		/// <summary>
		/// Pool for the Horde agent to assign, calculated based on platform and overrides
		/// </summary>
		public string? HordePool
		{
			get
			{
				string? Pool = DefaultHordePool;
				if (OverrideHordePool != null)
				{
					Pool = OverrideHordePool;
				}
				else if (OperatingSystem.IsWindows() && WindowsHordePool != null)
				{
					Pool = WindowsHordePool;
				}
				else if (OperatingSystem.IsMacOS() && MacHordePool != null)
				{
					Pool = MacHordePool;
				}
				else if (OperatingSystem.IsLinux() && LinuxHordePool != null)
				{
					Pool = LinuxHordePool;
				}

				//Console.WriteLine("CHOSEN UBA POOL: {0}", Pool);
				return Pool;
			}
		}

		/// <summary>
		/// Pool for the Horde agent to assign if no override current platform doesn't have it set
		/// </summary>
		[XmlConfigFile(Category = "Horde", Name = "Pool")]
		public string? DefaultHordePool { get; set; }

		/// <summary>
		/// Pool for the Horde agent to assign, only used for commandline override
		/// </summary>
		[CommandLine("-UBAHordePool=")]
		public string? OverrideHordePool { get; set; }

		/// <summary>
		/// Pool for the Horde agent to assign when on Linux
		/// </summary>
		[XmlConfigFile(Category = "Horde", Name = "LinuxPool")]
		public string? LinuxHordePool { get; set; }

		/// <summary>
		/// Pool for the Horde agent to assign when on Mac
		/// </summary>
		[XmlConfigFile(Category = "Horde", Name = "MacPool")]
		public string? MacHordePool { get; set; }

		/// <summary>
		/// Pool for the Horde agent to assign when on Windows
		/// </summary>
		[XmlConfigFile(Category = "Horde", Name = "WindowsPool")]
		public string? WindowsHordePool { get; set; }

		/// <summary>
		/// Requirements for the Horde agent to assign
		/// </summary>
		[XmlConfigFile(Category = "Horde", Name = "Requirements")]
		[CommandLine("-UBAHordeRequirements=")]
		public string? HordeCondition { get; set; }

		/// <summary>
		/// Which ip UBA server should give to agents. This will invert so host listens and agents connect
		/// </summary>
		[XmlConfigFile(Category = "Horde", Name = "LocalHost")]
		[CommandLine("-UBAHordeHost")]
		public string HordeHost { get; set; } = String.Empty;

		/// <summary>
		/// Max cores allowed to be used by build session
		/// </summary>
		[XmlConfigFile(Category = "UnrealBuildAccelerator")]
		[CommandLine("-UBAHordeMaxCores")]
		public int HordeMaxCores { get; set; } = 576;

		/// <summary>
		/// How long UBT should wait to ask for help. Useful in build configs where machine can delay remote work and still get same wall time results (pch dependencies etc)
		/// </summary>
		[XmlConfigFile(Category = "UnrealBuildAccelerator")]
		[CommandLine("-UBAHordeDelay")]
		public int HordeDelay { get; set; } = 0;

		/// <summary>
		/// Allow use of Wine. Only applicable to Horde agents running Linux. Can still be ignored if Wine executable is not set on agent.
		/// </summary>
		[XmlConfigFile(Category = "UnrealBuildAccelerator")]
		[CommandLine("-UBAHordeAllowWine", Value = "true")]
		public bool bHordeAllowWine { get; set; } = true;

		/// <summary>
		/// Connection mode for agent/compute communication
		/// <see cref="ConnectionMode" /> for valid modes.
		/// </summary>
		[XmlConfigFile(Category = "Horde", Name = "ConnectionMode")]
		[CommandLine("-UBAHordeConnectionMode=")]
		public string? HordeConnectionMode { get; set; }

		/// <summary>
		/// Encryption to use for agent/compute communication. Note that UBA agent uses its own encryption.
		/// <see cref="Encryption" /> for valid modes.
		/// </summary>
		[XmlConfigFile(Category = "Horde", Name = "Encryption")]
		[CommandLine("-UBAHordeEncryption=")]
		public string? HordeEncryption { get; set; }

		/// <summary>
		/// Sentry URL to send box data to. Optional.
		/// </summary>
		[XmlConfigFile(Category = "Horde")]
		[CommandLine("-UBASentryUrl=")]
		public string? UBASentryUrl { get; set; }

		/// <summary>
		/// Disable horde all together
		/// </summary>
		[XmlConfigFile(Category = "UnrealBuildAccelerator")]
		[CommandLine("-UBADisableHorde")]
		public bool bDisableHorde { get; set; } = false;
	}
}