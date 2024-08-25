// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Concurrent;
using System.Collections.Generic;
using System.IO;
using System.Threading;
using System.Threading.Tasks;
using EpicGames.Core;
using EpicGames.Horde.Storage;
using OpenTelemetry.Trace;

namespace Horde.Server.Ddc
{
	class BlobService : IBlobService
	{
		static BlobType s_rawBlobType = new BlobType(new Guid("{03E6C37B-33C1-491F-8541-D3C401B8B8EF}"), 1);

		readonly IStorageClientFactory _storageClientFactory;
		readonly Tracer _tracer;

		public BlobService(IStorageClientFactory storageClientFactory, Tracer tracer)
		{
			_storageClientFactory = storageClientFactory;
			_tracer = tracer;
		}

		public static string GetAlias(BlobId blobId) => $"ddc:{blobId}";

		public async Task<ContentHash> VerifyContentMatchesHashAsync(Stream content, ContentHash identifier, CancellationToken cancellationToken)
		{
			ContentHash blobHash;
			{
				using TelemetrySpan _ = _tracer.StartActiveSpan("web.hash").SetAttribute("operation.name", "web.hash");
				blobHash = await BlobId.FromStreamAsync(content, cancellationToken);
			}

			if (!identifier.Equals(blobHash))
			{
				throw new HashMismatchException(identifier, blobHash);
			}

			return identifier;
		}

		public async Task<BlobId> PutObjectAsync(NamespaceId ns, IBufferedPayload payload, BlobId identifier, CancellationToken cancellationToken)
		{
			using TelemetrySpan scope = _tracer.StartActiveSpan("put_blob")
				.SetAttribute("operation.name", "put_blob")
				.SetAttribute("resource.name", identifier.ToString())
				.SetAttribute("Content-Length", payload.Length.ToString());

			await using Stream hashStream = payload.GetStream();
			await VerifyContentMatchesHashAsync(hashStream, identifier, cancellationToken);

			await PutObjectKnownHashAsync(ns, payload, identifier, cancellationToken);
			return identifier;
		}

		public Task<BlobId> PutObjectAsync(NamespaceId ns, byte[] payload, BlobId identifier, CancellationToken cancellationToken)
		{
			using MemoryBufferedPayload bufferedPayload = new MemoryBufferedPayload(payload);
			return PutObjectAsync(ns, bufferedPayload, identifier, cancellationToken);
		}

		public async Task<bool> ExistsAsync(NamespaceId ns, BlobId blob, List<string>? storageLayers = null, CancellationToken cancellationToken = default)
		{
			using IStorageClient storageClient = _storageClientFactory.CreateClient(ns);
			return await storageClient.FindAliasAsync(GetAlias(blob), cancellationToken) != null;
		}

		public async Task DeleteObjectAsync(NamespaceId ns, BlobId blob, CancellationToken cancellationToken)
		{
			using IStorageClient storageClient = _storageClientFactory.CreateClient(ns);
			string aliasName = GetAlias(blob);

			BlobAlias[] aliases = await storageClient.FindAliasesAsync(aliasName, cancellationToken: cancellationToken);
			foreach (BlobAlias alias in aliases)
			{
				await storageClient.RemoveAliasAsync(aliasName, alias.Target, cancellationToken);
			}
		}

		public async Task<BlobId[]> FilterOutKnownBlobsAsync(NamespaceId ns, IEnumerable<BlobId> blobIds, CancellationToken cancellationToken)
		{
			using IStorageClient storageClient = _storageClientFactory.CreateClient(ns);

			List<BlobId> unknownBlobIds = new List<BlobId>();
			foreach (BlobId blobId in blobIds)
			{
				if (await storageClient.FindAliasAsync(GetAlias(blobId), cancellationToken) == null)
				{
					unknownBlobIds.Add(blobId);
				}
			}

			return unknownBlobIds.ToArray();
		}

		public async Task<BlobId[]> FilterOutKnownBlobsAsync(NamespaceId ns, IAsyncEnumerable<BlobId> blobs, CancellationToken cancellationToken)
		{
			ConcurrentBag<BlobId> missingBlobs = new ConcurrentBag<BlobId>();

			try
			{
				await Parallel.ForEachAsync(blobs, cancellationToken, async (identifier, ctx) =>
				{
					bool exists = await ExistsAsync(ns, identifier, cancellationToken: ctx);

					if (!exists)
					{
						missingBlobs.Add(identifier);
					}
				});
			}
			catch (AggregateException e)
			{
				if (e.InnerException is PartialReferenceResolveException)
				{
					throw e.InnerException;
				}

				throw;
			}

			return missingBlobs.ToArray();
		}

