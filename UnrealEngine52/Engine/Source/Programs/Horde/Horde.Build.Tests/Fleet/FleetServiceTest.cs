// Copyright Epic Games, Inc. All Rights Reserved.

extern alias HordeAgent;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Text.Json;
using System.Threading;
using System.Threading.Tasks;
using Horde.Build.Agents;
using Horde.Build.Agents.Fleet;
using Horde.Build.Agents.Fleet.Providers;
using Horde.Build.Agents.Pools;
using Horde.Build.Utilities;
using Microsoft.Extensions.DependencyInjection;
using Microsoft.Extensions.Logging;
using Microsoft.Extensions.Options;
using Microsoft.VisualStudio.TestTools.UnitTesting;
using StatsdClient;

namespace Horde.Build.Tests.Fleet
{
	using PoolId = StringId<IPool>;

	public class FleetManagerSpy : IFleetManager
	{
		public int ExpandPoolAsyncCallCount { get; private set; }
		public int ShrinkPoolAsyncCallCount { get; private set; }
		public IReadOnlyList<IAgent> ExpandAgents => _expandAgents;
		public IReadOnlyList<IAgent> ShrinkAgents => _shrinkAgents;
		
		private readonly List<IAgent> _expandAgents = new ();
		private readonly List<IAgent> _shrinkAgents = new ();
		
		public Task ExpandPoolAsync(IPool pool, IReadOnlyList<IAgent> agents, int count, CancellationToken cancellationToken)
		{
			ExpandPoolAsyncCallCount++;
			_expandAgents.AddRange(agents);
			return Task.CompletedTask;
		}

		public Task ShrinkPoolAsync(IPool pool, IReadOnlyList<IAgent> agents, int count, CancellationToken cancellationToken)
		{
			ShrinkPoolAsyncCallCount++;
			_shrinkAgents.AddRange(agents);
			return Task.CompletedTask;
		}

		public Task<int> GetNumStoppedInstancesAsync(IPool pool, CancellationToken cancellationToken)
		{
			throw new NotImplementedException();
		}
	}

	
	public class StubFleetManagerFactory : IFleetManagerFactory
	{
		private readonly IFleetManager _fleetManager;

		public StubFleetManagerFactory(IFleetManager fleetManager)
		{
			_fleetManager = fleetManager;
		}

		public IFleetManager CreateFleetManager(FleetManagerType type, string config = "{}")
		{
			return _fleetManager;
		}
	}

	[TestClass]
	public class FleetServiceTest : TestSetup
	{
		readonly FleetManagerSpy _fleetManagerSpy = new();
		readonly IDogStatsd _dogStatsD = new NoOpDogStatsd();

		protected override void Dispose(bool disposing)
		{
			base.Dispose(disposing);

			if (disposing)
			{
				_dogStatsD.Dispose();
			}
		}

		[TestMethod]
		public async Task OnlyEnabledAgentsAreAutoScaled()
		{
			using FleetService service = GetFleetService(_fleetManagerSpy);
			IPool pool = await PoolService.CreatePoolAsync("testPool", null, true, 0, 0, sizeStrategy: PoolSizeStrategy.LeaseUtilization);
			await CreateAgentAsync(pool, true);
			await CreateAgentAsync(pool, true);
			await CreateAgentAsync(pool, false);

			PoolWithAgents poolWithAgents = (await service.GetPoolsWithAgentsAsync())[0];
			await service.ScalePoolAsync(pool, poolWithAgents.Agents, new PoolSizeResult(2, 5), CancellationToken.None);
			Assert.AreEqual(2, _fleetManagerSpy.ExpandAgents.Count);
			Assert.IsTrue(_fleetManagerSpy.ExpandAgents[0].Enabled);
			Assert.IsTrue(_fleetManagerSpy.ExpandAgents[1].Enabled);
		}
		
