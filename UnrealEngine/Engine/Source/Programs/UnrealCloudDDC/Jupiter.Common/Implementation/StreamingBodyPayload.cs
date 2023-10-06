// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.IO;
using System.Threading.Tasks;
using Jupiter.Utils;
using Microsoft.AspNetCore.Http;
using Microsoft.Extensions.Options;
using OpenTelemetry.Trace;

namespace Jupiter.Common.Implementation
{
    public interface IBufferedPayload : IDisposable
    {
        Stream GetStream();
        long Length { get; }
    }

    /// <summary>
    /// Streaming request that is streamed into memory
    /// </summary>
    public sealed class MemoryBufferedPayload : IBufferedPayload
    {
        private readonly byte[] _buffer;

        public MemoryBufferedPayload(byte[] source)
        {
            _buffer = source;
        }

        public static async Task<MemoryBufferedPayload> Create(Tracer tracer, Stream s)
        {
            using TelemetrySpan scope = tracer.StartActiveSpan("payload.buffer")
                .SetAttribute("operation.name", "payload.buffer")
                .SetAttribute("bufferType", "Memory");
            MemoryBufferedPayload payload = new MemoryBufferedPayload(await s.ToByteArray());

            return payload;
        }

        public void Dispose()
        {
            
        }

        public Stream GetStream()
        {
            return new MemoryStream(_buffer);
        }

        public long Length => _buffer.LongLength;
    }

    /// <summary>
    /// A streaming request backed by a temporary file on disk
    /// </summary>
    public sealed class FilesystemBufferedPayload : IBufferedPayload
    {
        private readonly FileInfo _tempFile;
        private long _length;

        private FilesystemBufferedPayload()
        {
            _tempFile = new FileInfo(Path.GetTempFileName());
        }

        public static async Task<FilesystemBufferedPayload> Create(Tracer tracer, Stream s)
        {
            FilesystemBufferedPayload payload = new FilesystemBufferedPayload();

            {
                using TelemetrySpan? scope = tracer.StartActiveSpan("payload.buffer")
                    .SetAttribute("operation.name", "payload.buffer")
                    .SetAttribute("bufferType", "Filesystem");
                await using FileStream fs = payload._tempFile.OpenWrite();
                await s.CopyToAsync(fs);
            }
            
            payload._tempFile.Refresh();
            payload._length = payload._tempFile.Length;

            return payload;
        }

        public void Dispose()
        {
            if (_tempFile.Exists)
            {
                _tempFile.Delete();
            }
        }

        public Stream GetStream()
        {
            return _tempFile.OpenRead();
        }

        public long Length => _length;
    }

    public class BufferedPayloadFactory
    {
        private readonly IOptionsMonitor<JupiterSettings> _jupiterSettings;
        private readonly Tracer _tracer;

        public BufferedPayloadFactory(IOptionsMonitor<JupiterSettings> jupiterSettings, Tracer tracer)
        {
            _jupiterSettings = jupiterSettings;
            _tracer = tracer;
        }

        public Task<IBufferedPayload> CreateFromRequest(HttpRequest request)
        {
            long? contentLength = request.ContentLength;

            if (contentLength == null)
            {
                throw new Exception("Expected content-length on all requests");
            }

            return CreateFromStream(request.Body, contentLength.Value);
        }

        public async Task<IBufferedPayload> CreateFromStream(Stream s, long contentLength)
        {
            // blob is small enough to fit into memory we just read it as is
            if (contentLength < _jupiterSettings.CurrentValue.MemoryBufferSize)
            {
                return await MemoryBufferedPayload.Create(_tracer, s);
            }

            return await FilesystemBufferedPayload.Create(_tracer, s);
        }

        public async Task<IBufferedPayload> CreateFilesystemBufferedPayload(Stream s)
        {
            return await FilesystemBufferedPayload.Create(_tracer, s);
        }
    }
}
