// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.IO;
using System.Text;
using System.Threading;
using System.Threading.Tasks;
using Datadog.Trace;
using EpicGames.Horde.Storage;

namespace Jupiter.Implementation
{
    public class BatchWriter
    {
#pragma warning disable CA1028 // Enum Storage should be Int32
		public enum OpState : byte
		{
            OK = 0,
            Failed = 1,
            NotFound = 2,
            Exists = 3 // special case for HEAD checks, same as OK but indicates that there will be no payload
        };
#pragma warning restore CA1028 // Enum Storage should be Int32

		public async Task WriteToStream(Stream outStream, NamespaceId ns, List<Tuple<OpVerb, BucketId, KeyId>> ops, Func<OpVerb, NamespaceId, BucketId, KeyId, string[], Task<Tuple<ContentHash?, BlobContents?, OpState>>> fetchAction)
        {
            await outStream.WriteAsync(Encoding.ASCII.GetBytes("JPTR"));
            await outStream.WriteAsync(BitConverter.GetBytes((uint) ops.Count));

            byte[] entryHeaderBytes = Encoding.ASCII.GetBytes("JPEE");

            using SemaphoreSlim streamLock = new SemaphoreSlim(1, 1);

            foreach ((OpVerb opVerb, BucketId bucket, KeyId key) in ops)
            {
                string opTypeName = opVerb.ToString();
                using IScope scope = Tracer.Instance.StartActive($"batch.{opTypeName}");
                scope.Span.ResourceName = $"{ns}.{bucket}.{key}";

                (ContentHash? contentHash, BlobContents? blob, OpState opState) = await fetchAction(opVerb, ns, bucket, key, new [] {"blob", "contentHash"});

                // update the scope with the state of the operation to allow us to track how often each request in the batch succeeds or fails
                scope.Span.SetTag("Success", (opState is OpState.OK or OpState.Exists).ToString());
                scope.Span.SetTag("Error", (opState == OpState.Failed).ToString());

                try
                {
                    // make sure only one object writes to the result stream at a time
                    await streamLock.WaitAsync();

                    // Write custom header for each element, this header includes the cache key and its hash
                    // start with a preamble to let us verify that the stream is correct.
                    await outStream.WriteAsync(entryHeaderBytes);

                    string opName = $"{ns}.{bucket}.{key}";
                    // we null terminate the strings to be easier to use
                    await outStream.WriteAsync(Encoding.ASCII.GetBytes(opName + "\0"));

                    // write out the state byte
                    await outStream.WriteAsync(new byte[] {(byte) opState});
                    
                    // if we have a failure we do not know if the hash data or blob is valid so we only write the state code
                    // also for head checks we do not write anything more
                    if (opState != OpState.OK || opState == OpState.Exists || contentHash == null || blob == null)
                    {
                        continue;
                    }

                    await outStream.WriteAsync(contentHash.HashData);
                    await outStream.WriteAsync(BitConverter.GetBytes((ulong) blob.Length));
                    await blob.Stream.CopyToAsync(outStream);
                }
                finally
                {
                    streamLock.Release();
                }
            }
        }

        public enum OpVerb
        {
            GET,
            HEAD
        }
    }
}
