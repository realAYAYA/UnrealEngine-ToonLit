// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Diagnostics.Metrics;
using System.Linq;
using System.Net;
using System.Threading;
using System.Threading.Tasks;
using EpicGames.Horde.Agents.Pools;
using EpicGames.Horde.Compute;
using Horde.Server.Agents;
using Horde.Server.Compute;
using Horde.Server.Server;
using Microsoft.Extensions.Logging.Abstractions;
using Microsoft.VisualStudio.TestTools.UnitTesting;
using OpenTelemetry.Trace;

namespace Horde.Server.Tests.Compute
{
	[TestClass]
	public class ComputeServiceTest : TestSetup
	{
		private readonly ClusterId _cluster1 = new("cluster1");

		[TestMethod]
		public async Task ResourceNeedsMetricAsync()
		{
			await ComputeService.SetResourceNeedsAsync(new ClusterId("cluster1"), "session1", "pool1", new() { { "cpu", 101 }, { "ram", 1000 } });
			await ComputeService.SetResourceNeedsAsync(new ClusterId("cluster1"), "session1", "pool1", new() { { "cpu", 102 }, { "ram", 1001 } });
			await ComputeService.SetResourceNeedsAsync(new ClusterId("cluster1"), "session2", "pool1", new() { { "cpu", 210 }, { "ram", 1100 } });
			await ComputeService.SetResourceNeedsAsync(new ClusterId("cluster1"), "session2", "pool1", new() { { "cpu", 220 }, { "ram", 1200 } });

			await ComputeService.SetResourceNeedsAsync(new ClusterId("cluster1"), "session1", "pool2", new() { { "cpu", 301 }, { "ram", 1300 } });
			await ComputeService.SetResourceNeedsAsync(new ClusterId("cluster1"), "session2", "pool2", new() { { "cpu", 410 }, { "ram", 1400 } });

			await ComputeService.SetResourceNeedsAsync(new ClusterId("cluster2"), "session1", "pool1", new() { { "cpu", 20 }, { "ram", 4000 } });
			await ComputeService.SetResourceNeedsAsync(new ClusterId("cluster2"), "session2", "pool1", new() { { "cpu", 30 }, { "ram", 5500 } });

			List<Measurement<int>> measurements = await ComputeService.CalculateResourceNeedsAsync();
			Assert.AreEqual(6, measurements.Count);
			AssertContainsMeasurement(measurements, 322, "cpu", "cluster1", "pool1");
			AssertContainsMeasurement(measurements, 2201, "ram", "cluster1", "pool1");
			AssertContainsMeasurement(measurements, 711, "cpu", "cluster1", "pool2");
			AssertContainsMeasurement(measurements, 2700, "ram", "cluster1", "pool2");
			AssertContainsMeasurement(measurements, 50, "cpu", "cluster2", "pool1");
			AssertContainsMeasurement(measurements, 9500, "ram", "cluster2", "pool1");
		}

		[TestMethod]
		public async Task ResourceNeedsAreReplacedAsync()
		{
			await ComputeService.SetResourceNeedsAsync(new ClusterId("cluster1"), "session1", "pool1", new() { { "cpu", 1 }, { "ram", 5 } });
			await ComputeService.SetResourceNeedsAsync(new ClusterId("cluster1"), "session1", "pool1", new() { { "cpu", 10 }, { "ram", 50 } });
			List<ComputeService.SessionResourceNeeds> result = await ComputeService.GetResourceNeedsAsync();
			Assert.AreEqual(1, result.Count);
			Assert.AreEqual(10, result[0].ResourceNeeds["cpu"]);
			Assert.AreEqual(50, result[0].ResourceNeeds["ram"]);
		}

		[TestMethod]
		public async Task ResourceNeedsAreRemovedWhenOutdatedAsync()
		{
			await ComputeService.SetResourceNeedsAsync(new ClusterId("cluster1"), "session1", "pool1", new() { { "cpu", 1 }, { "ram", 5 } });
			Assert.AreEqual(1, (await ComputeService.GetResourceNeedsAsync()).Count);

			await Clock.AdvanceAsync(TimeSpan.FromMinutes(6));
			Assert.AreEqual(0, (await ComputeService.GetResourceNeedsAsync()).Count);
		}

