// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Linq;
using System.Text.Json;
using System.Threading.Tasks;
using Horde.Build.Acls;
using Horde.Build.Projects;
using Horde.Build.Users;
using Horde.Build.Utilities;
using Microsoft.AspNetCore.Authorization;
using Microsoft.AspNetCore.Mvc;
using Microsoft.Extensions.Logging;
using MongoDB.Bson;
using MongoDB.Driver;

namespace Horde.Build.Devices
{
	using DeviceId = StringId<IDevice>;
	using DevicePlatformId = StringId<IDevicePlatform>;
	using DevicePoolId = StringId<IDevicePool>;
	using ProjectId = StringId<IProject>;

	/// <summary>
	/// Controller for device service
	/// </summary>
	public class DevicesController : ControllerBase
	{

		/// <summary>
		/// The acl service singleton
		/// </summary>
		readonly AclService _aclService;

		/// <summary>
		/// The user collection instance
		/// </summary>
		IUserCollection UserCollection { get; set; }

		/// <summary>
		/// Singleton instance of the device service
		/// </summary>
		readonly DeviceService _deviceService;

		/// <summary>
		///  Logger for controller
		/// </summary>
		private readonly ILogger<DevicesController> _logger;

		/// <summary>
		/// Constructor
		/// </summary>
		public DevicesController(DeviceService deviceService, AclService aclService, IUserCollection userCollection, ILogger<DevicesController> logger)
		{
			UserCollection = userCollection;
			_deviceService = deviceService;
			_logger = logger;
			_aclService = aclService;
		}

		// DEVICES

		/// <summary>
		/// Create a new device
		/// </summary>
		[HttpPost]
		[Authorize]
		[Route("/api/v2/devices")]
		public async Task<ActionResult<CreateDeviceResponse>> CreateDeviceAsync([FromBody] CreateDeviceRequest deviceRequest)
		{

			DevicePoolAuthorization? poolAuth = await _deviceService.GetUserPoolAuthorizationAsync(new DevicePoolId(deviceRequest.PoolId!), User);

			if (poolAuth == null || !poolAuth.Write)
			{
				return Forbid();
			}

			IUser? internalUser = await UserCollection.GetUserAsync(User);
			if (internalUser == null)
			{
				return NotFound();
			}

			IDevicePlatform? platform = await _deviceService.GetPlatformAsync(new DevicePlatformId(deviceRequest.PlatformId!));

			if (platform == null)
			{
				return BadRequest($"Bad platform id {deviceRequest.PlatformId} on request");
			}

			IDevicePool? pool = await _deviceService.GetPoolAsync(new DevicePoolId(deviceRequest.PoolId!));

			if (pool == null)
			{
				return BadRequest($"Bad pool id {deviceRequest.PoolId} on request");
			}

			string? modelId = null;

			if (!String.IsNullOrEmpty(deviceRequest.ModelId))
			{
				modelId = new string(deviceRequest.ModelId);

				if (platform.Models?.FirstOrDefault(x => x == modelId) == null)
				{
					return BadRequest($"Bad model id {modelId} for platform {platform.Id} on request");
				}
			}

			string name = deviceRequest.Name.Trim();
			string? address = deviceRequest.Address?.Trim();

			IDevice? device = await _deviceService.TryCreateDeviceAsync(DeviceId.Sanitize(name), name, platform.Id, pool.Id, deviceRequest.Enabled, address, modelId, internalUser.Id);

			if (device == null)
			{
				return BadRequest($"Unable to create device");
			}

			return new CreateDeviceResponse(device.Id.ToString());
		}

		/// <summary>
		/// Get list of devices
		/// </summary>
		[HttpGet]
		[Authorize]
		[Route("/api/v2/devices")]
		[ProducesResponseType(typeof(List<GetDeviceResponse>), 200)]
		public async Task<ActionResult<List<object>>> GetDevicesAsync()
		{

			List<DevicePoolAuthorization> poolAuth = await _deviceService.GetUserPoolAuthorizationsAsync(User);

			List<IDevice> devices = await _deviceService.GetDevicesAsync();

			List<object> responses = new List<object>();

			foreach (IDevice device in devices)
			{
				DevicePoolAuthorization? auth = poolAuth.Where(x => x.Pool.Id == device.PoolId).FirstOrDefault();

				if (auth == null || !auth.Read)
				{
					continue;
				}

				responses.Add(new GetDeviceResponse(device.Id.ToString(), device.PlatformId.ToString(), device.PoolId.ToString(), device.Name, device.Enabled, device.Address, device.ModelId?.ToString(), device.ModifiedByUser, device.Notes, device.ProblemTimeUtc, device.MaintenanceTimeUtc, device.Utilization, device.CheckedOutByUser, device.CheckOutTime));
			}

			return responses;
		}

