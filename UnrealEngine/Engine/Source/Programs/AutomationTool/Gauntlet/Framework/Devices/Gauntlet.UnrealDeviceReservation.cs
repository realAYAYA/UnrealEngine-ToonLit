// Copyright Epic Games, Inc. All Rights Reserved.

using System.Collections.Generic;
using System.Linq;
using UnrealBuildTool;

namespace Gauntlet
{
	/// <summary>
	/// Device reservation utility class.
	/// </summary>
	public class UnrealDeviceReservation
	{
		protected List<ProblemDevice> ProblemDevices { get; private set; } = new List<ProblemDevice>();
		public List<ITargetDevice> ReservedDevices { get; protected set; }

		public bool TryReserveDevices(Dictionary<UnrealDeviceTargetConstraint, int> RequiredDeviceTypes, int ExpectedNumberOfDevices)
		{
			List<ITargetDevice> AcquiredDevices = new List<ITargetDevice>();
			List<ITargetDevice> SkippedDevices = new List<ITargetDevice>();

			ReleaseDevices();

			ProblemDevices.Clear();

			// check whether pool can accommodate devices
			if (!DevicePool.Instance.CheckAvailableDevices(RequiredDeviceTypes, ProblemDevices))
			{
				return false;
			}

			// nothing acquired yet...
			AcquiredDevices.Clear();

			// for each platform, enumerate and select from the available devices
			foreach (var PlatformReqKP in RequiredDeviceTypes)
			{
				UnrealDeviceTargetConstraint Constraint = PlatformReqKP.Key;
				UnrealTargetPlatform? Platform = Constraint.Platform;

				int NeedOfThisType = RequiredDeviceTypes[Constraint];

				DevicePool.Instance.EnumerateDevices(Constraint, Device =>
				{
					int HaveOfThisType = AcquiredDevices.Where(D => D.Platform == Device.Platform && Constraint.Check(Device)).Count();

					bool WeWant = NeedOfThisType > HaveOfThisType;

					if (WeWant)
					{
						bool Available = Device.IsAvailable;
						bool Have = AcquiredDevices.Contains(Device);

						bool Problem = ProblemDevices.Where(D => D.Name == Device.Name && D.Platform == Device.Platform).Count() > 0;

						Log.Verbose("Device {0}: Available:{1}, Have:{2}, HasProblem:{3}", Device.Name, Available, Have, Problem);

						if (Available
							&& Have == false
							&& Problem == false)
						{
							Log.Info("Acquiring device {0}", Device.Name);
							AcquiredDevices.Add(Device);
							HaveOfThisType++;
						}
						else
						{
							Log.Info("Skipping device {0}", Device.Name);
							SkippedDevices.Add(Device);
						}
					}

					// continue if we need more of this platform type
					return HaveOfThisType < NeedOfThisType;
				});
			}

			// If we got enough devices, go to step2 where we provision and try to connect them
			if (AcquiredDevices.Count == ExpectedNumberOfDevices)
			{
				// actually acquire them
				DevicePool.Instance.ReserveDevices(AcquiredDevices);

				Log.Info("Selected devices {0} for client(s). Prepping", string.Join(", ", AcquiredDevices));

				foreach (ITargetDevice Device in AcquiredDevices)
				{
					if (Device.IsOn == false)
					{
						Log.Info("Powering on {0}", Device);
						Device.PowerOn();
					}
					else if (Globals.Params.ParseParam("reboot"))
					{
						Log.Info("Rebooting {0}", Device);
						Device.Reboot();
					}

					if (Device.IsConnected == false)
					{
						Log.Verbose("Connecting to {0}", Device);
						Device.Connect();
					}
				}

				// Step 3: Verify we actually connected to them
				var LostDevices = AcquiredDevices.Where(D => !D.IsConnected);

				if (LostDevices.Count() > 0)
				{
					Log.Info("Lost connection to devices {0} for client(s). ", string.Join(", ", LostDevices));

					// mark these as problems. Could be something grabbed them before us, could be that they are
					// unresponsive in some way
					LostDevices.ToList().ForEach(D => MarkProblemDevice(D));
					AcquiredDevices.ToList().ForEach(D => D.Disconnect());

					DevicePool.Instance.ReleaseDevices(AcquiredDevices);
					AcquiredDevices.Clear();
				}
			}

			if (AcquiredDevices.Count() != ExpectedNumberOfDevices)
			{
				Log.Info("Failed to resolve all devices. Releasing the ones we have ");
				DevicePool.Instance.ReleaseDevices(AcquiredDevices);
				ReservedDevices = new List<ITargetDevice>();
			}
			else
			{
				ReservedDevices = AcquiredDevices;
			}

			// release devices that were skipped
			DevicePool.Instance.ReleaseDevices(SkippedDevices);

			return ReservedDevices.Count() == ExpectedNumberOfDevices;
		}

		public void ReleaseDevices()
		{
			if ((ReservedDevices != null) && (ReservedDevices.Count() > 0))
			{
				foreach (ITargetDevice Device in ReservedDevices)
				{
					IDeviceUsageReporter.RecordEnd(Device.Name, Device.Platform, IDeviceUsageReporter.EventType.Device);
				}
				DevicePool.Instance.ReleaseDevices(ReservedDevices);
				ReservedDevices.Clear();
			}
		}

		public IEnumerable<ITargetDevice> ReleaseProblemDevices()
		{
			List<ITargetDevice> Devices = new();
			bool bReservedDevices = ReservedDevices != null && ReservedDevices.Any();
			bool bProblemDevices = ProblemDevices != null && ProblemDevices.Any();

			if(bReservedDevices && bProblemDevices)
			{
				foreach(ProblemDevice Problem in ProblemDevices)
				{
					foreach(ITargetDevice Device in ReservedDevices)
					{
						if (Problem.Name == Device.Name && Problem.Platform == Device.Platform)
						{
							Devices.Add(Device);
						}
					}
				}

				DevicePool.Instance.ReleaseDevices(Devices);
				ProblemDevices.Clear();

				foreach (ITargetDevice Device in Devices)
				{
					ReservedDevices.Remove(Device);
					IDeviceUsageReporter.RecordEnd(Device.Name, Device.Platform, IDeviceUsageReporter.EventType.Device);
				}
			}
			return Devices;
		}

		public void MarkProblemDevice(ITargetDevice Device, string ErrorMessage = "MarkProblemDevice")
		{
			if (ProblemDevices.Where(D => D.Name == Device.Name && D.Platform == Device.Platform).Count() > 0)
			{
				return;
			}

			// report device has a problem to the pool
			DevicePool.Instance.ReportDeviceError(Device, ErrorMessage);

			if (Device.Platform != null)
			{
				ProblemDevices.Add(new ProblemDevice(Device.Name, Device.Platform.Value));
			}
		}
	}
}