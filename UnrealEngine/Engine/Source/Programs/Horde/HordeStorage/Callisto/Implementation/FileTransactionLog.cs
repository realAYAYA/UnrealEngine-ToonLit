// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using System.Linq;
using System.Security.Cryptography;
using System.Text;
using System.Threading.Tasks;
using Datadog.Trace;
using EpicGames.Horde.Storage;
using Jupiter;
using Jupiter.Implementation;
using Microsoft.Extensions.Options;
using Newtonsoft.Json;
using Newtonsoft.Json.Bson;
using Serilog;

namespace Callisto.Implementation
{
    public class FileTransactionLog : ITransactionLog
    {
        private FileTransactionLogIndex _index;
        private readonly FileInfo _transactionLogHandle;

        private readonly object _commitLock = new object();
        private readonly IOptionsMonitor<CallistoSettings>? _settings;
        private readonly NamespaceId _namespace;

        private const string IndexFilename = "Index.json";
        private const string LogFilename = "Log";

        private readonly ILogger _logger = Log.ForContext<FileTransactionLog>();

        public FileTransactionLog(IOptionsMonitor<CallistoSettings> settings, DirectoryInfo root, NamespaceId ns) : this(root, ns)
        {
            _settings = settings;
        }

        public FileTransactionLog(DirectoryInfo root, NamespaceId ns)
        {
            _namespace = ns;
            FileInfo indexFile = new FileInfo(Path.Combine(root.FullName, ns.ToString(), IndexFilename));

            if (!indexFile.Directory?.Exists ?? false)
            {
                if (indexFile.Directory != null)
                {
                    Directory.CreateDirectory(indexFile.Directory.FullName);
                }
            }

            if (indexFile.Exists)
            {
                _index = FileTransactionLogIndex.FromFile(indexFile);
            }
            else
            {
                _index = new FileTransactionLogIndex(indexFile);
            }

            _transactionLogHandle = new FileInfo(Path.Combine(root.FullName, ns.ToString(), LogFilename));
        }

        internal FileTransactionLogIndex IndexFile => _index;

        public async Task<long> NewTransaction(AddTransactionEvent @event)
        {
            OpRecord eventRecord = AddOp.FromEvent(_index, @event);
            return await CommitNewEvent(eventRecord);
        }

        public async Task<long> NewTransaction(RemoveTransactionEvent @event)
        {
            OpRecord eventRecord = RemoveOp.FromEvent(_index, @event);
            return await CommitNewEvent(eventRecord);
        }

        private async Task<long> CommitNewEvent(OpRecord eventRecord)
        {
            // datadog instrumentation of this method
            using IScope scope = Tracer.Instance.StartActive("log.write");
            ISpan span = scope.Span;
            span.ResourceName = $"write {_namespace}";
            span.OperationName = "WRITE";

            if (_index.Version != 1)
            {
                throw new InvalidOperationException($"Version mismatch of transaction log, found {_index.Version} but expected version 1");
            }

            byte[] eventBuffer = SerializeToBuffer(eventRecord);
            int eventLength = eventBuffer.Length;

            using MD5 md5 = MD5.Create();
            byte[] md5Hash = md5.ComputeHash(eventBuffer, 0, eventLength);

            long offset = await Task.Run(() =>
            {
                lock (_commitLock)
                {
                    using FileStream fs = _transactionLogHandle.Open(FileMode.OpenOrCreate, FileAccess.Write, FileShare.Read);
                    // seek to end
                    fs.Seek(0, SeekOrigin.End);
                    long position = fs.Position;
                    
                    if (_settings?.CurrentValue?.VerifySerialization ?? false)
                    {
                        VerifySerializationOfEvent(position, eventRecord.Op!, eventBuffer, md5Hash, eventLength, md5);
                    }

                    OpHeader header = new OpHeader(position, md5Hash, eventLength);
                    header.Serialize(fs);
                    fs.Write(eventBuffer, 0, eventLength);

                    return position;
                }
            });
            return offset;
        }

        private static byte[] SerializeToBuffer(OpRecord eventRecord)
        {
            using MemoryStream memoryStream = new MemoryStream();
            eventRecord.Serialize(memoryStream);
            
            byte[] buffer = new byte[memoryStream.Position];
            Array.Copy(memoryStream.GetBuffer(), buffer, memoryStream.Position);
            return buffer;
        }

