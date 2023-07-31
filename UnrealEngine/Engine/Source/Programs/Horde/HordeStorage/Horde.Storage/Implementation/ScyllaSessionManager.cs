// Copyright Epic Games, Inc. All Rights Reserved.

using Cassandra;

namespace Horde.Storage.Implementation
{
    public interface IScyllaSessionManager
    {
        ISession GetSessionForReplicatedKeyspace();
        ISession GetSessionForLocalKeyspace();

        /// <summary>
        /// True if scylla specific features can be used
        /// </summary>
        bool IsScylla { get; }

        /// <summary>
        /// True when using a base cassandra protocol
        /// </summary>
        bool IsCassandra { get; }
    }

    public class ScyllaSessionManager : IScyllaSessionManager
    {
        private readonly ISession _replicatedSession;
        private readonly ISession _localSession;

        public ScyllaSessionManager(ISession replicatedSession, ISession localSession, bool isScylla, bool isCassandra)
        {
            _replicatedSession = replicatedSession;
            _localSession = localSession;
            IsScylla = isScylla;
            IsCassandra = isCassandra;
        }

        public ISession GetSessionForReplicatedKeyspace()
        {
            return _replicatedSession;

        }

        public ISession GetSessionForLocalKeyspace()
        {
            return _localSession;
        }

        public bool IsScylla { get; }
        public bool IsCassandra { get; }
    }
}
