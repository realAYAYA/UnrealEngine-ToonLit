// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Threading;
using System.Threading.Tasks;
using EpicGames.Horde.Storage;
using Jupiter.Common;
using Jupiter.Implementation.Blob;
using Microsoft.Extensions.Logging;
using Microsoft.Extensions.Options;
using OpenTelemetry.Trace;

namespace Jupiter.Implementation
{
	public class RefConsistencyState
	{
	}

	// ReSharper disable once ClassNeverInstantiated.Global
	public class RefStoreConsistencyCheckService : PollingService<ConsistencyState>
	{
		private readonly IOptionsMonitor<ConsistencyCheckSettings> _settings;
		private readonly IOptionsMonitor<UnrealCloudDDCSettings> _unrealCloudDDCSettings;
		private readonly IServiceProvider _provider;
		private readonly ILeaderElection _leaderElection;
		private readonly IReferencesStore _referencesStore;
		private readonly IBlobIndex _blobIndex;
		private readonly Tracer _tracer;
		private readonly ILogger _logger;
		private readonly INamespacePolicyResolver _policyResolver;

		protected override bool ShouldStartPolling()
		{
			return _settings.CurrentValue.EnableRefStoreChecks;
		}

		public RefStoreConsistencyCheckService(IOptionsMonitor<ConsistencyCheckSettings> settings, IOptionsMonitor<UnrealCloudDDCSettings> unrealCloudDDCSettings, IServiceProvider provider, ILeaderElection leaderElection, IReferencesStore referencesStore, IBlobIndex blobIndex, Tracer tracer, ILogger<BlobStoreConsistencyCheckService> logger, INamespacePolicyResolver policyResolver) : base(serviceName: nameof(BlobStoreConsistencyCheckService), TimeSpan.FromSeconds(settings.CurrentValue.ConsistencyCheckPollFrequencySeconds), new ConsistencyState(), logger)
		{
			_settings = settings;
			_unrealCloudDDCSettings = unrealCloudDDCSettings;
			_provider = provider;
			_leaderElection = leaderElection;
			_referencesStore = referencesStore;
			_blobIndex = blobIndex;
			_tracer = tracer;
			_logger = logger;
			_policyResolver = policyResolver;
		}

		public override async Task<bool> OnPollAsync(ConsistencyState state, CancellationToken cancellationToken)
		{
			if (!_settings.CurrentValue.EnableRefStoreChecks)
			{
				_logger.LogInformation("Skipped running ref store consistency check as it is disabled");
				return false;
			}

			await RunConsistencyCheckAsync();

			return true;
		}

		public async Task RunConsistencyCheckAsync()
		{
			if (!_leaderElection.IsThisInstanceLeader())
			{
				_logger.LogInformation("Skipped running ref store consistency check because this instance was not the leader");
				return;
			}

			ulong countOfRefsChecked = 0;
			ulong countOfMissingLastAccessTime = 0;
			await foreach ((NamespaceId ns, BucketId bucket, RefId refId) in _referencesStore.GetRecordsWithoutAccessTimeAsync())
			{
				using TelemetrySpan scope = _tracer.StartActiveSpan("consistency_check.ref_store")
					.SetAttribute("operation.name", "consistency_check.ref_store")
					.SetAttribute("resource.name", $"{ns}.{bucket}.{refId}");

				DateTime? lastAccessTime = await _referencesStore.GetLastAccessTimeAsync(ns, bucket, refId);
				if (!lastAccessTime.HasValue)
				{
					// if there is no last access time record we add one so that the two tables are consistent, this will make the ref record be considered by the GC and thus cleanup anything that might be very old (but its added as a new record so it will take until the configured cleanup time has passed)
					await _referencesStore.UpdateLastAccessTimeAsync(ns, bucket, refId, DateTime.Now);

					Interlocked.Increment(ref countOfMissingLastAccessTime);
				}

				Interlocked.Increment(ref countOfRefsChecked);
			}
			_logger.LogInformation("Consistency check finished for ref store, found {CountOfMissingLastAccessTime} refs that were lacking last access time. Processed {CountOfRefs} refs.", countOfMissingLastAccessTime, countOfRefsChecked);
		}

		protected override Task OnStopping(ConsistencyState state)
		{
			return Task.CompletedTask;
		}
	}
}