		[TestMethod]
		public async Task ScaleOutCooldown()
		{
			using FleetService service = GetFleetService(_fleetManagerSpy);
			IPool pool = await PoolService.CreatePoolAsync("testPool", null, true, 0, 0, sizeStrategy: PoolSizeStrategy.NoOp);

			// First scale-out will succeed
			await service.ScalePoolAsync(pool, new List<IAgent>(), new PoolSizeResult(0, 1), CancellationToken.None);
			Assert.AreEqual(1, _fleetManagerSpy.ExpandPoolAsyncCallCount);
			
			// Cannot scale-out due to cool-down
			await service.ScalePoolAsync(pool, new List<IAgent>(), new PoolSizeResult(0, 2), CancellationToken.None);
			Assert.AreEqual(1, _fleetManagerSpy.ExpandPoolAsyncCallCount);

			// Wait some time and then try again
			await Clock.AdvanceAsync(TimeSpan.FromHours(2));
			await service.ScalePoolAsync(pool, new List<IAgent>(), new PoolSizeResult(0, 2), CancellationToken.None);
			Assert.AreEqual(2, _fleetManagerSpy.ExpandPoolAsyncCallCount);
		}
		
		[TestMethod]
		public async Task ScaleInCooldown()
		{
			using FleetService service = GetFleetService(_fleetManagerSpy);
			IPool pool = await PoolService.CreatePoolAsync("testPool", null, true, 0, 0, sizeStrategy: PoolSizeStrategy.NoOp);
			IAgent agent1 = await CreateAgentAsync(pool);
			IAgent agent2 = await CreateAgentAsync(pool);

			// First scale-in will succeed
			await service.ScalePoolAsync(pool, new List<IAgent> { agent1, agent2 }, new PoolSizeResult(2, 1), CancellationToken.None);
			Assert.AreEqual(1, _fleetManagerSpy.ShrinkPoolAsyncCallCount);
			
			// Cannot scale-in due to cool-down
			await service.ScalePoolAsync(pool, new List<IAgent> { agent1 }, new PoolSizeResult(1, 0), CancellationToken.None);
			Assert.AreEqual(1, _fleetManagerSpy.ShrinkPoolAsyncCallCount);

			// Wait some time and then try again
			await Clock.AdvanceAsync(TimeSpan.FromHours(2));
			await service.ScalePoolAsync(pool, new List<IAgent> { agent1 }, new PoolSizeResult(1, 0), CancellationToken.None);
			Assert.AreEqual(2, _fleetManagerSpy.ShrinkPoolAsyncCallCount);
		}
		
		[TestMethod]
		public async Task ScaleOutDuringDowntime()
		{
			using FleetService service = GetFleetService(_fleetManagerSpy, true);
			IPool pool = await PoolService.CreatePoolAsync("testPool", null, true, 0, 0, sizeStrategy: PoolSizeStrategy.NoOp);

			await service.ScalePoolAsync(pool, new List<IAgent>(), new PoolSizeResult(0, 1), CancellationToken.None);
			Assert.AreEqual(0, _fleetManagerSpy.ExpandPoolAsyncCallCount);
		}
		
		[TestMethod]
		public async Task ScaleInDuringDowntime()
		{
			using FleetService service = GetFleetService(_fleetManagerSpy, true);
			IPool pool = await PoolService.CreatePoolAsync("testPool", null, true, 0, 0, sizeStrategy: PoolSizeStrategy.NoOp);
			IAgent agent1 = await CreateAgentAsync(pool);
			IAgent agent2 = await CreateAgentAsync(pool);

			await service.ScalePoolAsync(pool, new List<IAgent> { agent1, agent2 }, new PoolSizeResult(2, 1), CancellationToken.None);
			Assert.AreEqual(1, _fleetManagerSpy.ShrinkPoolAsyncCallCount);
		}

		private FleetService GetFleetService(IFleetManager fleetManager, bool isDowntimeActive = false)
		{
			ILoggerFactory loggerFactory = ServiceProvider.GetRequiredService<ILoggerFactory>();
			IOptions<ServerSettings> serverSettingsOpt = ServiceProvider.GetRequiredService<IOptions<ServerSettings>>();
			serverSettingsOpt.Value.FleetManagerV2 = FleetManagerType.AwsReuse;
			
			FleetService service = new(
				AgentCollection, GraphCollection, JobCollection, LeaseCollection, PoolCollection, new DowntimeServiceStub(isDowntimeActive), StreamCollection, _dogStatsD,
				new StubFleetManagerFactory(fleetManager), Clock, Cache, serverSettingsOpt, GlobalConfig, loggerFactory.CreateLogger<FleetService>());
				
			return service;
		}
	}

