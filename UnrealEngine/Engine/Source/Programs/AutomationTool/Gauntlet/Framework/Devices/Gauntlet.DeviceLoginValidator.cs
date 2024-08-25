// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace Gauntlet
{
	/// <summary>
	/// DeviceLoginValidator works for any ITargetDevice that implements IOnlineServiceLogin (typically consoles)
	/// This validator ensures the device have an account logged into that platforms online service.
	/// </summary>
	public class DeviceLoginValidator : IDeviceValidator
	{
		[AutoParamWithNames(false, "VerifyLogin")]
		public bool bEnabled { get; set; }

		public DeviceLoginValidator()
		{
			AutoParam.ApplyParamsAndDefaults(this, Globals.Params.AllArguments);
		}

		public bool TryValidateDevice(ITargetDevice Device)
		{
			if(!IDeviceValidator.EnsureConnection(Device))
			{
				return false;
			}

			if (Device is IOnlineServiceLogin DeviceLogin)
			{
				Log.Info("Verifying device login...");
				if(DeviceLogin.VerifyLogin())
				{
					Log.Info("User signed-in");
					return true;
				}
				else
				{
					Log.Warning("Unable to secure login to an online platform account!");
					return false;
				}
			}
			else
			{
				Log.Verbose("{Platform} does not implement IOnlineServiceLogin, skipping login validation.", Device.Platform.Value);
				return true;
			}
		}
	}
}
