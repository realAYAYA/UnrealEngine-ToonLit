// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Globalization;
using System.IO;
using System.Text;
using System.Threading;
using System.Threading.Tasks;
using Amazon;
using Amazon.Extensions.NETCore.Setup;
using Amazon.Runtime;
using Amazon.S3;
using Amazon.S3.Model;
using Horde.Build.Utilities;
using Microsoft.Extensions.Configuration;
using Microsoft.Extensions.Logging;
using OpenTracing;
using OpenTracing.Util;

namespace Horde.Build.Storage.Backends
{
	/// <summary>
	/// Exception wrapper for S3 requests
	/// </summary>
	public sealed class AwsException : Exception
	{
		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="message">Message for the exception</param>
		/// <param name="innerException">Inner exception data</param>
		public AwsException(string? message, Exception? innerException)
			: base(message, innerException)
		{
		}
	}

	/// <summary>
	/// Credentials to use for AWS
	/// </summary>
	public enum AwsCredentialsType
	{
		/// <summary>
		/// Use default credentials from the AWS SDK
		/// </summary>
		Default,

		/// <summary>
		/// Read credentials from the <see cref="IAwsStorageOptions.AwsProfile"/> profile in the AWS config file
		/// </summary>
		Profile,

		/// <summary>
		/// Assume a particular role. Should specify ARN in <see cref="IAwsStorageOptions.AwsRole"/>
		/// </summary>
		AssumeRole,

		/// <summary>
		/// Assume a particular role using the current environment variables.
		/// </summary>
		AssumeRoleWebIdentity,
	}

	/// <summary>
	/// Options for AWS
	/// </summary>
	public interface IAwsStorageOptions
	{
		/// <summary>
		/// Type of credentials to use
		/// </summary>
		public AwsCredentialsType AwsCredentials { get; }

		/// <summary>
		/// Name of the bucket to use
		/// </summary>
		public string? AwsBucketName { get; }

		/// <summary>
		/// Base path within the bucket 
		/// </summary>
		public string? AwsBucketPath { get; }

		/// <summary>
		/// ARN of a role to assume
		/// </summary>
		public string? AwsRole { get; }

		/// <summary>
		/// The AWS profile to read credentials form
		/// </summary>
		public string? AwsProfile { get; }

		/// <summary>
		/// Region to connect to
		/// </summary>
		public string? AwsRegion { get; }
	}

	/// <summary>
	/// FileStorage implementation using an s3 bucket
	/// </summary>
	public sealed class AwsStorageBackend : IStorageBackend, IDisposable
	{
		/// <summary>
		/// S3 Client
		/// </summary>
		private readonly IAmazonS3 _client;

		/// <summary>
		/// Options for AWs
		/// </summary>
		private readonly IAwsStorageOptions _options;

		/// <summary>
		/// Semaphore for connecting to AWS
		/// </summary>
		private readonly SemaphoreSlim _semaphore;

		/// <summary>
		/// Logger interface
		/// </summary>
		private readonly ILogger _logger;

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="configuration">Global configuration object</param>
		/// <param name="options">Storage options</param>
		/// <param name="logger">Logger interface</param>
		public AwsStorageBackend(IConfiguration configuration, IAwsStorageOptions options, ILogger<AwsStorageBackend> logger)
		{
			AWSOptions awsOptions = GetAwsOptions(configuration, options);

			_client = awsOptions.CreateServiceClient<IAmazonS3>();
			_options = options;
			_semaphore = new SemaphoreSlim(16);
			_logger = logger;

			logger.LogInformation("Created AWS storage backend for bucket {BucketName} using credentials {Credentials} {CredentialsStr}", options.AwsBucketName, awsOptions.Credentials.GetType(), awsOptions.Credentials.ToString());
		}