        private void VerifySerializationOfEvent(long transactionId, BaseOp op, byte[] eventBuffer, byte[] originalMd5, int eventLength, MD5 md5)
        {
            using MemoryStream testStream = new MemoryStream(eventLength);
            OpHeader header = new OpHeader(0, originalMd5, eventLength);

            header.Serialize(testStream);
            testStream.Write(eventBuffer, 0, eventLength);

            testStream.Seek(0, SeekOrigin.Begin);

            OpHeader opHeader = OpHeader.Deserialize(testStream);

            byte[] eventBuf = new byte[opHeader.Length];
            testStream.Read(eventBuf, 0, opHeader.Length);

            byte[] testMd5Hash = md5.ComputeHash(eventBuf, 0, opHeader.Length);
            if (!opHeader.Md5hash.SequenceEqual(testMd5Hash))
            {
                string headerMd5 = StringUtils.FormatAsHexString(opHeader.Md5hash);
                string calculatedMd5 = StringUtils.FormatAsHexString(testMd5Hash);
                _logger.Warning("Verifying header {@Header} for op {@Op}, mismatching hash found {CalculatedHash} expected {ExpectedHash} {TransactionId}", opHeader, op, calculatedMd5, headerMd5, transactionId);

                using MemoryStream ms = new MemoryStream(eventBuf);
                OpRecord newOp = OpRecord.Deserialize(ms);
                _logger.Warning("Deserialized form of op {@NewOp} . Old op was {@Op}", newOp.Op, op);

                throw new Exception($"Verifying serialization failed! Expected hash {headerMd5} but calculated {calculatedMd5}");
            }
        }

        public Task Drop()
        {
            _transactionLogHandle.Delete();
            // file should never be null, its only nullable for serialization purposes
            _index.File!.Delete();
            _index = new FileTransactionLogIndex(_index.File);

            return Task.CompletedTask;
        }

