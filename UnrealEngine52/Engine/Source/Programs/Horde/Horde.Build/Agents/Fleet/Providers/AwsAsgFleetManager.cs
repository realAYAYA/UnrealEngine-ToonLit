// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Net;
using System.Text.Json.Serialization;
using System.Threading;
using System.Threading.Tasks;
using Amazon.AutoScaling;
using Amazon.AutoScaling.Model;
using Horde.Build.Agents.Pools;
using Horde.Build.Utilities;
using Microsoft.Extensions.Logging;
using OpenTracing;
using OpenTracing.Util;

namespace Horde.Build.Agents.Fleet.Providers
{
	/// <summary>
	/// Settings for controlling an EC2 auto-scaling group (ASG)
	/// </summary>
	public class AwsAsgSettings
	{
		/// <summary>
		/// Name of the auto-scaling group
		/// </summary>
		public string Name { get; }

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="name">Name</param>
		[JsonConstructor]
		public AwsAsgSettings(string? name)
		{
			// The JSON deserializer can call the constructor with null parameters
			Name = name ?? throw new ArgumentException($"Parameter {nameof(name)} is null");
		}
	}
	
	/// <summary>
	/// Fleet manager for handling AWS EC2 instances
	/// Uses an EC2 auto-scaling group for controlling the number of running instances.
	/// </summary>
	public sealed class AwsAsgFleetManager : IFleetManager
	{
		internal AwsAsgSettings Settings { get; }
		private readonly IAmazonAutoScaling _awsAutoScaling;
		private readonly ILogger _logger;

		/// <summary>
		/// Constructor
		/// </summary>
		public AwsAsgFleetManager(IAmazonAutoScaling awsAutoScaling, AwsAsgSettings asgSettings, ILogger<AwsAsgFleetManager> logger)
		{
			_awsAutoScaling = awsAutoScaling;
			Settings = asgSettings;
			_logger = logger;
		}

		/// <inheritdoc/>
		public async Task ExpandPoolAsync(IPool pool, IReadOnlyList<IAgent> agents, int count, CancellationToken cancellationToken)
		{
			int desiredCapacity = agents.Count + count;
			
			using IScope scope = GlobalTracer.Instance.BuildSpan("AwsAsgFleetManager.ExpandPool").StartActive();
			scope.Span.SetTag("poolName", pool.Name);
			scope.Span.SetTag("numAgents", agents.Count);
			scope.Span.SetTag("count", count);
			scope.Span.SetTag("desiredCapacity", desiredCapacity);
			
			await UpdateAsg(pool.Id, desiredCapacity, scope, cancellationToken);
		}

		/// <inheritdoc/>
		public async Task ShrinkPoolAsync(IPool pool, IReadOnlyList<IAgent> agents, int count, CancellationToken cancellationToken)
		{
			int desiredCapacity = Math.Max(0, agents.Count - count);
			
			using IScope scope = GlobalTracer.Instance.BuildSpan("AwsAsgFleetManager.ShrinkPool").StartActive();
			scope.Span.SetTag("poolName", pool.Name);
			scope.Span.SetTag("numAgents", agents.Count);
			scope.Span.SetTag("count", count);
			scope.Span.SetTag("desiredCapacity", desiredCapacity);
			
			await UpdateAsg(pool.Id, desiredCapacity, scope, cancellationToken);
		}
		
		private async Task UpdateAsg(StringId<IPool> poolId, int desiredCapacity, IScope scope, CancellationToken cancellationToken)
		{
			UpdateAutoScalingGroupRequest request = new () { AutoScalingGroupName = Settings.Name, DesiredCapacity = desiredCapacity };
			UpdateAutoScalingGroupResponse response = await _awsAutoScaling.UpdateAutoScalingGroupAsync(request, cancellationToken);
			
			scope.Span.SetTag("res.statusCode", (int)response.HttpStatusCode);
			if (response.HttpStatusCode != HttpStatusCode.OK)
			{
				_logger.LogError("Unable to update auto-scaling group {AutoScalingGroup} for pool {PoolId}.", Settings.Name, poolId);
			}
		}

		/// <inheritdoc/>
		public Task<int> GetNumStoppedInstancesAsync(IPool pool, CancellationToken cancellationToken)
		{
			return Task.FromResult(0);
		}
	}
}