		[TestMethod]
		public async Task DeniedRequestAsync()
		{
			await ComputeService.LogRequestAsync(AllocationOutcome.Denied, "req1", new Requirements { Pool = "pool1" }, null, Tracer.CurrentSpan);
			await Clock.AdvanceAsync(TimeSpan.FromSeconds(5));
			await ComputeService.LogRequestAsync(AllocationOutcome.Denied, "req1", new Requirements { Pool = "pool1" }, null, Tracer.CurrentSpan);
			await Clock.AdvanceAsync(TimeSpan.FromSeconds(5));
			await ComputeService.LogRequestAsync(AllocationOutcome.Denied, "req1", new Requirements { Pool = "pool1" }, null, Tracer.CurrentSpan);

			List<ComputeService.RequestInfo> ids = await ComputeService.GetUnservedRequestsAsync();
			Assert.AreEqual(1, ids.Count);
		}

		[TestMethod]
		public async Task AcceptedRequestAsync()
		{
			await ComputeService.LogRequestAsync(AllocationOutcome.Accepted, "req1", new Requirements { Pool = "pool1" }, null, Tracer.CurrentSpan);
			await Clock.AdvanceAsync(TimeSpan.FromSeconds(5));
			await ComputeService.LogRequestAsync(AllocationOutcome.Accepted, "req1", new Requirements { Pool = "pool1" }, null, Tracer.CurrentSpan);
			await Clock.AdvanceAsync(TimeSpan.FromSeconds(5));
			await ComputeService.LogRequestAsync(AllocationOutcome.Accepted, "req2", new Requirements { Pool = "pool1" }, null, Tracer.CurrentSpan);

			List<ComputeService.RequestInfo> ids = await ComputeService.GetUnservedRequestsAsync();
			Assert.AreEqual(0, ids.Count);
		}

		[TestMethod]
		public async Task DeniedThenAcceptedRequestAsync()
		{
			await ComputeService.LogRequestAsync(AllocationOutcome.Denied, "req1", new Requirements { Pool = "pool1" }, null, Tracer.CurrentSpan);
			await Clock.AdvanceAsync(TimeSpan.FromSeconds(5));
			await ComputeService.LogRequestAsync(AllocationOutcome.Accepted, "req1", new Requirements { Pool = "pool1" }, null, Tracer.CurrentSpan);
			await Clock.AdvanceAsync(TimeSpan.FromSeconds(5));
			await ComputeService.LogRequestAsync(AllocationOutcome.Denied, "req1", new Requirements { Pool = "pool1" }, null, Tracer.CurrentSpan);

			List<ComputeService.RequestInfo> ids = await ComputeService.GetUnservedRequestsAsync();
			Assert.AreEqual(0, ids.Count);
		}

		[TestMethod]
		public async Task OnlyIncludeLastMinuteAsync()
		{
			await ComputeService.LogRequestAsync(AllocationOutcome.Denied, "req1", new Requirements { Pool = "pool1" }, null, Tracer.CurrentSpan);
			await Clock.AdvanceAsync(TimeSpan.FromMinutes(61));
			await ComputeService.LogRequestAsync(AllocationOutcome.Denied, "req2", new Requirements { Pool = "pool1" }, null, Tracer.CurrentSpan);

			List<ComputeService.RequestInfo> ids = await ComputeService.GetUnservedRequestsAsync();
			Assert.AreEqual(1, ids.Count);
			Assert.AreEqual("req2", ids[0].RequestId);
		}

