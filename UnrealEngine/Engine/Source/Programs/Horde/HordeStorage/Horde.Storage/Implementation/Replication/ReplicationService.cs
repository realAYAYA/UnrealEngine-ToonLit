// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Threading;
using System.Threading.Tasks;
using EpicGames.Horde.Storage;
using Jupiter;
using Microsoft.Extensions.DependencyInjection;
using Microsoft.Extensions.Options;
using Serilog;

namespace Horde.Storage.Implementation
{
    public class ReplicationState
    {
        public List<IReplicator> Replicators { get; } = new List<IReplicator>();
    }

    // ReSharper disable once ClassNeverInstantiated.Global
    public class ReplicationService : PollingService<ReplicationState>
    {
        private readonly IOptionsMonitor<ReplicationSettings> _settings;
        private readonly ILeaderElection _leaderElection;
        private readonly ILogger _logger = Log.ForContext<ReplicationService>();
        private readonly Dictionary<string, Task<bool>> _currentReplications = new Dictionary<string, Task<bool>>();

        protected override bool ShouldStartPolling()
        {
            return _settings.CurrentValue.Enabled;
        }

        public ReplicationService(IOptionsMonitor<ReplicationSettings> settings, IServiceProvider provider, ILeaderElection leaderElection) : base(serviceName: nameof(ReplicationService), TimeSpan.FromSeconds(settings.CurrentValue.ReplicationPollFrequencySeconds), new ReplicationState(), startAtRandomTime: true)
        {
            _settings = settings;
            _leaderElection = leaderElection;

            _leaderElection.OnLeaderChanged += OnLeaderChanged;

            DirectoryInfo di = new DirectoryInfo(settings.CurrentValue.StateRoot);
            Directory.CreateDirectory(di.FullName);

            foreach (ReplicatorSettings replicator in settings.CurrentValue.Replicators)
            {
                try
                {
                    State.Replicators.Add(CreateReplicator(replicator, provider));
                }
                catch (Exception e)
                {
                    _logger.Error(e, "Failed to create replicator {Name}", replicator.ReplicatorName);
                }
            }
        }

        private void OnLeaderChanged(object? sender, OnLeaderChangedEventArgs e)
        {
            if (e.IsLeader)
            {
                return;
            }

            // if we are no longer the leader cancel any pending replications
            // TODO: Reset replication token if we are the new leader
            /*Task[] stopReplicatingTasks = State.Replicators.Select(replicator => replicator.StopReplicating()).ToArray();
            Task.WaitAll(stopReplicatingTasks);*/

        }

        private static IReplicator CreateReplicator(ReplicatorSettings replicatorSettings, IServiceProvider provider)
        {
            switch (replicatorSettings.Version)
            {
                case ReplicatorVersion.V1:
                    return ActivatorUtilities.CreateInstance<ReplicatorV1>(provider, replicatorSettings);
                case ReplicatorVersion.Refs:
                    return ActivatorUtilities.CreateInstance<RefsReplicator>(provider, replicatorSettings);
                default:
                    throw new NotImplementedException($"Unknown replicator version: {replicatorSettings.Version}");
            }
        }

        public override async Task<bool> OnPoll(ReplicationState state, CancellationToken cancellationToken)
        {
            if (!_settings.CurrentValue.Enabled)
            {
                _logger.Information("Skipped running replication as it is disabled");
                return false;
            }

            if (!_leaderElection.IsThisInstanceLeader())
            {
                _logger.Information("Skipped running replicators because this instance was not the leader");
                return false;
            }

            _logger.Information("Polling for new replication to start");

            foreach (IReplicator replicator in state.Replicators)
            {
                if (_currentReplications.TryGetValue(replicator.Info.ReplicatorName, out Task<bool>? replicationTask))
                {
                    if (replicationTask.IsCompleted)
                    {
                        _currentReplications.Remove(replicator.Info.ReplicatorName);

                        if (replicationTask.IsFaulted)
                        {
                            // we log the error but avoid raising it to make sure all replicators actually get to run even if there is a faulting one
                            _logger.Error(replicationTask.Exception, "Unhandled exception in replicator {Name}", replicator.Info.ReplicatorName);
                            continue;
                        }
                     
                        DateTime time = DateTime.Now;
                        _logger.Information("Joining replication task for replicator {Name}", replicator.Info.ReplicatorName);
                        await replicationTask;
                        _logger.Information("Waited for replication task {Name} for {Duration} seconds", replicator.Info.ReplicatorName, (DateTime.Now - time).TotalSeconds);
                    }
                    else
                    {
                        _logger.Debug("Replication of replicator: {Name} is still running", replicator.Info.ReplicatorName);
                        // if the replication is still running let it continue to run
                        continue;
                    }
                }

                // start a new run of the replication
                _logger.Debug("Triggering new replication of replicator: {Name}", replicator.Info.ReplicatorName);
                Task<bool> newReplication = replicator.TriggerNewReplications();
                _currentReplications[replicator.Info.ReplicatorName] = newReplication;
            }

            if (state.Replicators.Count == 0)
            {
                _logger.Information("Finished replication poll, but no replicators configured to run.");
            }

            return state.Replicators.Count != 0;

        }

        protected override async Task OnStopping(ReplicationState state)
        {
            await Task.WhenAll(state.Replicators.Select(replicator => replicator.StopReplicating()).ToArray());

            // we should have been stopped first so this should not be needed, but to make sure state is stored we dispose of it again
            foreach (IReplicator replicator in State.Replicators)
            {
                replicator.Dispose();
            }
        }
        
        public IEnumerable<IReplicator> GetReplicators(NamespaceId ns)
        {
            return State.Replicators
                .Where(pair => ns == pair.Info.NamespaceToReplicate)
                .Select(pair => pair);
        }
    }
}
