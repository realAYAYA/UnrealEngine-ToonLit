// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Linq;
using System.Threading.Tasks;
using Horde.Server.Acls;
using Horde.Server.Agents.Fleet;
using Horde.Server.Server;
using Horde.Server.Utilities;
using Microsoft.AspNetCore.Authorization;
using Microsoft.AspNetCore.Mvc;
using Microsoft.Extensions.Options;

namespace Horde.Server.Agents.Pools
{
	/// <summary>
	/// Controller for the /api/v1/pools endpoint
	/// </summary>
	[ApiController]
	[Authorize]
	[Route("[controller]")]
	public class PoolsController : HordeControllerBase
	{
		private readonly PoolService _poolService;
		private readonly IOptionsSnapshot<GlobalConfig> _globalConfig;

		/// <summary>
		/// Constructor
		/// </summary>
		public PoolsController(PoolService poolService, IOptionsSnapshot<GlobalConfig> globalConfig)
		{
			_poolService = poolService;
			_globalConfig = globalConfig;
		}

		/// <summary>
		/// Creates a new pool
		/// </summary>
		/// <param name="create">Parameters for the new pool.</param>
		/// <returns>Http result code</returns>
		[HttpPost]
		[Route("/api/v1/pools")]
		public async Task<ActionResult<CreatePoolResponse>> CreatePoolAsync([FromBody] CreatePoolRequest create)
		{
			if(!_globalConfig.Value.Authorize(PoolAclAction.CreatePool, User))
			{
				return Forbid(PoolAclAction.CreatePool);
			}

			List<PoolSizeStrategyInfo>? sizeStrategies = create.SizeStrategies?.Select(x => x.Convert()).ToList();
			List<FleetManagerInfo>? fleetManagers = create.FleetManagers?.Select(x => x.Convert()).ToList();
			LeaseUtilizationSettings? luSettings = create.LeaseUtilizationSettings?.Convert();
			JobQueueSettings? jqSettings = create.JobQueueSettings?.Convert();
			ComputeQueueAwsMetricSettings? cqamSettings = create.ComputeQueueAwsMetricSettings?.Convert();

			TimeSpan? conformInterval = create.ConformInterval == null ? null : TimeSpan.FromHours(create.ConformInterval.Value);
			TimeSpan? scaleOutCooldown = create.ScaleOutCooldown == null ? null : TimeSpan.FromSeconds(create.ScaleOutCooldown.Value);
			TimeSpan? scaleInCooldown = create.ScaleInCooldown == null ? null : TimeSpan.FromSeconds(create.ScaleInCooldown.Value);

			AddPoolOptions options = new AddPoolOptions();
			options.Condition = create.Condition;
			options.EnableAutoscaling = create.EnableAutoscaling;
			options.MinAgents = create.MinAgents;
			options.NumReserveAgents = create.NumReserveAgents; 
			options.ConformInterval = conformInterval;
			options.ScaleOutCooldown = scaleOutCooldown;
			options.ScaleInCooldown = scaleInCooldown; 
			options.SizeStrategies = sizeStrategies;
			options.FleetManagers = fleetManagers; 
			options.SizeStrategy = create.SizeStrategy;
			options.LeaseUtilizationSettings = luSettings; 
			options.JobQueueSettings = jqSettings; 
			options.ComputeQueueAwsMetricSettings = cqamSettings;
			options.Properties = create.Properties;

			IPool newPool = await _poolService.CreatePoolAsync(create.Name, options);
			return new CreatePoolResponse(newPool.Id.ToString());
		}

		/// <summary>
		/// Query all the pools
		/// </summary>
		/// <param name="filter">Filter for the properties to return</param>
		/// <returns>Information about all the pools</returns>
		[HttpGet]
		[Route("/api/v1/pools")]
		[ProducesResponseType(typeof(List<GetPoolResponse>), 200)]
		public async Task<ActionResult<List<object>>> GetPoolsAsync([FromQuery] PropertyFilter? filter = null)
		{
			if (!_globalConfig.Value.Authorize(PoolAclAction.ListPools, User))
			{
				return Forbid(PoolAclAction.ListPools);
			}

			List<IPool> pools = await _poolService.GetPoolsAsync();

			List<object> responses = new List<object>();
			foreach (IPool pool in pools)
			{
				responses.Add(new GetPoolResponse(pool).ApplyFilter(filter));
			}
			return responses;
		}

