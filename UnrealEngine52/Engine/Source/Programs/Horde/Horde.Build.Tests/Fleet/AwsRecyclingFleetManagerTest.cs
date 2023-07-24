// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Linq;
using System.Threading;
using System.Threading.Tasks;
using Amazon.EC2;
using Amazon.EC2.Model;
using Horde.Build.Agents.Pools;
using Horde.Build.Agents.Fleet;
using Microsoft.VisualStudio.TestTools.UnitTesting;
using Horde.Build.Agents;
using Horde.Build.Agents.Fleet.Providers;
using Horde.Build.Utilities;
using Microsoft.Extensions.Logging;
using JsonSerializer = System.Text.Json.JsonSerializer;

namespace Horde.Build.Tests.Fleet
{
	[TestClass]
	public class AwsRecyclingFleetManagerTest : TestSetup
	{
		[TestMethod]
		public async Task ExpandAmazonEc2ExceptionsArePropagated()
		{
			FakeAmazonEc2 ec2 = new ();
			ec2.AddInstance(FakeAmazonEc2.StateStopped, InstanceType.M5Large, FakeAmazonEc2.AzUsEast1A);
			ec2.SetCapacity(FakeAmazonEc2.AzUsEast1A, InstanceType.M5Large, 1);
			ec2.OnStartInstances = (_, _) => throw new AmazonEC2Exception("Something bad happened");
			await Assert.ThrowsExceptionAsync<AmazonEC2Exception>(() => ExpandPoolAsync(ec2.Get(), 1, new()));
		}
		
		[TestMethod]
		public async Task ExpandOneAzPartialCapacityWithFallback()
		{
			FakeAmazonEc2 ec2 = new ();
			Instance i1 = ec2.AddInstance(FakeAmazonEc2.StateStopped, InstanceType.M5Large, FakeAmazonEc2.AzUsEast1A);
			Instance i2 = ec2.AddInstance(FakeAmazonEc2.StateStopped, InstanceType.M5Large, FakeAmazonEc2.AzUsEast1A);
			ec2.SetCapacity(FakeAmazonEc2.AzUsEast1A, InstanceType.M5Large, 1);
			ec2.SetCapacity(FakeAmazonEc2.AzUsEast1A, InstanceType.C5Large, 2);

			await ExpandPoolAsync(ec2.Get(), 2, new(new List<string> { InstanceType.C5Large }));
			Assert.AreEqual(0, ec2.GetStoppedInstanceCount());
			Assert.AreEqual(2, ec2.GetPendingInstanceCount());
			Assert.AreEqual(InstanceType.C5Large, ec2.GetInstance(i1.InstanceId)!.InstanceType);
			Assert.AreEqual(InstanceType.C5Large, ec2.GetInstance(i2.InstanceId)!.InstanceType);
		}
		
		[TestMethod]
		public async Task ExpandOneAzPartialCapacityWithMultipleFallbacks()
		{
			FakeAmazonEc2 ec2 = new ();
			Instance i1 = ec2.AddInstance(FakeAmazonEc2.StateStopped, InstanceType.M5Large, FakeAmazonEc2.AzUsEast1A);
			Instance i2 = ec2.AddInstance(FakeAmazonEc2.StateStopped, InstanceType.M5Large, FakeAmazonEc2.AzUsEast1A);
			ec2.SetCapacity(FakeAmazonEc2.AzUsEast1A, InstanceType.M5Large, 1);
			ec2.SetCapacity(FakeAmazonEc2.AzUsEast1A, InstanceType.C5Large, 2);

			await ExpandPoolAsync(ec2.Get(), 2, new(new List<string> { InstanceType.M5Xlarge, InstanceType.R5Large, InstanceType.C5Large }));
			Assert.AreEqual(0, ec2.GetStoppedInstanceCount());
			Assert.AreEqual(2, ec2.GetPendingInstanceCount());
			Assert.AreEqual(InstanceType.C5Large, ec2.GetInstance(i1.InstanceId)!.InstanceType);
			Assert.AreEqual(InstanceType.C5Large, ec2.GetInstance(i2.InstanceId)!.InstanceType);
		}
		
		[TestMethod]
		public async Task ExpandOneAzPartialCapacityNoFallback()
		{
			FakeAmazonEc2 ec2 = new ();
			ec2.AddInstance(FakeAmazonEc2.StateStopped, InstanceType.M5Large, FakeAmazonEc2.AzUsEast1A);
			ec2.AddInstance(FakeAmazonEc2.StateStopped, InstanceType.M5Large, FakeAmazonEc2.AzUsEast1A);
			ec2.SetCapacity(FakeAmazonEc2.AzUsEast1A, InstanceType.M5Large, 1);

			await ExpandPoolAsync(ec2.Get(), 2, new());
			Assert.AreEqual(2, ec2.GetStoppedInstanceCount());
			Assert.AreEqual(0, ec2.GetPendingInstanceCount());
		}
		
