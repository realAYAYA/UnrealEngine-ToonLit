﻿// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Concurrent;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Threading;
using System.Threading.Tasks;
using EpicGames.Core;
using EpicGames.Horde.Storage;
using Microsoft.Extensions.Options;
using Microsoft.Extensions.Logging;
using OpenTelemetry.Trace;

namespace Jupiter.Implementation
{
    public class FileSystemStore : IBlobStore, IBlobCleanup 
    {
        private readonly ILogger _logger;
        private readonly IOptionsMonitor<FilesystemSettings> _settings;
        private readonly Tracer _tracer;
        private readonly ConcurrentDictionary<NamespaceId, IStorageBackend> _backends = new ConcurrentDictionary<NamespaceId, IStorageBackend>();

        public FileSystemStore(IOptionsMonitor<FilesystemSettings> settings, Tracer tracer, ILogger<FileSystemStore> logger)
        {
            _settings = settings;
            _tracer = tracer;
            _logger = logger;
        }

        private IStorageBackend GetBackend(NamespaceId ns)
        {
            return _backends.GetOrAdd(ns, x => new FileStorageBackend(_logger, GetFilesystemPath(x), _settings));
        }

        private string GetRootDir()
        {
            return PathUtil.ResolvePath(_settings.CurrentValue.RootDir);
        }

        public static string GetFilesystemPath(BlobIdentifier blob)
        {
            const int CountOfCharactersPerDirectory = 2;
            string objectName = blob.ToString();
            string firstPart = objectName.Substring(0, CountOfCharactersPerDirectory);
            string secondPart = objectName.Substring(CountOfCharactersPerDirectory, CountOfCharactersPerDirectory);
            string fileName = objectName;

            return Path.Combine(firstPart, secondPart, fileName);
        }

        public static FileInfo GetFilesystemPath(string rootDir, NamespaceId ns, BlobIdentifier blob)
        {
            return new FileInfo(Path.Combine(rootDir, ns.ToString(), GetFilesystemPath(blob)));
        }

        public DirectoryReference GetFilesystemPath(NamespaceId ns)
        {
            return DirectoryReference.Combine(new DirectoryReference(GetRootDir()), ns.ToString());
        }

        public async Task<BlobIdentifier> PutObject(NamespaceId ns, ReadOnlyMemory<byte> content, BlobIdentifier blobIdentifier)
        {
			using EpicGames.Core.ReadOnlyMemoryStream stream = new EpicGames.Core.ReadOnlyMemoryStream(content);
			return await PutObject(ns, stream, blobIdentifier);
        }

        public async Task<BlobIdentifier> PutObject(NamespaceId ns, Stream content, BlobIdentifier blobIdentifier)
        {
            string path = GetFilesystemPath(blobIdentifier);
            await GetBackend(ns).WriteAsync(path, content, CancellationToken.None);
            return blobIdentifier;
        }

        public async Task<BlobIdentifier> PutObject(NamespaceId ns, byte[] content, BlobIdentifier blobIdentifier)
        {
			using MemoryStream stream = new MemoryStream(content);
			return await PutObject(ns, stream, blobIdentifier);
        }

        public async Task<BlobContents> GetObject(NamespaceId ns, BlobIdentifier blob, LastAccessTrackingFlags flags)
        {
            string path = GetFilesystemPath(blob);

            BlobContents? contents = await GetBackend(ns).TryReadAsync(path, flags, CancellationToken.None);
            if (contents == null)
            {
                throw new BlobNotFoundException(ns, blob);
            }

            return contents;
        }

        public async Task<bool> Exists(NamespaceId ns, BlobIdentifier blob, bool forceCheck)
        {
            string path = GetFilesystemPath(blob);
            return await GetBackend(ns).ExistsAsync(path);
        }

        public async Task DeleteObject(NamespaceId ns, BlobIdentifier objectName)
        {
            string path = GetFilesystemPath(objectName);
            await GetBackend(ns).DeleteAsync(path);
        }

        public Task DeleteNamespace(NamespaceId ns)
        {
            DirectoryInfo namespaceDirectory = GetFilesystemPath(ns).ToDirectoryInfo();
            if (namespaceDirectory.Exists)
            {
                namespaceDirectory.Delete(true);
            }

            return Task.CompletedTask;
        }

        public async IAsyncEnumerable<(BlobIdentifier,DateTime)> ListObjects(NamespaceId ns)
        {
            IStorageBackend backend = GetBackend(ns);
            await foreach ((string path, DateTime time) in backend.ListAsync())
            {
                string name = path.Substring(path.LastIndexOf('/') + 1);
                yield return (new BlobIdentifier(name), time);
            }
        }

        public bool ShouldRun()
        {
            return true;
        }

        public async Task<ulong> Cleanup(CancellationToken cancellationToken)
        {
            return await CleanupInternal(cancellationToken);
        }

