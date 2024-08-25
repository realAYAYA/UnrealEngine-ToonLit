// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Threading;
using System.Threading.Tasks;
using Jupiter.Implementation;
using Microsoft.Extensions.Logging.Abstractions;
using Microsoft.Extensions.Options;
using Microsoft.VisualStudio.TestTools.UnitTesting;
using Moq;

namespace Jupiter.UnitTests
{
	[TestClass]
	public class BlobCleanupServiceTest
	{
		[TestMethod]
		public async Task CleanupOnPollAsync()
		{
			GCSettings gcSettings = new GCSettings()
			{
				CleanOldBlobs = false
			};
			IOptionsMonitor<GCSettings> gcSettingsMon = Mock.Of<IOptionsMonitor<GCSettings>>(_ => _.CurrentValue == gcSettings);
			await using BlobCleanupService blobCleanupService = new BlobCleanupService(Mock.Of<IServiceProvider>(), gcSettingsMon, NullLogger<BlobCleanupService>.Instance);

			Mock<IBlobCleanup> store1 = new Mock<IBlobCleanup>();
			store1.Setup(cleanup => cleanup.ShouldRun()).Returns(true);
			Mock<IBlobCleanup> store2 = new Mock<IBlobCleanup>();
			store2.Setup(cleanup => cleanup.ShouldRun()).Returns(true);
			blobCleanupService.RegisterCleanup(store1.Object);
			blobCleanupService.RegisterCleanup(store2.Object);

			using CancellationTokenSource tokenSource = new CancellationTokenSource();
			await blobCleanupService.OnPollAsync(blobCleanupService.State, tokenSource.Token);
			
			store1.Verify(m => m.CleanupAsync(It.IsAny<CancellationToken>()), Times.Once);
			store2.Verify(m => m.CleanupAsync(It.IsAny<CancellationToken>()), Times.Once);
		}
	}
}