		[TestMethod]
		public async Task ExpandTwoAzsPartialCapacityNoFallback()
		{
			FakeAmazonEc2 ec2 = new ();
			ec2.AddInstance(FakeAmazonEc2.StateStopped, InstanceType.M5Large, FakeAmazonEc2.AzUsEast1A);
			ec2.AddInstance(FakeAmazonEc2.StateStopped, InstanceType.M5Large, FakeAmazonEc2.AzUsEast1B);
			ec2.SetCapacity(FakeAmazonEc2.AzUsEast1A, InstanceType.M5Large, 1);
			ec2.SetCapacity(FakeAmazonEc2.AzUsEast1B, InstanceType.M5Large, 0);

			await ExpandPoolAsync(ec2.Get(), 2, new());
			Assert.AreEqual(1, ec2.GetStoppedInstanceCount());
			Assert.AreEqual(1, ec2.GetPendingInstanceCount());
		}
		
		[TestMethod]
		public async Task ExpandMoreThanNumStoppedInstances()
		{
			FakeAmazonEc2 ec2 = new ();
			ec2.AddInstance(FakeAmazonEc2.StateStopped, InstanceType.M5Large, FakeAmazonEc2.AzUsEast1A);
			ec2.AddInstance(FakeAmazonEc2.StateStopped, InstanceType.M5Large, FakeAmazonEc2.AzUsEast1B);
			ec2.SetCapacity(FakeAmazonEc2.AzUsEast1A, InstanceType.M5Large, 1);
			ec2.SetCapacity(FakeAmazonEc2.AzUsEast1B, InstanceType.M5Large, 1);

			Assert.AreEqual(2, ec2.GetStoppedInstanceCount());
			await ExpandPoolAsync(ec2.Get(), 10, new());
			Assert.AreEqual(0, ec2.GetStoppedInstanceCount());
			Assert.AreEqual(2, ec2.GetPendingInstanceCount());
			
			// TODO: Verify remainder is reported correctly
		}
		
		[TestMethod]
		public async Task ExpandTwoAzsFullCapacityNoFallback()
		{
			FakeAmazonEc2 ec2 = new ();
			ec2.AddInstance(FakeAmazonEc2.StateStopped, InstanceType.M5Large, FakeAmazonEc2.AzUsEast1A);
			ec2.AddInstance(FakeAmazonEc2.StateStopped, InstanceType.M5Large, FakeAmazonEc2.AzUsEast1B);
			ec2.SetCapacity(FakeAmazonEc2.AzUsEast1A, InstanceType.M5Large, 1);
			ec2.SetCapacity(FakeAmazonEc2.AzUsEast1B, InstanceType.M5Large, 1);

			await ExpandPoolAsync(ec2.Get(), 2, new());
			Assert.AreEqual(0, ec2.GetStoppedInstanceCount());
			Assert.AreEqual(2, ec2.GetPendingInstanceCount());
		}
		
		[TestMethod]
		public async Task ExpandTwoAzsFullCapacityEmptyFallback()
		{
			FakeAmazonEc2 ec2 = new ();
			ec2.AddInstance(FakeAmazonEc2.StateStopped, InstanceType.M5Large, FakeAmazonEc2.AzUsEast1A);
			ec2.AddInstance(FakeAmazonEc2.StateStopped, InstanceType.M5Large, FakeAmazonEc2.AzUsEast1B);
			ec2.SetCapacity(FakeAmazonEc2.AzUsEast1A, InstanceType.M5Large, 1);
			ec2.SetCapacity(FakeAmazonEc2.AzUsEast1B, InstanceType.M5Large, 1);

			await ExpandPoolAsync(ec2.Get(), 2, new(new List<string>()));
			Assert.AreEqual(0, ec2.GetStoppedInstanceCount());
			Assert.AreEqual(2, ec2.GetPendingInstanceCount());
		}
		
