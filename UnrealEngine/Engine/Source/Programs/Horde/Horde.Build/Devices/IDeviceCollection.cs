// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Threading.Tasks;
using Horde.Build.Projects;
using Horde.Build.Users;
using Horde.Build.Utilities;
using MongoDB.Bson;

namespace Horde.Build.Devices
{
	using DeviceId = StringId<IDevice>;
	using DevicePlatformId = StringId<IDevicePlatform>;
	using DevicePoolId = StringId<IDevicePool>;
	using ProjectId = StringId<IProject>;
	using UserId = ObjectId<IUser>;

	/// <summary>
	/// Device reservation request data
	/// </summary>
	public class DeviceRequestData
	{
		/// <summary>
		/// The platform of the device to reserve
		/// </summary>
		public DevicePlatformId PlatformId { get; set; }

		/// <summary>
		/// The platform client requested, which can differ from the devices platformid due to mapping
		/// </summary>
		public string RequestedPlatform { get; set; }

		/// <summary>
		/// Models to include for this request
		/// </summary>
		public List<string> IncludeModels { get; set; }

		/// <summary>
		/// Models to exclude for this request
		/// </summary>
		public List<string> ExcludeModels { get; set; }

		/// <summary>
		/// Constructor
		/// </summary>
		public DeviceRequestData(DevicePlatformId platformId, string requestedPlatform, List<string>? includeModels = null, List<string>? excludeModels = null)
		{
			PlatformId = platformId;
			IncludeModels = includeModels ?? new List<string>();
			ExcludeModels = excludeModels ?? new List<string>();
			RequestedPlatform = requestedPlatform;
		}
	}

	/// <summary>
	/// A collection for device management
	/// </summary>
	public interface IDeviceCollection
	{
		// PLATFORMS

		/// <summary>
		/// Add a platform 
		/// </summary>
		/// <param name="id">The id of the platform</param>
		/// <param name="name">The friendly name of the platform</param>
		Task<IDevicePlatform?> TryAddPlatformAsync(DevicePlatformId id, string name);

		/// <summary>
		/// Get a list of all available device platforms
		/// </summary>		
		Task<List<IDevicePlatform>> FindAllPlatformsAsync();

		/// <summary>
		/// Get a specific platform by id
		/// </summary>
		Task<IDevicePlatform?> GetPlatformAsync(DevicePlatformId platformId);

		/// <summary>
		/// Update a device platform
		/// </summary>
		/// <param name="platformId">The id of the device</param>
		/// <param name="modelIds">The available model ids for the platform</param>
		Task<bool> UpdatePlatformAsync(DevicePlatformId platformId, string[]? modelIds);

		// POOLS

		/// <summary>
		/// Add a new device pool to the collection
		/// </summary>
		/// <param name="id">The id of the new pool</param>
		/// <param name="name">The friendly name of the new pool</param>
		/// <param name="poolType">The pool type</param>
		/// <param name="projectIds">Projects associated with this pool</param>
		Task<IDevicePool?> TryAddPoolAsync(DevicePoolId id, string name, DevicePoolType poolType, List<ProjectId>? projectIds );

		/// <summary>
		/// Update a device pool
		/// </summary>
		/// <param name="id">The id of the device pool to update</param>
		/// <param name="projectIds">Associated project ids</param>
		Task UpdatePoolAsync(DevicePoolId id, List<ProjectId>? projectIds);

		/// <summary>
		/// Get a pool by id
		/// </summary>
		Task<IDevicePool?> GetPoolAsync(DevicePoolId poolId);

		/// <summary>
		/// Gets a list of existing device pools
		/// </summary>		
		Task<List<IDevicePool>> FindAllPoolsAsync();

		// DEVICES

		/// <summary>
		/// Get a device by id
		/// </summary>
		Task<IDevice?> GetDeviceAsync(DeviceId deviceId);

		/// <summary>
		/// Get a device by name
		/// </summary>
		Task<IDevice?> GetDeviceByNameAsync(string deviceName);

		/// <summary>
		/// Get a list of all devices
		/// </summary>
		/// <param name="deviceIds">Optional list of device ids to get</param>
		/// <param name="poolId">Optional id of pool for devices</param>
		/// <param name="platformId">Optional id of platform for devices</param>
		Task<List<IDevice>> FindAllDevicesAsync(List<DeviceId>? deviceIds, DevicePoolId? poolId = null, DevicePlatformId? platformId = null);

		/// <summary>
		/// Adds a new device
		/// </summary>
		/// <param name="id">The device id</param>
		/// <param name="name">The name of the device (unique)</param>
		/// <param name="platformId">The platform of the device</param>
		/// <param name="poolId">Which pool to add it to</param>
		/// <param name="enabled">Whather the device is enabled by default</param>
		/// <param name="address">The network address or hostname the device can be reached at</param>
		/// <param name="modelId">The vendor model id of the device</param>
        /// <param name="userId">The user adding the device</param>
		Task<IDevice?> TryAddDeviceAsync(DeviceId id, string name, DevicePlatformId platformId, DevicePoolId poolId, bool? enabled, string? address, string? modelId, UserId? userId);

