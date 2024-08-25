// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Threading;
using System.Threading.Tasks;
using Horde.Server.Agents;
using Horde.Server.Agents.Pools;
using Microsoft.Extensions.Logging.Abstractions;
using Microsoft.VisualStudio.TestTools.UnitTesting;

namespace Horde.Server.Tests.Agents.Pools
{
	[TestClass]
	public class PoolUpdateServiceTest : TestSetup
	{
		private readonly PoolUpdateService _pus;
		private IPool _pool = default!;
		private IAgent _enabledAgent = default!;
		private IAgent _disabledAgent = default!;
		private IAgent _disabledAgentBeyondGracePeriod = default!;

		public PoolUpdateServiceTest()
		{
			UpdateConfig(x => x.Pools.Clear());
			_pus = new(AgentCollection, PoolCollection, Clock, GlobalConfig, Tracer, new NullLogger<PoolUpdateService>());
		}

		[TestInitialize]
		public async Task SetupAsync()
		{
			_pool = await CreatePoolAsync(new PoolConfig { Name = "testPool", EnableAutoscaling = true });
			_enabledAgent = await CreateAgentAsync(_pool, true);
			_disabledAgent = await CreateAgentAsync(_pool, false);
			_disabledAgentBeyondGracePeriod = await CreateAgentAsync(_pool, enabled: false, adjustClockBy: -TimeSpan.FromHours(9));
		}

		private async Task RefreshAgentsAsync()
		{
			_enabledAgent = (await AgentService.GetAgentAsync(_enabledAgent.Id))!;
			_disabledAgent = (await AgentService.GetAgentAsync(_disabledAgent.Id))!;
			_disabledAgentBeyondGracePeriod = (await AgentService.GetAgentAsync(_disabledAgentBeyondGracePeriod.Id))!;
		}

		public override async ValueTask DisposeAsync()
		{
			GC.SuppressFinalize(this);
			await base.DisposeAsync();

			await _pus.DisposeAsync();
		}

		[TestMethod]
		public async Task ShutdownDisabledAgents_WithGlobalGracePeriod_RequestsShutdownAsync()
		{
			// Act
			await _pus.ShutdownDisabledAgentsAsync(CancellationToken.None);
			await RefreshAgentsAsync();

			// Assert
			Assert.IsFalse(_enabledAgent.RequestShutdown);
			Assert.IsFalse(_disabledAgent.RequestShutdown);
			Assert.IsTrue(_disabledAgentBeyondGracePeriod.RequestShutdown);
		}

		[TestMethod]
		public async Task ShutdownDisabledAgents_WithPerPoolGracePeriod_DoesNotRequestShutdownAsync()
		{
			// Arrange
			// Explicitly set the grace period for the pool to be longer than the default of 8 hours
			UpdateConfig(config => config.Pools[0].ShutdownIfDisabledGracePeriod = TimeSpan.FromHours(24.0));

			// Act
			await _pus.ShutdownDisabledAgentsAsync(CancellationToken.None);
			await RefreshAgentsAsync();

			// Assert
			Assert.IsFalse(_enabledAgent.RequestShutdown);
			Assert.IsFalse(_disabledAgent.RequestShutdown);
			Assert.IsFalse(_disabledAgentBeyondGracePeriod.RequestShutdown);
		}

		[TestMethod]
		public async Task ShutdownDisabledAgents_WithAutoScalingOff_DoesNotRequestShutdownAsync()
		{
			// Arrange
			UpdateConfig(config => config.Pools[0].EnableAutoscaling = false);

			// Act
			await _pus.ShutdownDisabledAgentsAsync(CancellationToken.None);
			await RefreshAgentsAsync();

			// Assert
			Assert.IsFalse(_enabledAgent.RequestShutdown);
			Assert.IsFalse(_disabledAgent.RequestShutdown);
			Assert.IsFalse(_disabledAgentBeyondGracePeriod.RequestShutdown);
		}
	}
}