		[TestMethod]
		public async Task ComplexAsync()
		{
			await ComputeService.LogRequestAsync(AllocationOutcome.Denied, "req1", new Requirements { Pool = "pool1" }, null, Tracer.CurrentSpan);
			await Clock.AdvanceAsync(TimeSpan.FromSeconds(5));
			await ComputeService.LogRequestAsync(AllocationOutcome.Denied, "req2", new Requirements { Pool = "pool1" }, null, Tracer.CurrentSpan);
			await Clock.AdvanceAsync(TimeSpan.FromSeconds(5));
			await ComputeService.LogRequestAsync(AllocationOutcome.Denied, "req1", new Requirements { Pool = "pool1" }, null, Tracer.CurrentSpan);
			await Clock.AdvanceAsync(TimeSpan.FromSeconds(5));
			await ComputeService.LogRequestAsync(AllocationOutcome.Denied, "req2", new Requirements { Pool = "pool1" }, null, Tracer.CurrentSpan);
			await Clock.AdvanceAsync(TimeSpan.FromSeconds(5));
			await ComputeService.LogRequestAsync(AllocationOutcome.Denied, "req3", new Requirements { Pool = "pool1" }, null, Tracer.CurrentSpan);
			await Clock.AdvanceAsync(TimeSpan.FromSeconds(5));
			await ComputeService.LogRequestAsync(AllocationOutcome.Accepted, "req2", new Requirements { Pool = "pool1" }, null, Tracer.CurrentSpan);

			List<ComputeService.RequestInfo> ids = await ComputeService.GetUnservedRequestsAsync();
			CollectionAssert.AreEquivalent(new List<string> { "req1", "req3" }, ids.Select(x => x.RequestId).ToList());
		}

		[TestMethod]
		public void SerializeRequestInfo()
		{
			{
				ComputeService.RequestInfo req = new(DateTimeOffset.UnixEpoch, AllocationOutcome.Accepted, "reqId1", "pool1", "parent1");
				string serialized = req.Serialize();
				ComputeService.RequestInfo deserialized = ComputeService.RequestInfo.Deserialize(serialized)!;
				Assert.AreEqual(DateTimeOffset.UnixEpoch, deserialized!.Timestamp);
				Assert.AreEqual(AllocationOutcome.Accepted, deserialized.Outcome);
				Assert.AreEqual("reqId1", deserialized.RequestId);
				Assert.AreEqual("pool1", deserialized.Pool);
				Assert.AreEqual("parent1", deserialized.ParentLeaseId);
			}

			{
				ComputeService.RequestInfo req = new(DateTimeOffset.UnixEpoch, AllocationOutcome.Accepted, "reqId1", "pool1", null);
				string serialized = req.Serialize();
				ComputeService.RequestInfo deserialized = ComputeService.RequestInfo.Deserialize(serialized)!;
				Assert.IsNull(deserialized.ParentLeaseId);
			}
		}

		[TestMethod]
		public void GroupByPoolAndCount()
		{
			List<ComputeService.RequestInfo> ris = new()
			{
				new ComputeService.RequestInfo(DateTimeOffset.UnixEpoch, AllocationOutcome.Denied, "reqId1", "poolA", null),
				new ComputeService.RequestInfo(DateTimeOffset.UnixEpoch, AllocationOutcome.Denied, "reqId2", "poolA", null),
				new ComputeService.RequestInfo(DateTimeOffset.UnixEpoch, AllocationOutcome.Denied, "reqId3", "poolA", null),
				new ComputeService.RequestInfo(DateTimeOffset.UnixEpoch, AllocationOutcome.Denied, "reqId4", "poolB", null),
				new ComputeService.RequestInfo(DateTimeOffset.UnixEpoch, AllocationOutcome.Denied, "reqId4", "poolB", null),
			};
			Dictionary<string, int> result = ComputeService.GroupByPoolAndCount(ris);
			Assert.AreEqual(3, result["poolA"]);
			Assert.AreEqual(2, result["poolB"]);
		}