		/// <summary>
		/// Get a specific device
		/// </summary>
		[HttpGet]
		[Authorize]
		[Route("/api/v2/devices/{deviceId}")]
		[ProducesResponseType(typeof(GetDeviceResponse), 200)]
		public async Task<ActionResult<object>> GetDeviceAsync(string deviceId)
		{

			DeviceId deviceIdValue = new DeviceId(deviceId);

			IDevice? device = await _deviceService.GetDeviceAsync(deviceIdValue);

			if (device == null)
			{
				return BadRequest($"Unable to find device with id {deviceId}");
			}

			DevicePoolAuthorization? poolAuth = await _deviceService.GetUserPoolAuthorizationAsync(device.PoolId, User);

			if (poolAuth == null || !poolAuth.Write)
			{
				return Forbid();
			}

			return new GetDeviceResponse(device.Id.ToString(), device.PlatformId.ToString(), device.PoolId.ToString(), device.Name, device.Enabled, device.Address, device.ModelId?.ToString(), device.ModifiedByUser?.ToString(), device.Notes, device.ProblemTimeUtc, device.MaintenanceTimeUtc, device.Utilization, device.CheckedOutByUser, device.CheckOutTime);
		}

		/// <summary>
		/// Update a specific device
		/// </summary>
		[HttpPut]
		[Authorize]
		[Route("/api/v2/devices/{deviceId}")]
		[ProducesResponseType(typeof(List<GetDeviceResponse>), 200)]
		public async Task<ActionResult> UpdateDeviceAsync(string deviceId, [FromBody] UpdateDeviceRequest update)
		{
			IUser? internalUser = await UserCollection.GetUserAsync(User);
			if (internalUser == null)
			{
				return NotFound();
			}

			DeviceId deviceIdValue = new DeviceId(deviceId);

			IDevice? device = await _deviceService.GetDeviceAsync(deviceIdValue);

			if (device == null)
			{
				return BadRequest($"Device with id ${deviceId} does not exist");
			}

			DevicePoolAuthorization? poolAuth = await _deviceService.GetUserPoolAuthorizationAsync(device.PoolId, User);

			if (poolAuth == null || !poolAuth.Write)
			{
				return Forbid();
			}

			IDevicePlatform? platform = await _deviceService.GetPlatformAsync(device.PlatformId);

			if (platform == null)
			{
				return BadRequest($"Platform id ${device.PlatformId} does not exist");
			}

			DevicePoolId? poolIdValue = null;

			if (update.PoolId != null)
			{
				poolIdValue = new DevicePoolId(update.PoolId);
			}

			string? modelIdValue = null;

			if (update.ModelId != null)
			{
				modelIdValue = new string(update.ModelId);

				if (platform.Models?.FirstOrDefault(x => x == modelIdValue) == null)
				{
					return BadRequest($"Bad model id {update.ModelId} for platform {platform.Id} on request");
				}
			}

			string? name = update.Name?.Trim();
			string? address = update.Address?.Trim();

			await _deviceService.UpdateDeviceAsync(deviceIdValue, poolIdValue, name, address, modelIdValue, update.Notes, update.Enabled, update.Problem, update.Maintenance, internalUser.Id);

			return Ok();
		}

		/// <summary>
		/// Checkout a device
		/// </summary>
		[HttpPut]
		[Authorize]
		[Route("/api/v2/devices/{deviceId}/checkout")]
		[ProducesResponseType(typeof(List<GetDeviceResponse>), 200)]
		public async Task<ActionResult> CheckoutDeviceAsync(string deviceId, [FromBody] CheckoutDeviceRequest request)
		{

			IUser? internalUser = await UserCollection.GetUserAsync(User);
			if (internalUser == null)
			{
				return NotFound();
			}

			DeviceId deviceIdValue = new DeviceId(deviceId);

			IDevice? device = await _deviceService.GetDeviceAsync(deviceIdValue);

			if (device == null)
			{
				return BadRequest($"Device with id ${deviceId} does not exist");
			}

			DevicePoolAuthorization? poolAuth = await _deviceService.GetUserPoolAuthorizationAsync(device.PoolId, User);

			if (poolAuth == null || !poolAuth.Write)
			{
				return Forbid();
			}

			if (request.Checkout)
			{
				if (!String.IsNullOrEmpty(device.CheckedOutByUser))
				{
					return BadRequest($"Already checked out by user {device.CheckedOutByUser}");
				}

				await _deviceService.CheckoutDeviceAsync(deviceIdValue, internalUser.Id);

			}
			else
			{
				await _deviceService.CheckoutDeviceAsync(deviceIdValue, null);
			}

			return Ok();
		}

		/// <summary>
		/// Delete a specific device
		/// </summary>
		[HttpDelete]
		[Authorize]
		[Route("/api/v2/devices/{deviceId}")]
		public async Task<ActionResult> DeleteDeviceAsync(string deviceId)
		{
			if (!await _deviceService.AuthorizeAsync(AclAction.DeviceWrite, User))
			{
				return Forbid();
			}

			DeviceId deviceIdValue = new DeviceId(deviceId);

			IDevice? device = await _deviceService.GetDeviceAsync(deviceIdValue);
			if (device == null)
			{
				return NotFound();
			}

			DevicePoolAuthorization? poolAuth = await _deviceService.GetUserPoolAuthorizationAsync(device.PoolId, User);

			if (poolAuth == null || !poolAuth.Write)
			{
				return Forbid();
			}

			await _deviceService.DeleteDeviceAsync(deviceIdValue);
			return Ok();
		}

