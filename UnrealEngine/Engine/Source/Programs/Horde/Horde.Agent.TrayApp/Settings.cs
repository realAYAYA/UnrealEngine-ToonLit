// Copyright Epic Games, Inc. All Rights Reserved.

#pragma warning disable CS1591

namespace Horde.Agent.TrayApp
{
	[Serializable]
	public class Settings
	{
		public IdleSettings Idle { get; set; } = new();
	}

	[Serializable]
	public class IdleSettings
	{
		public int MinIdleTimeSecs { get; set; } = 2;
		public int MinIdleCpuPct { get; set; } = 70;
		public int MinFreeVirtualMemMb { get; set; } = 256;

		public string[] CriticalProcesses { get; set; } = Array.Empty<string>();
	}
}