        public async Task<TransactionEvents> Get(long index, int count, string? notSeenAtSite = null)
        {
            // datadog instrumentation of this method
            using IScope scope = Tracer.Instance.StartActive("log.get");
            ISpan span = scope.Span;
            span.ResourceName = $"get {_namespace}";
            span.OperationName = "GET";

            if (_index.Version != 1)
            {
                throw new InvalidOperationException($"Version mismatch of transaction log, found {_index.Version} but expected version 1");
            }

            OpRecord[] events = new OpRecord[count];

            ulong siteFilter = 0;
            if (notSeenAtSite != null)
            {
                int filteredSiteIndex = _index.ToIndex(notSeenAtSite);
                siteFilter = 1UL << filteredSiteIndex;
            }

            _transactionLogHandle.Refresh();
            if (!_transactionLogHandle.Exists)
            {
                throw new FileNotFoundException("Transaction log is empty");
            }

            // we set a max amount of events to skip to make sure we actually move the current event counter forward for the replicator and do not have to start from the beginning again
            const int maxEventsSkippedDueToLocation = 1000;
            int countOfEventsSkipped = 0;

            // we allow parallel reads from the file, we only need to lock on writes.
            // ReSharper disable once InconsistentlySynchronizedField
            await using FileStream fs = _transactionLogHandle.Open(FileMode.Open, FileAccess.Read, FileShare.Read);
            long offset = index;
            fs.Seek(offset, SeekOrigin.Begin);
            int foundCount = 0;
            long? transactionLogOffsetMismatchFoundAt = null;
            while (foundCount != count)
            {
                // if we have reached the end of the file we stop
                if (fs.Length == fs.Position)
                {
                    break;
                }

                // if we would read outside the stream when reading the next header we stop
                if (fs.Position + OpHeader.HeaderLength > fs.Length)
                {
                    break;
                }

                OpHeader opHeader = OpHeader.Deserialize(fs);

                if (opHeader.Offset != offset)
                {
                    transactionLogOffsetMismatchFoundAt = offset;
                    break;
                }

                long eventPosition = fs.Position;
                byte[] eventBuf = new byte[opHeader.Length];
                await fs.ReadAsync(eventBuf, 0, opHeader.Length);

                // update the offset to our current position in the file in case we need to continue reading
                offset = fs.Position;

                if (offset - eventPosition != opHeader.Length)
                {
                    throw new Exception($"Read event size {offset - eventPosition} did not match expected size from header {opHeader.Length}");
                }

                byte[] md5Hash;
                using (MD5 md5 = MD5.Create())
                {
                    md5Hash = md5.ComputeHash(eventBuf, 0, opHeader.Length);
                }

                if (!opHeader.Md5hash.SequenceEqual(md5Hash))
                {
                    string headerMd5 = StringUtils.FormatAsHexString(opHeader.Md5hash);
                    string calculatedMd5 = StringUtils.FormatAsHexString(md5Hash);
                    _logger.Warning("Reading header {@Header}, mismatching hash found {CalculatedHash} expected {ExpectedHash} . Was read at offset {Offset}", opHeader, calculatedMd5, headerMd5, offset);
                    throw new EventHashMismatchException(opHeader.Offset, headerMd5, calculatedMd5);
                }

                OpRecord op;
                {
#pragma warning disable CA2000 // Dispose objects before losing scope -- object is disposed so false positive
                    await using MemoryStream eventStream = new MemoryStream(eventBuf);
#pragma warning restore CA2000 // Dispose objects before losing scope
                    op = OpRecord.Deserialize(eventStream);
                }

                if (siteFilter != 0)
                {
                    // we have a site filter, lets check that the event has not been seen at this site (flag is set)
                    bool hasBeenSeen = (op.Op!.Locations & siteFilter) != 0;
                    if (hasBeenSeen && countOfEventsSkipped > maxEventsSkippedDueToLocation)
                    {
                        // we have reached the maximum number of events we skip, lets stop searching for more events for now.
                        break;
                    }
                    if (hasBeenSeen)
                    {
                        ++countOfEventsSkipped;
                        continue;
                    }
                }

                op.Header = opHeader;
                events[foundCount] = op;
                foundCount++;
            }

            // if there is more content we check the next header
            if (fs.Length != fs.Position && fs.Position + OpHeader.HeaderLength < fs.Length)
            {
                // read another header and verify that it still is valid
                OpHeader nextHeader = OpHeader.Deserialize(fs);

                Debug.Assert(nextHeader != null);
                //Debug.Assert(offset == nextHeader.Offset);
            }

            TransactionEvent[] newEvents = new TransactionEvent[foundCount];
            for (int i = 0; i < foundCount; i++)
            {
                OpRecord tes = events[i];
                Debug.Assert(condition: tes.Op != null, "tes.Op != null");
                Debug.Assert(condition: tes.Header != null, "tes.Header != null");

                newEvents[i] = tes.Op.ToTransactionEvent(tes.Header, _index);
            }

            return new TransactionEvents(newEvents, offset, transactionLogOffsetMismatchFoundAt);
        }

        internal class OpHeader
        {
            // Offset of the current header being written
            public long Offset;

            // Hash of the transaction event
            public byte[] Md5hash;

            // Length of transaction event
            public int Length;

            public static int HeaderLength => sizeof(long) + 16 /* MD5 hash */ + sizeof(int);

            public OpHeader(in long offset, byte[] md5Hash, in int length)
            {
                Offset = offset;
                Md5hash = md5Hash;
                Length = length;
            }

            public void Serialize(Stream s)
            {
                using BinaryWriter writer = new BinaryWriter(s, Encoding.UTF8, leaveOpen: true);

                writer.Write(Offset);
                writer.Write(Md5hash, 0, 16);
                writer.Write(Length);
            }

            public static OpHeader Deserialize(Stream s)
            {
                using BinaryReader reader = new BinaryReader(s, Encoding.UTF8, leaveOpen: true);

                long offset = reader.ReadInt64();
                byte[] md5Hash = new byte[16];
                reader.Read(md5Hash, 0, 16);
                int length = reader.ReadInt32();

                OpHeader header = new OpHeader(offset, md5Hash, length);
                return header;
            }
        }

        internal enum OpType
        {
            Add,
            Remove
        };

        internal class OpRecord
        {
            public OpRecord(OpType type, BaseOp op)
            {
                OpType = type;
                Op = op;
            }

            public OpType OpType { get; set; }

            public BaseOp Op { get; set; }

            /// <summary>
            /// set when a object is being read back so that we have access to the header of the op as well
            /// </summary>
            public OpHeader? Header { get; set; } = null;

