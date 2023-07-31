// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Linq;
using System.Security.Claims;
using System.Threading;
using System.Threading.Tasks;
using Horde.Build.Acls;
using Horde.Build.Jobs;
using Horde.Build.Jobs.Graphs;
using Horde.Build.Notifications;
using Horde.Build.Projects;
using Horde.Build.Server;
using Horde.Build.Streams;
using Horde.Build.Users;
using Horde.Build.Utilities;
using HordeCommon;
using Microsoft.Extensions.Hosting;
using Microsoft.Extensions.Logging;
using MongoDB.Bson;
using OpenTracing;
using OpenTracing.Util;


namespace Horde.Build.Devices
{
	using DeviceId = StringId<IDevice>;
	using DevicePlatformId = StringId<IDevicePlatform>;
	using DevicePoolId = StringId<IDevicePool>;
	using JobId = ObjectId<IJob>;
	using ProjectId = StringId<IProject>;
	using UserId = ObjectId<IUser>;

	/// <summary>
	///  Device Pool Authorization (convenience class for pool ACL's)
	/// </summary>
	public class DevicePoolAuthorization
	{
		/// <summary>
		/// The device pool
		/// </summary>
		public IDevicePool Pool { get; private set; }

		/// <summary>
		///  Read access to pool
		/// </summary>
		public bool Read { get; private set; }

		/// <summary>
		///  Write access to pool
		/// </summary>
		public bool Write { get; private set; }

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="pool"></param>
		/// <param name="read"></param>
		/// <param name="write"></param>
		public DevicePoolAuthorization(IDevicePool pool, bool read, bool write)
		{
			Pool = pool;
			Read = read;
			Write = write;				
		}
	}

	/// <summary>
	/// Platform map required by V1 API
	/// </summary>
	[SingletonDocument("6165a2e26fd5f104e31e6862")]
	public class DevicePlatformMapV1 : SingletonBase
	{
		/// <summary>
		/// Platform V1 => Platform Id
		/// </summary>
		public Dictionary<string, DevicePlatformId> PlatformMap { get; set; } = new Dictionary<string, DevicePlatformId>();

		/// <summary>
		/// Platform Id => Platform V1
		/// </summary>
		public Dictionary<DevicePlatformId, string> PlatformReverseMap { get; set; } = new Dictionary<DevicePlatformId, string>();

		/// <summary>
		/// Perfspec V1 => Model
		/// </summary>
		public Dictionary<DevicePlatformId, string> PerfSpecHighMap { get; set; } = new Dictionary<DevicePlatformId, string>();

	}

	/// <summary>
	/// Device management service
	/// </summary>
	public sealed class DeviceService : IHostedService, IDisposable
	{
		/// <summary>
		/// The ACL service instance
		/// </summary>
		readonly AclService _aclService;

		/// <summary>
		/// Instance of the notification service
		/// </summary>
		readonly INotificationService _notificationService;

		/// <summary>
		/// Singleton instance of the job service
		/// </summary>
		readonly JobService _jobService;

		/// <summary>
		/// Singleton instance of the stream service
		/// </summary>
		readonly StreamService _streamService;

		/// <summary>
		/// Singleton instance of the project service
		/// </summary>
		readonly ProjectService _projectService;

		/// <summary>
		/// The user collection instance
		/// </summary>
		IUserCollection UserCollection { get; set; }

		/// <summary>
		/// Log output writer
		/// </summary>
		readonly ILogger<DeviceService> _logger;

		/// <summary>
		/// Device collection
		/// </summary>
		readonly IDeviceCollection _devices;
		readonly ITicker _ticker;
		readonly ITicker _telemetryTicker;

		/// <summary>
		/// Platform map V1 singleton
		/// </summary>
		readonly ISingletonDocument<DevicePlatformMapV1> _platformMapSingleton;

