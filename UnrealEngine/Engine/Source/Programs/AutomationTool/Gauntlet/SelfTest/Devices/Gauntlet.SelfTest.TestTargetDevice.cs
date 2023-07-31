// Copyright Epic Games, Inc. All Rights Reserved.

using Gauntlet;
using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using UnrealBuildTool;

namespace Gauntlet.SelfTest
{
	abstract class TestTargetDevice : BaseTestNode
	{
		protected bool CheckEssentialFunctions(ITargetDevice TestDevice)
		{
			// Device should power on (or ignore this if it's already on);
			CheckResult(TestDevice.PowerOn() && TestDevice.IsOn, "Failed to power on device");

			// Device should reboot (or pretend it did)
			CheckResult(TestDevice.Reboot(), "Failed to reboot device");
		
			// Device should connect
			CheckResult(TestDevice.Connect() && TestDevice.IsConnected, "Failed to connect to device");

			return TestFailures.Count == 0;
		}
	}
}