		/// <summary>
		/// Gets the AWS options 
		/// </summary>
		/// <param name="configuration">Global configuration object</param>
		/// <param name="options">AWS storage options</param>
		/// <returns></returns>
		static AWSOptions GetAwsOptions(IConfiguration configuration, IAwsStorageOptions options)
		{
			AWSOptions awsOptions = configuration.GetAWSOptions();
			if (options.AwsRegion != null)
			{
				awsOptions.Region = RegionEndpoint.GetBySystemName(options.AwsRegion);
			}

			switch (options.AwsCredentials)
			{
				case AwsCredentialsType.Default:
					// Using the fallback credentials from the AWS SDK, it will pick up credentials through a number of default mechanisms.
					awsOptions.Credentials = FallbackCredentialsFactory.GetCredentials();
					break;
				case AwsCredentialsType.Profile:
					if (options.AwsProfile == null)
					{
						throw new AwsException($"Missing {nameof(IAwsStorageOptions.AwsProfile)} setting for configuring {nameof(AwsStorageBackend)}", null);
					}

					(string accessKey, string secretAccessKey, string secretToken) = AwsHelper.ReadAwsCredentials(options.AwsProfile);
					awsOptions.Credentials = new Amazon.SecurityToken.Model.Credentials(accessKey, secretAccessKey, secretToken, DateTime.Now + TimeSpan.FromHours(12));
					break;
				case AwsCredentialsType.AssumeRole:
					if(options.AwsRole == null)
					{
						throw new AwsException($"Missing {nameof(IAwsStorageOptions.AwsRole)} setting for configuring {nameof(AwsStorageBackend)}", null);
					}
					awsOptions.Credentials = new AssumeRoleAWSCredentials(FallbackCredentialsFactory.GetCredentials(), options.AwsRole, "Horde");
					break;
				case AwsCredentialsType.AssumeRoleWebIdentity:
					awsOptions.Credentials = AssumeRoleWithWebIdentityCredentials.FromEnvironmentVariables();
					break;
			}
			return awsOptions;
		}

		/// <inheritdoc/>
		public void Dispose()
		{
			_client.Dispose();
			_semaphore.Dispose();
		}

		class WrappedResponseStream : Stream
		{
			readonly IDisposable _semaphore;
			readonly GetObjectResponse _response;
			readonly Stream _responseStream;

			public WrappedResponseStream(IDisposable semaphore, GetObjectResponse response)
			{
				_semaphore = semaphore;
				_response = response;
				_responseStream = response.ResponseStream;
			}

			public override bool CanRead => true;
			public override bool CanSeek => false;
			public override bool CanWrite => false;
			public override long Length => _responseStream.Length;
			public override long Position { get => _responseStream.Position; set => throw new NotSupportedException(); }

			public override void Flush() => _responseStream.Flush();

			public override int Read(byte[] buffer, int offset, int count) => _responseStream.Read(buffer, offset, count);
			public override ValueTask<int> ReadAsync(Memory<byte> buffer, CancellationToken cancellationToken = default) => _responseStream.ReadAsync(buffer, cancellationToken);
			public override Task<int> ReadAsync(byte[] buffer, int offset, int count, CancellationToken cancellationToken) => _responseStream.ReadAsync(buffer, offset, count, cancellationToken);

			public override long Seek(long offset, SeekOrigin origin) => throw new NotSupportedException();
			public override void SetLength(long value) => throw new NotSupportedException();
			public override void Write(byte[] buffer, int offset, int count) => throw new NotSupportedException();

			protected override void Dispose(bool disposing)
			{
				base.Dispose(disposing);

				_semaphore.Dispose();
				_response.Dispose();
				_responseStream.Dispose();
			}

			public override async ValueTask DisposeAsync()
			{
				await base.DisposeAsync();

				_semaphore.Dispose();
				_response.Dispose();
				await _responseStream.DisposeAsync();
			}
		}

		string GetFullPath(string path)
		{
			if (_options.AwsBucketPath == null)
			{
				return path;
			}

			StringBuilder result = new StringBuilder();
			result.Append(_options.AwsBucketPath);
			if (result.Length > 0 && result[^1] != '/')
			{
				result.Append('/');
			}
			result.Append(path);
			return result.ToString();
		}