		// PLATFORMS

		/// <summary>
		/// Create a new device platform
		/// </summary>
		[HttpPost]
		[Authorize]
		[Route("/api/v2/devices/platforms")]
		public async Task<ActionResult<CreateDevicePlatformResponse>> CreatePlatformAsync([FromBody] CreateDevicePlatformRequest request)
		{
			if (!await _deviceService.AuthorizeAsync(AclAction.DeviceWrite, User))
			{
				return Forbid();
			}

			string name = request.Name.Trim();

			IDevicePlatform? platform = await _deviceService.TryCreatePlatformAsync(DevicePlatformId.Sanitize(name), name);

			if (platform == null)
			{
				return BadRequest($"Unable to create platform for {name}");
			}

			return new CreateDevicePlatformResponse(platform.Id.ToString());
		}

		/// <summary>
		/// Update a device platform
		/// </summary>
		[HttpPut]
		[Authorize]
		[Route("/api/v2/devices/platforms/{platformId}")]
		public async Task<ActionResult<CreateDevicePlatformResponse>> UpdatePlatformAsync(string platformId, [FromBody] UpdateDevicePlatformRequest request)
		{
			if (!await _deviceService.AuthorizeAsync(AclAction.DeviceWrite, User))
			{
				return Forbid();
			}

			await _deviceService.UpdatePlatformAsync(new DevicePlatformId(platformId), request.ModelIds);

			return Ok();
		}

		/// <summary>
		/// Get a list of supported device platforms
		/// </summary>
		[HttpGet]
		[Authorize]
		[Route("/api/v2/devices/platforms")]
		[ProducesResponseType(typeof(List<GetDevicePlatformResponse>), 200)]
		public async Task<ActionResult<List<object>>> GetDevicePlatformsAsync()
		{

			if (!await _deviceService.AuthorizeAsync(AclAction.DeviceRead, User))
			{
				return Forbid();
			}

			List<IDevicePlatform> platforms = await _deviceService.GetPlatformsAsync();

			List<object> responses = new List<object>();

			foreach (IDevicePlatform platform in platforms)
			{
				// @todo: ACL per platform
				responses.Add(new GetDevicePlatformResponse(platform.Id.ToString(), platform.Name, platform.Models?.ToArray() ?? Array.Empty<string>()));
			}

			return responses;
		}

		// POOLS

		/// <summary>
		/// Create a new device pool
		/// </summary>
		[HttpPost]
		[Authorize]
		[Route("/api/v2/devices/pools")]
		public async Task<ActionResult<CreateDevicePoolResponse>> CreatePoolAsync([FromBody] CreateDevicePoolRequest request)
		{
			if (!await _deviceService.AuthorizeAsync(AclAction.DeviceWrite, User))
			{
				return Forbid();
			}

			string name = request.Name.Trim();

			IDevicePool? pool = await _deviceService.TryCreatePoolAsync(DevicePoolId.Sanitize(name), name, request.PoolType, request.ProjectIds?.Select(x => new ProjectId(x)).ToList());

			if (pool == null)
			{
				return BadRequest($"Unable to create pool for {request.Name}");
			}

			return new CreateDevicePoolResponse(pool.Id.ToString());

		}

		/// <summary>
		/// Update a device pool
		/// </summary>
		[HttpPut]
		[Authorize]
		[Route("/api/v2/devices/pools")]
		public async Task<ActionResult> UpdatePoolAsync([FromBody] UpdateDevicePoolRequest request)
		{

			DevicePoolAuthorization? poolAuth = await _deviceService.GetUserPoolAuthorizationAsync(new DevicePoolId(request.Id), User);

			if (poolAuth == null || !poolAuth.Write)
			{
				return Forbid();
			}

			List<ProjectId>? projectIds = null;

			if (request.ProjectIds != null)
			{
				projectIds = request.ProjectIds.Select(x => new ProjectId(x)).ToList();
			}

			await _deviceService.UpdatePoolAsync(new DevicePoolId(request.Id), projectIds);

			return Ok();
		}

		/// <summary>
		/// Get a list of existing device pools
		/// </summary>
		[HttpGet]
		[Authorize]
		[Route("/api/v2/devices/pools")]
		[ProducesResponseType(typeof(List<GetDevicePoolResponse>), 200)]
		public async Task<ActionResult<List<object>>> GetDevicePoolsAsync()
		{

			List<DevicePoolAuthorization> poolAuth = await _deviceService.GetUserPoolAuthorizationsAsync(User);

			List<IDevicePool> pools = await _deviceService.GetPoolsAsync();

			List<object> responses = new List<object>();

			foreach (IDevicePool pool in pools)
			{
				DevicePoolAuthorization? auth = poolAuth.Where(x => x.Pool.Id == pool.Id).FirstOrDefault();

				if (auth == null || !auth.Read)
				{
					continue;
				}

				responses.Add(new GetDevicePoolResponse(pool.Id.ToString(), pool.Name, pool.PoolType, auth.Write));
			}

			return responses;
		}

