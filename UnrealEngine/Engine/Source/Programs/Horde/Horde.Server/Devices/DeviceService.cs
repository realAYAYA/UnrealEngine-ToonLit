// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Linq;
using System.Security.Claims;
using System.Threading;
using System.Threading.Tasks;
using EpicGames.Horde.Api;
using Horde.Server.Acls;
using Horde.Server.Jobs;
using Horde.Server.Jobs.Graphs;
using Horde.Server.Notifications;
using Horde.Server.Projects;
using Horde.Server.Server;
using Horde.Server.Streams;
using Horde.Server.Users;
using Horde.Server.Utilities;
using HordeCommon;
using Microsoft.Extensions.Hosting;
using Microsoft.Extensions.Logging;
using Microsoft.Extensions.Options;
using MongoDB.Bson;
using OpenTelemetry.Trace;

namespace Horde.Server.Devices
{
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
	[SingletonDocument("device-platform-map", "6165a2e26fd5f104e31e6862")]
	public class DevicePlatformMapV1 : SingletonBase
	{
		/// <summary>
		/// Platform V1 => Platform Id
		/// </summary>
		public Dictionary<string, DevicePlatformId> PlatformMap { get; set; } = new Dictionary<string, DevicePlatformId>();

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
		readonly INotificationService _notificationService;
		readonly JobService _jobService;
		readonly IStreamCollection _streamCollection;
		readonly IUserCollection _userCollection;
		readonly Tracer _tracer;
		readonly ILogger<DeviceService> _logger;
		readonly IDeviceCollection _devices;
		readonly ITicker _ticker;
		readonly ITicker _telemetryTicker;
		readonly IOptionsMonitor<ServerSettings> _settings;
		readonly IOptionsMonitor<GlobalConfig> _globalConfig;

		/// <summary>
		/// Platform map V1 singleton
		/// </summary>
		readonly ISingletonDocument<DevicePlatformMapV1> _platformMapSingleton;

		bool runUpgrade = true;

		/// <summary>
		/// The number of days shared device sheckouts are held
		/// </summary>
		public int sharedDeviceCheckoutDays => _settings.CurrentValue.SharedDeviceCheckoutDays;

