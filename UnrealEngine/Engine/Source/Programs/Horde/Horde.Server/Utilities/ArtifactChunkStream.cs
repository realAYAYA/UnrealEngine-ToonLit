// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.IO;
using System.Threading;
using System.Threading.Tasks;
using Grpc.Core;
using HordeCommon.Rpc;

namespace Horde.Server.Utilities
{
	/// <summary>
	/// Stream that takes in rpc chunks from an upload and copies them?
	/// </summary>
	public class ArtifactChunkStream : Stream
	{
		/// <summary>
		/// The grpc client reader. Should probably be templatized
		/// </summary>
		readonly IAsyncStreamReader<UploadArtifactRequest> _reader;

		/// <summary>
		/// Position within the stream
		/// </summary>
		long _streamPosition;

		/// <summary>
		/// The length of the stream
		/// </summary>
		readonly long _streamLength;

		/// <summary>
		/// The current request being read from
		/// </summary>
		UploadArtifactRequest? _request;

		/// <summary>
		/// Position within the current request
		/// </summary>
		int _requestPos;

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="reader">the grpc reader</param>
		/// <param name="length">filesize reported by the client</param>
		public ArtifactChunkStream(IAsyncStreamReader<UploadArtifactRequest> reader, long length)
		{
			_reader = reader;
			_streamLength = length;
		}

		/// <inheritdoc/>
		public override long Position
		{
			get => _streamPosition;
			set => throw new NotSupportedException();
		}

		/// <inheritdoc/>
		public override long Length => _streamLength;

		/// <inheritdoc/>
		public override void SetLength(long value) => throw new NotSupportedException();

		/// <inheritdoc/>
		public override bool CanSeek => false;

		/// <inheritdoc/>
		public override long Seek(long offset, SeekOrigin origin) => throw new NotSupportedException();

		/// <inheritdoc/>
		public override bool CanWrite => false;

		/// <inheritdoc/>
		public override void Write(byte[] buffer, int offset, int count) => throw new NotSupportedException();

		/// <inheritdoc/>
		public override bool CanRead => true;

		/// <inheritdoc/>
		public override int Read(byte[] buffer, int offset, int count)
		{
#pragma warning disable VSTHRD002
			return ReadAsync(buffer, offset, count, CancellationToken.None).Result;
#pragma warning restore VSTHRD002
		}

		/// <inheritdoc/>
		public override async Task<int> ReadAsync(byte[] buffer, int offset, int count, CancellationToken cancellationToken)
		{
			return await ReadAsync(buffer.AsMemory(offset, count), cancellationToken);
		}

		/// <inheritdoc/>
		public override async ValueTask<int> ReadAsync(Memory<byte> buffer, CancellationToken cancellationToken)
		{
			int bytesRead = 0;
			while (bytesRead < buffer.Length && _streamPosition < _streamLength)
			{
				if (_request == null || _requestPos == _request.Data.Length)
				{
					// Read the next request object
					if (!await _reader.MoveNext())
					{
						throw new EndOfStreamException("Unexpected end of stream while reading artifact");
					}

					_request = _reader.Current;
					_requestPos = 0;
				}
				else
				{
					// Copy data from the current request object
					int numBytesToCopy = Math.Min(buffer.Length - bytesRead, _request.Data.Length - _requestPos);
					_request.Data.Span.Slice(_requestPos, numBytesToCopy).CopyTo(buffer.Slice(bytesRead, numBytesToCopy).Span);
					_requestPos += numBytesToCopy;
					bytesRead += numBytesToCopy;
					_streamPosition += numBytesToCopy;
				}
			}
			return bytesRead;
		}

		/// <inheritdoc/>
		public override void Flush()
		{
		}
	}
}
