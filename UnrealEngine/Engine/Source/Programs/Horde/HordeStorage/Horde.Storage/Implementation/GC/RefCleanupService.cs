// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Threading;
using System.Threading.Tasks;
using Dasync.Collections;
using EpicGames.Horde.Storage;
using Jupiter;
using Microsoft.Extensions.Options;
using Serilog;

namespace Horde.Storage.Implementation
{
    public class RefCleanupState
    {
        public RefCleanupState(IRefsStore refs, IRefCleanup refCleanup)
        {
            Refs = refs;
            RefCleanup = refCleanup;
        }

        public IRefsStore Refs { get; }
        public IRefCleanup RefCleanup { get; }
        public Dictionary<NamespaceId, Task> RunningCleanupTasks { get; } = new Dictionary<NamespaceId, Task>();
    }

    public class RefCleanupService : PollingService<RefCleanupState>
    {
        private readonly IOptionsMonitor<GCSettings> _settings;
        private readonly ILeaderElection _leaderElection;
        private readonly IReferencesStore _referencesStore;
        private volatile bool _alreadyPolling;
        private readonly ILogger _logger = Log.ForContext<RefCleanupService>();

        public RefCleanupService(IOptionsMonitor<GCSettings> settings, IRefsStore store, IRefCleanup refCleanup, ILeaderElection leaderElection, IReferencesStore referencesStore) : base(serviceName: nameof(RefCleanupService), settings.CurrentValue.RefCleanupPollFrequency, new RefCleanupState(store, refCleanup), startAtRandomTime: false)
        {
            _settings = settings;
            _leaderElection = leaderElection;
            _referencesStore = referencesStore;
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
                    _logger.Information("Skipped ref cleanup run as this instance was not the leader");
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

                // Do not run the cleanup of legacy namespaces as this can take a very long time and we do not care about these anymore
                /*
                 await foreach (NamespaceId ns in state.Refs.GetNamespaces().WithCancellation(cancellationToken))
                {
                    _logger.Information("Attempting to run Refs Cleanup of {Namespace}. ", ns);
                    try
                    {
                        int countOfRemovedRecords = await state.RefCleanup.Cleanup(ns, cancellationToken);
                        _logger.Information("Ran Refs Cleanup of {Namespace}. Deleted {CountRefRecords}", ns, countOfRemovedRecords);
                    }
                    catch (Exception e)
                    {
                        _logger.Error("Error running Refs Cleanup of {Namespace}. {Exception}", ns, e);
                    }
                }*/
                return true;

            }
            finally
            {
                _alreadyPolling = false;
            }
        }

        private async Task DoCleanup(NamespaceId ns, RefCleanupState state, CancellationToken cancellationToken)
        {
            _logger.Information("Attempting to run Refs Cleanup of {Namespace}. ", ns);
            try
            {
                int countOfRemovedRecords = await state.RefCleanup.Cleanup(ns, cancellationToken);
                _logger.Information("Ran Refs Cleanup of {Namespace}. Deleted {CountRefRecords}", ns,
                    countOfRemovedRecords);
            }
            catch (Exception e)
            {
                _logger.Error("Error running Refs Cleanup of {Namespace}. {Exception}", ns, e);
            }
        }
    }
}