		/// <summary>
		/// Device service constructor
		/// </summary>
		public DeviceService(IDeviceCollection devices, ISingletonDocument<DevicePlatformMapV1> platformMapSingleton, IUserCollection userCollection, JobService jobService, ProjectService projectService, StreamService streamService, AclService aclService, INotificationService notificationService, IClock clock, ILogger<DeviceService> logger)
		{
			UserCollection = userCollection;
			_devices = devices;
			_jobService = jobService;
			_projectService = projectService;
			_streamService = streamService;
			_aclService = aclService;
			_notificationService = notificationService;
			_ticker = clock.AddSharedTicker<DeviceService>(TimeSpan.FromMinutes(1.0), TickAsync, logger);
			_telemetryTicker = clock.AddSharedTicker("DeviceService.Telemetry", TimeSpan.FromMinutes(10.0), TickTelemetryAsync, logger);
			_logger = logger;

			_platformMapSingleton = platformMapSingleton;

		}

		/// <inheritdoc/>
		public async Task StartAsync(CancellationToken cancellationToken)
		{
			await _ticker.StartAsync();
			await _telemetryTicker.StartAsync();
		}
		

		/// <inheritdoc/>
		public async Task StopAsync(CancellationToken cancellationToken)
		{
			await _ticker.StopAsync();
			await _telemetryTicker.StopAsync();
		}

		/// <inheritdoc/>
		public void Dispose() 
		{ 
			_ticker.Dispose(); 
			_telemetryTicker.Dispose(); 
		}

		/// <summary>
		/// Ticks service
		/// </summary>
		async ValueTask TickTelemetryAsync(CancellationToken stoppingToken)
		{
			if (!stoppingToken.IsCancellationRequested)
			{
				using IScope scope = GlobalTracer.Instance.BuildSpan("DeviceService.TickTelemetryAsync").StartActive();

				_logger.LogInformation("Updating pool telemetry");
				await _devices.CreatePoolTelemetrySnapshot();
			}
		}

		/// <summary>
		/// Ticks service
		/// </summary>
		async ValueTask TickAsync(CancellationToken stoppingToken)
		{
			if (!stoppingToken.IsCancellationRequested)
			{
				using IScope scope = GlobalTracer.Instance.BuildSpan("DeviceService.TickAsync").StartActive();

				try
				{
					_logger.LogInformation("Expiring reservations");
					await _devices.ExpireReservationsAsync();
				}
				catch (Exception ex)
				{
					_logger.LogError(ex, "Exception while expiring reservations: {Message}", ex.Message);
				}


				// expire shared devices and notifications
				try
				{

					_logger.LogInformation("Sending shared device notifications");
					List<(UserId, IDevice)>? expireNotifications = await _devices.ExpireNotificatonsAsync();
					if (expireNotifications != null && expireNotifications.Count > 0)
					{
						foreach ((UserId, IDevice) expiredDevice in expireNotifications)
						{
							await NotifyDeviceServiceAsync($"Device {expiredDevice.Item2.PlatformId.ToString().ToUpperInvariant()} / {expiredDevice.Item2.Name} checkout will expire in 24 hours.  Please visit https://horde.devtools.epicgames.com/devices to renew the checkout if needed.", null, null, null, expiredDevice.Item1);
						}
					}

					_logger.LogInformation("Expiring shared device checkouts");
					List<(UserId, IDevice)>? expireCheckouts = await _devices.ExpireCheckedOutAsync();
					if (expireCheckouts != null && expireCheckouts.Count > 0)
					{
						foreach ((UserId, IDevice) expiredDevice in expireCheckouts)
						{
							await NotifyDeviceServiceAsync($"Device {expiredDevice.Item2.PlatformId.ToString().ToUpperInvariant()} / {expiredDevice.Item2.Name} checkout has expired.  The device has been returned to the shared pool and should no longer be accessed.  Please visit https://horde.devtools.epicgames.com/devices to checkout devices as needed.", null, null, null, expiredDevice.Item1);
						}
					}
				}
				catch (Exception ex)
				{
					_logger.LogError(ex, "Exception while expiring device checkouts: {Message}", ex.Message);
				}
			}
		}

		internal async Task TickForTestingAsync()
		{
			await TickAsync(CancellationToken.None);
			await TickTelemetryAsync(CancellationToken.None);
		}


