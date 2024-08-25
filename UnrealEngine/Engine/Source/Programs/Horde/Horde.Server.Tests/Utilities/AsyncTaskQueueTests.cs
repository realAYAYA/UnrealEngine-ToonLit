// Copyright Epic Games, Inc. All Rights Reserved.

using System.Threading.Tasks;
using Horde.Server.Utilities;
using Microsoft.Extensions.Logging.Abstractions;
using Microsoft.VisualStudio.TestTools.UnitTesting;

namespace Horde.Server.Tests.Utilities
{
	[TestClass]
	public class AsyncTaskQueueTests
	{
		[TestMethod]
		public async Task TestQueueAsync()
		{
			await using AsyncTaskQueue queue = new AsyncTaskQueue(NullLogger.Instance);

			bool executed1 = false;
			queue.Enqueue(async _ =>
			{
				await Task.Yield();
				executed1 = true;
			});

			bool executed2 = false;
			queue.Enqueue(async _ =>
			{
				await Task.Yield();
				executed2 = true;
			});

			await queue.FlushAsync();

			Assert.IsTrue(executed1);
			Assert.IsTrue(executed2);
		}
	}
}