            public static OpRecord Deserialize(Stream stream)
            {
                int opType = stream.ReadByte();
                OpType op = (OpType) opType;

                BaseOp baseOp;
                switch (op)
                {
                    case OpType.Add:
                        baseOp = AddOp.Deserialize(stream);
                        break;
                    case OpType.Remove:
                        baseOp = RemoveOp.Deserialize(stream);
                        break;
                    default:
                        throw new NotImplementedException($"Op type {op} not supported");
                }

                return new OpRecord(op, baseOp);
            }

            public void Serialize(Stream stream)
            {
                stream.WriteByte((byte)OpType);
                Op.Serialize(stream);
            }
        }

        internal abstract class BaseOp
        {
            protected BaseOp(string name, string bucket, ulong locations)
            {
                Name = name;
                Bucket = bucket;
                Locations = locations;
            }

            public string Name { get; set; }

            public string Bucket { get; set; }

            public ulong Locations { get; set; }

            // 64 entries means we can encode this in a long e.g. 8 bytes
            private const int MaxLocations = 64;

            public abstract TransactionEvent ToTransactionEvent(OpHeader opHeader, FileTransactionLogIndex index);

            protected List<string> ResolveLocations(FileTransactionLogIndex index)
            {
                List<string> locations = new List<string>();
                for (int i = 0; i < MaxLocations; i++)
                {
                    bool flag = (Locations & (1UL << i)) != 0;
                    if (flag)
                    {
                        locations.Add(index.NameOfIndex(i));
                    }
                }

                return locations;
            }

            protected static ulong FlagSetFromLocations(FileTransactionLogIndex index, IEnumerable<string> eventLocations)
            {
                List<string> enumerable = eventLocations.ToList();
                if (!enumerable.Any())
                {
                    throw new Exception("No locations set, at least the location of the current instance has to be specified");
                }

                ulong flags = 0;
                foreach (int indexToSet in enumerable.Select(index.ToIndex))
                {
                    flags |= (1UL << indexToSet);
                }

                return flags;
            }

            public void Serialize(Stream stream)
            {
                using BinaryWriter writer = new BinaryWriter(stream, Encoding.UTF8, leaveOpen: true);
                writer.Write(Name);
                writer.Write(Bucket);
                writer.Write(Locations);

                DoSerialize(stream);
            }

            protected static (string, string, ulong) Deserialize(Stream stream)
            {
                using BinaryReader reader = new BinaryReader(stream, Encoding.UTF8, leaveOpen: true);
                string name = reader.ReadString();
                string bucket = reader.ReadString();
                ulong flags = reader.ReadUInt64();
                return (name, bucket, flags);
            }

            protected abstract void DoSerialize(Stream stream);
        }

        internal class RemoveOp : BaseOp
        {
            private RemoveOp(string name, string bucket, ulong locations) : base(name, bucket, locations)
            {
            }

            public static OpRecord FromEvent(FileTransactionLogIndex index, RemoveTransactionEvent @event)
            {
                RemoveOp op = new RemoveOp(@event.Name, @event.Bucket, FlagSetFromLocations(index, @event.Locations));

                return new OpRecord(OpType.Remove, op);
            }

            public override TransactionEvent ToTransactionEvent(OpHeader opHeader, FileTransactionLogIndex index)
            {
                Debug.Assert(condition: Name != null, message: nameof(Name) + " != null");
                Debug.Assert(condition: Bucket != null, message: nameof(Bucket) + " != null");

                List<string> locations = ResolveLocations(index);
                return new RemoveTransactionEvent(Name, Bucket, locations, identifier: opHeader.Offset, nextIdentifier: opHeader.Offset + opHeader.Length + OpHeader.HeaderLength);
            }

            protected override void DoSerialize(Stream stream)
            {
                // nothing extra to serialize
            }

            public static new BaseOp Deserialize(Stream stream)
            {
                (string name, string bucket, ulong locations) = BaseOp.Deserialize(stream);
                return new RemoveOp(name, bucket, locations);
            }
        }

        internal class AddOp : BaseOp
        {
            private readonly ILogger _logger = Log.ForContext<AddOp>();

            private AddOp(string name, string bucket, ulong locations, List<BlobIdentifier> blobs, byte[] metadataBuffer) : base(name, bucket, locations)
            {
                Blobs = blobs;
                Metadata = metadataBuffer;
            }

            public List<BlobIdentifier> Blobs { get; set; } = null!;

            public byte[] Metadata { get; set; } = Array.Empty<byte>();

