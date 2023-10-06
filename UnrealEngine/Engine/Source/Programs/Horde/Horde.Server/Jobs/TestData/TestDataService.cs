// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Threading;
using System.Threading.Tasks;
using EpicGames.Core;
using Horde.Server.Streams;
using Horde.Server.Utilities;
using HordeCommon;
using Microsoft.Extensions.Hosting;
using Microsoft.Extensions.Logging;
using Microsoft.Extensions.Options;

namespace Horde.Server.Jobs.TestData
{
	/// <summary>
	/// Device management service
	/// </summary>
	public sealed class TestDataService : IHostedService, IDisposable
	{
		
		readonly ITestDataCollection _testData;
		readonly IOptionsMonitor<ServerSettings> _settings;
		readonly ITicker _ticker;
		readonly ILogger<TestDataService> _logger;

		/// <summary>
		/// Device service constructor
		/// </summary>
		public TestDataService(ITestDataCollection testData, IOptionsMonitor<ServerSettings> settings, IClock clock, ILogger<TestDataService> logger)
		{
			_testData = testData;
			_settings = settings;
			_logger = logger;
			_ticker = clock.AddSharedTicker<TestDataService>(TimeSpan.FromMinutes(10.0), TickAsync, logger);
		}

		/// <inheritdoc/>
		public async Task StartAsync(CancellationToken cancellationToken)
		{
			await _ticker.StartAsync();

			try
			{
				await _testData.UpgradeAsync();
			}
			catch (Exception ex)
			{
				_logger.LogError(ex, "Exception while upgrading test data collection: {Message}", ex.Message);
			}
		}

		/// <inheritdoc/>
		public async Task StopAsync(CancellationToken cancellationToken)
		{
			await _ticker.StopAsync();
		}

		/// <inheritdoc/>
		public void Dispose()
		{
			_ticker.Dispose();
		}

		/// <summary>
		/// Ticks service
		/// </summary>
		async ValueTask TickAsync(CancellationToken stoppingToken)
		{
			if (!stoppingToken.IsCancellationRequested)
			{
				try
				{
					await _testData.UpdateAsync(_settings.CurrentValue.TestDataRetainMonths);					
				}
				catch (Exception ex)
				{
					_logger.LogError(ex, "Exception while ticking test data collection: {Message}", ex.Message);
				}

			}
		}

		internal async Task TickForTestingAsync()
		{
			await TickAsync(CancellationToken.None);			
		}

		/// <summary>
		/// Find test streams
		/// </summary>
		/// <param name="streamIds"></param>
		/// <returns></returns>
		public async Task<List<ITestStream>> FindTestStreams(StreamId[] streamIds)
		{

			return await _testData.FindTestStreams(streamIds);			
		}

		/// <summary>
		/// Find tests
		/// </summary>
		/// <param name="testIds"></param>
		/// <returns></returns>
		public async Task<List<ITest>> FindTests(TestId[] testIds)
		{
			return await _testData.FindTests(testIds);
		}

		/// <summary>
		/// Find test suites
		/// </summary>
		/// <param name="suiteIds"></param>
		/// <returns></returns>
		public async Task<List<ITestSuite>> FindTestSuites(TestSuiteId[] suiteIds)
		{
			return await _testData.FindTestSuites(suiteIds);
		}

		/// <summary>
		/// Find test meta data
		/// </summary>
		/// <param name="projectNames"></param>
		/// <param name="platforms"></param>
		/// <param name="configurations"></param>
		/// <param name="buildTargets"></param>
		/// <param name="rhi"></param>
		/// <param name="variation"></param>
		/// <param name="metaIds"></param>
		/// <returns></returns>
		public async Task<List<ITestMeta>> FindTestMeta(string[]? projectNames = null, string[]? platforms = null, string[]? configurations = null, string[]? buildTargets = null, string? rhi = null, string? variation = null, TestMetaId[]? metaIds = null)
		{
			return await _testData.FindTestMeta(projectNames, platforms, configurations, buildTargets, rhi, variation, metaIds);
		}

		/// <summary>
		/// Gets test data refs
		/// </summary>
		/// <param name="streamIds"></param>
		/// <param name="metaIds"></param>
		/// <param name="testIds"></param>
		/// <param name="suiteIds"></param>
		/// <param name="minCreateTime"></param>
		/// <param name="maxCreateTime"></param>
		/// <param name="minChange"></param>
		/// <param name="maxChange"></param>
		/// <returns></returns>
		public async Task<List<ITestDataRef>> FindTestRefs(StreamId[] streamIds, TestMetaId[] metaIds, string[]? testIds = null, string[]? suiteIds = null, DateTime? minCreateTime = null, DateTime? maxCreateTime = null, int? minChange = null, int? maxChange = null)
		{
			TestId[]? tids = testIds?.ConvertAll(x => TestId.Parse(x));
			TestSuiteId[]? sids = suiteIds?.ConvertAll(x => TestSuiteId.Parse(x));

			return await _testData.FindTestRefs(streamIds, metaIds, tids, sids, minCreateTime, maxCreateTime, minChange, maxChange);
		}

		/// <summary>
		/// Find test details
		/// </summary>
		/// <param name="ids"></param>
		/// <returns></returns>
		public async Task<List<ITestDataDetails>> FindTestDetails(TestRefId[] ids)
		{
			return await _testData.FindTestDetails(ids);
		}
	}
}