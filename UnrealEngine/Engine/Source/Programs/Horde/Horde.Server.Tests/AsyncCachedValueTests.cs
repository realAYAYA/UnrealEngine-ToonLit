// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading;
using System.Threading.Tasks;
using Horde.Server.Utilities;
using Microsoft.VisualStudio.TestTools.UnitTesting;

namespace Horde.Server.Tests
{
	[TestClass]
	public class AsyncCachedValueTests
	{
		[TestMethod]
		public async Task BasicTest()
		{
			using CancellationTokenSource cts = new CancellationTokenSource();
			for (int idx = 0; idx < 20; idx++)
			{
				AsyncCachedValue<int> value = new AsyncCachedValue<int>(() => Task.FromResult(GenerateValue()), TimeSpan.FromSeconds(1.0));

				await value.GetAsync(cts.Token);
				await value.GetAsync(cts.Token);
				await Task.Delay(100);
				await value.GetAsync(cts.Token);
				await Task.Delay(1000);
				await value.GetAsync(cts.Token);
			}
		}

		static int GenerateValue()
		{
			return 7;
		}
	}
}
