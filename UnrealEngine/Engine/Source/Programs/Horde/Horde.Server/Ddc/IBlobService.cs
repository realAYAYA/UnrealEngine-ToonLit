// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.IO;
using System.Net.Mime;
using System.Threading;
using System.Threading.Tasks;
using EpicGames.AspNet;
using EpicGames.Horde.Storage;
using Microsoft.Extensions.DependencyInjection;
using OpenTelemetry.Trace;

#pragma warning disable CS1591

namespace Horde.Server.Ddc;

public interface IBlobService
{
	Task<ContentHash> VerifyContentMatchesHashAsync(Stream content, ContentHash identifier, CancellationToken cancellationToken = default);
	Task<BlobId> PutObjectKnownHashAsync(NamespaceId ns, IBufferedPayload content, BlobId identifier, CancellationToken cancellationToken = default);
	Task<BlobId> PutObjectAsync(NamespaceId ns, IBufferedPayload payload, BlobId identifier, CancellationToken cancellationToken = default);
	Task<BlobId> PutObjectAsync(NamespaceId ns, byte[] payload, BlobId identifier, CancellationToken cancellationToken = default);
	Task<Uri?> MaybePutObjectWithRedirectAsync(NamespaceId ns, BlobId identifier, CancellationToken cancellationToken = default);

	Task<BlobContents> GetObjectAsync(NamespaceId ns, BlobId blob, List<string>? storageLayers = null, bool supportsRedirectUri = false, bool allowOndemandReplication = true, CancellationToken cancellationToken = default);

	Task<Uri?> GetObjectWithRedirectAsync(NamespaceId ns, BlobId blobIdentifier, List<string>? storageLayers = null, CancellationToken cancellationToken = default);

	Task<BlobMetadata> GetObjectMetadataAsync(NamespaceId ns, BlobId blobId, CancellationToken cancellationToken = default);

	Task<BlobContents> ReplicateObjectAsync(NamespaceId ns, BlobId blob, bool force = false, CancellationToken cancellationToken = default);

	Task<bool> ExistsAsync(NamespaceId ns, BlobId blob, List<string>? storageLayers = null, CancellationToken cancellationToken = default);

	/// <summary>
	/// Checks that the blob exists in the root store, the store which is last in the list and thus is intended to have every blob in it
	/// </summary>
	/// <param name="ns">The namespace</param>
	/// <param name="blob">The identifier of the blob</param>
	/// <param name="cancellationToken"></param>
	/// <returns></returns>
	Task<bool> ExistsInRootStoreAsync(NamespaceId ns, BlobId blob, CancellationToken cancellationToken = default);

	// Delete a object
	Task DeleteObjectAsync(NamespaceId ns, BlobId blob, CancellationToken cancellationToken = default);

	// delete the whole namespace
	Task DeleteNamespaceAsync(NamespaceId ns, CancellationToken cancellationToken = default);

	IAsyncEnumerable<(BlobId, DateTime)> ListObjectsAsync(NamespaceId ns, CancellationToken cancellationToken = default);
	Task<BlobId[]> FilterOutKnownBlobsAsync(NamespaceId ns, IEnumerable<BlobId> blobs, CancellationToken cancellationToken = default);
	Task<BlobId[]> FilterOutKnownBlobsAsync(NamespaceId ns, IAsyncEnumerable<BlobId> blobs, CancellationToken cancellationToken = default);
	Task<BlobContents> GetObjectsAsync(NamespaceId ns, BlobId[] refRequestBlobReferences, CancellationToken cancellationToken = default);

	bool ShouldFetchBlobOnDemand(NamespaceId ns);
}

public class BlobMetadata
{
	public BlobMetadata(long length, DateTime creationTime)
	{
		Length = length;
		CreationTime = creationTime;
	}

	public long Length { get; set; }
	public DateTime CreationTime { get; set; }
}