		/// <summary>
		/// Get device telemetry
		/// </summary>		
		[HttpGet]
		[Authorize]
		[Route("/api/v2/devices/pools/telemetry")]
		[ProducesResponseType(typeof(List<GetDevicePoolTelemetryResponse>), 200)]
		public async Task<ActionResult<List<GetDevicePoolTelemetryResponse>>> GetDevicePoolTelemetry(
			[FromQuery] DateTimeOffset? minCreateTime = null,
			[FromQuery] DateTimeOffset? maxCreateTime = null,
			[FromQuery] int index = 0,
			[FromQuery] int count = 1024)
		{
			if (!await _deviceService.AuthorizeAsync(AclAction.DeviceRead, User))
			{
				return Forbid();
			}

			if (minCreateTime == null)
			{
				minCreateTime = new DateTimeOffset(DateTime.UtcNow - TimeSpan.FromHours(24));
			}

			List<IDevicePoolTelemetry> telemetryData = await _deviceService.GetDevicePoolTelemetryAsync(minCreateTime, maxCreateTime, index, count);

			List<GetDevicePoolTelemetryResponse> response = new List<GetDevicePoolTelemetryResponse>();
						
			foreach (IDevicePoolTelemetry t in telemetryData)
			{
				Dictionary<string, List<GetDevicePlatformTelemetryResponse>> poolData = new Dictionary<string, List<GetDevicePlatformTelemetryResponse>>();

				foreach ( KeyValuePair<DevicePoolId, IReadOnlyList<IDevicePlatformTelemetry>> pool in t.Pools)
				{
					List<GetDevicePlatformTelemetryResponse> platformTelemetry = new List<GetDevicePlatformTelemetryResponse>();

					foreach (IDevicePlatformTelemetry telemetry in pool.Value)
					{
						IReadOnlyDictionary<string, IReadOnlyList<IDevicePoolReservationTelemetry>>? reserved = telemetry.Reserved?.ToDictionary(kvp => kvp.Key.ToString(), kvp => kvp.Value);
						platformTelemetry.Add(new GetDevicePlatformTelemetryResponse(telemetry.PlatformId.ToString(), telemetry.Available?.Select(d => d.ToString()).ToList(), telemetry.Maintenance?.Select(d => d.ToString()).ToList(), telemetry.Problem?.Select(d => d.ToString()).ToList(), telemetry.Disabled?.Select(d => d.ToString()).ToList(), reserved));
					}

					poolData[pool.Key.ToString()] = platformTelemetry;
				}

				response.Add(new GetDevicePoolTelemetryResponse(t.CreateTimeUtc, poolData));
			}		

			return response;
		}


		// RESERVATIONS

		/// <summary>
		/// Create a new device reservation
		/// </summary>
		[HttpPost]
		[Authorize]
		[Route("/api/v2/devices/reservations")]
		[ProducesResponseType(typeof(CreateDeviceReservationResponse), 200)]
		public async Task<ActionResult<CreateDeviceReservationResponse>> CreateDeviceReservation([FromBody] CreateDeviceReservationRequest request)
		{

			if (!await _deviceService.AuthorizeAsync(AclAction.DeviceWrite, User))
			{
				return Forbid();
			}

			List<IDevicePool> pools = await _deviceService.GetPoolsAsync();
			List<IDevicePlatform> platforms = await _deviceService.GetPlatformsAsync();

			DevicePoolId poolIdValue = new DevicePoolId(request.PoolId);
			IDevicePool? pool = pools.FirstOrDefault(x => x.Id == poolIdValue);
			if (pool == null)
			{
				return BadRequest($"Unknown pool {request.PoolId} on device reservation request");
			}

			List<DeviceRequestData> requestedDevices = new List<DeviceRequestData>();

			foreach (DeviceReservationRequest deviceRequest in request.Devices)
			{
				DevicePlatformId platformIdValue = new DevicePlatformId(deviceRequest.PlatformId);
				IDevicePlatform? platform = platforms.FirstOrDefault(x => x.Id == platformIdValue);

				if (platform == null)
				{
					return BadRequest($"Unknown platform {deviceRequest.PlatformId} on device reservation request");
				}

				if (deviceRequest.IncludeModels != null)
				{
					foreach (string model in deviceRequest.IncludeModels)
					{
						if (platform.Models?.FirstOrDefault(x => x == model) == null)
						{
							return BadRequest($"Unknown model {model} for platform {deviceRequest.PlatformId} on device reservation request");
						}
					}
				}

				if (deviceRequest.ExcludeModels != null)
				{
					foreach (string model in deviceRequest.ExcludeModels)
					{
						if (platform.Models?.FirstOrDefault(x => x == model) == null)
						{
							return BadRequest($"Unknown model {model} for platform {deviceRequest.PlatformId} on device reservation request");
						}
					}
				}

				requestedDevices.Add(new DeviceRequestData(platformIdValue, platformIdValue.ToString(), deviceRequest.IncludeModels, deviceRequest.ExcludeModels));
			}

			IDeviceReservation? reservation = await _deviceService.TryCreateReservationAsync(poolIdValue, requestedDevices);

			if (reservation == null)
			{
				return Conflict("Unable to allocated devices for reservation");
			}

			List<IDevice> devices = await _deviceService.GetDevicesAsync(reservation.Devices);

			CreateDeviceReservationResponse response = new CreateDeviceReservationResponse();

			response.Id = reservation.Id.ToString();

			foreach (IDevice device in devices)
			{
				response.Devices.Add(new GetDeviceResponse(device.Id.ToString(), device.PlatformId.ToString(), device.PoolId.ToString(), device.Name, device.Enabled, device.Address, device.ModelId?.ToString(), device.ModifiedByUser, device.Notes, device.ProblemTimeUtc, device.MaintenanceTimeUtc, device.Utilization));
			}

			return response;
		}