            public static OpRecord FromEvent(FileTransactionLogIndex index, AddTransactionEvent @event)
            {
                // create a bson representation of the metadata
                byte[] metadataBuffer = Array.Empty<byte>();
                if (@event.Metadata != null)
                {
                    using MemoryStream ms = new MemoryStream();
                    using BsonDataWriter writer = new BsonDataWriter(ms);
                    JsonSerializer serializer = JsonSerializer.CreateDefault();
                    serializer.Serialize(writer, @event.Metadata);

                    long metadataLength = ms.Position;
                    metadataBuffer = new byte[metadataLength];
                    Array.Copy(sourceArray: ms.GetBuffer(), metadataBuffer, metadataLength);
                }

                ulong flags = FlagSetFromLocations(index, @event.Locations);

                AddOp op = new AddOp(@event.Name, @event.Bucket, flags, @event.Blobs.ToList(), metadataBuffer);
                return new OpRecord(OpType.Add, op);
            }

            public override TransactionEvent ToTransactionEvent(OpHeader opHeader, FileTransactionLogIndex index)
            {
                Dictionary<string, object>? metadata = null;
                if (Metadata != null && Metadata.Length != 0)
                {
                    using MemoryStream ms = new MemoryStream(Metadata);
                    using BsonDataReader reader = new BsonDataReader(ms);
                    JsonSerializer serializer = JsonSerializer.CreateDefault();
                    try
                    {
                        metadata = serializer.Deserialize<Dictionary<string, object>>(reader);
                    }
                    catch (JsonReaderException e)
                    {
                        _logger.Warning(e, "Failed to deserialize {@Metadata} into a json blob for object {Name}", Metadata, Name);
                        metadata = null;
                    }
                    catch (Exception e)
                    {
                        _logger.Warning(e, "Failed to deserialize {@Metadata} into a json blob for object {Name} due to a unknown exception", Metadata, Name);
                        metadata = null;
                    }
                }

                Debug.Assert(condition: Name != null, message: nameof(Name) + " != null");
                Debug.Assert(condition: Bucket != null, message: nameof(Bucket) + " != null");
                Debug.Assert(condition: Blobs != null, message: nameof(Blobs) + " != null");

                List<string> locations = ResolveLocations(index);

                return new AddTransactionEvent(Name, Bucket, Blobs.ToArray(), locations, metadata, identifier: opHeader.Offset, nextIdentifier: opHeader.Offset + opHeader.Length + OpHeader.HeaderLength);
            }

            protected override void DoSerialize(Stream stream)
            {
                using BinaryWriter writer = new BinaryWriter(stream, Encoding.UTF8, leaveOpen: true);
                writer.Write((byte)Blobs.Count);
                foreach (BlobIdentifier blob in Blobs)
                {
                    writer.Write(blob.HashData,0, ContentHash.HashLength);
                }
                writer.Write(Metadata.Length);
                writer.Write(Metadata, 0, Metadata.Length);
            }

            public static new BaseOp Deserialize(Stream stream)
            {
                (string name, string bucket, ulong locations) = BaseOp.Deserialize(stream);

                using BinaryReader reader = new BinaryReader(stream, Encoding.UTF8, leaveOpen: true);
                byte blobsCount = reader.ReadByte();
                List<BlobIdentifier> blobs = new List<BlobIdentifier>();
                for (int i = 0; i < blobsCount; i++)
                {
                    byte[] blob = new byte[ContentHash.HashLength];
                    reader.Read(blob, 0, ContentHash.HashLength);
                    blobs.Add(new BlobIdentifier(blob));
                }

                int metadataLength = reader.ReadInt32();
                byte[] metadataBuffer = new byte[metadataLength];
                reader.Read(metadataBuffer, 0, metadataLength);

                return new AddOp(name, bucket, locations, blobs, metadataBuffer);
            }
        }

        public TransactionLogDescription Describe()
        {
            return new TransactionLogDescription(_index.LogGeneration, _index.Locations);
        }
    }

    public class EventHashMismatchException : Exception
    {
        public long Offset { get; }
        public string HeaderMd5 { get; }
        public string CalculatedMd5 { get; }

        public EventHashMismatchException(long offset, string headerMd5, string calculatedMd5) : base(
            $"Read transaction id {offset} expected hash {headerMd5} but calculated {calculatedMd5}")
        {
            Offset = offset;
            HeaderMd5 = headerMd5;
            CalculatedMd5 = calculatedMd5;
        }
    }
}
