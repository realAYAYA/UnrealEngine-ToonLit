// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.IO;
using System.Threading.Tasks;

namespace Jupiter.Implementation
{
    public sealed class BlobContents : IDisposable, IAsyncDisposable
    {
        private readonly Stream? _stream;

        public BlobContents(Stream stream, long length, string? localPath = null)
        {
            _stream = stream;

            Length = length;
            LocalPath = localPath;
        }
        
        public BlobContents(byte[] payload)
        {
            _stream = new MemoryStream(payload);
            Length = payload.LongLength;
            LocalPath = null;
        }

        public BlobContents(Uri redirectUri)
        {
            RedirectUri = redirectUri;
        }

        public Uri? RedirectUri { get; }

        public Stream Stream 
        { 
            get
            {
                if (_stream == null)
                {
                    throw new Exception("Stream not set in blob contents, did you specify support for redirect uris but forgot to check result?");
                }
                return _stream;
            }
        }
        public long Length { get; }

        public string? LocalPath { get; }

        public void Dispose()
        {
            _stream?.Dispose();
        }
        public ValueTask DisposeAsync()
        {
            _stream?.Dispose();
            return ValueTask.CompletedTask;
        }
    }
}
