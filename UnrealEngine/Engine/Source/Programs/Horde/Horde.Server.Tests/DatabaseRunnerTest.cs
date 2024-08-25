// Copyright Epic Games, Inc. All Rights Reserved.

using System.Runtime.InteropServices;
using System.Threading;
using System.Threading.Tasks;
using Horde.Server.Server;
using Microsoft.Extensions.Logging.Abstractions;
using Microsoft.VisualStudio.TestTools.UnitTesting;

namespace Horde.Server.Tests
{
	[TestClass]
	public class DatabaseRunnerTest
	{
		[TestMethod]
		public void RunMongoDbTest()
		{
			using MongoDbRunnerLocal runner = new MongoDbRunnerLocal();
			runner.Start();
			Thread.Sleep(100);
			runner.Stop();
		}

		[TestMethod]
		public async Task RunRedisTestAsync()
		{
			if (RuntimeInformation.IsOSPlatform(OSPlatform.Windows))
			{
				await using RedisProcess runner = new RedisProcess(NullLogger.Instance);
				runner.Start("--save \"\" --appendonly no");
				await Task.Delay(100);
				await runner.StopAsync();
			}
		}
	}
}