		/// <summary>
		/// Create a new device platform
		/// </summary>
		public Task<IDevicePlatform?> TryCreatePlatformAsync(DevicePlatformId id, string name)
		{
			return _devices.TryAddPlatformAsync(id, name);
		}

		/// <summary>
		/// Get a list of existing device platforms
		/// </summary>
		public Task<List<IDevicePlatform>> GetPlatformsAsync()
		{
			return _devices.FindAllPlatformsAsync();
		}

		/// <summary>
		/// Update an existing platform
		/// </summary>
		public Task<bool> UpdatePlatformAsync(DevicePlatformId platformId, string[]? modelIds)
		{
			return _devices.UpdatePlatformAsync(platformId, modelIds);
		}

		/// <summary>
		/// Get a specific device platform
		/// </summary>
		public Task<IDevicePlatform?> GetPlatformAsync(DevicePlatformId id)
		{
			return _devices.GetPlatformAsync(id);
		}

		/// <summary>
		/// Get a device pool by id
		/// </summary>
		public Task<IDevicePool?> GetPoolAsync(DevicePoolId id)
		{
			return _devices.GetPoolAsync(id);
		}

		/// <summary>
		/// Create a new device pool
		/// </summary>
		public Task<IDevicePool?> TryCreatePoolAsync(DevicePoolId id, string name, DevicePoolType poolType, List<ProjectId>? projectIds)
		{
			return _devices.TryAddPoolAsync(id, name, poolType, projectIds);
		}

		/// <summary>
		/// Update a device pool
		/// </summary>
		public Task UpdatePoolAsync(DevicePoolId id, List<ProjectId>? projectIds)
		{
			return _devices.UpdatePoolAsync(id, projectIds);
		}

		/// <summary>
		/// Get a list of existing device pools
		/// </summary>
		public Task<List<IDevicePool>> GetPoolsAsync()
		{
			return _devices.FindAllPoolsAsync();
		}

		/// <summary>
		/// Get a list of devices, optionally filtered to provided ids
		/// </summary>
		public Task<List<IDevice>> GetDevicesAsync(List<DeviceId>? deviceIds = null, DevicePoolId? poolId = null, DevicePlatformId? platformId = null)
		{
			return _devices.FindAllDevicesAsync(deviceIds, poolId, platformId);
		}

		/// <summary>
		/// Get device telemetry
		/// </summary>
		public Task<List<IDeviceTelemetry>> GetDeviceTelemetryAsync(DeviceId[]? deviceIds = null, DateTimeOffset? minCreateTime=null, DateTimeOffset? maxCreateTime=null, int? index = null, int? count = null)
		{
			return _devices.FindDeviceTelemetryAsync(deviceIds, minCreateTime, maxCreateTime, index, count);
		}

		/// <summary>
		/// Get device pool telemetry
		/// </summary>
		public Task<List<IDevicePoolTelemetry>> GetDevicePoolTelemetryAsync(DateTimeOffset? minCreateTime = null, DateTimeOffset? maxCreateTime = null, int? index = null, int? count = null)
		{
			return _devices.FindPoolTelemetryAsync(minCreateTime, maxCreateTime, index, count);
		}


		/// <summary>
		/// Get a specific device
		/// </summary>
		public Task<IDevice?> GetDeviceAsync(DeviceId id)
		{
			return _devices.GetDeviceAsync(id);
		}

		/// <summary>
		/// Get a device by name
		/// </summary>
		public Task<IDevice?> GetDeviceByNameAsync(string deviceName)
		{
			return _devices.GetDeviceByNameAsync(deviceName);
		}

		/// <summary>
		/// Delete a device
		/// </summary>
		public async Task<bool> DeleteDeviceAsync(DeviceId id)
		{
			return await _devices.DeleteDeviceAsync(id);
		}

