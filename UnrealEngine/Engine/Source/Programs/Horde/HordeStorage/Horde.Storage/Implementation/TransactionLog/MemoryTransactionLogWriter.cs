// Copyright Epic Games, Inc. All Rights Reserved.

using System.Collections.Concurrent;
using System.Threading.Tasks;
using EpicGames.Horde.Storage;
using Jupiter.Implementation;

namespace Horde.Storage.Implementation
{
    internal class MemoryTransactionLogWriter : ITransactionLogWriter
    {
        private readonly ConcurrentDictionary<NamespaceId, long> _transactionIds = new ConcurrentDictionary<NamespaceId, long>();

        public Task<long> Add(NamespaceId ns, TransactionEvent @event)
        {
            // since we are never reading back the value of the transaction log in Europa we can just fake it by adding incrementing a id

            return DoInsert(ns);
        }

        private async Task<long> DoInsert(NamespaceId ns)
        {
            return await Task.Run(() =>
            {
                return _transactionIds.AddOrUpdate(ns, _ => 0,
                    (_, transactionId) => transactionId + 1);
            });
        }

        public Task<long> Delete(NamespaceId ns, BucketId bucket, KeyId key)
        {
            return DoInsert(ns); 
        }
    }
}