		/// <inheritdoc/>
		public async Task<Stream?> ReadAsync(string path, CancellationToken cancellationToken)
		{
			using IScope scope = GlobalTracer.Instance.BuildSpan("AwsStorageBackend.ReadAsync").StartActive();
			scope.Span.SetTag("Path", path);
			
			string fullPath = GetFullPath(path);

			IDisposable? semaLock = null;
			GetObjectResponse? response = null;
			try
			{
				semaLock = await _semaphore.UseWaitAsync(cancellationToken);

				GetObjectRequest newGetRequest = new GetObjectRequest();
				newGetRequest.BucketName = _options.AwsBucketName;
				newGetRequest.Key = fullPath;

				response = await _client.GetObjectAsync(newGetRequest, cancellationToken);

				return new WrappedResponseStream(semaLock, response);
			}
			catch (Exception ex)
			{
				_logger.LogWarning(ex, "Unable to read {Path} from S3", fullPath);

				semaLock?.Dispose();
				response?.Dispose();

				return null;
			}
		}

		/// <inheritdoc/>
		public async Task WriteAsync(string path, Stream inputStream, CancellationToken cancellationToken)
		{
			TimeSpan[] retryTimes =
			{
				TimeSpan.FromSeconds(1.0),
				TimeSpan.FromSeconds(5.0),
				TimeSpan.FromSeconds(10.0),
			};

			string fullPath = GetFullPath(path);
			for (int attempt = 0; ; attempt++)
			{
				try
				{
					using IDisposable semaLock = await _semaphore.UseWaitAsync(cancellationToken);
					await WriteInternalAsync(fullPath, inputStream, cancellationToken);
					_logger.LogDebug("Written data to {Path}", path);
					break;
				}
				catch (Exception ex)
				{
					_logger.LogError(ex, "Unable to write data to {Path} ({Attempt}/{AttemptCount})", fullPath, attempt + 1, retryTimes.Length + 1);
					if (attempt >= retryTimes.Length)
					{
						throw new AwsException($"Unable to write to bucket {_options.AwsBucketName} path {fullPath}", ex);
					}
				}

				await Task.Delay(retryTimes[attempt], cancellationToken);
			}
		}