		/// <summary>
		/// Create a new device
		/// </summary>
		/// <param name="id">Unique id of the device</param>
		/// <param name="name">Friendly name of the device</param>
		/// <param name="platformId">The device platform</param>
		/// <param name="poolId">Which pool to add the device</param>
		/// <param name="enabled">Whether the device is enabled</param>
		/// <param name="address">Address or hostname of device</param>
		/// <param name="modelId">Vendor model id</param>
        /// <param name="userId">User adding the device</param>
		/// <returns></returns>
		public Task<IDevice?> TryCreateDeviceAsync(DeviceId id, string name, DevicePlatformId platformId, DevicePoolId poolId, bool? enabled, string? address, string? modelId, UserId? userId = null)
		{
			return _devices.TryAddDeviceAsync(id, name, platformId, poolId, enabled, address, modelId, userId);
		}

		/// <summary>
		/// Update a device
		/// </summary>
		public Task UpdateDeviceAsync(DeviceId deviceId, DevicePoolId? newPoolId = null, string? newName = null, string? newAddress = null, string? newModelId = null, string? newNotes = null, bool? newEnabled = null, bool? newProblem = null, bool? newMaintenance = null, UserId? modifiedByUserId = null)
		{
			return _devices.UpdateDeviceAsync(deviceId, newPoolId, newName, newAddress, newModelId, newNotes, newEnabled, newProblem, newMaintenance, modifiedByUserId);
		}

		/// <summary>
		/// Checkout a device
		/// </summary>
		public Task CheckoutDeviceAsync(DeviceId deviceId, UserId? userId)
		{
            return _devices.CheckoutDeviceAsync(deviceId, userId);
        }

		/// <summary>
		/// Try to create a reservation satisfying the specified device platforms and models
		/// </summary>
		public async Task<IDeviceReservation?> TryCreateReservationAsync(DevicePoolId pool, List<DeviceRequestData> request, string? hostname = null, string? reservationDetails = null, string? jobId = null, string? stepId = null)
		{
			string? streamId = null;
			string? jobName = null;
			string? stepName = null;

			if (jobId != null)
			{
				IJob? job = await _jobService.GetJobAsync(new JobId(jobId));
				if (job != null)
				{					
					streamId = job.StreamId.ToString();
					jobName = job.Name;

					if (stepId != null)
					{
						SubResourceId id = SubResourceId.Parse(stepId);
						IGraph graph = await _jobService.GetGraphAsync(job);
						foreach (IJobStepBatch batch in job.Batches)
						{
							IJobStep? step;							
							if (batch.TryGetStep(id, out step))
							{
								stepName = graph.Groups[batch.GroupIdx].Nodes[step.NodeIdx].Name;
								break;
							}
						}
					}
				}
			}

			return await _devices.TryAddReservationAsync(pool, request, hostname, reservationDetails, streamId, jobId, stepId, jobName, stepName);
		}

		/// <summary>
		/// Update/renew an existing reservation
		/// </summary>
		public Task<bool> TryUpdateReservationAsync(ObjectId id)
		{
			return _devices.TryUpdateReservationAsync(id);
		}

		/// <summary>
		///  Delete an existing reservation
		/// </summary>
		public Task<bool> DeleteReservationAsync(ObjectId id)
		{
			return _devices.DeleteReservationAsync(id);
		}

		/// <summary>
		/// Get a reservation from a legacy guid
		/// </summary>
		public Task<IDeviceReservation?> TryGetReservationFromLegacyGuidAsync(string legacyGuid)
		{
			return _devices.TryGetReservationFromLegacyGuidAsync(legacyGuid);
		}

		/// <summary>
		/// Get a reservation from a device id
		/// </summary>
		public Task<IDeviceReservation?> TryGetDeviceReservation(DeviceId deviceId)
		{
			return _devices.TryGetDeviceReservationAsync(deviceId);
		}

		/// <summary>
		/// Get a list of existing device reservations
		/// </summary>
		public Task<List<IDeviceReservation>> GetReservationsAsync()
		{
			return _devices.FindAllReservationsAsync();
		}

