// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Threading;
using System.Threading.Tasks;
using Dasync.Collections;
using Datadog.Trace;
using EpicGames.Horde.Storage;
using Jupiter.Implementation;
using Microsoft.Extensions.Options;
using Serilog;

namespace Horde.Storage.Implementation
{
    public class FileSystemStore : IBlobStore, IBlobCleanup 
    {
        private readonly ILogger _logger = Log.ForContext<FileSystemStore>();
        private readonly IOptionsMonitor<FilesystemSettings> _settings;

        private const int DefaultBufferSize = 4096;

        public FileSystemStore(IOptionsMonitor<FilesystemSettings> settings)
        {
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

        public FileInfo GetFilesystemPath(NamespaceId ns, BlobIdentifier objectName)
        {
            return GetFilesystemPath(GetRootDir(), ns, objectName);
        }

        public DirectoryInfo GetFilesystemPath(NamespaceId ns)
        {
            return new DirectoryInfo(Path.Combine(GetRootDir(), ns.ToString()));
        }

        static readonly string s_processSuffix = Guid.NewGuid().ToString();
        static int s_uniqueId = 0;

        public async Task<BlobIdentifier> PutObject(NamespaceId ns, ReadOnlyMemory<byte> content, BlobIdentifier blobIdentifier)
        {
			using EpicGames.Core.ReadOnlyMemoryStream stream = new EpicGames.Core.ReadOnlyMemoryStream(content);
			return await PutObject(ns, stream, blobIdentifier);
        }

        public async Task<BlobIdentifier> PutObject(NamespaceId ns, Stream content, BlobIdentifier blobIdentifier)
        {
            FileInfo filePath = GetFilesystemPath(ns, blobIdentifier);
            filePath.Directory?.Create();

            if (!filePath.Exists)
            {
                int uniqueId = Interlocked.Increment(ref s_uniqueId);

                string tempFilePath = $"{filePath.FullName}.{s_processSuffix}.{uniqueId}";
                await using (FileStream fs = new FileStream(tempFilePath, FileMode.Create, FileAccess.Write, FileShare.Read, DefaultBufferSize, FileOptions.Asynchronous | FileOptions.SequentialScan))
                {
                    CancellationToken cancellationToken = default(CancellationToken);
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
                    _logger.Warning("0 byte file written as {Blob} {Namespace} {Method}", blobIdentifier, ns, "Stream");
                }
            }

            UpdateLastWriteTime(filePath.FullName, DateTime.UnixEpoch);
            return blobIdentifier;
        }

        public async Task<BlobIdentifier> PutObject(NamespaceId ns, byte[] content, BlobIdentifier blobIdentifier)
        {
			using MemoryStream stream = new MemoryStream(content);
			return await PutObject(ns, stream, blobIdentifier);
        }

        public Task<BlobContents> GetObject(NamespaceId ns, BlobIdentifier blob, LastAccessTrackingFlags flags)
        {
            FileInfo filePath = GetFilesystemPath(ns, blob);

            if (!filePath.Exists)
            {
                throw new BlobNotFoundException(ns, blob);
            }

            if (flags == LastAccessTrackingFlags.DoTracking)
            {
                UpdateLastWriteTime(filePath.FullName, DateTime.UtcNow);
            }
            FileStream fs = new FileStream(filePath.FullName, FileMode.Open, FileAccess.Read, FileShare.Read, DefaultBufferSize, FileOptions.Asynchronous | FileOptions.SequentialScan);

            return Task.FromResult(new BlobContents(fs, fs.Length));
        }

        /// <summary>
        /// Update the last modified/write time that is used for determining when file was last accessed
        /// 
        /// Using access time is tricky as many file systems disable that for performance reasons.
        /// A new blob written to disk should be set with the oldest possible write time.
        /// This will ensure sorting of least recently accessed files during garbage collection works as intended.
        /// The write time update will happen async without any waiting to prevent blocking the critical path
        /// as it's best-effort only.
        /// 
        /// See <seealso cref="GetLeastRecentlyAccessedObjects" />.
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

        public Task<bool> Exists(NamespaceId ns, BlobIdentifier blob, bool forceCheck)
        {
            FileInfo filePath = GetFilesystemPath(ns, blob);

            return Task.FromResult(filePath.Exists);
        }

        public Task DeleteObject(NamespaceId ns, BlobIdentifier objectName)
        {
            FileInfo filePath = GetFilesystemPath(ns, objectName);
            filePath.Delete();
            return Task.CompletedTask;
        }

        public Task DeleteNamespace(NamespaceId ns)
        {
            DirectoryInfo namespaceDirectory = GetFilesystemPath(ns);
            if (namespaceDirectory.Exists)
            {
                namespaceDirectory.Delete(true);
            }

            return Task.CompletedTask;
        }

        public IAsyncEnumerable<(BlobIdentifier,DateTime)> ListObjects(NamespaceId ns)
        {
            return DoListOldObjects(ns).ToAsyncEnumerable();
        }

        private IEnumerable<(BlobIdentifier, DateTime)> DoListOldObjects(NamespaceId ns)
        {
            DirectoryInfo di = GetFilesystemPath(ns);
            if (!di.Exists)
            {
                yield break;
            }

            foreach (FileSystemInfo? file in di.EnumerateFileSystemInfos("*", SearchOption.AllDirectories))
            {
                if (file.Attributes.HasFlag(FileAttributes.Directory))
                {
                    continue;
                }

                yield return (new BlobIdentifier(file.Name), file.LastWriteTime);
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
            using IScope scope = Tracer.Instance.StartActive("gc.filesystem");

            long size = await CalculateDiskSpaceUsed();
            ulong maxSizeBytes = _settings.CurrentValue.MaxSizeBytes;
            long triggerSize = (long) (maxSizeBytes * _settings.CurrentValue.TriggerThresholdPercentage);
            long targetSize = (long) (maxSizeBytes * _settings.CurrentValue.TargetThresholdPercentage); // Target to shrink to if triggered
            ulong countOfBlobsRemoved = 0;

            if (size < triggerSize)
            {
                return 0;
            }

            // Perform a maximum of 5 clean up runs
            for (int i = 0; i < 5; i++)
            {
                size = await CalculateDiskSpaceUsed();
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
        /// <returns>Enumerable of least recently accessed objects as FileInfos</returns>
        public IEnumerable<FileInfo> GetLeastRecentlyAccessedObjects(NamespaceId? ns = null, int maxResults = 10000)
        {
            string path = ns != null ? Path.Combine(GetRootDir(), ns.ToString()!) : GetRootDir();
            DirectoryInfo di = new DirectoryInfo(path);
            if (!di.Exists)
            {
                return Array.Empty<FileInfo>();
            }

            return di.EnumerateFiles("*", SearchOption.AllDirectories).OrderBy(x => x.LastWriteTime).Take(maxResults);
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
        
        public  IAsyncEnumerable<NamespaceId> ListNamespaces()
        {
            DirectoryInfo di = new DirectoryInfo(GetRootDir());
            return di.GetDirectories().Select(x => new NamespaceId(x.Name)).ToAsyncEnumerable();
        }
    }
}
