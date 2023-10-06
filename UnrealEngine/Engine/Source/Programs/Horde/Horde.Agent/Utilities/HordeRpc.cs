// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Diagnostics.CodeAnalysis;
using System.IO;
using System.Linq;
using System.Security.Cryptography;
using Google.Protobuf;
using Google.Protobuf.Collections;

// These partial classes/extensions operate on generated gRPC and Protobuf code.
// Warnings below are disabled to avoid documenting every class touched.
#pragma warning disable CS1591
#pragma warning disable CA1716
namespace HordeCommon.Rpc
{
	partial class Property
	{
		public Property(string name, string value)
		{
			Name = name;
			Value = value;
		}

		public Property(KeyValuePair<string, string> pair)
		{
			Name = pair.Key;
			Value = pair.Value;
		}
	}

	partial class PropertyUpdate
	{
		public PropertyUpdate(string name, string? value)
		{
			Name = name;
			Value = value;
		}
	}

	static class PropertyExtensions
	{
		public static string GetValue(this RepeatedField<Property> properties, string name)
		{
			return properties.First(x => x.Name == name).Value;
		}

		public static bool TryGetValue(this RepeatedField<Property> properties, string name, [MaybeNullWhen(false)] out string result)
		{
			Property? property = properties.FirstOrDefault(x => x.Name == name);
			if (property == null)
			{
				result = null!;
				return false;
			}
			else
			{
				result = property.Value;
				return true;
			}
		}
	}

	partial class GetStreamRequest
	{
		public GetStreamRequest(string streamId)
		{
			StreamId = streamId;
		}
	}

	partial class UpdateStreamRequest
	{
		public UpdateStreamRequest(string streamId, Dictionary<string, string?> properties)
		{
			StreamId = streamId;
			Properties.AddRange(properties.Select(x => new PropertyUpdate(x.Key, x.Value)));
		}
	}

	partial class GetJobRequest
	{
		public GetJobRequest(string jobId)
		{
			JobId = jobId;
		}
	}

	partial class BeginBatchRequest
	{
		public BeginBatchRequest(string jobId, string batchId, string leaseId)
		{
			JobId = jobId;
			BatchId = batchId;
			LeaseId = leaseId;
		}
	}

	partial class FinishBatchRequest
	{
		public FinishBatchRequest(string jobId, string batchId, string leaseId)
		{
			JobId = jobId;
			BatchId = batchId;
			LeaseId = leaseId;
		}
	}

	partial class BeginStepRequest
	{
		public BeginStepRequest(string jobId, string batchId, string leaseId)
		{
			JobId = jobId;
			BatchId = batchId;
			LeaseId = leaseId;
		}
	}

	partial class UpdateStepRequest
	{
		public UpdateStepRequest(string jobId, string batchId, string stepId, JobStepState state, JobStepOutcome outcome)
		{
			JobId = jobId;
			BatchId = batchId;
			StepId = stepId;
			State = state;
			Outcome = outcome;
		}
	}
	
	partial class GetStepRequest
	{
		public GetStepRequest(string jobId, string batchId, string stepId)
		{
			JobId = jobId;
			BatchId = batchId;
			StepId = stepId;
		}
	}
	
	partial class GetStepResponse
	{
		public GetStepResponse(JobStepOutcome outcome, JobStepState state, bool abortRequested)
		{
			Outcome = outcome;
			State = state;
			AbortRequested = abortRequested;
		}
	}

	partial class CreateEventRequest
	{
		public CreateEventRequest(EventSeverity severity, string logId, int lineIndex, int lineCount)
		{
			Severity = severity;
			LogId = logId;
			LineIndex = lineIndex;
			LineCount = lineCount;
		}
	}

	partial class CreateEventsRequest
	{
		public CreateEventsRequest(IEnumerable<CreateEventRequest> events)
		{
			Events.AddRange(events);
		}
	}

	partial class WriteOutputRequest
	{
		public WriteOutputRequest(string logId, long offset, int lineIndex, ByteString data, bool flush)
		{
			LogId = logId;
			Offset = offset;
			LineIndex = lineIndex;
			Data = data;
			Flush = flush;
		}
	}

	partial class DownloadSoftwareRequest
	{
		public DownloadSoftwareRequest(string version)
		{
			Version = version;
		}
	}
}

namespace HordeCommon.Rpc.Messages.Telemetry
{
	partial class AgentMetadataEvent
	{
		/// <summary>
		/// Calculate an agent ID
		/// </summary>
		/// <returns>A unique hash for all fields</returns>
		public ulong CalculateAgentId()
		{
			using SHA256 sha256 = SHA256.Create();
			using MemoryStream ms = new(200);
			using BinaryWriter bw = new(ms);
			
			bw.Write(Ip ?? "<empty ip>");
			bw.Write(Hostname ?? "<empty hostname>");
			bw.Write(Region ?? "<empty region>");
			bw.Write(AvailabilityZone ?? "<empty az>");
			bw.Write(Environment ?? "<empty env>");
			bw.Write(AgentVersion ?? "<empty version>");
			bw.Write(Os ?? "<empty os>");
			bw.Write(OsVersion ?? "<empty os version>");
			bw.Write(Architecture ?? "<empty os architecture>");
			
			foreach (KeyValuePair<string, string> pair in Properties)
			{
				bw.Write(pair.Key ?? "<empty key>");
				bw.Write(pair.Value ?? "<empty value>");
			}
			
			foreach (string poolId in PoolIds)
			{
				bw.Write(poolId);
			}
			
			ms.Position = 0;
			byte[] hash = sha256.ComputeHash(ms);
			return BitConverter.ToUInt64(hash, 0);
		}
	}
}

#pragma warning restore CA1716
#pragma warning restore CS1591