		/// <summary>
		/// Get active device reservations
		/// </summary>		
		[HttpGet]
		[Authorize]
		[Route("/api/v2/devices/reservations")]
		[ProducesResponseType(typeof(List<GetDeviceReservationResponse>), 200)]
		public async Task<ActionResult<List<GetDeviceReservationResponse>>> GetDeviceReservations()
		{
			if (!await _deviceService.AuthorizeAsync(AclAction.DeviceRead, User))
			{
				return Forbid();
			}

			List<IDeviceReservation> reservations = await _deviceService.GetReservationsAsync();

			List<GetDeviceReservationResponse> response = new List<GetDeviceReservationResponse>();

			foreach (IDeviceReservation reservation in reservations)
			{
				GetDeviceReservationResponse reservationResponse = new GetDeviceReservationResponse()
				{
					Id = reservation.Id.ToString(),
					PoolId = reservation.PoolId.ToString(),
					Devices = reservation.Devices.Select(x => x.ToString()).ToList(),
					JobId = reservation.JobId?.ToString(),
					StepId = reservation.StepId?.ToString(),
					JobName = reservation.JobName?.ToString(),
					StepName = reservation.StepName?.ToString(),
					UserId = reservation.UserId?.ToString(),
					Hostname = reservation.Hostname,
					ReservationDetails = reservation.ReservationDetails,
					CreateTimeUtc = reservation.CreateTimeUtc,
					LegacyGuid = reservation.LegacyGuid
				};

				response.Add(reservationResponse);

			}

			return response;
		}

		/// <summary>
		/// Renew an existing reservation
		/// </summary>
		[HttpPut]
		[Authorize]
		[Route("/api/v2/devices/reservations/{reservationId}")]
		public async Task<ActionResult> UpdateReservationAsync(string reservationId)
		{
			if (!await _deviceService.AuthorizeAsync(AclAction.DeviceWrite, User))
			{
				return Forbid();
			}

			ObjectId reservationIdValue = new ObjectId(reservationId);

			bool updated = await _deviceService.TryUpdateReservationAsync(reservationIdValue);

			if (!updated)
			{
				return BadRequest("Failed to update reservation");
			}

			return Ok();
		}

		/// <summary>
		/// Delete a reservation
		/// </summary>
		[HttpDelete]
		[Authorize]
		[Route("/api/v2/devices/reservations/{reservationId}")]
		public async Task<ActionResult> DeleteReservationAsync(string reservationId)
		{
			if (!await _deviceService.AuthorizeAsync(AclAction.DeviceWrite, User))
			{
				return Forbid();
			}

			ObjectId reservationIdValue = new ObjectId(reservationId);

			bool deleted = await _deviceService.DeleteReservationAsync(reservationIdValue);

			if (!deleted)
			{
				return BadRequest("Failed to delete reservation");
			}

			return Ok();
		}