	[TestClass]
	public class PoolSizeStrategyFactoryTest : TestSetup
	{
		private static int s_poolCount;

		[TestMethod]
		public async Task CreateJobQueueFromLegacySettings()
		{
			IPool pool1 = await PoolService.CreatePoolAsync("test1", sizeStrategy: PoolSizeStrategy.JobQueue);
			Assert.AreEqual(typeof(JobQueueStrategy), FleetService.CreatePoolSizeStrategy(pool1).GetType());
			
			IPool pool2 = await PoolService.CreatePoolAsync("test2", sizeStrategy: PoolSizeStrategy.JobQueue, jobQueueSettings: new JobQueueSettings(22, 33));
			IPoolSizeStrategy s = FleetService.CreatePoolSizeStrategy(pool2);
			Assert.AreEqual(typeof(JobQueueStrategy), s.GetType());
			Assert.AreEqual(22.0, ((JobQueueStrategy)s).Settings.ScaleOutFactor);
			Assert.AreEqual(33.0, ((JobQueueStrategy)s).Settings.ScaleInFactor);
		}
		
		[TestMethod]
		public async Task CreateLeaseUtilizationFromLegacySettings()
		{
			IPool pool = await PoolService.CreatePoolAsync("test1", sizeStrategy: PoolSizeStrategy.LeaseUtilization);
			Assert.AreEqual(typeof(LeaseUtilizationStrategy), FleetService.CreatePoolSizeStrategy(pool).GetType());
		}

		[TestMethod]
		public async Task CreateJobQueueStrategy()
		{
			IPoolSizeStrategy s = await CreateStrategy(new PoolSizeStrategyInfo(PoolSizeStrategy.JobQueue, null, "{\"ScaleOutFactor\": 100, \"ScaleInFactor\": 200}"));
			Assert.AreEqual(typeof(JobQueueStrategy), s.GetType());
			Assert.AreEqual(100.0, ((JobQueueStrategy)s).Settings.ScaleOutFactor);
			Assert.AreEqual(200.0, ((JobQueueStrategy)s).Settings.ScaleInFactor);
		}
		
		[TestMethod]
		public async Task CreateLeaseUtilizationStrategy()
		{
			string config = "{\"SampleTimeSec\": 10, \"NumSamples\": 20, \"NumSamplesForResult\": 30}";
			IPoolSizeStrategy s = await CreateStrategy(new PoolSizeStrategyInfo(PoolSizeStrategy.LeaseUtilization, null, config));
			Assert.AreEqual(typeof(LeaseUtilizationStrategy), s.GetType());
			Assert.AreEqual(10, ((LeaseUtilizationStrategy)s).Settings.SampleTimeSec);
			Assert.AreEqual(20, ((LeaseUtilizationStrategy)s).Settings.NumSamples);
			Assert.AreEqual(30, ((LeaseUtilizationStrategy)s).Settings.NumSamplesForResult);
		}
		
		[TestMethod]
		public async Task CreateStrategyWithExtraAgentCount()
		{
			IPoolSizeStrategy s = await CreateStrategy(new PoolSizeStrategyInfo(PoolSizeStrategy.NoOp, null, "{}", 39));
			PoolSizeResult result = await s.CalculatePoolSizeAsync(null!, new List<IAgent>());
			Assert.AreEqual(39, result.DesiredAgentCount);
		}
		
		[TestMethod]
		public async Task EmptyOrInvalidJsonConfig()
		{
			await CreateStrategy(new PoolSizeStrategyInfo(PoolSizeStrategy.LeaseUtilization, null, ""));
			await CreateStrategy(new PoolSizeStrategyInfo(PoolSizeStrategy.LeaseUtilization, null, "{}"));
			await CreateStrategy(new PoolSizeStrategyInfo(PoolSizeStrategy.LeaseUtilization, null, "  {} "));
			
			await Assert.ThrowsExceptionAsync<JsonException>(() => CreateStrategy(new PoolSizeStrategyInfo(PoolSizeStrategy.LeaseUtilization, null, "BAD_JSON")));
		}
		
