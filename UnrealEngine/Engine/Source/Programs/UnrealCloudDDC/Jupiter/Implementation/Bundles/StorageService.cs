// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Concurrent;
using System.Collections.Generic;
using System.IO;
using System.Runtime.CompilerServices;
using System.Threading;
using System.Threading.Tasks;
using EpicGames.Core;
using EpicGames.Horde.Storage;
using EpicGames.Serialization;
using Jupiter.Implementation.Blob;
using Microsoft.Extensions.DependencyInjection;
using Microsoft.Extensions.Logging.Abstractions;

namespace Jupiter.Implementation.Bundles;

public interface IStorageClientJupiter : IStorageClient
{
    Task<(BlobLocator Locator, Uri UploadUrl)?> GetWriteRedirectAsync(string prefix, CancellationToken cancellationToken);
    bool SupportsRedirects { get; set; }
    Task<Uri?> GetReadRedirectAsync(BlobLocator locator, CancellationToken cancellationToken);
    Task WriteRefTargetAsync(RefName refName, HashedNodeLocator target, RefOptions? requestOptions, CancellationToken cancellationToken);
}

public interface IStorageService
{
    Task<IStorageClientJupiter> GetClientAsync(NamespaceId namespaceId, CancellationToken cancellationToken);
}

public class StorageService : IStorageService
{
    private readonly IServiceProvider _provider;
    private readonly ConcurrentDictionary<NamespaceId, StorageClient> _backends = new ConcurrentDictionary<NamespaceId, StorageClient>();

    public StorageService(IServiceProvider provider)
    {
        _provider = provider;
    }

    public Task<IStorageClientJupiter> GetClientAsync(NamespaceId namespaceId, CancellationToken cancellationToken)
    {
        IStorageClientJupiter storageClient = _backends.GetOrAdd(namespaceId, x => ActivatorUtilities.CreateInstance<StorageClient>(_provider, namespaceId));
        return Task.FromResult(storageClient);
    }
}

public class StorageClient : IStorageClientJupiter
{
    private readonly NamespaceId _namespaceId;
    private readonly IBlobService _blobService;
    private readonly IReferencesStore _refStore;
    private readonly IBlobIndex _blobIndex;
    private readonly BucketId _defaultBucket = new BucketId("bundles");
    private readonly BundleReader _treeReader;

    public bool SupportsRedirects { get; set; } = true;

    public StorageClient(NamespaceId namespaceId, IBlobService blobService, IReferencesStore refStore, IBlobIndex blobIndex)
    {
        _namespaceId = namespaceId;
        _blobService = blobService;
        _refStore = refStore;
        _blobIndex = blobIndex;
        _treeReader = new BundleReader(this, null, NullLogger.Instance);
    }

    public async Task<(BlobLocator Locator, Uri UploadUrl)?> GetWriteRedirectAsync(string prefix, CancellationToken cancellationToken)
    {
        BlobLocator locator = BlobLocator.Create(HostId.Empty, prefix);
        BlobIdentifier blobIdentifier = BlobIdentifier.FromBlobLocator(locator);
        Uri? redirectUri = await _blobService.MaybePutObjectWithRedirect(_namespaceId, blobIdentifier);
        if (redirectUri == null)
        {
            return null;
        }
        return (locator, redirectUri);
    }

    public async Task<BlobLocator> WriteBlobAsync(Stream stream, Utf8String prefix, CancellationToken cancellationToken)
    {
        BlobLocator locator = BlobLocator.Create(HostId.Empty, prefix);
        BlobIdentifier blobIdentifier = BlobIdentifier.FromBlobLocator(locator);
        await using MemoryStream ms = new MemoryStream();
        await stream.CopyToAsync(ms, cancellationToken);
        byte[] blob = ms.ToArray();
        await _blobService.PutObject(_namespaceId, blob, blobIdentifier);

        bool isBundle;
        try
        {
            BundleHeader.ReadPrelude(blob);
            isBundle = true;
        }
        catch (InvalidDataException)
        {
            isBundle = false;
        }

        if (isBundle)
        {
            await using MemoryStream bundleStream = new MemoryStream(blob);
            BundleHeader bundleHeader = await BundleHeader.FromStreamAsync(bundleStream, cancellationToken);
            List<Task> addReferencesTasks = new List<Task>();
            foreach (BlobLocator import in bundleHeader.Imports)
            {
                BlobIdentifier dependentBlob = BlobIdentifier.FromBlobLocator(import);
                addReferencesTasks.Add(_blobIndex.AddBlobReferences(_namespaceId, dependentBlob, blobIdentifier));
            }

            await Task.WhenAll(addReferencesTasks);
        }
        
        return locator;
    }

    public async Task AddAliasAsync(Utf8String name, BlobHandle handle, CancellationToken cancellationToken = default)
    {
        // TODO: Implement aliases
        await Task.CompletedTask;
    }

    public async Task RemoveAliasAsync(Utf8String name, BlobHandle handle, CancellationToken cancellationToken = default)
    {
        // TODO: Implement aliases
        await Task.CompletedTask;
    }

