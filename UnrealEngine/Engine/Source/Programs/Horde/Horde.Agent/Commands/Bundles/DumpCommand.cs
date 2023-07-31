// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Linq;
using System.Threading;
using System.Threading.Tasks;
using EpicGames.Core;
using EpicGames.Horde.Storage;
using EpicGames.Horde.Storage.Bundles;
using Microsoft.Extensions.Logging;

namespace Horde.Agent.Commands.Bundles
{
	[Command("bundle", "dump", "Dumps the contents of a bundle")]
	internal class DumpCommand : BundleCommandBase
	{
		[CommandLine("-Ref=")]
		public RefName RefName { get; set; } = DefaultRefName;

		[CommandLine("-Blob=")]
		public BlobId? BlobId { get; set; }

		public override async Task<int> ExecuteAsync(ILogger logger)
		{
			IBlobStore blobStore = CreateBlobStore(logger);

			IBlob blob;
			if (BlobId != null)
			{
				blob = await blobStore.ReadBlobAsync(BlobId.Value);
				logger.LogInformation("Summary for blob {BlobId}", BlobId.Value);
			}
			else
			{
				blob = await blobStore.ReadRefAsync(RefName);
				logger.LogInformation("Summary for ref {RefId}", RefName);
			}

			IReadOnlyList<BlobId> references = blob.References;
			logger.LogInformation("");
			logger.LogInformation("BlobRefs: {NumRefs}", references.Count);
			foreach (BlobId reference in references)
			{
				logger.LogInformation("  {BlobId}", reference);
			}

			ReadOnlyMemory<byte> data = blob.Data;
			MemoryReader reader = new MemoryReader(data);
			BundleHeader header = new BundleHeader(reader);
			int packetStart = data.Length - reader.Memory.Length;

			logger.LogInformation("");
			logger.LogInformation("Imports: {NumImports}", header.Imports.Count);

			int refIdx = 0;
			foreach (BundleImport import in header.Imports)
			{
				logger.LogInformation("  From blob {BlobId} ({NumExports}/{TotalExports} nodes)", import.BlobId, import.Exports.Count, import.ExportCount);
				foreach ((int exportIdx, IoHash exportHash) in import.Exports)
				{
					logger.LogInformation("    [{Index}] IMP {BlobId}:{ExportIdx} = {ExportHash}", refIdx, import.BlobId, exportIdx, exportHash);
					refIdx++;
				}
			}

			logger.LogInformation("");
			logger.LogInformation("Exports: {NumExports}", header.Exports.Count);

			int packetIdx = 0;
			int packetOffset = 0;
			foreach (BundleExport export in header.Exports)
			{
				string refs = (export.References.Count == 0) ? "[ ]" : $"[ {String.Join(", ", export.References.Select(x => x.ToString()))} ]";
				logger.LogInformation("  [{Index}] EXP {ExportHash} (length: {NumBytes:n0}, packet: {PacketIdx}, refs: {Refs})", refIdx, export.Hash, export.Length, packetIdx, refs);
				refIdx++;

				packetOffset += export.Length;
				if(packetOffset >= header.Packets[packetIdx].DecodedLength)
				{
					packetIdx++;
					packetOffset = 0;
				}
			}

			logger.LogInformation("");
			logger.LogInformation("Packets: {NumPackets}", header.Packets.Count);
			for (int idx = 0; idx < header.Packets.Count; idx++)
			{
				BundlePacket packet = header.Packets[idx];
				logger.LogInformation("  PKT {Idx} (file offset: {Offset:n0}, encoded: {EncodedLength:n0}, decoded: {DecodedLength:n0}, ratio: {Ratio}%)", idx, packetStart, packet.EncodedLength, packet.DecodedLength, (int)(packet.EncodedLength * 100) / packet.DecodedLength);
				packetStart += packet.EncodedLength;
			}

			return 0;
		}
	}
}
