// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.IO;
using AutomationTool;
using AutomationTool.DeviceReservation;
using UnrealBuildTool;
using System.Threading;
using System.Text.RegularExpressions;
using System.Linq;
using Newtonsoft.Json;

namespace Gauntlet
{
	public interface IPlatformFirmwareHandler
	{
		bool CanSupportPlatform(UnrealTargetPlatform platform);

		bool CanSupportProject(string projectName);

		bool GetDesiredVersion(UnrealTargetPlatform platform, string projectName, out string version);

		bool GetCurrentVersion(ITargetDevice Device,  out string version);

		bool UpdateDeviceFirmware(ITargetDevice Device, string version);
	};
};
