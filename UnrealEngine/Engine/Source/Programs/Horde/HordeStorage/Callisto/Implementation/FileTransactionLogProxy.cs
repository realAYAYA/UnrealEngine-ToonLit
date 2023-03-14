// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Concurrent;
using System.IO;
using System.Linq;
using EpicGames.Horde.Storage;
using Microsoft.Extensions.Options;

namespace Callisto.Implementation
{
    public class FileTransactionLogProxy : ITransactionLogs
    {
        private readonly IOptionsMonitor<CallistoSettings> _settings;
        private readonly ConcurrentDictionary<NamespaceId, FileTransactionLog> _writers = new ConcurrentDictionary<NamespaceId, FileTransactionLog>();

        private readonly DirectoryInfo _root;

        public FileTransactionLogProxy(IOptionsMonitor<CallistoSettings> settings)
        {
            _settings = settings;
            string root = settings.CurrentValue.TransactionLogRoot;

            if (!Directory.Exists(root))
            {
                Directory.CreateDirectory(root);
            }

            _root = new DirectoryInfo(root);
            foreach (DirectoryInfo dir in _root.EnumerateDirectories())
            {
                NamespaceId ns;
                try
                {
                    ns = new NamespaceId(dir.Name);
                }
                catch (ArgumentException)
                {
                    // ignore any directories that are not valid namespaces
                    continue;
                }
                _writers.TryAdd(ns, new FileTransactionLog(_settings, _root, ns));
            }
        }

        public NamespaceId[] GetNamespaces()
        {
            return _writers.Keys.Distinct().ToArray();
        }

        public ITransactionLog Get(NamespaceId ns)
        {
            FileTransactionLog writer = _writers.GetOrAdd(ns, s => new FileTransactionLog(_settings, _root, ns));
            return writer;
        }
    }
}
