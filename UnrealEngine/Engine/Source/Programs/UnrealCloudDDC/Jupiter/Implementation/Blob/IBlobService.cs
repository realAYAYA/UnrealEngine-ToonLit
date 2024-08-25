// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.IO;
using System.Net.Mime;
using System.Threading.Tasks;
using EpicGames.AspNet;
using EpicGames.Horde.Storage;
using Jupiter.Common.Implementation;
using Microsoft.Extensions.DependencyInjection;
using OpenTelemetry.Trace;

namespace Jupiter.Implementation;

public interface IBlobService
{
	Task<ContentHash> VerifyContentMatchesHashAsync(Stream content, ContentHash identifier);
	Task<BlobId> PutObjectKnownHashAsync(NamespaceId ns, IBufferedPayload content, BlobId identifier);
	Task<BlobId> PutObjectAsync(NamespaceId ns, IBufferedPayload payload, BlobId identifier);
	Task<BlobId> PutObjectAsync(NamespaceId ns, byte[] payload, BlobId identifier);
	Task<Uri?> MaybePutObjectWithRedirectAsync(NamespaceId ns, BlobId identifier);

	Task<BlobContents> GetObjectAsync(NamespaceId ns, BlobId blob, List<string>? storageLayers = null, bool supportsRedirectUri = false, bool allowOndemandReplication = true);
	
	Task<Uri?> GetObjectWithRedirectAsync(NamespaceId ns, BlobId blobIdentifier, List<string>? storageLayers = null);

	Task<BlobMetadata> GetObjectMetadataAsync(NamespaceId ns, BlobId blobId);

	Task<BlobContents> ReplicateObjectAsync(NamespaceId ns, BlobId blob, bool force = false);

	Task<bool> ExistsAsync(NamespaceId ns, BlobId blob, List<string>? storageLayers = null);

	/// <summary>
	/// Checks that the blob exists in the root store, the store which is last in the list and thus is intended to have every blob in it
	/// </summary>
	/// <param name="ns">The namespace</param>
	/// <param name="blob">The identifier of the blob</param>
	/// <returns></returns>
	Task<bool> ExistsInRootStoreAsync(NamespaceId ns, BlobId blob);

	// Delete a object
	Task DeleteObjectAsync(NamespaceId ns, BlobId blob);

	// delete the whole namespace
	Task DeleteNamespaceAsync(NamespaceId ns);

	IAsyncEnumerable<(BlobId,DateTime)> ListObjectsAsync(NamespaceId ns);
	Task<BlobId[]> FilterOutKnownBlobsAsync(NamespaceId ns, IEnumerable<BlobId> blobs);
	Task<BlobId[]> FilterOutKnownBlobsAsync(NamespaceId ns, IAsyncEnumerable<BlobId> blobs);
	Task<BlobContents> GetObjectsAsync(NamespaceId ns, BlobId[] refRequestBlobReferences);

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
	public static async Task<ContentId> PutCompressedObjectAsync(this IBlobService blobService, NamespaceId ns, IBufferedPayload payload, ContentId? id, IServiceProvider provider)
	{
		IContentIdStore contentIdStore = provider.GetService<IContentIdStore>()!;
		CompressedBufferUtils compressedBufferUtils = provider.GetService<CompressedBufferUtils>()!;
		Tracer tracer = provider.GetService<Tracer>()!;

		// decompress the content and generate a identifier from it to verify the identifier we got
		await using Stream decompressStream = payload.GetStream();

		using IBufferedPayload bufferedPayload = await compressedBufferUtils.DecompressContentAsync(decompressStream, (ulong)payload.Length);
		await using Stream decompressedStream = bufferedPayload.GetStream();

		ContentId identifierDecompressedPayload;
		if (id != null)
		{
			identifierDecompressedPayload = ContentId.FromContentHash(await blobService.VerifyContentMatchesHashAsync(decompressedStream, id));
		}
		else
		{
			ContentHash blobHash;
			{
				using TelemetrySpan _ = tracer.StartActiveSpan("web.hash").SetAttribute("operation.name", "web.hash");
				blobHash = await BlobId.FromStreamAsync(decompressedStream);
			}

			identifierDecompressedPayload = ContentId.FromContentHash(blobHash);
		}

		BlobId identifierCompressedPayload;
		{
			using TelemetrySpan _ = tracer.StartActiveSpan("web.hash").SetAttribute("operation.name", "web.hash");
			await using Stream hashStream = payload.GetStream();
			identifierCompressedPayload = await BlobId.FromStreamAsync(hashStream);
		}

		// commit the mapping from the decompressed hash to the compressed hash, we run this in parallel with the blob store submit
		// TODO: let users specify weight of the blob compared to previously submitted content ids
		int contentIdWeight = (int)payload.Length;
		Task contentIdStoreTask = contentIdStore.PutAsync(ns, identifierDecompressedPayload, identifierCompressedPayload, contentIdWeight);

		// we still commit the compressed buffer to the object store using the hash of the compressed content
		{
			await blobService.PutObjectKnownHashAsync(ns, payload, identifierCompressedPayload);
		}

		await contentIdStoreTask;

		return identifierDecompressedPayload;
	}

	public static async Task<(BlobContents, string)> GetCompressedObjectAsync(this IBlobService blobService, NamespaceId ns, ContentId contentId, IServiceProvider provider, bool supportsRedirectUri = false)
	{
		IContentIdStore contentIdStore = provider.GetService<IContentIdStore>()!;
		Tracer tracer = provider.GetService<Tracer>()!;

		BlobId[]? chunks = await contentIdStore.ResolveAsync(ns, contentId, mustBeContentId: false);
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

			return (await blobService.GetObjectAsync(ns, blobToReturn, supportsRedirectUri: supportsRedirectUri), mimeType);
		}

		// chunked content, combine the chunks into a single stream
		using TelemetrySpan _ = tracer.StartActiveSpan("blob.combine").SetAttribute("operation.name", "blob.combine");
		Task<BlobContents>[] tasks = new Task<BlobContents>[chunks.Length];
		for (int i = 0; i < chunks.Length; i++)
		{
			// even if it was requested to support redirect, since we need to combine the chunks using redirects is not possible
			tasks[i] = blobService.GetObjectAsync(ns, chunks[i], supportsRedirectUri: false);
		}

		MemoryStream ms = new MemoryStream();
		foreach (Task<BlobContents> task in tasks)
		{
			BlobContents blob = await task;
			await using Stream s = blob.Stream;
			await s.CopyToAsync(ms);
		}

		ms.Seek(0, SeekOrigin.Begin);

		// chunking could not have happened for a non compressed buffer so assume it is compressed
		return (new BlobContents(ms, ms.Length), CustomMediaTypeNames.UnrealCompressedBuffer);
	}
}
