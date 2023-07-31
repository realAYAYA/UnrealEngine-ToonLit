// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Threading.Tasks;
using EpicGames.Horde.Storage;
using Jupiter.Implementation;

namespace Callisto.Implementation
{
    public interface ITransactionLog
    {
        Task<long> NewTransaction(AddTransactionEvent @event);

        Task<long> NewTransaction(RemoveTransactionEvent @event);

        Task<TransactionEvents> Get(long index, int count = 100, string? notSeenAtSite = null);
        Task Drop();
        TransactionLogDescription Describe();
    }

    public interface ITransactionLogs
    {
        NamespaceId[] GetNamespaces();

        ITransactionLog Get(NamespaceId ns);
    }

    public class TransactionLogDescription
    {
        public TransactionLogDescription(Guid generation, List<string> knownSites)
        {
            Generation = generation;
            KnownSites = knownSites;
        }

        public Guid Generation { get; }
        public List<string> KnownSites { get; }
    }

    public class TransactionEvents
    {
        public TransactionEvents(TransactionEvent[] events, long currentOffset, long? transactionLogOffsetMismatchFoundAt)
        {
            Events = events;
            CurrentOffset = currentOffset;
            TransactionLogMismatchFoundAt = transactionLogOffsetMismatchFoundAt;
        }

        public TransactionEvent[] Events { get; set; }
        public long CurrentOffset { get; set; }
        public long? TransactionLogMismatchFoundAt { get; }
    }
}