		[TestMethod]
		public async Task ExpandLaunchesLastUsedInstancesFirst()
		{
			FakeAmazonEc2 ec2 = new ();
			Instance i1 = ec2.AddInstance(FakeAmazonEc2.StateStopped, InstanceType.M5Large, launchTime: DateTime.Parse("Aug 18, 2018"));
			Instance i2 = ec2.AddInstance(FakeAmazonEc2.StateStopped, InstanceType.M5Large, launchTime: DateTime.Parse("Aug 22, 2018"));
			Instance i3 = ec2.AddInstance(FakeAmazonEc2.StateStopped, InstanceType.M5Large, launchTime: DateTime.Parse("Aug 5, 2018"));
			Instance i4 = ec2.AddInstance(FakeAmazonEc2.StateStopped, InstanceType.M5Large, launchTime: DateTime.Parse("Aug 28, 2018"));
			Instance i5 = ec2.AddInstance(FakeAmazonEc2.StateStopped, InstanceType.M5Large, launchTime: DateTime.Parse("Aug 1, 2018"));
			ec2.SetCapacity(FakeAmazonEc2.AzUsEast1A, InstanceType.M5Large, 2);

			await ExpandPoolAsync(ec2.Get(), 2, new(new List<string>()));
			Assert.AreEqual(3, ec2.GetStoppedInstanceCount());
			Assert.AreEqual(2, ec2.GetPendingInstanceCount());
			Assert.AreEqual(FakeAmazonEc2.StatePending.Name, ec2.GetInstance(i4.InstanceId)!.State.Name);
			Assert.AreEqual(FakeAmazonEc2.StatePending.Name, ec2.GetInstance(i2.InstanceId)!.State.Name);
		}

		[TestMethod]
		public void DistributesInstanceRequestsEvenly()
		{
			AssertInstanceCounts(0, new () { ["a"] = 3, ["b"] = 4 }, new() { ["a"] = 0, ["b"] = 0 }, 0);
			AssertInstanceCounts(0, new () { }, new() { }, 0);
			AssertInstanceCounts(1, new () { ["a"] = 1, ["b"] = 1, ["c"] = 1 }, new() { ["a"] = 1, ["b"] = 0, ["c"] = 0 }, 0);
			AssertInstanceCounts(2, new () { ["a"] = 1, ["b"] = 1, ["c"] = 1 }, new() { ["a"] = 1, ["b"] = 1, ["c"] = 0 }, 0);
			AssertInstanceCounts(5, new () { ["a"] = 10 }, new() { ["a"] = 5 }, 0);
			AssertInstanceCounts(6, new () { ["a"] = 10, ["b"] = 2 }, new() { ["a"] = 4, ["b"] = 2 }, 0);
			AssertInstanceCounts(3, new () { ["a"] = 1, ["b"] = 1, ["c"] = 1 }, new() { ["a"] = 1, ["b"] = 1, ["c"] = 1 }, 0);
			
			// Remainder > 0
			AssertInstanceCounts(10, new () { ["a"] = 3, ["b"] = 4 }, new() { ["a"] = 3, ["b"] = 4 }, 3);
			AssertInstanceCounts(10, new () { }, new() { }, 10);
		}

		private static void AssertInstanceCounts(int requestCount, Dictionary<string, int> availableCountPerAz, Dictionary<string, int> expected, int expectedRemainder)
		{
			Dictionary<string, int> actual = AwsRecyclingFleetManager.DistributeInstanceRequestsOverAzs(
				requestCount, availableCountPerAz, out int actualRemainder, false);
			try
			{
				CollectionAssert.AreEquivalent(expected.ToList(), actual.ToList());
				Assert.AreEqual(expectedRemainder, actualRemainder);
			}
			catch (Exception)
			{
				Console.WriteLine("Expected=" + JsonSerializer.Serialize(expected));
				Console.WriteLine("  Actual=" + JsonSerializer.Serialize(actual));
				throw;
			}
		}

		private async Task ExpandPoolAsync(IAmazonEC2 ec2, int numRequestedInstances, AwsRecyclingFleetManagerSettings settings)
		{
			using ILoggerFactory loggerFactory = LoggerFactory.Create(builder =>
				{
					builder.SetMinimumLevel(LogLevel.Debug);
					builder.AddSimpleConsole(options => { options.SingleLine = true; });
				});

			ILogger<AwsRecyclingFleetManager> logger = loggerFactory.CreateLogger<AwsRecyclingFleetManager>();
			IPool pool = await PoolService.CreatePoolAsync("testPool", null, true, 0, 0, sizeStrategy: PoolSizeStrategy.NoOp);
			using NoOpDogStatsd dogStatsd = new ();
			AwsRecyclingFleetManager manager = new (ec2, AgentCollection, dogStatsd, settings, logger);
			await manager.ExpandPoolAsync(pool, new List<IAgent>(), numRequestedInstances, CancellationToken.None);
		}
	}
}