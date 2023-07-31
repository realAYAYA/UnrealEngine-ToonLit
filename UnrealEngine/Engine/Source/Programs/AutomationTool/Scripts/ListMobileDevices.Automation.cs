// Copyright Epic Games, Inc. All Rights Reserved.

using System.Collections.Generic;
using AutomationTool;
using UnrealBuildTool;
using EpicGames.Core;

namespace AutomationScripts.Automation
{
	public class ListMobileDevices : BuildCommand
	{
		public override void ExecuteBuild()
		{
			LogInformation("======= ListMobileDevices - Start =======");

			var GlobalParams = new ProjectParams(
				Command: this,
				RawProjectPath: new FileReference(@"D:\UE\Samples\Games\TappyChicken\TappyChicken.uproject")
				);

			if (ParseParam("android"))
			{
				GetConnectedDevices(GlobalParams, Platform.GetPlatform(UnrealTargetPlatform.Android));
			}

			if (ParseParam("ios"))
			{
				throw new AutomationException("iOS is not yet implemented.");
			}

			LogInformation("======= ListMobileDevices - Done ========");
		}

		private static void GetConnectedDevices(ProjectParams Params, Platform TargetPlatform)
		{
			var PlatformName = TargetPlatform.PlatformType.ToString();
			List<string> ConnectedDevices;
			TargetPlatform.GetConnectedDevices(Params, out ConnectedDevices);

			try
			{
				foreach (var DeviceName in ConnectedDevices)
				{
					LogInformation("Device:{0}:{1}", PlatformName, DeviceName);
				}
			}
			catch
			{
				throw new AutomationException("No {0} devices", PlatformName);
			}		
		}
	}
}

