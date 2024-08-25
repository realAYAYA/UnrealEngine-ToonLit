// Copyright Epic Games, Inc. All Rights Reserved.

namespace Gauntlet
{
	/// <summary>
	/// IDeviceValidator provides the ability to verify a TargetDevice matches a set of requirements.
	/// They are particularily useful for ensuring development kits are in a desired state prior to a test.
	/// This can include, but is not limited to:
	///		- Proper firmware versions
	///		- Having an account signed in
	///		- Pushing a list of settings onto the device
	///	To add new DeviceValidators, simply create a concrete type that inherits from IDeviceValidator.
	/// Implement ValidateDevice() and determine whether this validator should be enabled within the constructor
	/// It can be especially helpful to toggle whether a validator is enabled via commandline arguments, for ease of opt-in
	/// </summary>
	public interface IDeviceValidator
	{
		/// <summary>
		/// If false, this validator will not be used when reserving devices for automated tests
		/// </summary>
		bool bEnabled { get; }

		/// <summary>
		/// Verifies if the device is valid in the context of the running program
		/// </summary>
		/// <param name="Device">The device to validate</param>
		/// <returns>True if the device succeeded validation</returns>
		bool TryValidateDevice(ITargetDevice Device);

		/// <summary>
		/// Helper function that can be used to ensure a device is powered on/connected
		/// before attempting to run any validation checks
		/// </summary>
		/// <param name="Device"></param>
		/// <returns></returns>
		protected static bool EnsureConnection(ITargetDevice Device)
		{
			if (Device == null)
			{
				Log.Warning("Cannot ensure connection for null device");
				return false;
			}

			if (!Device.IsOn)
			{
				if (!Device.PowerOn())
				{
					Log.Warning("Failed to power on device");
					return false;
				}
				else
				{
					Log.VeryVerbose("Powered on device while ensuring connection");
				}
			}

			if (!Device.IsConnected)
			{
				if (!Device.Connect())
				{
					Log.Warning("Failed to connect to device");
					return false;
				}
				else
				{
					Log.VeryVerbose("Connected to device while ensuring connection");
				}
			}

			return Device.IsAvailable;
		}
	}
}