public static class BlobServiceExtensions
{
	public static async Task<ContentId> PutCompressedObjectAsync(this IBlobService blobService, NamespaceId ns, IBufferedPayload payload, ContentId? id, IServiceProvider provider, CancellationToken cancellationToken)
	{
		IContentIdStore contentIdStore = provider.GetService<IContentIdStore>()!;
		CompressedBufferUtils compressedBufferUtils = provider.GetService<CompressedBufferUtils>()!;
		Tracer tracer = provider.GetService<Tracer>()!;

		// decompress the content and generate a identifier from it to verify the identifier we got
		await using Stream decompressStream = payload.GetStream();

		using IBufferedPayload bufferedPayload = await compressedBufferUtils.DecompressContentAsync(decompressStream, (ulong)payload.Length, cancellationToken);
		await using Stream decompressedStream = bufferedPayload.GetStream();

		ContentId identifierDecompressedPayload;
		if (id != null)
		{
			identifierDecompressedPayload = ContentId.FromContentHash(await blobService.VerifyContentMatchesHashAsync(decompressedStream, id, cancellationToken));
		}
		else
		{
			ContentHash blobHash;
			{
				using TelemetrySpan _ = tracer.StartActiveSpan("web.hash").SetAttribute("operation.name", "web.hash");
				blobHash = await BlobId.FromStreamAsync(decompressedStream, cancellationToken);
			}

			identifierDecompressedPayload = ContentId.FromContentHash(blobHash);
		}

		BlobId identifierCompressedPayload;
		{
			using TelemetrySpan _ = tracer.StartActiveSpan("web.hash").SetAttribute("operation.name", "web.hash");
			await using Stream hashStream = payload.GetStream();
			identifierCompressedPayload = await BlobId.FromStreamAsync(hashStream, cancellationToken);
		}

		// we still commit the compressed buffer to the object store using the hash of the compressed content
		{
			await blobService.PutObjectKnownHashAsync(ns, payload, identifierCompressedPayload, cancellationToken);
		}

		// commit the mapping from the decompressed hash to the compressed hash, we run this in parallel with the blob store submit
		// TODO: let users specify weight of the blob compared to previously submitted content ids
		int contentIdWeight = (int)payload.Length;
		Task contentIdStoreTask = contentIdStore.PutAsync(ns, identifierDecompressedPayload, identifierCompressedPayload, contentIdWeight, cancellationToken);

		await contentIdStoreTask;

		return identifierDecompressedPayload;
	}

	public static async Task<(BlobContents, string)> GetCompressedObjectAsync(this IBlobService blobService, NamespaceId ns, ContentId contentId, IServiceProvider provider, bool supportsRedirectUri = false, CancellationToken cancellationToken = default)
	{
		IContentIdStore contentIdStore = provider.GetService<IContentIdStore>()!;
		Tracer tracer = provider.GetService<Tracer>()!;

		BlobId[]? chunks = await contentIdStore.ResolveAsync(ns, contentId, mustBeContentId: false, cancellationToken);
		if (chunks == null || chunks.Length == 0)
		{
			throw new ContentIdResolveException(contentId);
		}

		// single chunk, we just return that chunk
		if (chunks.Length == 1)
		{
			BlobId blobToReturn = chunks[0];
			string mimeType = CustomMediaTypeNames.UnrealCompressedBuffer;
			if (contentId.Equals(blobToReturn))
			{
				// this was actually the unmapped blob, meaning its not a compressed buffer
				mimeType = MediaTypeNames.Application.Octet;
			}

			return (await blobService.GetObjectAsync(ns, blobToReturn, supportsRedirectUri: supportsRedirectUri, cancellationToken: cancellationToken), mimeType);
		}

		// chunked content, combine the chunks into a single stream
		using TelemetrySpan _ = tracer.StartActiveSpan("blob.combine").SetAttribute("operation.name", "blob.combine");
		Task<BlobContents>[] tasks = new Task<BlobContents>[chunks.Length];
		for (int i = 0; i < chunks.Length; i++)
		{
			// even if it was requested to support redirect, since we need to combine the chunks using redirects is not possible
			tasks[i] = blobService.GetObjectAsync(ns, chunks[i], supportsRedirectUri: false, cancellationToken: cancellationToken);
		}

		MemoryStream ms = new MemoryStream();
		foreach (Task<BlobContents> task in tasks)
		{
			BlobContents blob = await task;
			await using Stream s = blob.Stream;
			await s.CopyToAsync(ms, cancellationToken);
		}

		ms.Seek(0, SeekOrigin.Begin);

		// chunking could not have happened for a non compressed buffer so assume it is compressed
		return (new BlobContents(ms, ms.Length), CustomMediaTypeNames.UnrealCompressedBuffer);
	}
}