		/// <summary>
		/// 
		/// </summary>
		/// <param name="message"></param>
		/// <param name="deviceId"></param>
		/// <param name="jobId"></param>
		/// <param name="stepId"></param>
		/// <param name="userId"></param>
		public async Task NotifyDeviceServiceAsync(string message, DeviceId? deviceId = null, string? jobId = null, string? stepId = null, UserId? userId = null)
		{
			try 
			{
				IDevice? device = null;
				IDevicePool? pool = null;
				IJob? job = null;
				IJobStep? step = null;
				INode? node = null;
				IStream? stream = null;
				IUser? user = null;

				if (userId.HasValue)
				{
					user = await UserCollection.GetUserAsync(userId.Value);
					if (user == null)
					{
						_logger.LogError("Unable to send device notification, can't find User {UserId}", userId.Value);
						return;
					}
				}

				if (deviceId.HasValue)
				{
					device = await GetDeviceAsync(deviceId.Value);
					pool = await GetPoolAsync(device!.PoolId);
				}

				if (jobId != null)
				{
					job = await _jobService.GetJobAsync(new JobId(jobId));

					if (job != null)
					{
						stream = await _streamService.GetStreamAsync(job.StreamId);

						if (stepId != null)
						{
							IGraph graph = await _jobService.GetGraphAsync(job)!;

							SubResourceId stepIdValue = SubResourceId.Parse(stepId);
							IJobStepBatch? batch = job.Batches.FirstOrDefault(b => b.Steps.FirstOrDefault(s => s.Id == stepIdValue) != null);
							if (batch != null)
							{
								step = batch.Steps.FirstOrDefault(s => s.Id == stepIdValue)!;
								INodeGroup group = graph.Groups[batch.GroupIdx];
								node = group.Nodes[step.NodeIdx];
							}
						}
					}
				}

				_notificationService.NotifyDeviceService(message, device, pool, stream, job, step, node, user);

			}
			catch (Exception ex)
			{
                _logger.LogError(ex, "Error on device notification {Message}", ex.Message);
            }
        }

		/// <summary>
		/// Authorize device action
		/// </summary>
		/// <param name="action"></param>
		/// <param name="user"></param>
		/// <returns></returns>
		public async Task<bool> AuthorizeAsync(AclAction action, ClaimsPrincipal user)
		{
			// Tihs is deprecated and device auth should be going through GetUserPoolAuthorizationsAsync

			if (action == AclAction.DeviceRead)
			{
				return true;
			}

			if (user.IsInRole("Internal-Employees"))
			{
				return true;
			}

			if (await _aclService.AuthorizeAsync(AclAction.AdminWrite, user))
			{
				return true;
			}

			return false;

		}

		/// <summary>
		/// Get list of pool authorizations for a user
		/// </summary>
		/// <param name="user"></param>
		/// <returns></returns>
		public async Task<List<DevicePoolAuthorization>> GetUserPoolAuthorizationsAsync(ClaimsPrincipal user)
		{
			
			List<DevicePoolAuthorization> authPools = new List<DevicePoolAuthorization>();

			List<IDevicePool> allPools = await GetPoolsAsync();
			List<IProject> projects = await _projectService.GetProjectsAsync();

			// Set of projects associated with device pools
			HashSet<ProjectId> projectIds = new HashSet<ProjectId>(allPools.Where(x => x.ProjectIds != null).SelectMany(x => x.ProjectIds!));

			Dictionary<ProjectId, bool> deviceRead = new Dictionary<ProjectId, bool>();
			Dictionary<ProjectId, bool> deviceWrite = new Dictionary<ProjectId, bool>();
			ProjectPermissionsCache permissionsCache = new ProjectPermissionsCache();

			foreach (ProjectId projectId in projectIds)
			{

				if (projects.Where(x => x.Id == projectId).FirstOrDefault() == null)
				{
					_logger.LogWarning("Device pool authorization references missing project id {ProjectId}", projectId);
					continue;
				}

				deviceRead.Add(projectId,  await _projectService.AuthorizeAsync(projectId, AclAction.DeviceRead, user, permissionsCache));
				deviceWrite.Add(projectId, await _projectService.AuthorizeAsync(projectId, AclAction.DeviceWrite, user, permissionsCache));
			}

			// for global pools which aren't associated with a project
			bool globalPoolAccess = user.IsInRole("Internal-Employees");

			if (!globalPoolAccess)
			{
				if (await _aclService.AuthorizeAsync(AclAction.AdminWrite, user))
				{
					globalPoolAccess = true;
				}
			}

			foreach (IDevicePool pool in allPools)
			{				
				if (pool.ProjectIds == null || pool.ProjectIds.Count == 0)
				{
					if (pool.PoolType == DevicePoolType.Shared && !globalPoolAccess)
					{
						authPools.Add(new DevicePoolAuthorization(pool, false, false));
						continue;
					}

					authPools.Add(new DevicePoolAuthorization(pool, true, true));
					continue;
				}

				bool read = false;
				bool write = false;

				foreach (ProjectId projectId in pool.ProjectIds)
				{
					bool value;

					if (!read && deviceRead.TryGetValue(projectId, out value))
					{
						read = value;
					}

					if (!write && deviceWrite.TryGetValue(projectId, out value))
					{
						write = value;
					}
				}

				authPools.Add(new DevicePoolAuthorization(pool, read, write));

			}

			return authPools;
		}

