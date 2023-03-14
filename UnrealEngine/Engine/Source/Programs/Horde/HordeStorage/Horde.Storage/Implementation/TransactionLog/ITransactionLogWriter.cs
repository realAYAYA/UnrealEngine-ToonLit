// Copyright Epic Games, Inc. All Rights Reserved.

using System.Threading.Tasks;
using EpicGames.Horde.Storage;
using Jupiter.Implementation;

namespace Horde.Storage.Implementation
{
    public interface ITransactionLogWriter
    {
        Task<long> Add(NamespaceId ns, TransactionEvent @event);
        Task<long> Delete(NamespaceId ns, BucketId bucket, KeyId key);
    }
}