		[TestMethod]
		public async Task CreateNoOpStrategy()
		{
			IPoolSizeStrategy s = await CreateStrategy(new PoolSizeStrategyInfo(PoolSizeStrategy.NoOp, null, "{}"));
			Assert.AreEqual(typeof(NoOpPoolSizeStrategy), s.GetType());
		}
		
		[TestMethod]
		public async Task UnknownConfigFieldsAreIgnored()
		{
			IPoolSizeStrategy s = await CreateStrategy(new PoolSizeStrategyInfo(PoolSizeStrategy.JobQueue, null, "{\"BAD-PROPERTY\": 1337}"));
			Assert.AreEqual(typeof(JobQueueStrategy), s.GetType());
			Assert.AreEqual(0.25, ((JobQueueStrategy)s).Settings.ScaleOutFactor);
			Assert.AreEqual(0.9, ((JobQueueStrategy)s).Settings.ScaleInFactor);
		}
		
		[TestMethod]
		public async Task ConfigHandlesMixedCase()
		{
			IPoolSizeStrategy s = await CreateStrategy(new PoolSizeStrategyInfo(PoolSizeStrategy.JobQueue, null, "{\"scaleOutFactor\": 100, \"SCALEINFACTOR\": 200}"));
			Assert.AreEqual(typeof(JobQueueStrategy), s.GetType());
			Assert.AreEqual(100.0, ((JobQueueStrategy)s).Settings.ScaleOutFactor);
			Assert.AreEqual(200.0, ((JobQueueStrategy)s).Settings.ScaleInFactor);
		}
		
		[TestMethod]
		public async Task CreateFromEmptyStrategyList()
		{
			IPoolSizeStrategy s = await CreateStrategy();
			Assert.AreEqual(typeof(NoOpPoolSizeStrategy), s.GetType());
		}
		
		[TestMethod]
		public async Task ConditionSimple()
		{
			IPoolSizeStrategy s = await CreateStrategy(
				new PoolSizeStrategyInfo(PoolSizeStrategy.LeaseUtilization, "false", "{}"),
				new PoolSizeStrategyInfo(PoolSizeStrategy.JobQueue, "true", "{\"ScaleOutFactor\": 100, \"ScaleInFactor\": 200}")
			);
			Assert.AreEqual(typeof(JobQueueStrategy), s.GetType());
		}
		
		[TestMethod]
		public void ConditionYear()
		{
			Assert.IsTrue(EvalCondition(new DateTime(2022, 9, 1, 12, 0, 0, DateTimeKind.Utc), "timeUtcYear == 2022"));
			Assert.IsTrue(EvalCondition(new DateTime(1900, 1, 1, 12, 0, 0, DateTimeKind.Utc), "timeUtcYear == 1900"));
			Assert.IsFalse(EvalCondition(new DateTime(2050, 5, 1, 12, 0, 0, DateTimeKind.Utc), "timeUtcYear == 3"));
		}
		
		[TestMethod]
		public void ConditionMonth()
		{
			Assert.IsTrue(EvalCondition(new DateTime(2022, 9, 1, 12, 0, 0, DateTimeKind.Utc), "timeUtcMonth == 9"));
			Assert.IsTrue(EvalCondition(new DateTime(2022, 1, 1, 12, 0, 0, DateTimeKind.Utc), "timeUtcMonth == 1"));
			Assert.IsFalse(EvalCondition(new DateTime(2022, 5, 1, 12, 0, 0, DateTimeKind.Utc), "timeUtcMonth == 3"));
		}
		