		public async Task<BlobContents> GetObjectAsync(NamespaceId ns, BlobId blob, List<string>? storageLayers, bool supportsRedirectUri, bool allowOndemandReplication = true, CancellationToken cancellationToken = default)
		{
			using IStorageClient storageClient = _storageClientFactory.CreateClient(ns);

			BlobAlias? alias = await storageClient.FindAliasAsync(GetAlias(blob), cancellationToken);
			if (alias == null)
			{
				throw new BlobNotFoundException(ns, blob);
			}

			using BlobData data = await alias.Target.ReadBlobDataAsync(cancellationToken);
			return new BlobContents(data.Data.ToArray());
		}

		public Task<BlobMetadata> GetObjectMetadataAsync(NamespaceId ns, BlobId blobId, CancellationToken cancellationToken)
		{
			throw new NotImplementedException();
		}

		public async Task<BlobContents> GetObjectsAsync(NamespaceId ns, BlobId[] blobs, CancellationToken cancellationToken)
		{
			using TelemetrySpan _ = _tracer.StartActiveSpan("blob.combine").SetAttribute("operation.name", "blob.combine");
			Task<BlobContents>[] tasks = new Task<BlobContents>[blobs.Length];
			for (int i = 0; i < blobs.Length; i++)
			{
				tasks[i] = GetObjectAsync(ns, blobs[i], storageLayers: null, supportsRedirectUri: false, cancellationToken: cancellationToken);
			}

			MemoryStream ms = new MemoryStream();
			foreach (Task<BlobContents> task in tasks)
			{
				BlobContents blob = await task;
				await using Stream s = blob.Stream;
				await s.CopyToAsync(ms, cancellationToken);
			}

			ms.Seek(0, SeekOrigin.Begin);

			return new BlobContents(ms, ms.Length);
		}

		public Task<Uri?> GetObjectWithRedirectAsync(NamespaceId ns, BlobId blobIdentifier, List<string>? storageLayers, CancellationToken cancellationToken)
		{
			return Task.FromResult<Uri?>(null);
		}

		public Task<Uri?> MaybePutObjectWithRedirectAsync(NamespaceId ns, BlobId identifier, CancellationToken cancellationToken)
		{
			return Task.FromResult<Uri?>(null);
		}

		public async Task<BlobId> PutObjectKnownHashAsync(NamespaceId ns, IBufferedPayload content, BlobId identifier, CancellationToken cancellationToken)
		{
			using IStorageClient storageClient = _storageClientFactory.CreateClient(ns);

			IBlobRef blobRef;
			await using (IBlobWriter writer = storageClient.CreateBlobWriter())
			{
				int contentLength = (int)content.Length;
				Memory<byte> memory = writer.GetMemory(contentLength).Slice(0, contentLength);

				using Stream stream = content.GetStream();
				await stream.ReadFixedLengthBytesAsync(memory, cancellationToken);
				writer.Advance(contentLength);

				blobRef = await writer.CompleteAsync(s_rawBlobType, cancellationToken);
				await writer.FlushAsync(cancellationToken);
			}

			await storageClient.AddAliasAsync(GetAlias(identifier), blobRef, data: identifier.AsIoHash().ToByteArray(), cancellationToken: cancellationToken);
			return identifier;
		}

		public Task<BlobContents> ReplicateObjectAsync(NamespaceId ns, BlobId blob, bool force = false, CancellationToken cancellationToken = default)
			=> throw new NotSupportedException();

		public Task<bool> ExistsInRootStoreAsync(NamespaceId ns, BlobId blob, CancellationToken cancellationToken)
			=> Task.FromResult(false);

		public Task DeleteNamespaceAsync(NamespaceId ns, CancellationToken cancellationToken)
			=> throw new NotSupportedException();

		public IAsyncEnumerable<(BlobId, DateTime)> ListObjectsAsync(NamespaceId ns, CancellationToken cancellationToken)
			=> throw new NotSupportedException();

		public bool ShouldFetchBlobOnDemand(NamespaceId ns)
			=> false;
	}
}
