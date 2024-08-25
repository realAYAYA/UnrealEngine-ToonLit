// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Linq;
using System.Security.Claims;
using System.Threading.Tasks;
using EpicGames.Core;
using EpicGames.Horde.Devices;
using EpicGames.Horde.Jobs;
using EpicGames.Horde.Jobs.Templates;
using EpicGames.Horde.Projects;
using EpicGames.Horde.Streams;
using Horde.Server.Devices;
using Horde.Server.Jobs;
using Horde.Server.Jobs.Graphs;
using Horde.Server.Jobs.Templates;
using Horde.Server.Server;
using Horde.Server.Users;
using Horde.Server.Utilities;
using HordeCommon;
using Microsoft.AspNetCore.Http;
using Microsoft.AspNetCore.Mvc;
using Microsoft.VisualStudio.TestTools.UnitTesting;
using Moq;

namespace Horde.Server.Tests.Devices
{
	/// <summary>
	/// Tests for the device service
	/// </summary>
	[TestClass]
	public class DeviceServiceTest : TestSetup
	{
		private DevicesController? _deviceController;

		IGraph? _graph = null;

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
						new List<Claim>
						{
							HordeClaims.AdminClaim.ToClaim(),
							new Claim(ClaimTypes.Name, "TestUser"),
							new Claim(HordeClaimTypes.UserId, user.Id.ToString())
						}
						, "TestAuthType"));
					_deviceController.ControllerContext = controllerContext;

				}
				return _deviceController;
			}
		}

		static T ResultToValue<T>(ActionResult<T> result) where T : class
		{
			return ((result.Result! as JsonResult)!.Value! as T)!;
		}

		static NewGroup AddGroup(List<NewGroup> groups)
		{
			NewGroup group = new NewGroup("win64", new List<NewNode>());
			groups.Add(group);
			return group;
		}

		static NewNode AddNode(NewGroup group, string name, string[]? inputDependencies, Action<NewNode>? action = null, IReadOnlyNodeAnnotations? annotations = null)
		{
			NewNode node = new NewNode(name, inputDependencies: inputDependencies?.ToList(), orderDependencies: inputDependencies?.ToList(), annotations: annotations);
			action?.Invoke(node);
			group.Nodes.Add(node);
			return node;
		}

		async Task<IJob> StartBatchAsync(IJob job, IGraph graph, int batchIdx)
		{
			Assert.AreEqual(JobStepBatchState.Ready, job.Batches[batchIdx].State);
			job = Deref(await JobCollection.TryUpdateBatchAsync(job, graph, job.Batches[batchIdx].Id, null, JobStepBatchState.Running, null));
			Assert.AreEqual(JobStepBatchState.Running, job.Batches[batchIdx].State);
			return job;
		}

		async Task<IJob> StartStepAsync(IJob job, IGraph graph, int batchIdx, int stepIdx)
		{
			Assert.AreEqual(JobStepState.Ready, job.Batches[batchIdx].Steps[stepIdx].State);
			job = Deref(await JobCollection.TryUpdateStepAsync(job, graph, job.Batches[batchIdx].Id, job.Batches[batchIdx].Steps[stepIdx].Id, JobStepState.Running, JobStepOutcome.Success));
			return job;
		}

		async Task<IJob> FinishStepAsync(IJob job, IGraph graph, int batchIdx, int stepIdx, JobStepOutcome outcome)
		{
			job = Deref(await JobCollection.TryUpdateStepAsync(job, graph, job.Batches[batchIdx].Id, job.Batches[batchIdx].Steps[stepIdx].Id, JobStepState.Completed, outcome));
			Assert.AreEqual(JobStepState.Completed, job.Batches[batchIdx].Steps[stepIdx].State);
			Assert.AreEqual(outcome, job.Batches[batchIdx].Steps[stepIdx].Outcome);
			return job;
		}

		async Task<IJob> RunStepAsync(IJob job, IGraph graph, int batchIdx, int stepIdx, JobStepOutcome outcome)
		{
			job = Deref(await StartStepAsync(job, graph, batchIdx, stepIdx));
			return Deref(await FinishStepAsync(job, graph, batchIdx, stepIdx, outcome));
		}

		JobStepId GetStepId(IJob job, string nodeName)
		{
			NodeRef? installNode;
			_graph!.TryFindNode(nodeName, out installNode);
			Assert.IsNotNull(installNode);

			IJobStep? step;
			job.TryGetStepForNode(installNode, out step);
			Assert.IsNotNull(step);

			return step.Id;
		}

		async Task<bool> SetupDevicesAsync()
		{

			await CreateFixtureAsync();

			DeviceConfig devices = new DeviceConfig();

			// create 2 pools
			for (int i = 1; i < 3; i++)
			{
				devices.Pools.Add(new DevicePoolConfig() { Id = new DevicePoolId("TestDevicePool" + i), Name = "TestDevicePool" + i, PoolType = DevicePoolType.Automation, ProjectIds = new List<ProjectId>() { new ProjectId("ue5") } });
			}

			// create 3 platforms
			for (int i = 1; i < 4; i++)
			{
				string platformName = "TestDevicePlatform" + i;

				List<string> modelIds = new List<string>();
				for (int j = 2; j < 5; j++)
				{
					modelIds.Add(platformName + "_Model" + j);
				}

				DevicePlatformConfig platform = new DevicePlatformConfig() { Id = new DevicePlatformId(platformName), Name = platformName, Models = modelIds };

				// platform 3 has some name aliases
				if (i == 3)
				{
					platform.LegacyNames = new List<string>() { "TestDevicePlatform3Alias" };
					platform.LegacyPerfSpecHighModel = "TestDevicePlatform3_Model4";
				}

				devices.Platforms.Add(platform);
			}

			UpdateConfig(x => x.Devices = devices);

			for (int i = 1; i < 4; i++)
			{
				for (int j = 0; j < 4; j++)
				{
					for (int k = 1; k < 5; k++)
					{
						// one base model, and 3 other models 
						string? modelId = null;
						if (k > 1)
						{
							modelId = "TestDevicePlatform" + i + "_Model" + k;
						}

						string poolId = (k & 1) != 0 ? "testdevicepool1" : "testdevicepool2";

						await DeviceController.CreateDeviceAsync(new CreateDeviceRequest() { Name = "TestDevice" + (j * 5 + k) + "_Platform" + i + "_" + poolId, Address = "10.0.0.1", Enabled = true, PlatformId = "testdeviceplatform" + i, ModelId = modelId, PoolId = poolId });
					}
				}
			}

			await DeviceService.TickForTestingAsync();

			return true;
		}

		async Task<IJob> SetupJobAsync(string? annotationsIn = null)
		{
			Mock<ITemplate> templateMock = new Mock<ITemplate>(MockBehavior.Strict);
			templateMock.SetupGet(x => x.InitialAgentType).Returns((string?)null);

			IGraph baseGraph = await GraphCollection.AddAsync(templateMock.Object, null);

			CreateJobOptions options = new CreateJobOptions();
			options.Arguments.Add("-Target=Run Tests");

			IJob job = await JobCollection.AddAsync(JobIdUtils.GenerateNewId(), new StreamId("ue5-main"), new TemplateId("test-build"), ContentHash.SHA1("hello"), baseGraph, "Test job", 123, 123, options);

			job = await StartBatchAsync(job, baseGraph, 0);
			job = await RunStepAsync(job, baseGraph, 0, 0, JobStepOutcome.Success); // Setup Build

			List<NewGroup> newGroups = new List<NewGroup>();

			NewGroup initialGroup = AddGroup(newGroups);
			AddNode(initialGroup, "Update Version Files", null);
			AddNode(initialGroup, "Compile Editor", new[] { "Update Version Files" });

			NewGroup compileGroup = AddGroup(newGroups);
			AddNode(compileGroup, "Compile Client", new[] { "Update Version Files" });

			NewGroup cookGroup = AddGroup(newGroups);
			AddNode(cookGroup, "Cook Client", new[] { "Compile Editor" });

			NewGroup testGroup = AddGroup(newGroups);
			NodeAnnotations? annotations = String.IsNullOrEmpty(annotationsIn) ? null : new NodeAnnotations();
			if (annotationsIn == "DeviceReserveNodes")
			{
				annotations!.Add("DeviceReserveNodes", "Run Test 1,Run Test 2,Run Test 3,Run Test 4");
			}
			else if (annotationsIn == "DeviceReserve")
			{
				annotations!.Add("DeviceReserve", "Begin");
			}

			AddNode(testGroup, "Install Build", new[] { "Cook Client", "Compile Client" }, annotations: annotations);
			AddNode(testGroup, "Run Test 1", new[] { "Install Build" });
			AddNode(testGroup, "Run Test 2", new[] { "Install Build" });

			annotations = annotationsIn == "DeviceReserve" ? new NodeAnnotations() : null;
			if (annotations != null)
			{
				annotations!.Add("DeviceReserve", "End");
			}

			AddNode(testGroup, "Run Test 3", new[] { "Install Build" }, annotations: annotations);
			AddNode(testGroup, "Run Test 4", new[] { "Install Build" });

			AddNode(testGroup, "Run Tests", new[] { "Run Test 1", "Run Test 2", "Run Test 3", "Run Test 4" });

			_graph = await GraphCollection.AppendAsync(baseGraph, newGroups, null, null);
			job = Deref(await JobCollection.TryUpdateGraphAsync(job, baseGraph, _graph));

			return job;
		}

		static LegacyCreateReservationRequest SetupReservationTestAsync(IJob job, string poolId = "TestDevicePool1", string deviceType = "TestDevicePlatform1", JobStepId? stepId = null, string? modelId = null)
		{
			if (stepId == null)
			{
				stepId = JobStepId.Parse("abcd");
			}

			if (modelId != null)
			{
				deviceType += $":{modelId}";
			}

			// Gauntlet uses the legacy v1 API
			LegacyCreateReservationRequest request = new LegacyCreateReservationRequest();
			request.PoolId = poolId;
			request.DeviceTypes = new string[] { deviceType };
			request.Hostname = "localhost";
			request.Duration = "00:10:00";
			request.JobId = job.Id.ToString();
			request.StepId = stepId.ToString();

			return request;
		}

		[TestMethod]
		public async Task TestReservationAsync()
		{
			await SetupDevicesAsync();

			IJob job = await SetupJobAsync();
			LegacyCreateReservationRequest request = SetupReservationTestAsync(job, modelId: "Base");

			// create a reservation
			GetLegacyReservationResponse reservation = ResultToValue(await DeviceController!.CreateDeviceReservationV1Async(request));
			Assert.AreEqual(1, reservation.DeviceNames.Length);
			Assert.AreEqual(1, reservation.DeviceModels.Length);
			Assert.AreEqual("Base", reservation.DeviceModels[0]);
			Assert.AreEqual("Test job", reservation.JobName);
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
			List<GetDeviceTelemetryResponse> telemetry = (await DeviceController!.GetDeviceTelemetryAsync()).Value!;
			Assert.AreEqual(telemetry.Count, 1);
			Assert.AreEqual(telemetry[0].Telemetry.Count, 1);
			Assert.AreEqual(telemetry[0].Telemetry[0].StreamId, "ue5-main");
			Assert.AreEqual(telemetry[0].Telemetry[0].StepId, "abcd");
			Assert.AreEqual(telemetry[0].Telemetry[0].JobName, "Test job");
		}

		[TestMethod]
		public async Task TestReservationNodesAsync()
		{
			await SetupDevicesAsync();
			IJob job = await SetupJobAsync("DeviceReserveNodes");

			IGraph? graph = _graph!;

			job = await StartBatchAsync(job, graph, 1);
			job = await RunStepAsync(job, graph, 1, 0, JobStepOutcome.Success); // Update Version Files
			job = await RunStepAsync(job, graph, 1, 1, JobStepOutcome.Success); // Compile Editor

			job = await StartBatchAsync(job, graph, 2);
			job = await RunStepAsync(job, graph, 2, 0, JobStepOutcome.Success); // Compile Client

			job = await StartBatchAsync(job, graph, 3);
			job = await RunStepAsync(job, graph, 3, 0, JobStepOutcome.Success); // Cook Client

			job = await StartBatchAsync(job, graph, 4);

			// Install  the build
			JobStepId stepId = GetStepId(job, "Install Build");
			job = await StartStepAsync(job, graph, 4, 0); // Install Build														  
			LegacyCreateReservationRequest request = SetupReservationTestAsync(job, stepId: stepId);
			GetLegacyReservationResponse installReservation = ResultToValue(await DeviceController!.CreateDeviceReservationV1Async(request));
			job = await FinishStepAsync(job, graph, 4, 0, JobStepOutcome.Success); // Install Build

			for (int i = 1; i < 5; i++)
			{
				// Run Test 1
				stepId = GetStepId(job, $"Run Test {i}");
				job = await StartStepAsync(job, graph, 4, i);

				request = SetupReservationTestAsync(job, stepId: stepId);

				ActionResult<GetLegacyReservationResponse> result = await DeviceController!.CreateDeviceReservationV1Async(request);

				GetLegacyReservationResponse? reservation;

				if (i == 4)
				{
					// check parallel error conflict
					Assert.AreEqual((result.Result as ConflictObjectResult)!.StatusCode, 409);
					Assert.AreEqual((result.Result as ConflictObjectResult)!.Value, "Reserved nodes must not run in parallel: Run Test 3,Run Test 4");

					// finish the step
					job = await FinishStepAsync(job, graph, 4, 3, JobStepOutcome.Success);

					result = await DeviceController!.CreateDeviceReservationV1Async(request);
					reservation = ResultToValue(result);
				}
				else
				{
					reservation = ResultToValue(result);
				}

				Assert.IsNotNull(reservation);
				Assert.AreEqual(installReservation.Guid, reservation.Guid);

				// Do not finish test 3, to test parallel step error
				if (i != 3)
				{
					await DeviceController!.DeleteReservationV1Async(reservation.Guid);
					job = await FinishStepAsync(job, graph, 4, i, JobStepOutcome.Success);
				}

				await DeviceService.TickForTestingAsync();
			}

			List<IDeviceReservation> reservations = await DeviceService.GetReservationsAsync();
			Assert.AreEqual(reservations.Count, 0);

			// check that telemetry was created
			List<GetDeviceTelemetryResponse> telemetry = (await DeviceController!.GetDeviceTelemetryAsync()).Value!;
			Assert.AreEqual(telemetry.Count, 1);
			Assert.AreEqual(telemetry[0].Telemetry.Count, 1);
			Assert.AreEqual(telemetry[0].Telemetry[0].StreamId, "ue5-main");
			Assert.AreEqual(telemetry[0].Telemetry[0].StepId, GetStepId(job, "Install Build").ToString());
			Assert.AreEqual(telemetry[0].Telemetry[0].StepName, "Install Build");
			Assert.AreEqual(telemetry[0].Telemetry[0].JobName, "Test job");
		}

		[TestMethod]
		public async Task TestReservationMarkersAsync()
		{
			await SetupDevicesAsync();
			IJob job = await SetupJobAsync("DeviceReserve");

			IGraph? graph = _graph!;

			job = await StartBatchAsync(job, graph, 1);
			job = await RunStepAsync(job, graph, 1, 0, JobStepOutcome.Success); // Update Version Files
			job = await RunStepAsync(job, graph, 1, 1, JobStepOutcome.Success); // Compile Editor

			job = await StartBatchAsync(job, graph, 2);
			job = await RunStepAsync(job, graph, 2, 0, JobStepOutcome.Success); // Compile Client

			job = await StartBatchAsync(job, graph, 3);
			job = await RunStepAsync(job, graph, 3, 0, JobStepOutcome.Success); // Cook Client

			job = await StartBatchAsync(job, graph, 4);

			// Install  the build
			JobStepId stepId = GetStepId(job, "Install Build");
			job = await StartStepAsync(job, graph, 4, 0); // Install Build														  
			LegacyCreateReservationRequest request = SetupReservationTestAsync(job, stepId: stepId, modelId: "Base");
			GetLegacyReservationResponse installReservation = ResultToValue(await DeviceController!.CreateDeviceReservationV1Async(request));
			Assert.IsTrue(installReservation.InstallRequired);

			string installProblemDeviceName = installReservation!.DeviceNames[0];

			await DeviceController!.PutDeviceErrorAsync(installProblemDeviceName);
			installReservation = ResultToValue(await DeviceController!.CreateDeviceReservationV1Async(request));
			Assert.IsTrue(installReservation.InstallRequired);
			Assert.AreNotEqual(installProblemDeviceName, installReservation.DeviceNames[0]);

			job = await FinishStepAsync(job, graph, 4, 0, JobStepOutcome.Success); // Install Build

			for (int i = 1; i < 5; i++)
			{
				// Run Test 1
				stepId = GetStepId(job, $"Run Test {i}");
				job = await StartStepAsync(job, graph, 4, i);

				if (i == 2)
				{
					request = SetupReservationTestAsync(job, stepId: stepId, modelId: "TestDevicePlatform1_Model3");
				}
				else
				{
					request = SetupReservationTestAsync(job, stepId: stepId, modelId: "Base");
				}

				ActionResult<GetLegacyReservationResponse> result = await DeviceController!.CreateDeviceReservationV1Async(request);

				GetLegacyReservationResponse? reservation = ResultToValue(result);

				Assert.IsNotNull(reservation);

				if (i == 1)
				{
					Assert.IsFalse(reservation.InstallRequired);
				}

				if (i == 2)
				{
					Assert.AreNotEqual(reservation.DeviceNames[0], installReservation.DeviceNames[0]);
					Assert.AreEqual(reservation.DeviceModels[0], "TestDevicePlatform1_Model3");
					Assert.IsTrue(reservation.InstallRequired);
				}

				if (i == 3)
				{
					string problemDeviceName = reservation.DeviceNames[0];
					await DeviceController!.PutDeviceErrorAsync(problemDeviceName);
					result = await DeviceController!.CreateDeviceReservationV1Async(request);
					reservation = ResultToValue(result);
					Assert.IsNotNull(reservation);
					Assert.IsTrue(reservation.InstallRequired);
					Assert.AreNotEqual(problemDeviceName, reservation.DeviceNames[0]);
					Assert.AreEqual(reservation.DeviceModels[0], "Base");
				}

				if (i != 4)
				{
					Assert.AreEqual(installReservation.Guid, reservation.Guid);
				}
				else
				{
					Assert.IsNull(reservation.InstallRequired);
					Assert.AreNotEqual(installReservation.Guid, reservation.Guid);
				}

				await DeviceController!.DeleteReservationV1Async(reservation.Guid);
				job = await FinishStepAsync(job, graph, 4, i, JobStepOutcome.Success);

				await DeviceService.TickForTestingAsync();
			}

			List<IDeviceReservation> reservations = await DeviceService.GetReservationsAsync();
			Assert.AreEqual(reservations.Count, 0);

			// check that telemetry was created
			List<GetDeviceTelemetryResponse> telemetry = (await DeviceController!.GetDeviceTelemetryAsync()).Value!;
			Assert.AreEqual(2, telemetry.Count, 5);
			Assert.AreEqual(telemetry[0].Telemetry.Count, 1);
			Assert.AreEqual(telemetry[0].Telemetry[0].StreamId, "ue5-main");
			Assert.AreEqual(telemetry[0].Telemetry[0].StepId, GetStepId(job, "Install Build").ToString());
			Assert.AreEqual(telemetry[0].Telemetry[0].StepName, "Install Build");
			Assert.AreEqual(telemetry[0].Telemetry[0].JobName, "Test job");
		}

		[TestMethod]
		public async Task TestReservationPerfSpecWithAliasAsync()
		{
			await SetupDevicesAsync();

			IJob job = await SetupJobAsync();
			LegacyCreateReservationRequest request = SetupReservationTestAsync(job, "TestDevicePool2", "TestDevicePlatform3Alias:High");

			// create a reservation
			GetLegacyReservationResponse reservation = ResultToValue(await DeviceController!.CreateDeviceReservationV1Async(request));
			Assert.AreEqual(1, reservation.DeviceNames.Length);
			Assert.AreEqual("Test job", reservation.JobName);
			Assert.AreEqual("abcd", reservation.StepId);

			// get the device in the reservation, and make sure it is the right platform
			GetLegacyDeviceResponse device = ResultToValue(await DeviceController!.GetDeviceV1Async(reservation.DeviceNames[0]));
			Assert.AreEqual("TestDevicePlatform3Alias", device.Type);
			Assert.AreEqual("TestDevicePlatform3_Model4", device.Model);

			// update the reservation
			GetLegacyReservationResponse renewed = ResultToValue(await DeviceController!.UpdateReservationV1Async(reservation.Guid));
			Assert.AreEqual(renewed.Guid, reservation.Guid);

			// delete the reservation, would be nice to also test reservation expiration
			OkResult? deleted = (await DeviceController!.DeleteReservationV1Async(reservation.Guid)) as OkResult;
			Assert.IsNotNull(deleted);

			// check that telemetry was created
			List<GetDeviceTelemetryResponse> telemetry = (await DeviceController!.GetDeviceTelemetryAsync()).Value!;
			Assert.AreEqual(telemetry.Count, 1);
			Assert.AreEqual(telemetry[0].Telemetry.Count, 1);
			Assert.AreEqual(telemetry[0].Telemetry[0].StreamId, "ue5-main");
			Assert.AreEqual(telemetry[0].Telemetry[0].StepId, "abcd");
			Assert.AreEqual(telemetry[0].Telemetry[0].JobName, "Test job");
		}

		[TestMethod]
		public async Task TestReservationPerfModelAsync()
		{
			await SetupDevicesAsync();

			List<string> deviceModels = new List<string>() { "TestDevicePlatform1:TestDevicePlatform1_Model2", "TestDevicePlatform1_Model3" };

			IJob job = await SetupJobAsync();
			LegacyCreateReservationRequest request = SetupReservationTestAsync(job, "TestDevicePool1", String.Join(';', deviceModels));

			// update device model
			UpdateDeviceRequest updateRequest = new UpdateDeviceRequest() { ModelId = "TestDevicePlatform1_Model2" };
			await DeviceController!.UpdateDeviceAsync("testdevice1_platform1_testdevicepool1", updateRequest);

			// create a reservation
			GetLegacyReservationResponse reservation = ResultToValue(await DeviceController!.CreateDeviceReservationV1Async(request));
			Assert.AreEqual(1, reservation.DeviceNames.Length);
			Assert.AreEqual("Test job", reservation.JobName);
			Assert.AreEqual("abcd", reservation.StepId);

			// get the device in the reservation, and make sure it is the right platform and an acceptable model
			GetLegacyDeviceResponse device = ResultToValue(await DeviceController!.GetDeviceV1Async(reservation.DeviceNames[0]));
			Assert.AreEqual("TestDevicePlatform1", device.Type);

			string? firstModel = device.Model;
			Assert.IsTrue(firstModel == "TestDevicePlatform1_Model2" || firstModel == "TestDevicePlatform1_Model3");

			// get a 2nd reservation
			reservation = ResultToValue(await DeviceController!.CreateDeviceReservationV1Async(request));
			Assert.AreEqual(1, reservation.DeviceNames.Length);

			device = ResultToValue(await DeviceController!.GetDeviceV1Async(reservation.DeviceNames[0]));
			Assert.AreEqual("TestDevicePlatform1", device.Type);

			string? secondModel = device.Model;
			Assert.IsTrue(secondModel != firstModel && (secondModel == "TestDevicePlatform1_Model2" || secondModel == "TestDevicePlatform1_Model3"));
		}

		[TestMethod]
		public async Task TestProblemDeviceAsync()
		{
			await SetupDevicesAsync();

			IJob job = await SetupJobAsync();
			LegacyCreateReservationRequest request = SetupReservationTestAsync(job);

			// create a reservation
			GetLegacyReservationResponse reservation = ResultToValue(await DeviceController!.CreateDeviceReservationV1Async(request));
			Assert.AreEqual(1, reservation.DeviceNames.Length);

			// report a problem with the device 
			OkResult? problem = (await DeviceController!.PutDeviceErrorAsync(reservation.DeviceNames[0])) as OkResult;
			Assert.IsNotNull(problem);

			OkResult? deleted = (await DeviceController!.DeleteReservationV1Async(reservation.Guid)) as OkResult;
			Assert.IsNotNull(deleted);

			// check that telemetry was created
			List<GetDeviceTelemetryResponse> telemetry = (await DeviceController!.GetDeviceTelemetryAsync()).Value!;
			Assert.AreEqual(telemetry.Count, 1);
			// check that problem was recorded
			Assert.IsNotNull(telemetry[0].Telemetry[0].ProblemTimeUtc);
			// make sure the reswrvation time was finished for the problem device
			Assert.IsNotNull(telemetry[0].Telemetry[0].ReservationFinishUtc);
		}

		[TestMethod]
		public async Task TestDevicePoolTelemetryCaptureAsync()
		{
			await SetupDevicesAsync();

			IJob job = await SetupJobAsync();
			LegacyCreateReservationRequest reservationRequest = SetupReservationTestAsync(job);

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

			List<GetDevicePoolTelemetryResponse> telemetry = (await DeviceController!.GetDevicePoolTelemetryAsync()).Value!;

			// 2, as generate an initial telemetry tick in setup
			Assert.AreEqual(2, telemetry.Count);
			foreach (GetDevicePlatformTelemetryResponse t in telemetry[1].Telemetry["testdevicepool1"])
			{
				if (t.PlatformId == "testdeviceplatform1")
				{
					Assert.AreEqual(6, t.Available?.Count);
					Assert.AreEqual(1, t.Reserved?.Count);
					Assert.AreEqual(1, t.Maintenance?.Count);

					Assert.AreEqual(true, t.Reserved?.ContainsKey("ue5-main"));
					Assert.AreEqual(t.Reserved?["ue5-main"].Count, 1);
					Assert.AreEqual(t.Reserved?["ue5-main"][0].DeviceId, reservedDevice.Id);
					Assert.AreEqual(t.Reserved?["ue5-main"][0].JobName, "Test job");
					Assert.AreEqual(t.Reserved?["ue5-main"][0].StepId, "abcd");
				}
				else if (t.PlatformId == "testdeviceplatform3")
				{
					Assert.AreEqual(7, t.Available?.Count);
					Assert.AreEqual(1, t.Disabled?.Count);
				}
				else
				{
					Assert.AreEqual(8, t.Available?.Count);
				}
			}

			foreach (GetDevicePlatformTelemetryResponse t in telemetry[1].Telemetry["testdevicepool2"])
			{
				if (t.PlatformId == "testdeviceplatform2")
				{
					Assert.AreEqual(7, t.Available?.Count);
					Assert.AreEqual(1, t.Problem?.Count);
				}
				else
				{
					Assert.AreEqual(8, t.Available?.Count);
				}
			}
		}
	}
}