		/// <summary>
		/// Retrieve information about a specific pool
		/// </summary>
		/// <param name="poolId">Id of the pool to get information about</param>
		/// <param name="filter">Filter to apply to the returned properties</param>
		/// <returns>Information about the requested pool</returns>
		[HttpGet]
		[Route("/api/v1/pools/{poolId}")]
		[ProducesResponseType(typeof(GetPoolResponse), 200)]
		public async Task<ActionResult<object>> GetPoolAsync(string poolId, [FromQuery] PropertyFilter? filter = null)
		{
			if (!_globalConfig.Value.Authorize(PoolAclAction.ViewPool, User))
			{
				return Forbid(PoolAclAction.ViewPool);
			}

			PoolId poolIdValue = new PoolId(poolId);

			IPool? pool = await _poolService.GetPoolAsync(poolIdValue);
			if (pool == null)
			{
				return NotFound(poolIdValue);
			}

			return new GetPoolResponse(pool).ApplyFilter(filter);
		}

		/// <summary>
		/// Update a pool's properties.
		/// </summary>
		/// <param name="poolId">Id of the pool to update</param>
		/// <param name="update">Items on the pool to update</param>
		/// <returns>Http result code</returns>
		[HttpPut]
		[Route("/api/v1/pools/{poolId}")]
		public async Task<ActionResult> UpdatePoolAsync(string poolId, [FromBody] UpdatePoolRequest update)
		{
			if (!_globalConfig.Value.Authorize(PoolAclAction.UpdatePool, User))
			{
				return Forbid(PoolAclAction.UpdatePool);
			}

			PoolId poolIdValue = new PoolId(poolId);
			
			IPool? pool = await _poolService.GetPoolAsync(poolIdValue);
			if(pool == null)
			{
				return NotFound(poolIdValue);
			}

			List<PoolSizeStrategyInfo>? newSizeStrategies = update.SizeStrategies?.Select(x => x.Convert()).ToList();
			List<FleetManagerInfo>? newFleetManagers = update.FleetManagers?.Select(x => x.Convert()).ToList();
			TimeSpan? conformInterval = update.ConformInterval == null ? null : TimeSpan.FromHours(update.ConformInterval.Value);
			TimeSpan? scaleOutCooldown = update.ScaleOutCooldown == null ? null : TimeSpan.FromSeconds(update.ScaleOutCooldown.Value);
			TimeSpan? scaleInCooldown = update.ScaleInCooldown == null ? null : TimeSpan.FromSeconds(update.ScaleInCooldown.Value);

			UpdatePoolOptions options = new UpdatePoolOptions();
			options.Name = update.Name;
			options.Condition = update.Condition;
			options.EnableAutoscaling = update.EnableAutoscaling;
			options.MinAgents = update.MinAgents;
			options.NumReserveAgents = update.NumReserveAgents;
			options.Properties = update.Properties;
			options.ConformInterval = conformInterval;
			options.ScaleOutCooldown = scaleOutCooldown;
			options.ScaleInCooldown = scaleInCooldown;
			options.SizeStrategy = update.SizeStrategy;
			options.SizeStrategies = newSizeStrategies;
			options.FleetManagers = newFleetManagers;
			options.LeaseUtilizationSettings = update.LeaseUtilizationSettings?.Convert();
			options.JobQueueSettings = update.JobQueueSettings?.Convert();
			options.ComputeQueueAwsMetricSettings = update.ComputeQueueAwsMetricSettings?.Convert();
			options.UseDefaultStrategy = update.UseDefaultStrategy;

			await _poolService.UpdatePoolAsync(pool, options);
			return new OkResult();
		}

		/// <summary>
		/// Delete a pool
		/// </summary>
		/// <param name="poolId">Id of the pool to delete</param>
		/// <returns>Http result code</returns>
		[HttpDelete]
		[Route("/api/v1/pools/{poolId}")]
		public async Task<ActionResult> DeletePoolAsync(string poolId)
		{
			if (!_globalConfig.Value.Authorize(PoolAclAction.DeletePool, User))
			{
				return Forbid(PoolAclAction.DeletePool);
			}

			PoolId poolIdValue = new PoolId(poolId);
			if(!await _poolService.DeletePoolAsync(poolIdValue))
			{
				return NotFound(poolIdValue);
			}
			return Ok();
		}

		/// <summary>
		/// Batch update pool properties
		/// </summary>
		/// <param name="batchUpdates">List of pools to update</param>
		/// <returns>Http result code</returns>
		[HttpPut]
		[Route("/api/v1/pools")]
		public async Task<ActionResult> UpdatePoolAsync([FromBody] List<BatchUpdatePoolRequest> batchUpdates)
		{
			if (!_globalConfig.Value.Authorize(PoolAclAction.UpdatePool, User))
			{
				return Forbid(PoolAclAction.UpdatePool);
			}

			foreach (BatchUpdatePoolRequest update in batchUpdates)
			{
				PoolId poolIdValue = new PoolId(update.Id);

				IPool? pool = await _poolService.GetPoolAsync(poolIdValue);
				if (pool == null)
				{
					return NotFound(poolIdValue);
				}

				await _poolService.UpdatePoolAsync(pool, new UpdatePoolOptions { Name = update.Name, Properties = update.Properties });
			}
			return Ok();
		}
	}
}