		[TestMethod]
		public async Task PoolNameTemplatingAsync()
		{
			GlobalConfig.CurrentValue.Networks = new List<NetworkConfig>
			{
				new() { CidrBlock = "12.0.0.0/16", Id = "myNetworkId", ComputeId = "myComputeId" }
			};

			IPAddress ip = IPAddress.Parse("12.0.10.30");
			List<string> props = new() { "ComputeIp=11.0.0.1", "ComputePort=5000" };
			IAgent agent1 = await CreateAgentAsync(new PoolId("foo"), properties: props);
			IAgent agent2 = await CreateAgentAsync(new PoolId("bar-default"), properties: props);
			IAgent agent3 = await CreateAgentAsync(new PoolId("bar-myNetworkId"), properties: props);
			IAgent agent4 = await CreateAgentAsync(new PoolId("qux-myComputeId"), properties: props);
			ClusterId clusterId = new("default");

			AllocateResourceParams arp1 = new(clusterId, ComputeProtocol.Latest, new Requirements { Pool = "foo" }) { RequestId = "req1", RequesterIp = ip, ParentLeaseId = null };
			ComputeResource? resource1 = await ComputeService.TryAllocateResourceAsync(arp1, CancellationToken.None);
			Assert.AreEqual(agent1.Id, resource1!.AgentId);

			AllocateResourceParams arp2 = new(clusterId, ComputeProtocol.Latest, new Requirements { Pool = "bar-%REQUESTER_NETWORK_ID%" }) { RequestId = "req2", RequesterIp = IPAddress.Parse("15.0.0.1"), ParentLeaseId = null };
			ComputeResource? resource2 = await ComputeService.TryAllocateResourceAsync(arp2, CancellationToken.None);
			Assert.AreEqual(agent2.Id, resource2!.AgentId);

			AllocateResourceParams arp3 = new(clusterId, ComputeProtocol.Latest, new Requirements { Pool = "bar-%REQUESTER_NETWORK_ID%" }) { RequestId = "req3", RequesterIp = ip, ParentLeaseId = null };
			ComputeResource? resource3 = await ComputeService.TryAllocateResourceAsync(arp3, CancellationToken.None);
			Assert.AreEqual(agent3.Id, resource3!.AgentId);

			AllocateResourceParams arp4 = new(clusterId, ComputeProtocol.Latest, new Requirements { Pool = "qux-%REQUESTER_COMPUTE_ID%" }) { RequestId = "req4", RequesterIp = ip, ParentLeaseId = null };
			ComputeResource? resource4 = await ComputeService.TryAllocateResourceAsync(arp4, CancellationToken.None);
			Assert.AreEqual(agent4.Id, resource4!.AgentId);
		}

		[TestMethod]
		public async Task Connection_Direct_IpConnection_Async()
		{
			ComputeResource? cr = await AllocateAsync(ConnectionMode.Direct);
			Assert.AreEqual(ConnectionMode.Direct, cr!.ConnectionMode);
			Assert.AreEqual("11.0.0.1", cr.Ip.ToString());
			Assert.IsNull(cr.ConnectionAddress);
		}

		[TestMethod]
		public async Task Connection_Direct_PortsAreMapped_Async()
		{
			ComputeResource? cr = await AllocateAsync(ConnectionMode.Direct, ports: new Dictionary<string, int> { { "myOtherPort", 13000 }, { "myPort", 12000 } });
			Assert.AreEqual(3, cr!.Ports.Count);
			Assert.AreEqual(new ComputeResourcePort(5000, 5000), cr.Ports[ConnectionMetadataPort.ComputeId]);
			Assert.AreEqual(new ComputeResourcePort(12000, 12000), cr.Ports["myPort"]);
			Assert.AreEqual(new ComputeResourcePort(13000, 13000), cr.Ports["myOtherPort"]);
		}

		[TestMethod]
		public async Task Connection_Tunnel_IpConnection_Async()
		{
			ComputeResource? cr = await AllocateAsync(ConnectionMode.Tunnel, tunnelAddress: "localhost:3344");
			Assert.AreEqual(ConnectionMode.Tunnel, cr!.ConnectionMode);
			Assert.AreEqual("11.0.0.1", cr.Ip.ToString());
			Assert.AreEqual("localhost:3344", cr.ConnectionAddress);
		}

		[TestMethod]
		public async Task Connection_Tunnel_PortsAreMapped_Async()
		{
			ComputeResource? cr = await AllocateAsync(ConnectionMode.Tunnel, tunnelAddress: "localhost:3344", ports: new Dictionary<string, int> { { "myOtherPort", 13000 }, { "myPort", 12000 } });
			Assert.AreEqual(3, cr!.Ports.Count);
			Assert.AreEqual(new ComputeResourcePort(-1, 5000), cr.Ports[ConnectionMetadataPort.ComputeId]);
			Assert.AreEqual(new ComputeResourcePort(-1, 12000), cr.Ports["myPort"]);
			Assert.AreEqual(new ComputeResourcePort(-1, 13000), cr.Ports["myOtherPort"]);
		}

