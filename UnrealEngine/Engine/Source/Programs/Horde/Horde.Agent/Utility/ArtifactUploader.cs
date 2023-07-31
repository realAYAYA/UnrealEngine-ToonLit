// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.IO;
using System.Threading;
using System.Threading.Tasks;
using EpicGames.Core;
using Grpc.Core;
using HordeCommon.Rpc;
using Microsoft.Extensions.Logging;

namespace Horde.Agent.Utility
{
	/// <summary>
	/// Helper class for uploading artifacts to the server
	/// </summary>
	static class ArtifactUploader
	{
        /// <summary>
        /// Dictionary of horde common mime types
        /// </summary>
        private static readonly Dictionary<string, string> s_hordeMimeTypes = new Dictionary<string, string>()
        {
            { ".bin", "application/octet-stream" },
            { ".json", "application/json" },
            { ".pm", "text/plain" },
            { ".pl", "text/plain" },
            { ".sh", "application/x-sh" },
            { ".txt", "text/plain" },
            { ".log", "text/plain" },
            { ".xml", "text/xml" }
        };

        /// <summary>
        /// Gets a mime type from a file extension
        /// </summary>
        /// <param name="file">The file object to parse</param>
        /// <returns>the mimetype if present in the dictionary, binary otherwise</returns>
        private static string GetMimeType(FileReference file)
        {
            string? contentType;
            if (!s_hordeMimeTypes.TryGetValue(file.GetExtension(), out contentType))
            {
                contentType = "application/octet-stream";
            }
            return contentType;
        }
        
        /// <summary>
        /// Uploads an artifact (with retries)
        /// </summary>
        /// <param name="rpcConnection">The grpc client</param>
        /// <param name="jobId">Job id</param>
        /// <param name="batchId">Job batch id</param>
        /// <param name="stepId">Job step id</param>
        /// <param name="artifactName">Name of the artifact</param>
        /// <param name="artifactFile">File to upload</param>
        /// <param name="logger">Logger interfact</param>
        /// <param name="cancellationToken">Cancellation token</param>
        /// <returns></returns>
        public static async Task<string?> UploadAsync(IRpcConnection rpcConnection, string jobId, string batchId, string stepId, string artifactName, FileReference artifactFile, ILogger logger, CancellationToken cancellationToken)
        {
			try
			{
				string artifactId = await rpcConnection.InvokeAsync(rpcClient => DoUploadAsync(rpcClient, jobId, batchId, stepId, artifactName, artifactFile, logger, cancellationToken), new RpcContext(), cancellationToken);
				return artifactId;
			}
			catch (Exception ex)
			{
				logger.LogInformation(KnownLogEvents.Systemic_Horde_ArtifactUpload, ex, "Exception while attempting to upload artifact: {Message}", ex.Message);
				return null;
			}
        }

        /// <summary>
		/// Uploads an artifact
		/// </summary>
		/// <param name="client">The grpc client</param>
		/// <param name="jobId">Job id</param>
		/// <param name="batchId">Job batch id</param>
		/// <param name="stepId">Job step id</param>
		/// <param name="artifactName">Name of the artifact</param>
		/// <param name="artifactFile">File to upload</param>
		/// <param name="logger">Logger interfact</param>
		/// <param name="cancellationToken">Cancellation token</param>
		/// <returns></returns>
		private static async Task<string> DoUploadAsync(HordeRpc.HordeRpcClient client, string jobId, string batchId, string stepId, string artifactName, FileReference artifactFile, ILogger logger, CancellationToken cancellationToken)
        {
			logger.LogInformation("Uploading artifact {ArtifactName} from {ArtifactFile}", artifactName, artifactFile);
            using (FileStream artifactStream = FileReference.Open(artifactFile, FileMode.Open, FileAccess.Read, FileShare.Read))
			{
				using (AsyncClientStreamingCall<UploadArtifactRequest, UploadArtifactResponse> cursor = client.UploadArtifact(null, null, cancellationToken))
                {
					// Upload the metadata in the initial request
					UploadArtifactMetadata metadata = new UploadArtifactMetadata();
					metadata.JobId = jobId;
					metadata.BatchId = batchId;
					metadata.StepId = stepId;
					metadata.Name = artifactName;
					metadata.MimeType = GetMimeType(artifactFile);
					metadata.Length = artifactStream.Length;

					UploadArtifactRequest initialRequest = new UploadArtifactRequest();
					initialRequest.Metadata = metadata;

					await cursor.RequestStream.WriteAsync(initialRequest, cancellationToken);

					// Upload the data in chunks
					byte[] buffer = new byte[4096];
					for (int offset = 0; offset < metadata.Length; )
                    {
                        int bytesRead = await artifactStream.ReadAsync(buffer, 0, buffer.Length, cancellationToken);
						if(bytesRead == 0)
						{
							throw new InvalidDataException($"Unable to read data from {artifactFile} beyond offset {offset}; expected length to be {metadata.Length}");
						}

						UploadArtifactRequest request = new UploadArtifactRequest();
						request.Data = Google.Protobuf.ByteString.CopyFrom(buffer, 0, bytesRead);
						await cursor.RequestStream.WriteAsync(request, cancellationToken);

                        offset += bytesRead;
                    }

					// Close the stream
                    await cursor.RequestStream.CompleteAsync();

					// Read the response
					UploadArtifactResponse response = await cursor.ResponseAsync;
					return response.Id;
                }
            }
        }
	}
}