		/// <inheritdoc/>
		public async Task WriteInternalAsync(string fullPath, Stream stream, CancellationToken cancellationToken)
		{
			using IScope scope = GlobalTracer.Instance.BuildSpan("AwsStorageBackend.WriteInternalAsync").StartActive();
			scope.Span.SetTag("Path", fullPath);
			
			const int MinPartSize = 5 * 1024 * 1024;

			long streamLen = stream.Length;
			if (streamLen < MinPartSize)
			{
				// Read the data into memory. Errors with hash value not matching if we don't do this (?)
				byte[] buffer = new byte[streamLen];
				await ReadExactLengthAsync(stream, buffer, (int)streamLen, cancellationToken);

				// Upload it to S3
				using (MemoryStream inputStream = new MemoryStream(buffer))
				{
					PutObjectRequest uploadRequest = new PutObjectRequest();
					uploadRequest.BucketName = _options.AwsBucketName;
					uploadRequest.Key = fullPath;
					uploadRequest.InputStream = inputStream;
					uploadRequest.Metadata.Add("bytes-written", streamLen.ToString(CultureInfo.InvariantCulture));
					await _client.PutObjectAsync(uploadRequest, cancellationToken);
				}
			}
			else
			{
				// Initiate a multi-part upload
				InitiateMultipartUploadRequest initiateRequest = new InitiateMultipartUploadRequest();
				initiateRequest.BucketName = _options.AwsBucketName;
				initiateRequest.Key = fullPath;

				InitiateMultipartUploadResponse initiateResponse = await _client.InitiateMultipartUploadAsync(initiateRequest, cancellationToken);
				try
				{
					// Buffer for reading the data
					byte[] buffer = new byte[MinPartSize];

					// Upload all the parts
					List<PartETag> partTags = new List<PartETag>();
					for (long streamPos = 0; streamPos < streamLen;)
					{
						// Read the next chunk of data into the buffer
						int bufferLen = (int)Math.Min((long)MinPartSize, streamLen - streamPos);
						await ReadExactLengthAsync(stream, buffer, bufferLen, cancellationToken);
						streamPos += bufferLen;

						// Upload the part
						using (MemoryStream inputStream = new MemoryStream(buffer, 0, bufferLen))
						{
							UploadPartRequest partRequest = new UploadPartRequest();
							partRequest.BucketName = _options.AwsBucketName;
							partRequest.Key = fullPath;
							partRequest.UploadId = initiateResponse.UploadId;
							partRequest.InputStream = inputStream;
							partRequest.PartSize = bufferLen;
							partRequest.PartNumber = partTags.Count + 1;
							partRequest.IsLastPart = (streamPos == streamLen);

							UploadPartResponse partResponse = await _client.UploadPartAsync(partRequest, cancellationToken);
							partTags.Add(new PartETag(partResponse.PartNumber, partResponse.ETag));
						}
					}

					// Mark the upload as complete
					CompleteMultipartUploadRequest completeRequest = new CompleteMultipartUploadRequest();
					completeRequest.BucketName = _options.AwsBucketName;
					completeRequest.Key = fullPath;
					completeRequest.UploadId = initiateResponse.UploadId;
					completeRequest.PartETags = partTags;
					await _client.CompleteMultipartUploadAsync(completeRequest, cancellationToken);
				}
				catch
				{
					// Abort the upload
					AbortMultipartUploadRequest abortRequest = new AbortMultipartUploadRequest();
					abortRequest.BucketName = _options.AwsBucketName;
					abortRequest.Key = fullPath;
					abortRequest.UploadId = initiateResponse.UploadId;
					await _client.AbortMultipartUploadAsync(abortRequest, cancellationToken);

					throw;
				}
			}
		}

		/// <summary>
		/// Reads data of an exact length into a stream
		/// </summary>
		/// <param name="stream">The stream to read from</param>
		/// <param name="buffer">The buffer to read into</param>
		/// <param name="length">Length of the data to read</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		/// <returns>Async task</returns>
		static async Task ReadExactLengthAsync(System.IO.Stream stream, byte[] buffer, int length, CancellationToken cancellationToken)
		{
			int bufferPos = 0;
			while (bufferPos < length)
			{
				int bytesRead = await stream.ReadAsync(buffer, bufferPos, length - bufferPos, cancellationToken);
				if (bytesRead == 0)
				{
					throw new InvalidOperationException("Unexpected end of stream");
				}
				bufferPos += bytesRead;
			}
		}

		/// <inheritdoc/>
		public async Task DeleteAsync(string path, CancellationToken cancellationToken)
		{
			using IScope scope = GlobalTracer.Instance.BuildSpan("AwsStorageBackend.DeleteAsync").StartActive();
			scope.Span.SetTag("Path", path);
			
			DeleteObjectRequest newDeleteRequest = new DeleteObjectRequest();
			newDeleteRequest.BucketName = _options.AwsBucketName;
			newDeleteRequest.Key = GetFullPath(path);
			await _client.DeleteObjectAsync(newDeleteRequest, cancellationToken);
		}

		/// <inheritdoc/>
		public async Task<bool> ExistsAsync(string path, CancellationToken cancellationToken)
		{
			using IScope scope = GlobalTracer.Instance.BuildSpan("AwsStorageBackend.ExistsAsync").StartActive();
			scope.Span.SetTag("Path", path);
			
			try
			{
				GetObjectMetadataRequest request = new GetObjectMetadataRequest();
				request.BucketName = _options.AwsBucketName;
				request.Key = GetFullPath(path);
				await _client.GetObjectMetadataAsync(request, cancellationToken);
				return true;
			}
			catch (AmazonS3Exception ex) when (ex.StatusCode == System.Net.HttpStatusCode.NotFound)
			{
				return false;
			}
		}
	}
}
