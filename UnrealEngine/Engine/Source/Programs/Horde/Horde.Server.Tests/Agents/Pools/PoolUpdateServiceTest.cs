// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Threading.Tasks;
using Microsoft.VisualStudio.TestTools.UnitTesting;
using System.Threading;
using Horde.Server.Agents;
using Horde.Server.Agents.Pools;
using Microsoft.Extensions.Logging.Abstractions;

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
			_pus = new (AgentCollection, PoolCollection, Clock, GlobalConfig, Tracer, new NullLogger<PoolUpdateService>());
		}

		[TestInitialize]
		public async Task Setup()
		{
			_pool = await PoolService.CreatePoolAsync("testPool", new AddPoolOptions { EnableAutoscaling = true });
			_enabledAgent = await CreateAgentAsync(_pool, true);
			_disabledAgent = await CreateAgentAsync(_pool, false);
			_disabledAgentBeyondGracePeriod = await CreateAgentAsync(_pool, enabled: false, adjustClockBy: -TimeSpan.FromHours(9));
		}

		private async Task RefreshAgents()
		{
			_enabledAgent = (await AgentService.GetAgentAsync(_enabledAgent.Id))!;
			_disabledAgent = (await AgentService.GetAgentAsync(_disabledAgent.Id))!;
			_disabledAgentBeyondGracePeriod = (await AgentService.GetAgentAsync(_disabledAgentBeyondGracePeriod.Id))!;
		}

		protected override void Dispose(bool disposing)
		{
			if (disposing)
			{
				_pus.Dispose();
			}

			base.Dispose(disposing);
		}

		[TestMethod]
		public async Task ShutdownDisabledAgents_WithGlobalGracePeriod_RequestsShutdown()
		{
			// Act
			await _pus.ShutdownDisabledAgentsAsync(CancellationToken.None);
			await RefreshAgents();
			
			// Assert
			Assert.IsFalse(_enabledAgent.RequestShutdown);
			Assert.IsFalse(_disabledAgent.RequestShutdown);
			Assert.IsTrue(_disabledAgentBeyondGracePeriod.RequestShutdown);
		}
		
		[TestMethod]
		public async Task ShutdownDisabledAgents_WithPerPoolGracePeriod_DoesNotRequestShutdown()
		{
			// Arrange
			// Explicitly set the grace period for the pool to be longer than the default of 8 hours
			await PoolCollection.TryUpdateAsync(_pool, new UpdatePoolOptions { ShutdownIfDisabledGracePeriod = TimeSpan.FromHours(24) });
			
			// Act
			await _pus.ShutdownDisabledAgentsAsync(CancellationToken.None);
			await RefreshAgents();
			
			// Assert
			Assert.IsFalse(_enabledAgent.RequestShutdown);
			Assert.IsFalse(_disabledAgent.RequestShutdown);
			Assert.IsFalse(_disabledAgentBeyondGracePeriod.RequestShutdown);
		}
		
		[TestMethod]
		public async Task ShutdownDisabledAgents_WithAutoScalingOff_DoesNotRequestShutdown()
		{
			// Arrange
			await PoolCollection.TryUpdateAsync(_pool, new UpdatePoolOptions { EnableAutoscaling = false });
			
			// Act
			await _pus.ShutdownDisabledAgentsAsync(CancellationToken.None);
			await RefreshAgents();
			
			// Assert
			Assert.IsFalse(_enabledAgent.RequestShutdown);
			Assert.IsFalse(_disabledAgent.RequestShutdown);
			Assert.IsFalse(_disabledAgentBeyondGracePeriod.RequestShutdown);
		}
	}
}
