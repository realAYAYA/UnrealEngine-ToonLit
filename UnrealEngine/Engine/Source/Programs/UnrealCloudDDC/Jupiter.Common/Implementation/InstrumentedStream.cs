// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.IO;
using System.Threading;
using System.Threading.Tasks;

namespace Jupiter.Common.Implementation
{
	/// <summary>
	/// Wraps a stream and adds instrumentation for throughput which is written to a counter
	/// </summary>
	public class InstrumentedStream : Stream
	{
		private readonly Stream _streamImplementation;
		private readonly string _sourceIdentifier;
		private bool _started = false;
		private long _totalBytesWritten = 0;
		private DateTime _consumeStartedAt;

		public InstrumentedStream(Stream s, string sourceIdentifier)
		{
			_streamImplementation = s;
			_sourceIdentifier = sourceIdentifier;
		}

		public override async ValueTask DisposeAsync()
		{
			await _streamImplementation.DisposeAsync();
			await base.DisposeAsync();
			GC.SuppressFinalize(this);
		}

		public override void Close()
		{
			TimeSpan duration = DateTime.Now - _consumeStartedAt;
			double rate = _totalBytesWritten / duration.TotalSeconds;

			StatsdClient.DogStatsd.Histogram("jupiter.stream_throughput", rate, tags: new string[] {"sourceIdentifier:" + _sourceIdentifier});
			base.Close();
		}

		public override void Flush()
		{
			_streamImplementation.Flush();
		}

		public override int Read(byte[] buffer, int offset, int count)
		{
			if (!_started)
			{
				_consumeStartedAt = DateTime.Now;
				_started = true;
			}

			Interlocked.Add(ref _totalBytesWritten, count);
			return _streamImplementation.Read(buffer, offset, count);
		}

		public override long Seek(long offset, SeekOrigin origin)
		{
			return _streamImplementation.Seek(offset, origin);
		}

		public override void SetLength(long value)
		{
			_streamImplementation.SetLength(value);
		}

		public override void Write(byte[] buffer, int offset, int count)
		{
			if (!_started)
			{
				_consumeStartedAt = DateTime.Now;
				_started = true;
			}

			Interlocked.Add(ref _totalBytesWritten, count);
			_streamImplementation.Write(buffer, offset, count);
		}

		public override bool CanRead => _streamImplementation.CanRead;

		public override bool CanSeek => _streamImplementation.CanSeek;

		public override bool CanWrite => _streamImplementation.CanWrite;

		public override long Length => _streamImplementation.Length;

		public override long Position
		{
			get => _streamImplementation.Position;
			set => _streamImplementation.Position = value;
		}
	}
}