		/// <summary>
		/// Update a device
		/// </summary>
		/// <param name="deviceId">The id of the device to update</param>
		/// <param name="newPoolId">The new pool to assign</param>
		/// <param name="newName">The new name to assign</param>		
		/// <param name="newAddress">The new device address or hostname</param>
		/// <param name="newModelId">The new model id</param>
		/// <param name="newNotes">The devices markdown notes</param>
		/// <param name="newEnabled">Whether the device is enabled or not</param>
		/// <param name="newProblem">Whether to set or clear problem state</param>
		/// <param name="newMaintenance">Whether to set or clear maintenance state</param>
        /// <param name="modifiedByUserId">The user who is updating the device</param>
		Task UpdateDeviceAsync(DeviceId deviceId, DevicePoolId? newPoolId, string? newName, string? newAddress, string? newModelId, string? newNotes, bool? newEnabled, bool? newProblem, bool? newMaintenance, UserId? modifiedByUserId = null);

		/// <summary>
		/// Delete a device from the collection
		/// </summary>
		/// <param name="deviceId">The id of the device to delete</param>
		Task<bool> DeleteDeviceAsync(DeviceId deviceId);

		/// <summary>
        /// Checkout or checkin the specified device
        /// </summary>
        /// <param name="deviceId"></param>
        /// <param name="checkedOutByUserId"></param>
        /// <returns></returns>
        Task CheckoutDeviceAsync(DeviceId deviceId, UserId? checkedOutByUserId);

		// RESERVATIONS

		/// <summary>
		/// Create a new reseveration in the pool with the specified devices
		/// </summary>
		/// <param name="poolId">The pool of devices to use for the new reservation</param>
		/// <param name="request">The requested devices for the reservation</param>
		/// <param name="hostname">The hostname of the machine making the reservation</param>
		/// <param name="reservationDetails">The details of the reservation</param>
		/// <param name="streamId">The Stream Id associated with the job</param>
		/// <param name="jobId">The Job Id associated with the job</param>
		/// <param name="stepId">The Step Id associated with the job</param>
		/// <param name="jobName">The Job name associated with the job</param>
		/// <param name="stepName">The Step name associated with the job</param>		
		Task<IDeviceReservation?> TryAddReservationAsync(DevicePoolId poolId, List<DeviceRequestData> request, string? hostname, string? reservationDetails, string? streamId, string? jobId, string? stepId, string? jobName, string? stepName);

		/// <summary>
		/// Gets a reservation by guid for legacy clients
		/// </summary>
		/// <param name="legacyGuid">YThe legacy guid of the reservation</param>
		Task<IDeviceReservation?> TryGetReservationFromLegacyGuidAsync(string legacyGuid);

        /// <summary>
        /// Gets a reservation by device id
        /// </summary>
        /// <param name="id">A device contained in reservation</param>
        Task<IDeviceReservation?> TryGetDeviceReservationAsync(DeviceId id);

        /// <summary>
        /// Get a list of all reservations
        /// </summary>
        Task<List<IDeviceReservation>> FindAllReservationsAsync();

		/// <summary>
		/// Updates a reservation to the current time, for expiration
		/// </summary>
		/// <param name="id">The id of the reservation to update</param>
		public Task<bool> TryUpdateReservationAsync(ObjectId id);

		/// <summary>
		/// Deletes a reservation and releases reserved devices
		/// </summary>
		/// <param name="id">The id of the reservation to delete</param>
		public Task<bool> DeleteReservationAsync(ObjectId id);

		/// <summary>
		/// Deletes expired reservations
		/// </summary>
		public Task<bool> ExpireReservationsAsync();

		/// <summary>
		/// Deletes user device checkouts
		/// </summary>
		public Task<List<(UserId, IDevice)>?> ExpireCheckedOutAsync();

		/// <summary>
		/// Gets a list of users to notify whose device checkout is about to expire
		/// </summary>
		public Task<List<(UserId, IDevice)>?> ExpireNotificatonsAsync();

		/// <summary>
		/// Find device telemetry
		/// </summary>
		/// <param name="deviceIds"></param>
		/// <param name="minCreateTime"></param>
		/// <param name="maxCreateTime"></param>
		/// <param name="index"></param>
		/// <param name="count"></param>
		/// <returns></returns>
		public Task<List<IDeviceTelemetry>> FindDeviceTelemetryAsync(DeviceId[]? deviceIds = null, DateTimeOffset? minCreateTime = null, DateTimeOffset? maxCreateTime = null, int? index = null, int? count = null);

		/// <summary>
		/// Creates a device pool telemetry snapshot
		/// </summary>
		/// <returns></returns>
		public Task CreatePoolTelemetrySnapshot();

		/// <summary>
		/// Gets pool telemetry for an optional date range
		/// </summary>
		/// <param name="minCreateTime"></param>
		/// <param name="maxCreateTime"></param>
		/// <param name="index"></param>
		/// <param name="count"></param>
		/// <returns></returns>
		public Task<List<IDevicePoolTelemetry>> FindPoolTelemetryAsync(DateTimeOffset? minCreateTime = null, DateTimeOffset? maxCreateTime = null, int? index = null, int? count = null);


	}
}