// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.IO;
using System.Threading.Tasks;

namespace Jupiter.Implementation
{
    public sealed class BlobContents : IDisposable, IAsyncDisposable
    {
        public BlobContents(Stream stream, long length)
        {
            Stream = stream;
            Length = length;
        }

        public BlobContents(byte[] payload)
        {
            Stream = new MemoryStream(payload);
            Length = payload.LongLength;
        }

        public Stream Stream { get; }
        public long Length {get;}

        public void Dispose()
        {
            Stream?.Dispose();
        }

        public ValueTask DisposeAsync()
        {
            Stream?.Dispose();
            return ValueTask.CompletedTask;
        }
    }
}
