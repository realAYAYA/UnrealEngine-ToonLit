// Copyright Epic Games, Inc. All Rights Reserved.

using System.Collections.Generic;
using System.Threading.Tasks;
using Horde.Build.Devices;
using Horde.Build.Jobs;
using Horde.Build.Server;
using Microsoft.AspNetCore.Mvc;
using Microsoft.VisualStudio.TestTools.UnitTesting;
using Horde.Build.Projects;
using Horde.Build.Utilities;
using Horde.Build.Users;
using System.Security.Claims;
using Microsoft.AspNetCore.Http;

namespace Horde.Build.Tests
{

	using ProjectId = StringId<IProject>;
	using DeviceId = StringId<IDevice>;

	/// <summary>
	/// Tests for the device service
	/// </summary>
	[TestClass]
	public class DeviceServiceTest : TestSetup
	{
		private DevicesController? _deviceController;

		// override DeviceController with valid user
		private DevicesController DeviceController
		{
			get
			{
				if (_deviceController == null)
				{
					IUser user = UserCollection.FindOrAddUserByLoginAsync("TestUser").Result;
					_deviceController = base.DevicesController;
					ControllerContext controllerContext = new ControllerContext();
					controllerContext.HttpContext = new DefaultHttpContext();
					controllerContext.HttpContext.User = new ClaimsPrincipal(new ClaimsIdentity(
						new List<Claim> {new Claim(ServerSettings.AdminClaimType, ServerSettings.AdminClaimValue),
					new Claim(ClaimTypes.Name, "TestUser"),
					new Claim(HordeClaimTypes.UserId, user.Id.ToString()) }
						, "TestAuthType"));
					_deviceController.ControllerContext = controllerContext;

				}
				return _deviceController;
			}
		}


		private async Task PopulateDevices()
		{

			await ProjectCollection.AddOrUpdateAsync(new ProjectId("ue5"), "", "", 0, new ProjectConfig { Name = "UE5" });

			// create 2 pools
			for (int i = 1; i < 3; i++)
			{
				await DeviceController.CreatePoolAsync(new CreateDevicePoolRequest() { Name = "TestDevicePool" + i, PoolType = DevicePoolType.Automation, ProjectIds = new List<string>() { "ue5" } });
			}

			Dictionary<string, string> platformMap = new Dictionary<string, string>();
			Dictionary<string, string> platformReverseMap = new Dictionary<string, string>();

			// create 3 platforms
			for (int i = 1; i < 4; i++)
			{
				string platformName = "TestDevicePlatform" + i;
				await DevicesController.CreatePlatformAsync(new CreateDevicePlatformRequest() { Name = platformName });
				platformMap[platformName] = platformName;
				platformReverseMap[platformName] = platformName;
			}

			// add 4 devices to each platform, split between 2 pools
			for (int i = 1; i < 4; i++)
			{
				for (int j = 1; j < 5; j++)
				{
					// @todo: add model tests
					// one base model, and 3 other models 
					/*
					string? ModelId = null;
					if (j > 1)
					{
						ModelId = "TestDevicePlatform" + i + "_Model" + j;
					}
					*/

					string poolId = (j & 1) != 0 ? "testdevicepool1" : "testdevicepool2";

					await DeviceController.CreateDeviceAsync(new CreateDeviceRequest() { Name = "TestDevice" + j + "_Platform" + i + "_" + poolId, Address = "10.0.0.1", Enabled = true, PlatformId = "testdeviceplatform" + i/*, ModelId = ModelId*/, PoolId = poolId });
				}
			}

			// populate the legacy platform map, this exists to avoid having platform names in server code, though needs to be cleaned up.
			// Gauntlet in various streams can't/hasn't been updated and asks for non-platform-id device types
			await DeviceController.UpdatePlatformMapV1(new UpdatePlatformMapRequest() { PlatformMap = platformMap, PlatformReverseMap = platformReverseMap, PerfSpecHighMap = new Dictionary<string, string>() });

		}

		static T ResultToValue<T>(ActionResult<T> result) where T: class
		{
			return ((result.Result! as JsonResult)!.Value! as T)!;
		}
	
		async Task<LegacyCreateReservationRequest> SetupReservationTest()
		{
			await CreateFixtureAsync();
			await PopulateDevices();

			ActionResult<List<object>> res = await JobsController.FindJobsAsync();
			Assert.AreEqual(2, res.Value!.Count);
			GetJobResponse job = (res.Value[0] as GetJobResponse)!;

			// Gauntlet uses the legacy v1 API
			LegacyCreateReservationRequest request = new LegacyCreateReservationRequest();
			request.PoolId = "TestDevicePool1";
			request.DeviceTypes = new string[] { "TestDevicePlatform1" };
			request.Hostname = "localhost";
			request.Duration = "00:10:00";
			request.JobId = job.Id;
			request.StepId = "abcd";

			return request;
		}