		/// <summary>
		/// Get device telemetry
		/// </summary>		
		[HttpGet]
		[Authorize]
		[Route("/api/v2/devices/telemetry")]
		[ProducesResponseType(typeof(List<GetDeviceTelemetryResponse>), 200)]
		public async Task<ActionResult<List<GetDeviceTelemetryResponse>>> GetDeviceTelemetry(
			[FromQuery(Name = "Id")] string[]? deviceIds = null,
			[FromQuery] string? poolId = null,
			[FromQuery] string? platformId = null,
			[FromQuery] DateTimeOffset? minCreateTime = null,
			[FromQuery] DateTimeOffset? maxCreateTime = null,
			[FromQuery] int index = 0,
			[FromQuery] int count = 1024)
		{
			if (!await _deviceService.AuthorizeAsync(AclAction.DeviceRead, User))
			{
				return Forbid();
			}

			if (minCreateTime == null)
			{
				minCreateTime = new DateTimeOffset(DateTime.UtcNow - TimeSpan.FromHours(24));
			}

			HashSet<DeviceId> deviceIdValues = new HashSet<DeviceId>();

			if (deviceIds != null && deviceIds.Length > 0)
			{
				foreach(string deviceId in deviceIds)
				{
					deviceIdValues.Add(new DeviceId(deviceId));
				}
			}

			DevicePoolId? poolIdValue = poolId == null ? null : new DevicePoolId(poolId);
			DevicePlatformId? platformIdValue = platformId == null ? null : new DevicePlatformId(platformId);

			List<IDevice> devices = await _deviceService.GetDevicesAsync(deviceIdValues.Count > 0 ? deviceIdValues.ToList() : null, poolIdValue, platformIdValue);

			List<GetDeviceTelemetryResponse> response = new List<GetDeviceTelemetryResponse>();

			if (devices.Count == 0)
			{
				return response;
			}
			
			List<IDeviceTelemetry> telemetryData = await _deviceService.GetDeviceTelemetryAsync(devices.Select(x => x.Id).ToArray(), minCreateTime, maxCreateTime, index, count);

			Dictionary<string, List<GetTelemetryInfoResponse>> results = new Dictionary<string, List<GetTelemetryInfoResponse>>();

			foreach (IDeviceTelemetry t in telemetryData)
			{
				List<GetTelemetryInfoResponse>? info = null;
				string deviceId = t.DeviceId.ToString();
				if (!results.TryGetValue(deviceId, out info))
				{
					info = new List<GetTelemetryInfoResponse>();
					results[deviceId] = info;
				}

				info.Add(new GetTelemetryInfoResponse(t));
			}

			foreach(KeyValuePair<string, List<GetTelemetryInfoResponse >> result in results)
			{
				GetDeviceTelemetryResponse deviceTelemetry = new GetDeviceTelemetryResponse(result.Key, result.Value);
				response.Add(deviceTelemetry);
			}

			return response;
		}

		// LEGACY V1 API

		enum LegacyPerfSpec
		{
			Unspecified,
			Minimum,
			Recommended,
			High
		};

