// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Threading;
using System.Threading.Tasks;
using EpicGames.Core;
using EpicGames.Horde.Streams;
using HordeCommon;
using Microsoft.Extensions.Hosting;
using Microsoft.Extensions.Logging;
using Microsoft.Extensions.Options;

namespace Horde.Server.Jobs.TestData
{
	/// <summary>
	/// Device management service
	/// </summary>
	public sealed class TestDataService : IHostedService, IAsyncDisposable
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
				await _testData.UpgradeAsync(cancellationToken);
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
		public async ValueTask DisposeAsync()
		{
			await _ticker.DisposeAsync();
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
					await _testData.UpdateAsync(_settings.CurrentValue.TestDataRetainMonths, stoppingToken);
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
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		/// <returns></returns>
		public async Task<IReadOnlyList<ITestStream>> FindTestStreamsAsync(StreamId[] streamIds, CancellationToken cancellationToken = default)
		{
			return await _testData.FindTestStreamsAsync(streamIds, cancellationToken);
		}

		/// <summary>
		/// Find tests
		/// </summary>
		/// <param name="testIds"></param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		/// <returns></returns>
		public async Task<IReadOnlyList<ITest>> FindTestsAsync(TestId[] testIds, CancellationToken cancellationToken = default)
		{
			return await _testData.FindTestsAsync(testIds, cancellationToken);
		}

		/// <summary>
		/// Find test suites
		/// </summary>
		/// <param name="suiteIds"></param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		/// <returns></returns>
		public async Task<IReadOnlyList<ITestSuite>> FindTestSuitesAsync(TestSuiteId[] suiteIds, CancellationToken cancellationToken = default)
		{
			return await _testData.FindTestSuitesAsync(suiteIds, cancellationToken);
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
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		/// <returns></returns>
		public async Task<IReadOnlyList<ITestMeta>> FindTestMetaAsync(string[]? projectNames = null, string[]? platforms = null, string[]? configurations = null, string[]? buildTargets = null, string? rhi = null, string? variation = null, TestMetaId[]? metaIds = null, CancellationToken cancellationToken = default)
		{
			return await _testData.FindTestMetaAsync(projectNames, platforms, configurations, buildTargets, rhi, variation, metaIds, cancellationToken);
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
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		/// <returns></returns>
		public async Task<IReadOnlyList<ITestDataRef>> FindTestRefsAsync(StreamId[] streamIds, TestMetaId[] metaIds, string[]? testIds = null, string[]? suiteIds = null, DateTime? minCreateTime = null, DateTime? maxCreateTime = null, int? minChange = null, int? maxChange = null, CancellationToken cancellationToken = default)
		{
			TestId[]? tids = testIds?.ConvertAll(x => TestId.Parse(x));
			TestSuiteId[]? sids = suiteIds?.ConvertAll(x => TestSuiteId.Parse(x));

			return await _testData.FindTestRefsAsync(streamIds, metaIds, tids, sids, minCreateTime, maxCreateTime, minChange, maxChange, cancellationToken);
		}

		/// <summary>
		/// Find test details
		/// </summary>
		/// <param name="ids"></param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		/// <returns></returns>
		public async Task<IReadOnlyList<ITestDataDetails>> FindTestDetailsAsync(TestRefId[] ids, CancellationToken cancellationToken = default)
		{
			return await _testData.FindTestDetailsAsync(ids, cancellationToken);
		}
	}
}