		/// <summary>
		/// Device service constructor
		/// </summary>
		public DeviceService(IDeviceCollection devices, ISingletonDocument<DevicePlatformMapV1> platformMapSingleton, IUserCollection userCollection, JobService jobService, IStreamCollection streamCollection, IOptionsMonitor<ServerSettings> settings, IOptionsMonitor<GlobalConfig> globalConfig, INotificationService notificationService, IClock clock, Tracer tracer, ILogger<DeviceService> logger)
		{
			_userCollection = userCollection;
			_devices = devices;
			_jobService = jobService;
			_streamCollection = streamCollection;
			_notificationService = notificationService;
			_ticker = clock.AddSharedTicker<DeviceService>(TimeSpan.FromMinutes(1.0), TickAsync, logger);
			_telemetryTicker = clock.AddSharedTicker("DeviceService.Telemetry", TimeSpan.FromMinutes(10.0), TickTelemetryAsync, logger);
			_tracer = tracer;
			_logger = logger;
			_settings = settings;
			_globalConfig = globalConfig;
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
			try
			{
				GlobalConfig globalConfig = _globalConfig.CurrentValue;
				if (globalConfig.Devices != null)
				{
					await _platformMapSingleton.UpdateAsync(platformMap => {

						platformMap.PlatformMap.Clear();
						platformMap.PerfSpecHighMap.Clear();

						foreach (DevicePlatformConfig platform in globalConfig.Devices.Platforms)
						{
							DevicePlatformId id = new DevicePlatformId(platform.Id);
							foreach (string name in platform.Names)
							{
								platformMap.PlatformMap[name] = id;
							}

							if (platform.LegacyPerfSpecHighModel != null)
							{
								platformMap.PerfSpecHighMap[id] = platform.LegacyPerfSpecHighModel;
							}
						}
					} );
				}				
			}
			catch (Exception ex)
			{
				_logger.LogError(ex, "Exception while updating platform map: {Message}", ex.Message);
			}

			if (!stoppingToken.IsCancellationRequested)
			{
				using TelemetrySpan span = _tracer.StartActiveSpan($"{nameof(DeviceService)}.{nameof(TickTelemetryAsync)}");
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
				using TelemetrySpan span = _tracer.StartActiveSpan($"{nameof(DeviceService)}.{nameof(TickAsync)}");

				GlobalConfig globalConfig = _globalConfig.CurrentValue;

				if (runUpgrade)
				{
					try
					{
						await _devices.UpgradeAsync();
						runUpgrade = false;
					}
					catch (Exception ex)
					{
						_logger.LogError(ex, "Exception while upgrading device collection: {Message}", ex.Message);
					}

				}

				try
				{
					_logger.LogDebug("Expiring reservations");
					await _devices.ExpireReservationsAsync();
				}
				catch (Exception ex)
				{
					_logger.LogError(ex, "Exception while expiring reservations: {Message}", ex.Message);
				}

				// expire shared devices and notifications
				try
				{

					_logger.LogDebug("Sending shared device notifications");
					List<(UserId, IDevice)>? expireNotifications = await _devices.ExpireNotificatonsAsync(_settings.CurrentValue.SharedDeviceCheckoutDays);
					if (expireNotifications != null && expireNotifications.Count > 0)
					{
						foreach ((UserId, IDevice) expiredDevice in expireNotifications)
						{
							await NotifyDeviceServiceAsync(globalConfig, $"Device {expiredDevice.Item2.PlatformId.ToString().ToUpperInvariant()} / {expiredDevice.Item2.Name} checkout will expire in 24 hours.  Please visit https://horde.devtools.epicgames.com/devices to renew the checkout if needed.", null, null, null, expiredDevice.Item1);
						}
					}

					_logger.LogDebug("Expiring shared device checkouts");
					List<(UserId, IDevice)>? expireCheckouts = await _devices.ExpireCheckedOutAsync(_settings.CurrentValue.SharedDeviceCheckoutDays);
					if (expireCheckouts != null && expireCheckouts.Count > 0)
					{
						foreach ((UserId, IDevice) expiredDevice in expireCheckouts)
						{
							await NotifyDeviceServiceAsync(globalConfig, $"Device {expiredDevice.Item2.PlatformId.ToString().ToUpperInvariant()} / {expiredDevice.Item2.Name} checkout has expired.  The device has been returned to the shared pool and should no longer be accessed.  Please visit https://horde.devtools.epicgames.com/devices to checkout devices as needed.", null, null, null, expiredDevice.Item1);
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
				IJob? job = await _jobService.GetJobAsync(JobId.Parse(jobId));
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
		public async Task NotifyDeviceServiceAsync(GlobalConfig globalConfig, string message, DeviceId? deviceId = null, string? jobId = null, string? stepId = null, UserId? userId = null)
		{
			try 
			{
				IDevice? device = null;
				IDevicePool? pool = null;
				IJob? job = null;
				IJobStep? step = null;
				INode? node = null;
				StreamConfig? streamConfig = null;
				IUser? user = null;

				if (userId.HasValue)
				{
					user = await _userCollection.GetUserAsync(userId.Value);
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
					job = await _jobService.GetJobAsync(JobId.Parse(jobId));

					if (job != null)
					{
						globalConfig.TryGetStream(job.StreamId, out streamConfig);

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

				_notificationService.NotifyDeviceService(message, device, pool, streamConfig, job, step, node, user);

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
		/// <param name="globalConfig"></param>
		/// <returns></returns>
		public static bool Authorize(AclAction action, ClaimsPrincipal user, GlobalConfig globalConfig)
		{
			// This is deprecated and device auth should be going through GetUserPoolAuthorizationsAsync

			if (action == DeviceAclAction.DeviceRead)
			{
				return true;
			}

			if (user.IsInRole("Internal-Employees"))
			{
				return true;
			}

			if (globalConfig.Authorize(AdminAclAction.AdminWrite, user))
			{
				return true;
			}

			return false;
		}

		/// <summary>
		/// Get list of pool authorizations for a user
		/// </summary>
		/// <param name="user"></param>
		/// <param name="globalConfig"></param>
		/// <returns></returns>
		public async Task<List<DevicePoolAuthorization>> GetUserPoolAuthorizationsAsync(ClaimsPrincipal user, GlobalConfig globalConfig)
		{
			List<DevicePoolAuthorization> authPools = new List<DevicePoolAuthorization>();

			List<IDevicePool> allPools = await GetPoolsAsync();
			IReadOnlyList<ProjectConfig> projects = globalConfig.Projects;

			// Set of projects associated with device pools
			HashSet<ProjectId> projectIds = new HashSet<ProjectId>(allPools.Where(x => x.ProjectIds != null).SelectMany(x => x.ProjectIds!));

			Dictionary<ProjectId, bool> deviceRead = new Dictionary<ProjectId, bool>();
			Dictionary<ProjectId, bool> deviceWrite = new Dictionary<ProjectId, bool>();

			foreach (ProjectId projectId in projectIds)
			{
				if (projects.Where(x => x.Id == projectId).FirstOrDefault() == null)
				{
					_logger.LogWarning("Device pool authorization references missing project id {ProjectId}", projectId);
					continue;
				}

				if (globalConfig.TryGetProject(projectId, out ProjectConfig? projectConfig))
				{
					deviceRead.Add(projectId, projectConfig.Authorize(DeviceAclAction.DeviceRead, user));
					deviceWrite.Add(projectId, projectConfig.Authorize(DeviceAclAction.DeviceWrite, user));
				}
			}

			// for global pools which aren't associated with a project
			bool globalPoolAccess = user.IsInRole("Internal-Employees");

			if (!globalPoolAccess)
			{
				if (globalConfig.Authorize(AdminAclAction.AdminWrite, user))
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
		/// <param name="globalConfig"></param>
		/// <returns></returns>
		public async Task<DevicePoolAuthorization?> GetUserPoolAuthorizationAsync(DevicePoolId id, ClaimsPrincipal user, GlobalConfig globalConfig)
		{
			List<DevicePoolAuthorization> auth = await GetUserPoolAuthorizationsAsync(user, globalConfig);
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
	}
}