		/// <summary>
		/// Create a device reservation
		/// </summary>
		[HttpPost]
		[Route("/api/v1/reservations")]
		[ProducesResponseType(typeof(GetLegacyReservationResponse), 200)]
		public async Task<ActionResult<GetLegacyReservationResponse>> CreateDeviceReservationV1Async([FromBody] LegacyCreateReservationRequest request)
		{

			List<IDevicePool> pools = await _deviceService.GetPoolsAsync();
			List<IDevicePlatform> platforms = await _deviceService.GetPlatformsAsync();

			string? poolId = request.PoolId;

			// @todo: Remove this once all streams are updated to provide jobid
			string details = "";
			if ((String.IsNullOrEmpty(request.JobId) || String.IsNullOrEmpty(request.StepId)))
			{
				if (!String.IsNullOrEmpty(request.ReservationDetails))
				{
					details = $" - {request.ReservationDetails}";
				}
				else
				{
					details = $" - Host {request.Hostname}";
				}
			}

			if (String.IsNullOrEmpty(poolId))
			{
				_logger.LogError("No pool specified, defaulting to UE4 {Details} JobId: {JobId}, StepId: {StepId}", details, request.JobId, request.StepId);
				string message = $"No pool specified, defaulting to UE4" + details;
				await _deviceService.NotifyDeviceServiceAsync(message, null, request.JobId, request.StepId);
				poolId = "ue4";
				//return BadRequest(Message);
			}

			DevicePoolId poolIdValue = DevicePoolId.Sanitize(poolId);
			IDevicePool? pool = pools.FirstOrDefault(x => x.Id == poolIdValue);
			if (pool == null)
			{
				_logger.LogError("Unknown pool {PoolId} {Details}", poolId, details);
				string message = $"Unknown pool {poolId} " + details;
				await _deviceService.NotifyDeviceServiceAsync(message, null, request.JobId, request.StepId);
				return BadRequest(message);
			}

			List<DeviceRequestData> requestedDevices = new List<DeviceRequestData>();

			HashSet<string> legacyPerfSpecs = new HashSet<string> { "Unspecified", "Minimum", "Recommended", "High" };

			List<string> perfSpecs = new List<string>();

			foreach (string deviceType in request.DeviceTypes)
			{

				string platformName = deviceType;
				string? constraint = null;

				if (deviceType.Contains(':', StringComparison.Ordinal))
				{
					string[] tokens = deviceType.Split(":");
					platformName = tokens[0];
					constraint = tokens[1];
				}

				DevicePlatformId platformId;

				DevicePlatformMapV1 mapV1 = await _deviceService.GetPlatformMapV1();

				if (!mapV1.PlatformMap.TryGetValue(platformName, out platformId))
				{
					string message = $"Unknown platform {platformName}" + details;
					_logger.LogError("Unknown platform {PlatformName} {Details}", platformName, details);
					await _deviceService.NotifyDeviceServiceAsync(message, null, request.JobId, request.StepId);
					return BadRequest(message);
				}

				IDevicePlatform? platform = platforms.FirstOrDefault(x => x.Id == platformId);

				if (platform == null)
				{
					string message = $"Unknown platform {platformId}" + details;
					_logger.LogError("Unknown platform {PlatformId} {Details}", platformId, details);
					await _deviceService.NotifyDeviceServiceAsync(message, null, request.JobId, request.StepId);
					return BadRequest(message);
				}

				List<string> includeModels = new List<string>();
				List<string> excludeModels = new List<string>();

				if (constraint == null)
				{
					perfSpecs.Add("Unspecified");
				}
				else if (legacyPerfSpecs.Contains(constraint))
				{
					perfSpecs.Add(constraint);

					if (constraint == "High")
					{
						string? model = null;
						if (mapV1.PerfSpecHighMap.TryGetValue(platformId, out model))
						{
							includeModels.Add(model);
						}
					}
					else if (constraint == "Minimum" || constraint == "Recommended")
					{
						string? model = null;
						if (mapV1.PerfSpecHighMap.TryGetValue(platformId, out model))
						{
							excludeModels.Add(model);
						}
					}
				}
				else
				{
					string? model = platform.Models?.FirstOrDefault(x => x == constraint);
					if (model == null)
					{
						return NotFound($"Platform {platform.Id} has no model {model}");
					}

					string modelPerfSpec = "Minimum";
					string? specModel = null;

					if (mapV1.PerfSpecHighMap.TryGetValue(platform.Id, out specModel))
					{
						if (model == specModel)
						{
							modelPerfSpec = "High";
						}
					}

					perfSpecs.Add(modelPerfSpec);

					includeModels.Add(constraint);
				}

				requestedDevices.Add(new DeviceRequestData(platformId, platformName, includeModels, excludeModels));
			}

			IDeviceReservation? reservation = await _deviceService.TryCreateReservationAsync(poolIdValue, requestedDevices, request.Hostname, request.ReservationDetails, request.JobId, request.StepId);

			if (reservation == null)
			{
				return Conflict("Unable to allocated devices for reservation");
			}

			List<IDevice> devices = await _deviceService.GetDevicesAsync(reservation.Devices);

			GetLegacyReservationResponse response = new GetLegacyReservationResponse();

			response.Guid = reservation.LegacyGuid;
			response.DeviceNames = devices.Select(x => x.Name).ToArray();
			response.DevicePerfSpecs = perfSpecs.ToArray();
			response.DeviceModels = devices.Select(x => x.ModelId ?? "Base").ToArray();
			response.HostName = request.Hostname;
			response.StartDateTime = reservation.CreateTimeUtc.ToString("O", System.Globalization.CultureInfo.InvariantCulture);
			response.Duration = $"{request.Duration}";
			response.JobId = reservation.JobId;
			response.StepId = reservation.StepId;
			response.JobName = reservation.JobName;
			response.StepName = reservation.StepName;

			return new JsonResult(response, new JsonSerializerOptions() { PropertyNamingPolicy = null });

		}

		/// <summary>
		/// Renew a reservation
		/// </summary>
		[HttpPut]
		[Route("/api/v1/reservations/{reservationGuid}")]
		[ProducesResponseType(typeof(GetLegacyReservationResponse), 200)]
		public async Task<ActionResult<GetLegacyReservationResponse>> UpdateReservationV1Async(string reservationGuid /* [FromBody] string Duration */)
		{
			IDeviceReservation? reservation = await _deviceService.TryGetReservationFromLegacyGuidAsync(reservationGuid);

			if (reservation == null)
			{
				_logger.LogError("Unable to find reservation for legacy guid {ReservationGuid}", reservationGuid);
				return BadRequest();
			}

			bool updated = await _deviceService.TryUpdateReservationAsync(reservation.Id);

			if (!updated)
			{
				_logger.LogError("Unable to find reservation for reservation {ReservationId}", reservation.Id);
				return BadRequest();
			}

			List<IDevice> devices = await _deviceService.GetDevicesAsync(reservation.Devices);

			GetLegacyReservationResponse response = new GetLegacyReservationResponse();

			response.Guid = reservation.LegacyGuid;
			response.DeviceNames = reservation.Devices.Select(deviceId => devices.First(d => d.Id == deviceId).Name).ToArray();
			response.HostName = reservation.Hostname ?? "";
			response.StartDateTime = reservation.CreateTimeUtc.ToString("O", System.Globalization.CultureInfo.InvariantCulture);
			response.Duration = "00:10:00"; // matches gauntlet duration

			return new JsonResult(response, new JsonSerializerOptions() { PropertyNamingPolicy = null });
		}

		/// <summary>
		/// Delete a reservation
		/// </summary>
		[HttpDelete]
		[Route("/api/v1/reservations/{reservationGuid}")]
		public async Task<ActionResult> DeleteReservationV1Async(string reservationGuid)
		{
			IDeviceReservation? reservation = await _deviceService.TryGetReservationFromLegacyGuidAsync(reservationGuid);

			if (reservation == null)
			{
				return BadRequest($"Unable to find reservation for guid {reservationGuid}");
			}

			bool deleted = await _deviceService.DeleteReservationAsync(reservation.Id);

			if (!deleted)
			{
				return BadRequest("Failed to delete reservation");
			}

			return Ok();
		}