        /// <summary>
        /// Clean the store from expired files
        /// 
        /// Uses the configuration to remove the least recently accessed (modified) blobs
        /// </summary>
        /// <param name="cancellationToken">Cancellation token</param>
        /// <param name="batchSize">Number of files to scan for clean up. A higher number is recommended since blob store can contain many small blobs</param>
        /// <returns></returns>
        public async Task<ulong> CleanupInternal(CancellationToken cancellationToken, int batchSize = 100000)
        {
            using TelemetrySpan scope = _tracer.StartActiveSpan("gc.filesystem")
                .SetAttribute("operation.name", "gc.filesystem");

            ulong maxSizeBytes = _settings.CurrentValue.MaxSizeBytes;
            long triggerSize = (long) (maxSizeBytes * _settings.CurrentValue.TriggerThresholdPercentage);
            long targetSize = (long) (maxSizeBytes * _settings.CurrentValue.TargetThresholdPercentage); // Target to shrink to if triggered
            ulong countOfBlobsRemoved = 0;

            // Perform a maximum of 5 clean up runs
            for (int i = 0; i < 5; i++)
            {
                long size = await CalculateDiskSpaceUsed();

                // first check to see if we should trigger at all, this happens for each run but only really matters for the first attempt
                if (size < triggerSize)
                {
                    return countOfBlobsRemoved;
                }

                // then check if we have reached the target size, if not we should continue running
                if (size <= targetSize)
                {
                    return countOfBlobsRemoved;
                }
                
                IEnumerable<FileInfo> fileInfos = GetLeastRecentlyAccessedObjects(maxResults: batchSize);

                bool hadFiles = false;
                long totalBytesDeleted = 0;
                foreach (FileInfo fi in fileInfos)
                {
                    hadFiles = true;
                    try
                    {
                        totalBytesDeleted += fi.Length;
                        fi.Delete();
                        ++countOfBlobsRemoved;

                        long currentSize = size - totalBytesDeleted;
                        if (currentSize <= targetSize || cancellationToken.IsCancellationRequested)
                        {
                            return countOfBlobsRemoved;
                        }
                    }
                    catch (FileNotFoundException)
                    {
                        // if the file was gced while running we can just ignore it
                    }
                }

                if (!hadFiles)
                {
                    return countOfBlobsRemoved;
                }
            }
            
            return countOfBlobsRemoved;
        }

        /// <summary>
        /// Get least recently accessed objects
        /// Assumes files on disk have the their last access timestamp up-to-date
        /// </summary>
        /// <param name="ns">Namespace, if set to null all namespaces will be scanned</param>
        /// <param name="maxResults">Max results to return. Note that the entire namespace will be scanned no matter what.</param>
        /// <param name="maxCountOfObjectsScanned">Max count of objects scanned before we stop.</param>
        /// <returns>Enumerable of least recently accessed objects as FileInfos</returns>
        public IEnumerable<FileInfo> GetLeastRecentlyAccessedObjects(NamespaceId? ns = null, int maxResults = 10_000, int maxCountOfObjectsScanned = 40_000_000)
        {
            // TODO: The maxCountOfObjectsScanned is not a ideal solution, we should likely find a solution were we do not have to read all objects into memory like this but rather can scan over them to determine a reasonable last write time cutoff
            string path = ns != null ? Path.Combine(GetRootDir(), ns.ToString()!) : GetRootDir();
            DirectoryInfo di = new DirectoryInfo(path);
            if (!di.Exists)
            {
                return Array.Empty<FileInfo>();
            }

            return di.EnumerateFiles("*", SearchOption.AllDirectories).Take(maxCountOfObjectsScanned).OrderBy(x => x.LastWriteTime).Take(maxResults);
        }
        
        /// <summary>
        /// Calculate the total size of blobs on disk for given namespace
        /// </summary>
        /// <param name="ns">Namespace, if set to null the total size of all namespaces will be returned</param>
        /// <returns>Total size of blobs in bytes</returns>
        public async Task<long> CalculateDiskSpaceUsed(NamespaceId? ns = null)
        {
            string path = ns != null ? Path.Combine(GetRootDir(), ns.ToString()!) : GetRootDir();
            DirectoryInfo di = new DirectoryInfo(path);
            if (!di.Exists)
            {
                return 0;
            }
            
            return await Task.Run(() => di.EnumerateFiles("*", SearchOption.AllDirectories).Sum(x =>
            {
                try
                {
                    return x.Length;
                }
                catch (FileNotFoundException)
                {
                    // if the file has been gced we just ignore it
                    return 0;
                }
            }));
        }
        
        public IAsyncEnumerable<NamespaceId> ListNamespaces()
        {
            DirectoryInfo di = new DirectoryInfo(GetRootDir());
            return di.GetDirectories().Select(x => new NamespaceId(x.Name)).ToAsyncEnumerable();
        }
    }

    public class FileStorageBackend : IStorageBackend
    {
        private readonly ILogger _logger;
        private readonly DirectoryReference _baseDir;
        private readonly IOptionsMonitor<FilesystemSettings> _settings;

        private const int DefaultBufferSize = 4096;

        public FileStorageBackend(ILogger logger, DirectoryReference baseDir, IOptionsMonitor<FilesystemSettings> settings)
        {
            _logger = logger;
            _baseDir = baseDir;
            _settings = settings;
        }