    public async IAsyncEnumerable<BlobHandle> FindNodesAsync(Utf8String name, [EnumeratorCancellation] CancellationToken cancellationToken = default)
    {
        // TODO: Implement aliases
        await Task.CompletedTask;
        yield break;
    }

    public async Task<Uri?> GetReadRedirectAsync(BlobLocator locator, CancellationToken cancellationToken)
    {
        BlobIdentifier blobIdentifier = BlobIdentifier.FromBlobLocator(locator);
        Uri? redirectUri = await _blobService.GetObjectWithRedirect(_namespaceId, blobIdentifier);
        return redirectUri;
    }

    public async Task<Stream> ReadBlobAsync(BlobLocator locator, CancellationToken cancellationToken)
    {
        BlobIdentifier blobIdentifier = BlobIdentifier.FromBlobLocator(locator);
        BlobContents blobContents = await _blobService.GetObject(_namespaceId, blobIdentifier);
        return blobContents.Stream;
    }

    public async Task<Stream> ReadBlobRangeAsync(BlobLocator locator, int offset, int length, CancellationToken cancellationToken)
    {
        await using Stream stream = await ReadBlobAsync(locator, cancellationToken);
        using BinaryReader br = new BinaryReader(stream);
        // skip in the stream, TODO: will fail if stream is very large
        br.ReadBytes(offset);
        byte[] data = br.ReadBytes(length);
        return new MemoryStream(data);

    }

    public async Task<BlobHandle?> TryReadRefTargetAsync(RefName name, RefCacheTime cacheTime = default, CancellationToken cancellationToken = default)
    {
        // TODO: Cache time is ignored
        try
        {
            ObjectRecord record = await _refStore.Get(_namespaceId, _defaultBucket,  IoHashKey.FromName(name.ToString()), IReferencesStore.FieldFlags.IncludePayload);
            if (record.InlinePayload == null)
            {
                // if there is no inline payload this is not a bundle ref
                return null;
            }

            RefInlinePayload inlinePayload = CbSerializer.Deserialize<RefInlinePayload>(record.InlinePayload);
            IoHash nodeHash = inlinePayload.BlobHash;
            BlobLocator blobLocator = new BlobLocator(inlinePayload.BlobLocator);
            int exportId = inlinePayload.ExportId;

            return new FlushedNodeHandle(_treeReader, nodeHash, new NodeLocator(blobLocator, exportId));
        }
        catch (ObjectNotFoundException )
        {
            return null;
        }
    }

    public async Task<BlobHandle> WriteRefAsync(RefName name, Bundle bundle, int exportIdx, Utf8String prefix = default, RefOptions? options = null, CancellationToken cancellationToken = default)
    {
        BlobLocator locator = await this.WriteBundleAsync(bundle, prefix, cancellationToken);
        BlobHandle target = new FlushedNodeHandle(_treeReader, bundle.Header.Exports[exportIdx].Hash, new NodeLocator(locator, exportIdx));
        await WriteRefTargetAsync(name, target, options, cancellationToken);

        return target;
    }

    public Task WriteRefTargetAsync(RefName refName, BlobHandle target, RefOptions? requestOptions, CancellationToken cancellationToken)
    {
        return WriteRefTargetAsync(refName, new HashedNodeLocator(target.Hash, target.GetLocator()), requestOptions, cancellationToken);
    }

    public async Task WriteRefTargetAsync(RefName refName, HashedNodeLocator target, RefOptions? requestOptions, CancellationToken cancellationToken)
    {
        BlobIdentifier bundleBlob = BlobIdentifier.FromBlobLocator(target.Blob);
        IoHashKey refKey = IoHashKey.FromName(refName.ToString());
        RefInlinePayload inlinePayload = new RefInlinePayload()
        {
            BlobHash = target.Hash, BlobLocator = target.Blob.ToString(), ExportId = target.ExportIdx
        };
        byte[] payload = CbSerializer.SerializeToByteArray(inlinePayload);
        BlobIdentifier blobIdentifier = BlobIdentifier.FromBlob(payload);
        await _blobIndex.AddRefToBlobs(_namespaceId, _defaultBucket, refKey, new BlobIdentifier[] { bundleBlob });

        // TODO: Calculate isFinalized which requires us to be able to resovle references
        await _refStore.Put(_namespaceId, _defaultBucket, refKey, blobIdentifier, payload, isFinalized: true); 
    }

    public  async Task DeleteRefAsync(RefName name, CancellationToken cancellationToken = default)
    {
        await _refStore.Delete(_namespaceId, _defaultBucket, IoHashKey.FromName(name.ToString()));
    }

    public BundleWriter CreateWriter(RefName refName = default, BundleOptions? options = null)
    {
        return new BundleWriter(this, _treeReader, refName, options);
    }

    IStorageWriter IStorageClient.CreateWriter(RefName refName) => CreateWriter(refName);
}

public class RefInlinePayload
{
    [CbField("hash")]
    public IoHash BlobHash { get; set; }

    [CbField("loc")]
    public string BlobLocator { get; set; } = null!;

    [CbField("export")]
    public int ExportId { get; set; }
}