		/// <summary>
		/// Get list of pool authorizations for a user
		/// </summary>
		/// <param name="id"></param>
		/// <param name="user"></param>
		/// <returns></returns>
		public async Task<DevicePoolAuthorization?> GetUserPoolAuthorizationAsync(DevicePoolId id, ClaimsPrincipal user)
		{
			List<DevicePoolAuthorization> auth = await GetUserPoolAuthorizationsAsync(user);
			return auth.Where(x => x.Pool.Id == id).FirstOrDefault();
		}

		/// <summary>
		/// Update a device
		/// </summary>
		public async Task UpdateDevicePoolAsync(DevicePoolId poolId, List<ProjectId>? projectIds)
		{
			await _devices.UpdatePoolAsync(poolId, projectIds);
		}

		/// <summary>
		/// Get Platform mappings for V1 API
		/// </summary>		
		public async Task<DevicePlatformMapV1> GetPlatformMapV1()
		{
			return await _platformMapSingleton.GetAsync();
		}

		/// <summary>
		/// Updates the platform mapping information required for the V1 api
		/// </summary>				
		public async Task<bool> UpdatePlatformMapAsync(UpdatePlatformMapRequest request)
		{

			List<IDevicePlatform> platforms = await GetPlatformsAsync();

			// Update the platform map
			for (int i = 0; i < 10; i++)
			{
				DevicePlatformMapV1 instance = await _platformMapSingleton.GetAsync();

				instance.PlatformMap = new Dictionary<string, DevicePlatformId>();
				instance.PlatformReverseMap = new Dictionary<DevicePlatformId, string>();
				instance.PerfSpecHighMap = new Dictionary<DevicePlatformId, string>();

				foreach(KeyValuePair<string, string> entry in request.PlatformMap)
				{
					IDevicePlatform? platform = platforms.FirstOrDefault(p => p.Id == new DevicePlatformId(entry.Value));
					if (platform == null)
					{
						throw new Exception($"Unknowm platform in map {entry.Key} : {entry.Value}");
					}

					instance.PlatformMap.Add(entry.Key, platform.Id);
				}

				foreach (KeyValuePair<string, string> entry in request.PlatformReverseMap)
				{
					IDevicePlatform? platform = platforms.FirstOrDefault(p => p.Id == new DevicePlatformId(entry.Key));
					if (platform == null)
					{
						throw new Exception($"Unknowm platform in reverse map {entry.Key} : {entry.Value}");
					}

					instance.PlatformReverseMap.Add(platform.Id, entry.Value);
				}

				foreach (KeyValuePair<string, string> entry in request.PerfSpecHighMap)
				{
					IDevicePlatform? platform = platforms.FirstOrDefault(p => p.Id == new DevicePlatformId(entry.Key));
					if (platform == null)
					{
						throw new Exception($"Unknowm platform in spec map {entry.Key} : {entry.Value}");
					}

					instance.PerfSpecHighMap.Add(platform.Id, entry.Value);
				}

				if (await _platformMapSingleton.TryUpdateAsync(instance))
				{
					return true;
				}

				Thread.Sleep(1000);
			}

			return false;
		}
	}
}
