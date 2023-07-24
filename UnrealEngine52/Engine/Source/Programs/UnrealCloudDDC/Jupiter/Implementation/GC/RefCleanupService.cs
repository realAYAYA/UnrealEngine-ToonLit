// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Linq;
using System.Threading;
using System.Threading.Tasks;
using EpicGames.Horde.Storage;
using Microsoft.Extensions.Logging;
using Microsoft.Extensions.Options;

namespace Jupiter.Implementation
{
    public class RefCleanupState
    {
        public RefCleanupState(IRefCleanup refCleanup)
        {
            RefCleanup = refCleanup;
        }

        public IRefCleanup RefCleanup { get; }
        public Dictionary<NamespaceId, Task> RunningCleanupTasks { get; } = new Dictionary<NamespaceId, Task>();
    }

    public class RefCleanupService : PollingService<RefCleanupState>
    {
        private readonly IOptionsMonitor<GCSettings> _settings;
        private readonly ILeaderElection _leaderElection;
        private readonly IReferencesStore _referencesStore;
        private volatile bool _alreadyPolling;
        private readonly ILogger _logger;

        public RefCleanupService(IOptionsMonitor<GCSettings> settings, IRefCleanup refCleanup, ILeaderElection leaderElection, IReferencesStore referencesStore, ILogger<RefCleanupService> logger) : base(serviceName: nameof(RefCleanupService), settings.CurrentValue.RefCleanupPollFrequency, new RefCleanupState(refCleanup), logger, startAtRandomTime: false)
        {
            _settings = settings;
            _leaderElection = leaderElection;
            _referencesStore = referencesStore;
            _logger = logger;
        }

        protected override bool ShouldStartPolling()
        {
            return _settings.CurrentValue.CleanOldRefRecords;
        }

        public override async Task<bool> OnPoll(RefCleanupState state, CancellationToken cancellationToken)
        {
            if (_alreadyPolling)
            {
                return false;
            }

            _alreadyPolling = true;
            try
            {
                if (!_leaderElection.IsThisInstanceLeader())
                {
                    _logger.LogInformation("Skipped ref cleanup run as this instance was not the leader");
                    return false;
                }
                List<NamespaceId>? namespaces = await _referencesStore.GetNamespaces().ToListAsync(cancellationToken);

                foreach(NamespaceId ns in namespaces)
                {
                    if (state.RunningCleanupTasks.TryGetValue(ns, out Task? runningTask))
                    {
                        if (!runningTask.IsCompleted)
                        {
                            continue;
                        }
                        await runningTask;
                        state.RunningCleanupTasks.Remove(ns);
                    }
                    state.RunningCleanupTasks.Add(ns, DoCleanup(ns, state, cancellationToken));
                }
                return true;

            }
            finally
            {
                _alreadyPolling = false;
            }
        }

        private async Task DoCleanup(NamespaceId ns, RefCleanupState state, CancellationToken cancellationToken)
        {
            _logger.LogInformation("Attempting to run Refs Cleanup of {Namespace}. ", ns);
            try
            {
                int countOfRemovedRecords = await state.RefCleanup.Cleanup(ns, cancellationToken);
                _logger.LogInformation("Ran Refs Cleanup of {Namespace}. Deleted {CountRefRecords}", ns,
                    countOfRemovedRecords);
            }
            catch (Exception e)
            {
                _logger.LogError("Error running Refs Cleanup of {Namespace}. {Exception}", ns, e);
            }
        }
    }
}