		[TestMethod]
		public void ConditionDay()
		{
			Assert.IsTrue(EvalCondition(new DateTime(2022, 9, 1, 12, 0, 0, DateTimeKind.Utc), "timeUtcDay == 1"));
			Assert.IsTrue(EvalCondition(new DateTime(2022, 1, 31, 12, 0, 0, DateTimeKind.Utc), "timeUtcDay == 31"));
			Assert.IsFalse(EvalCondition(new DateTime(2022, 5, 5, 12, 0, 0, DateTimeKind.Utc), "timeUtcDay == 3"));
		}
		
		[TestMethod]
		public void ConditionDayOfWeek()
		{
			Assert.IsTrue(EvalCondition(new DateTime(2022, 9, 5, 15, 0, 0, DateTimeKind.Utc), "dayOfWeek == 'monday'")); // A Monday
			Assert.IsTrue(EvalCondition(new DateTime(2022, 9, 5, 15, 0, 0, DateTimeKind.Utc), "dayOfWeek == 'Monday'")); // A Monday
			Assert.IsFalse(EvalCondition(new DateTime(2022, 9, 6, 15, 0, 0, DateTimeKind.Utc), "dayOfWeek == 'monday'")); // A Tuesday

			Assert.IsTrue(EvalCondition(new DateTime(2022, 9, 5, 15, 0, 0, DateTimeKind.Utc), "timeUtcDayOfWeek == 'monday'")); // A Monday
			Assert.IsTrue(EvalCondition(new DateTime(2022, 9, 5, 15, 0, 0, DateTimeKind.Utc), "timeUtcDayOfWeek == 'Monday'")); // A Monday
			Assert.IsFalse(EvalCondition(new DateTime(2022, 9, 6, 15, 0, 0, DateTimeKind.Utc), "timeUtcDayOfWeek == 'monday'")); // A Tuesday
		}
		
		[TestMethod]
		public void ConditionHour()
		{
			Assert.IsTrue(EvalCondition(new DateTime(2022, 9, 5, 15, 0, 0, DateTimeKind.Utc), "timeUtcHour == 15"));
			Assert.IsTrue(EvalCondition(new DateTime(2022, 9, 5, 7, 0, 0, DateTimeKind.Utc), "timeUtcHour == 7"));
			Assert.IsFalse(EvalCondition(new DateTime(2022, 9, 5, 15, 0, 0, DateTimeKind.Utc), "timeUtcHour == 3"));
		}
		
		[TestMethod]
		public void ConditionMin()
		{
			Assert.IsTrue(EvalCondition(new DateTime(2022, 9, 5, 15, 55, 0, DateTimeKind.Utc), "timeUtcMin == 55"));
			Assert.IsTrue(EvalCondition(new DateTime(2022, 9, 5, 7, 0, 0, DateTimeKind.Utc), "timeUtcMin == 0"));
			Assert.IsFalse(EvalCondition(new DateTime(2022, 9, 5, 15, 1, 0, DateTimeKind.Utc), "timeUtcMin == 2"));
		}
		
		[TestMethod]
		public void ConditionSec()
		{
			Assert.IsTrue(EvalCondition(new DateTime(2022, 9, 5, 15, 0, 0, DateTimeKind.Utc), "timeUtcSec == 0"));
			Assert.IsTrue(EvalCondition(new DateTime(2022, 9, 5, 7, 0, 1, DateTimeKind.Utc), "timeUtcSec == 1"));
			Assert.IsTrue(EvalCondition(new DateTime(2022, 9, 5, 7, 0, 15, DateTimeKind.Utc), "timeUtcSec > 5"));
			Assert.IsFalse(EvalCondition(new DateTime(2022, 9, 5, 15, 0, 55, DateTimeKind.Utc), "timeUtcSec == 3"));
		}

		private bool EvalCondition(DateTime now, string condition)
		{
			Clock.UtcNow = now;
			return EpicGames.Horde.Common.Condition.Parse(condition).Evaluate(FleetService.GetPropValues);
		}
		
		private async Task<IPoolSizeStrategy> CreateStrategy(params PoolSizeStrategyInfo[] infos)
		{
			IPool pool = await PoolService.CreatePoolAsync("testPool-" + s_poolCount++, null, true, 0, 0, sizeStrategy: PoolSizeStrategy.NoOp);
			await PoolService.UpdatePoolAsync(pool, newSizeStrategies: infos.ToList());
			return FleetService.CreatePoolSizeStrategy(pool);
		}
	}

