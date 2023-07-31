// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Linq;
using System.Threading;
using System.Threading.Tasks;
using Amazon;
using Amazon.EC2;
using Amazon.EC2.Model;
using Horde.Build.Agents.Pools;
using Horde.Build.Auditing;
using Microsoft.Extensions.Logging;
using OpenTracing;
using OpenTracing.Util;

namespace Horde.Build.Agents.Fleet.Providers
{
	/// <summary>
	/// Fleet manager for handling AWS EC2 instances
	/// Will start already existing but stopped instances to reuse existing EBS disks.
	/// See <see cref="AwsFleetManager" /> for creating and terminating instances from scratch.
	/// </summary>
	public sealed class AwsReuseFleetManager : IFleetManager, IDisposable
	{
		readonly AmazonEC2Client _client;
		readonly IAgentCollection _agentCollection;
		readonly ILogger _logger;

		/// <summary>
		/// Constructor
		/// </summary>
		public AwsReuseFleetManager(IAgentCollection agentCollection, ILogger<AwsReuseFleetManager> logger)
		{
			_agentCollection = agentCollection;
			_logger = logger;

			AmazonEC2Config config = new AmazonEC2Config();
			config.RegionEndpoint = RegionEndpoint.USEast1;

			logger.LogInformation("Initializing AWS fleet manager for region {Region}", config.RegionEndpoint);

			_client = new AmazonEC2Client(config);
		}

		/// <inheritdoc/>
		public void Dispose()
		{
			_client.Dispose();
		}

		/// <inheritdoc/>
		public async Task ExpandPoolAsync(IPool pool, IReadOnlyList<IAgent> agents, int count, CancellationToken cancellationToken)
		{
			using IScope scope = GlobalTracer.Instance.BuildSpan("ExpandPool").StartActive();
			scope.Span.SetTag("poolName", pool.Name);
			scope.Span.SetTag("numAgents", agents.Count);
			scope.Span.SetTag("count", count);

			DescribeInstancesResponse describeResponse;
			using (IScope describeScope = GlobalTracer.Instance.BuildSpan("DescribeInstances").StartActive())
			{
				// Find stopped instances in the correct pool
				DescribeInstancesRequest describeRequest = new DescribeInstancesRequest();
				describeRequest.Filters = new List<Filter>();
				describeRequest.Filters.Add(new Filter("instance-state-name", new List<string> { InstanceStateName.Stopped.Value }));
				describeRequest.Filters.Add(new Filter("tag:" + AwsFleetManager.PoolTagName, new List<string> { pool.Name }));
				describeResponse = await _client.DescribeInstancesAsync(describeRequest);
				describeScope.Span.SetTag("res.statusCode", (int)describeResponse.HttpStatusCode);
				describeScope.Span.SetTag("res.numReservations", describeResponse.Reservations.Count);
			}

			using (IScope startScope = GlobalTracer.Instance.BuildSpan("StartInstances").StartActive())
			{
				// Try to start the given instances
				StartInstancesRequest startRequest = new StartInstancesRequest();
				startRequest.InstanceIds.AddRange(describeResponse.Reservations.SelectMany(x => x.Instances).Select(x => x.InstanceId).Take(count));
				
				startScope.Span.SetTag("req.instanceIds", String.Join(",", startRequest.InstanceIds));
				if (startRequest.InstanceIds.Count > 0)
				{
					StartInstancesResponse startResponse = await _client.StartInstancesAsync(startRequest);
					startScope.Span.SetTag("res.statusCode", (int)startResponse.HttpStatusCode);
					startScope.Span.SetTag("res.numInstances", startResponse.StartingInstances.Count);
					if ((int)startResponse.HttpStatusCode >= 200 && (int)startResponse.HttpStatusCode <= 299)
					{
						foreach (InstanceStateChange instanceChange in startResponse.StartingInstances)
						{
							_logger.LogInformation("Starting instance {InstanceId} for pool {PoolId} (prev state {PrevState}, current state {CurrentState}", instanceChange.InstanceId, pool.Id, instanceChange.PreviousState, instanceChange.CurrentState);
						}
					}
				}

				if (startRequest.InstanceIds.Count < count)
				{
					_logger.LogInformation("Unable to expand pool {PoolName} with the requested number of instances. " +
					                      "Num requested instances to add {RequestedCount}. Actual instances started {InstancesStarted}", pool.Name, count, startRequest.InstanceIds.Count);
				}
			}
		}

		/// <inheritdoc/>
		public Task ShrinkPoolAsync(IPool pool, IReadOnlyList<IAgent> agents, int count, CancellationToken cancellationToken)
		{
			return AwsFleetManager.ShrinkPoolViaAgentShutdownRequestAsync(_agentCollection, pool, agents, count, cancellationToken);
		} 

		/// <inheritdoc/>
		public async Task<int> GetNumStoppedInstancesAsync(IPool pool, CancellationToken cancellationToken)
		{
			// Find all instances in the pool
			DescribeInstancesRequest describeRequest = new DescribeInstancesRequest();
			describeRequest.Filters = new List<Filter>();
			describeRequest.Filters.Add(new Filter("instance-state-name", new List<string> { InstanceStateName.Stopped.Value }));
			describeRequest.Filters.Add(new Filter("tag:" + AwsFleetManager.PoolTagName, new List<string> { pool.Name }));

			DescribeInstancesResponse describeResponse = await _client.DescribeInstancesAsync(describeRequest);
			return describeResponse.Reservations.SelectMany(x => x.Instances).Select(x => x.InstanceId).Distinct().Count();
		}
	}
}