        private string GetRootDir()
        {
            return PathUtil.ResolvePath(_settings.CurrentValue.RootDir);
        }

        public static FileInfo GetFilesystemPath(string rootDir, NamespaceId ns, BlobIdentifier blob)
        {
            const int CountOfCharactersPerDirectory = 2;
            string objectName = blob.ToString();
            string firstPart = objectName.Substring(0, CountOfCharactersPerDirectory);
            string secondPart = objectName.Substring(CountOfCharactersPerDirectory, CountOfCharactersPerDirectory);
            string fileName = objectName;

            return new FileInfo(Path.Combine(rootDir, ns.ToString(), firstPart, secondPart, fileName));
        }

        public FileInfo GetFilesystemPath(string path)
        {
            return FileReference.Combine(_baseDir, path).ToFileInfo();
        }

        public DirectoryInfo GetFilesystemPath(NamespaceId ns)
        {
            return new DirectoryInfo(Path.Combine(GetRootDir(), ns.ToString()));
        }

        static readonly string s_processSuffix = Guid.NewGuid().ToString();
        static int s_uniqueId = 0;

        public async Task WriteAsync(string path, Stream content, CancellationToken cancellationToken)
        {
            FileInfo filePath = GetFilesystemPath(path);
            filePath.Directory?.Create();

            if (!filePath.Exists)
            {
                int uniqueId = Interlocked.Increment(ref s_uniqueId);

                string tempFilePath = $"{filePath.FullName}.{s_processSuffix}.{uniqueId}";
                await using (FileStream fs = new FileStream(tempFilePath, FileMode.Create, FileAccess.Write, FileShare.Read, DefaultBufferSize, FileOptions.Asynchronous | FileOptions.SequentialScan))
                {
                    await content.CopyToAsync(fs, cancellationToken);
                }

                try
                {
                    File.Move(tempFilePath, filePath.FullName, true);
                }
                catch (IOException) when (File.Exists(filePath.FullName))
                {
                }

                filePath.Refresh();

                if (filePath.Length == 0)
                {
                    _logger.LogWarning("0 byte file written as {Path} {Method}", path, "Stream");
                }
            }

            UpdateLastWriteTime(filePath.FullName, DateTime.UnixEpoch);
        }


        /// <summary>
        /// Update the last modified/write time that is used for determining when file was last accessed
        /// 
        /// Using access time is tricky as many file systems disable that for performance reasons.
        /// A new blob written to disk should be set with the oldest possible write time.
        /// This will ensure sorting of least recently accessed files during garbage collection works as intended.
        /// The write time update will happen async without any waiting to prevent blocking the critical path
        /// as it's best-effort only.
        /// </summary>
        /// <param name="filePath"></param>
        /// <param name="lastAccessed">Time the file was last accessed</param>
        private static void UpdateLastWriteTime(string filePath, DateTime lastAccessed)
        {
            try
            {
                File.SetLastWriteTimeUtc(filePath, lastAccessed);
            }
            catch (FileNotFoundException)
            {
                // it is okay if the file does not exist anymore, that just means it got gced
            }
        }

        public Task<BlobContents?> TryReadAsync(string path, LastAccessTrackingFlags flags, CancellationToken cancellationToken)
        {
            FileInfo filePath = GetFilesystemPath(path);

            if (!filePath.Exists)
            {
                return Task.FromResult<BlobContents?>(null);
            }

            if (flags == LastAccessTrackingFlags.DoTracking)
            {
                UpdateLastWriteTime(filePath.FullName, DateTime.UtcNow);
            }
            FileStream fs = new FileStream(filePath.FullName, FileMode.Open, FileAccess.Read, FileShare.Read, DefaultBufferSize, FileOptions.Asynchronous | FileOptions.SequentialScan);

            return Task.FromResult<BlobContents?>(new BlobContents(fs, fs.Length));
        }

        public Task<bool> ExistsAsync(string path, CancellationToken cancellationToken)
        {
            FileInfo filePath = GetFilesystemPath(path);

            return Task.FromResult(filePath.Exists);
        }

        public Task DeleteAsync(string path, CancellationToken cancellationToken)
        {
            FileInfo filePath = GetFilesystemPath(path);
            filePath.Delete();
            return Task.CompletedTask;
        }

        public IAsyncEnumerable<(string, DateTime)> ListAsync(CancellationToken cancellationToken)
        {
            return DoListOldObjects().ToAsyncEnumerable();
        }

        private IEnumerable<(string, DateTime)> DoListOldObjects()
        {
            DirectoryInfo di = _baseDir.ToDirectoryInfo();
            if (!di.Exists)
            {
                yield break;
            }

            foreach (FileInfo file in di.EnumerateFiles("*", SearchOption.AllDirectories))
            {
                string path = new FileReference(file).MakeRelativeTo(_baseDir).Replace(Path.DirectorySeparatorChar, '/');
                yield return (path, file.LastWriteTime);
            }
        }
    }
}
