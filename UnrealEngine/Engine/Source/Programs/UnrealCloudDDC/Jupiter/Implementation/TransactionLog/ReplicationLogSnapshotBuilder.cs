// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Threading;
using System.Threading.Tasks;
using EpicGames.Horde.Storage;
using EpicGames.Serialization;
using Jupiter.Common.Implementation;

namespace Jupiter.Implementation.TransactionLog
{
	public class ReplicationLogSnapshotBuilder
	{
		private readonly IReplicationLog _replicationLog;
		private readonly IBlobService _blobService;
		private readonly IRefService _refService;
		private readonly BufferedPayloadFactory _bufferedPayloadFactory;
		private readonly ReplicationLogFactory _replicationLogFactory;

		public ReplicationLogSnapshotBuilder(IReplicationLog replicationLog, IBlobService blobService, IRefService refService, BufferedPayloadFactory bufferedPayloadFactory, ReplicationLogFactory replicationLogFactory)
		{
			_replicationLog = replicationLog;
			_blobService = blobService;
			_refService = refService;
			_bufferedPayloadFactory = bufferedPayloadFactory;
			_replicationLogFactory = replicationLogFactory;
		}

		public async Task<BlobId> BuildSnapshotAsync(NamespaceId ns, NamespaceId storeInNamespace, CancellationToken cancellationToken = default(CancellationToken))
		{
			// builds a snapshot and commits it to the blob store with the identifier specified

			SnapshotInfo? snapshotInfo = await _replicationLog.GetLatestSnapshotAsync(ns);

			if (cancellationToken.IsCancellationRequested)
			{
				throw new TaskCanceledException();
			}

			ReplicationLogSnapshot snapshot;
			string? lastBucket;
			Guid? lastEvent;
			if (snapshotInfo != null)
			{
				// append to the previous snapshot if one is available
				await using BlobContents blobContents = await _blobService.GetObjectAsync(snapshotInfo.BlobNamespace, snapshotInfo.SnapshotBlob);
				if (cancellationToken.IsCancellationRequested)
				{
					throw new TaskCanceledException();
				}

				using IBufferedPayload snapshotPayload = await _bufferedPayloadFactory.CreateFilesystemBufferedPayloadAsync(blobContents.Stream);
				await using Stream s = snapshotPayload.GetStream();
				snapshot = _replicationLogFactory.DeserializeSnapshotFromStream(s);
				lastBucket = snapshot.LastBucket;
				lastEvent = snapshot.LastEvent;
			}
			else
			{
				snapshot = ReplicationLogFactory.CreateEmptySnapshot(ns);

				lastBucket = null;
				lastEvent = null;
			}

			if (cancellationToken.IsCancellationRequested)
			{
				throw new TaskCanceledException();
			}

			await foreach (ReplicationLogEvent entry in _replicationLog.GetAsync(ns, lastBucket, lastEvent))
			{
				if (cancellationToken.IsCancellationRequested)
				{
					throw new TaskCanceledException();
				}

				snapshot.ProcessEvent(entry);
			}

			FileInfo tempFile = new FileInfo(Path.GetTempFileName());
			{
				await using FileStream fs = tempFile.OpenWrite();
				snapshot.Serialize(fs);
				await fs.FlushAsync(cancellationToken);
			}

			Stream tempFileStream = tempFile.OpenRead();
			using IBufferedPayload payload = await _bufferedPayloadFactory.CreateFromStreamAsync(tempFileStream, tempFile.Length);
			tempFileStream.Close();
			tempFile.Delete();

			{
				BlobId blobIdentifier;
				{
					await using Stream stream = payload.GetStream();
					blobIdentifier = await BlobId.FromStreamAsync(stream);
				}

				CbWriter writer = new CbWriter();
				writer.BeginObject();
				writer.WriteBinaryAttachment("snapshotBlob", blobIdentifier.AsIoHash());
				writer.WriteDateTime("timestamp", DateTime.Now);
				writer.EndObject();

				byte[] cbObjectBytes = writer.ToByteArray();
				BlobId cbBlobId = BlobId.FromBlob(cbObjectBytes);

				if (cancellationToken.IsCancellationRequested)
				{
					throw new TaskCanceledException();
				}

				// upload the attachment first so we are not missing any references when we go to create the ref
				await _blobService.PutObjectAsync(storeInNamespace, payload, blobIdentifier);
			
				(ContentId[] missingContentIds, BlobId[] missingBlobs) = await _refService.PutAsync(storeInNamespace, new BucketId("snapshot"), new RefId(blobIdentifier.ToString()), cbBlobId, new CbObject(cbObjectBytes));
				List<ContentHash> missingHashes = new List<ContentHash>(missingContentIds);
				missingHashes.AddRange(missingBlobs);
				if (missingHashes.Count != 0)
				{
					throw new Exception($"Failed to upload snapshot to object service, missing references {string.Join(',' , missingHashes.Select(b => b.ToString()))}");
				}

				if (cancellationToken.IsCancellationRequested)
				{
					throw new TaskCanceledException();
				}

				// update the replication log with the new snapshot
				await _replicationLog.AddSnapshotAsync(new SnapshotInfo(ns, storeInNamespace, blobIdentifier, DateTime.Now));

				return blobIdentifier;

			}
		}
	}
}