		[TestMethod]
		public async Task Connection_Relay_IpConnection_Async()
		{
			ComputeResource? cr = await AllocateAsync(ConnectionMode.Relay);
			Assert.AreEqual(ConnectionMode.Relay, cr!.ConnectionMode);
			Assert.AreEqual("11.0.0.1", cr.Ip.ToString());
			Assert.AreEqual("192.168.1.1", cr.ConnectionAddress);
		}

		[TestMethod]
		public async Task Connection_Relay_PortsAreMapped_Async()
		{
			ComputeResource? cr = await AllocateAsync(ConnectionMode.Relay, ports: new Dictionary<string, int> { { "myOtherPort", 13000 }, { "myPort", 12000 } });
			Assert.AreEqual(ConnectionMode.Relay, cr!.ConnectionMode);
			Assert.AreEqual(3, cr.Ports.Count);
			Assert.AreEqual(new ComputeResourcePort(10000, 5000), cr.Ports[ConnectionMetadataPort.ComputeId]);
			Assert.AreEqual(new ComputeResourcePort(10002, 12000), cr.Ports["myPort"]);
			Assert.AreEqual(new ComputeResourcePort(10004, 13000), cr.Ports["myOtherPort"]);
		}

		private async Task<ComputeService> CreateComputeServiceAsync(string? tunnelAddress)
		{
			ServerSettings ss = new() { ComputeTunnelAddress = tunnelAddress };
			ComputeService cs = new(AgentCollection, LogFileService, AgentService, AgentRelayService, GetRedisServiceSingleton(),
				new TestOptionsMonitor<ServerSettings>(ss), GlobalConfig, Clock, Tracer, Meter,
				NullLogger<ComputeService>.Instance);
			List<string> props = new() { "ComputeIp=11.0.0.1", "ComputePort=5000" };
			await CreateAgentAsync(new PoolId("foo"), properties: props);
			return cs;
		}

		private async Task<ComputeResource?> AllocateAsync(
			ConnectionMode connectionMode,
			Dictionary<string, int>? ports = null,
			bool usePublicIp = false,
			string[]? relayIps = null,
			string? tunnelAddress = null
			)
		{
			await using ComputeService cs = await CreateComputeServiceAsync(tunnelAddress);
			AllocateResourceParams arp = new(_cluster1, ComputeProtocol.Latest, new Requirements())
			{
				ConnectionMode = connectionMode,
				Ports = ports ?? new Dictionary<string, int>(),
				UsePublicIp = usePublicIp
			};
			IPAddress[] defaultRelayIps = { IPAddress.Parse("192.168.1.1") };
			await AgentRelayService.UpdateAgentHeartbeatAsync(_cluster1, "myrelay", relayIps?.Select(IPAddress.Parse) ?? defaultRelayIps);
			return await cs.TryAllocateResourceAsync(arp, CancellationToken.None);
		}

		private static void AssertContainsMeasurement(List<Measurement<int>> actualMeasurements, int expectedValue, string expectedResource, string expectedClusterId, string expectedPool)
		{
			Dictionary<string, string> expectedTags = new()
			{
				{ "resource", expectedResource },
				{ "cluster", expectedClusterId },
				{ "pool", expectedPool }
			};

			foreach (Measurement<int> measurement in actualMeasurements)
			{
				Dictionary<string, string> actualTags = measurement.Tags.ToArray().ToDictionary(
					kvp => kvp.Key,
					kvp => (string)kvp.Value!);

				bool areTagsEqual = actualTags.Count == expectedTags.Count &&
									actualTags.OrderBy(kvp => kvp.Key)
										.SequenceEqual(expectedTags.OrderBy(kvp => kvp.Key));

				if (areTagsEqual && expectedValue == measurement.Value)
				{
					return;
				}
			}

			Console.WriteLine("Actual:");
			foreach (Measurement<int> m in actualMeasurements)
			{
				Console.WriteLine($"Measurement(value={m.Value} tags={String.Join(',', m.Tags.ToArray())}");
			}
			Assert.Fail("Unable to find measurement");
		}
	}
}