		/// <summary>
		/// Get device info for a reserved device
		/// </summary>
		[HttpGet]
		[Route("/api/v1/devices/{deviceName}")]
		[ProducesResponseType(typeof(GetLegacyDeviceResponse), 200)]
		public async Task<ActionResult<GetLegacyDeviceResponse>> GetDeviceV1Async(string deviceName)
		{
			IDevice? device = await _deviceService.GetDeviceByNameAsync(deviceName);

			if (device == null)
			{
				return BadRequest($"Unknown device {deviceName}");
			}

			IDeviceReservation? reservation = await _deviceService.TryGetDeviceReservation(device.Id);

			string? platformName = null;

			if (reservation != null && reservation.Devices.Count == reservation.RequestedDevicePlatforms.Count)
			{
				for (int i = 0; i < reservation.Devices.Count; i++)
				{
					if (reservation.Devices[i] == device.Id)
					{
						platformName = reservation.RequestedDevicePlatforms[i];
					}
				}
			}

			DevicePlatformMapV1 mapV1 = await _deviceService.GetPlatformMapV1();

			if (String.IsNullOrEmpty(platformName))
			{
				if (!mapV1.PlatformReverseMap.TryGetValue(device.PlatformId, out platformName))
				{
					return BadRequest($"Unable to map platform for {deviceName} : {device.PlatformId}");
				}
			}

			if (String.IsNullOrEmpty(platformName))
			{
				return BadRequest($"Unable to get platform for {deviceName} from reservation or mapping : {device.PlatformId}");
			}

			GetLegacyDeviceResponse response = new GetLegacyDeviceResponse();

			response.Id = device.Id.ToString();
			response.Name = device.Name;
			response.Type = platformName;
			response.IPOrHostName = device.Address ?? "";
			response.AvailableStartTime = "00:00:00";
			response.AvailableEndTime = "00:00:00";
			response.Enabled = true;
			response.Model = device.ModelId ?? "Base";
			response.DeviceData = "";
			response.PerfSpec = "Minimum";

			if (device.ModelId != null)
			{
				string? model;
				if (mapV1.PerfSpecHighMap.TryGetValue(device.PlatformId, out model))
				{
					if (model == device.ModelId)
					{
						response.PerfSpec = "High";
					}
				}
			}

			return new JsonResult(response, new JsonSerializerOptions() { PropertyNamingPolicy = null });
		}

		/// <summary>
		/// Mark a problem device
		/// </summary>
		[HttpPut]
		[Route("/api/v1/deviceerror/{deviceName}")]
		public async Task<ActionResult> PutDeviceErrorAsync(string deviceName)
		{
			IDevice? device = await _deviceService.GetDeviceByNameAsync(deviceName);

			if (device == null)
			{
				_logger.LogError("Device error reported for unknown device {DeviceName}", deviceName);
				return BadRequest($"Unknown device {deviceName}");
			}

			await _deviceService.UpdateDeviceAsync(device.Id, newProblem: true);

			string message = $"Device problem, {device.Name} : {device.PoolId.ToString().ToUpperInvariant()}";

			IDeviceReservation? reservation = await _deviceService.TryGetDeviceReservation(device.Id);

			string? jobId = null;
			string? stepId = null;
			if (reservation != null)
			{
				jobId = !String.IsNullOrEmpty(reservation.JobId) ? reservation.JobId : null;
				stepId = !String.IsNullOrEmpty(reservation.StepId) ? reservation.StepId : null;

				if ((jobId == null || stepId == null))
				{
					if (!String.IsNullOrEmpty(reservation.ReservationDetails))
					{
						message += $" - {reservation.ReservationDetails}";
					}
					else
					{
						message += $" - Host {reservation.Hostname}";
					}
				}
			}

			_logger.LogError("{DeviceError}", message);

			await _deviceService.NotifyDeviceServiceAsync(message, device.Id, jobId, stepId);

			return Ok();

		}

		/// <summary>
		/// Updates the platform map for v1 requests
		/// </summary>
		[HttpPut]
		[Route("/api/v1/devices/platformmap")]
		public async Task<ActionResult> UpdatePlatformMapV1([FromBody] UpdatePlatformMapRequest request)
		{
			if (!await _aclService.AuthorizeAsync(AclAction.AdminWrite, User))
			{
				return Forbid();
			}

			bool result;
			try
			{
				result = await _deviceService.UpdatePlatformMapAsync(request);
			}
			catch (Exception ex)
			{
				_logger.LogError(ex, "Error updating device platform map {Message}", ex.Message);
				throw;
			}

			if (!result)
			{
				_logger.LogError("Unable to update device platform mapping");
				return BadRequest();
			}

			return Ok();
		}
	}
}
