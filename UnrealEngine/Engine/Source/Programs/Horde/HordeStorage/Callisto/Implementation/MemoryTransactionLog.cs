// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Concurrent;
using System.Collections.Generic;
using System.Linq;
using System.Threading.Tasks;
using EpicGames.Horde.Storage;
using Jupiter.Implementation;

namespace Callisto.Implementation
{
    // since we back this with a list we do not support as many values as the interface expects (a full long)
    // but as this is for development testing only this should be okay
    public class MemoryTransactionLog : ITransactionLog
    {
        private List<TransactionEvent> _events = new List<TransactionEvent>();

        private Guid _generation = Guid.NewGuid();

        private Task<long> InsertNewTransaction(TransactionEvent @event)
        {
            long eventId;
            lock (_events)
            {
                eventId = _events.Count;
                _events.Add(@event);
            }

            return Task.FromResult(eventId);
        }

        public async Task<long> NewTransaction(AddTransactionEvent @event)
        {
            return await InsertNewTransaction(@event);
        }

        public async Task<long> NewTransaction(RemoveTransactionEvent @event)
        {
            return await InsertNewTransaction(@event);
        }

        public Task<TransactionEvents> Get(long index, int count, string? notSeenAtSite)
        {
            if (!string.IsNullOrEmpty(notSeenAtSite))
            {
                throw new NotImplementedException("Memory transaction log does not support site filtering");
            }

            if (count != 1)
            {
                throw new NotImplementedException("Memory transaction log only support returning a single event");
            }

            return Task.FromResult(new TransactionEvents(new [] {_events[(int)index]}, 0, null));
        }

        public Task Drop()
        {
            _events = new List<TransactionEvent>();
            _generation = Guid.NewGuid();

            return Task.CompletedTask;
        }

        public TransactionLogDescription Describe()
        {
            return new TransactionLogDescription(_generation, _events.SelectMany((e) => e.Locations).Distinct().ToList());
        }
    }

    class MemoryTransactionLogs : ITransactionLogs
    {
        private readonly ConcurrentDictionary<NamespaceId, MemoryTransactionLog> _transactionLogs = new ConcurrentDictionary<NamespaceId, MemoryTransactionLog>();

        public NamespaceId[] GetNamespaces()
        {
            return _transactionLogs.Keys.ToArray();
        }

        public ITransactionLog Get(NamespaceId ns)
        {
            return _transactionLogs.GetOrAdd(ns, s => new MemoryTransactionLog());
        }
    }
}