		[TestMethod]
		public async Task TestReservation()
		{
			LegacyCreateReservationRequest request = await SetupReservationTest();
			
			// create a reservation
			GetLegacyReservationResponse reservation = ResultToValue(await DeviceController!.CreateDeviceReservationV1Async(request));			
			Assert.AreEqual(1, reservation.DeviceNames.Length);
			Assert.AreEqual("hello2", reservation.JobName);
			Assert.AreEqual("abcd", reservation.StepId);

			// get the device in the reservation, and make sure it is the right platform
			GetLegacyDeviceResponse device = ResultToValue(await DeviceController!.GetDeviceV1Async(reservation.DeviceNames[0]));
			Assert.AreEqual("TestDevicePlatform1", device.Type);

			// update the reservation
			GetLegacyReservationResponse renewed = ResultToValue(await DeviceController!.UpdateReservationV1Async(reservation.Guid));
			Assert.AreEqual(renewed.Guid, reservation.Guid);

			// delete the reservation, would be nice to also test reservation expiration
			OkResult? deleted = (await DeviceController!.DeleteReservationV1Async(reservation.Guid)) as OkResult;
			Assert.IsNotNull(deleted);

			// check that telemetry was created
			List<GetDeviceTelemetryResponse> telemetry = (await DeviceController!.GetDeviceTelemetry()).Value!;
			Assert.AreEqual(telemetry.Count, 1);
			Assert.AreEqual(telemetry[0].Telemetry.Count, 1);
			Assert.AreEqual(telemetry[0].Telemetry[0].StreamId, "ue5-main");
			Assert.AreEqual(telemetry[0].Telemetry[0].StepId, "abcd");
			Assert.AreEqual(telemetry[0].Telemetry[0].JobName, "hello2");
		}

		[TestMethod]
		public async Task TestProblemDevice()
		{
			LegacyCreateReservationRequest request = await SetupReservationTest();

			// create a reservation
			GetLegacyReservationResponse reservation = ResultToValue(await DeviceController!.CreateDeviceReservationV1Async(request));
			Assert.AreEqual(1, reservation.DeviceNames.Length);

			// report a problem with the device 
			OkResult? problem = (await DeviceController!.PutDeviceErrorAsync(reservation.DeviceNames[0])) as OkResult;
			Assert.IsNotNull(problem);

			OkResult? deleted = (await DeviceController!.DeleteReservationV1Async(reservation.Guid)) as OkResult;
			Assert.IsNotNull(deleted);

			// check that telemetry was created
			List<GetDeviceTelemetryResponse> telemetry = (await DeviceController!.GetDeviceTelemetry()).Value!;
			Assert.AreEqual(telemetry.Count, 1);
			// check that problem was recorded
			Assert.IsNotNull(telemetry[0].Telemetry[0].ProblemTimeUtc);
			// make sure the reswrvation time was finished for the problem device
			Assert.IsNotNull(telemetry[0].Telemetry[0].ReservationFinishUtc);
		}

		[TestMethod]
		public async Task TestDevicePoolTelemetryCapture()
		{
			LegacyCreateReservationRequest reservationRequest = await SetupReservationTest();

			// set some device status
			UpdateDeviceRequest request = new UpdateDeviceRequest();

			request.Maintenance = true;
			await DeviceController!.UpdateDeviceAsync("testdevice1_platform1_testdevicepool1", request);

			request.Maintenance = null;
			request.Enabled = false;
			await DeviceController!.UpdateDeviceAsync("testdevice3_platform3_testdevicepool1", request);

			request.Enabled = null;
			request.Problem = true;
			await DeviceController!.UpdateDeviceAsync("testdevice4_platform2_testdevicepool2", request);


			IDevice? maintenanceDevice = await DeviceService.GetDeviceAsync(new DeviceId("testdevice1_platform1_testdevicepool1"));
			Assert.IsNotNull(maintenanceDevice);
			
			GetLegacyReservationResponse reservation = ResultToValue(await DeviceController!.CreateDeviceReservationV1Async(reservationRequest));
			Assert.AreEqual(1, reservation.DeviceNames.Length);
			Assert.AreNotSame(maintenanceDevice!.Name, reservation.DeviceNames[0]);

			GetLegacyDeviceResponse reservedDevice = ResultToValue(await DeviceController!.GetDeviceV1Async(reservation.DeviceNames[0]));

			await DeviceService.TickForTestingAsync();

			List<GetDevicePoolTelemetryResponse> telemetry = (await DeviceController!.GetDevicePoolTelemetry()).Value!;
			Assert.AreEqual(1, telemetry.Count);
			foreach (GetDevicePlatformTelemetryResponse t in telemetry[0].Telemetry["testdevicepool1"])
			{
				if (t.PlatformId == "testdeviceplatform1")
				{
					Assert.IsNull(t.Available);
					Assert.AreEqual(1, t.Reserved?.Count);
					Assert.AreEqual(1, t.Maintenance?.Count);

					Assert.AreEqual(true, t.Reserved?.ContainsKey("ue5-main"));
					Assert.AreEqual(t.Reserved?["ue5-main"].Count, 1);
					Assert.AreEqual(t.Reserved?["ue5-main"][0].DeviceId, reservedDevice.Id);
					Assert.AreEqual(t.Reserved?["ue5-main"][0].JobName, "hello2");
					Assert.AreEqual(t.Reserved?["ue5-main"][0].StepId, "abcd");
				}
				else if (t.PlatformId == "testdeviceplatform3")
				{
					Assert.AreEqual(1, t.Available?.Count);
					Assert.AreEqual(1, t.Disabled?.Count);
				}
				else
				{
					Assert.AreEqual(2, t.Available?.Count);
				}
			}

			foreach (GetDevicePlatformTelemetryResponse t in telemetry[0].Telemetry["testdevicepool2"])
			{
				if (t.PlatformId == "testdeviceplatform2")
				{
					Assert.AreEqual(1, t.Available?.Count);
					Assert.AreEqual(1, t.Problem?.Count);
				}
				else
				{
					Assert.AreEqual(2, t.Available?.Count);
				}
			}
		}
	}
}