	[TestClass]
	public class FleetManagerFactoryTest : TestSetup
	{
		private static int s_poolCount;
		
		[TestMethod]
		public async Task CreateNoOp()
		{
			IFleetManager fm = await CreateFleetManager(new FleetManagerInfo(FleetManagerType.NoOp, null, "{}"));
			Assert.AreEqual(typeof(NoOpFleetManager), fm.GetType());
		}
		
		[TestMethod]
		public async Task CreateAws()
		{
			IFleetManager fm = await CreateFleetManager(new FleetManagerInfo(FleetManagerType.Aws, null, "{\"ImageId\": \"bogusImageId\"}"));
			Assert.AreEqual(typeof(AwsFleetManager), fm.GetType());
			Assert.AreEqual("bogusImageId", ((AwsFleetManager)fm).Settings.ImageId);
		}
		
		[TestMethod]
		public async Task CreateAwsReuse()
		{
			IFleetManager fm = await CreateFleetManager(new FleetManagerInfo(FleetManagerType.AwsReuse, null, "{\"InstanceTypes\": [\"foo\", \"bar\"]}"));
			Assert.AreEqual(typeof(AwsReuseFleetManager), fm.GetType());
			Assert.AreEqual("foo", ((AwsReuseFleetManager)fm).Settings.InstanceTypes![0]);
			Assert.AreEqual("bar", ((AwsReuseFleetManager)fm).Settings.InstanceTypes![1]);
		}
		
		[TestMethod]
		public async Task CreateAwsAsg()
		{
			IFleetManager fm = await CreateFleetManager(new FleetManagerInfo(FleetManagerType.AwsAsg, null, "{\"Name\": \"bogusName\"}"));
			Assert.AreEqual(typeof(AwsAsgFleetManager), fm.GetType());
			Assert.AreEqual("bogusName", ((AwsAsgFleetManager)fm).Settings.Name);
		}

		[TestMethod]
		public async Task UnknownConfigFieldsAreIgnored()
		{
			IFleetManager fm = await CreateFleetManager(new FleetManagerInfo(FleetManagerType.AwsAsg, null, "{\"Name\": \"bogusName\", \"BAD-PROPERTY\": 1337}"));
			Assert.AreEqual(typeof(AwsAsgFleetManager), fm.GetType());
			Assert.AreEqual("bogusName", ((AwsAsgFleetManager)fm).Settings.Name);
		}
		
		[TestMethod]
		public async Task EmptyConfigThrowsException()
		{
			IFleetManager fm = await CreateFleetManager(new FleetManagerInfo(FleetManagerType.AwsRecycle, null, ""));
			Assert.AreEqual(typeof(AwsRecyclingFleetManager), fm.GetType());
			Assert.IsNull(((AwsRecyclingFleetManager)fm).Settings.InstanceTypes);
		}
		
		[TestMethod]
		public async Task CreateFromEmptyList()
		{
			IFleetManager fm = await CreateFleetManager();
			Assert.AreEqual(typeof(NoOpFleetManager), fm.GetType());
		}
		
		[TestMethod]
		public async Task ConditionSimple()
		{
			IFleetManager s = await CreateFleetManager(
				new FleetManagerInfo(FleetManagerType.Aws, "false", "{}"),
				new FleetManagerInfo(FleetManagerType.AwsReuse, "true", "{\"ScaleOutFactor\": 100, \"ScaleInFactor\": 200}")
			);
			Assert.AreEqual(typeof(AwsReuseFleetManager), s.GetType());
		}
		
		private async Task<IFleetManager> CreateFleetManager(params FleetManagerInfo[] infos)
		{
			IPool pool = await PoolService.CreatePoolAsync("testPool-" + s_poolCount++, null, true, 0, 0, fleetManagers: infos.ToList(), sizeStrategy: PoolSizeStrategy.NoOp);
			return FleetService.CreateFleetManager(pool);
		}
	}
}
