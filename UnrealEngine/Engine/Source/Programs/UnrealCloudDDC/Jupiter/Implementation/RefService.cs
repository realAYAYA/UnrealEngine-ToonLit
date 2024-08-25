// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Linq;
using System.Threading.Tasks;
using EpicGames.AspNet;
using EpicGames.Horde.Storage;
using EpicGames.Serialization;
using Jupiter.Common;
using Jupiter.Implementation.Blob;
using Jupiter.Utils;
using Microsoft.AspNetCore.Http;
using Microsoft.Extensions.DependencyInjection;
using Microsoft.Extensions.Logging;
using Microsoft.Extensions.Options;
using OpenTelemetry.Trace;

namespace Jupiter.Implementation
{
	public class ObjectService : IRefService
	{
		private readonly IHttpContextAccessor _httpContextAccessor;
		private readonly IReferencesStore _referencesStore;
		private readonly IBlobService _blobService;
		private readonly IReferenceResolver _referenceResolver;
		private readonly IReplicationLog _replicationLog;
		private readonly IBlobIndex _blobIndex;
		private readonly INamespacePolicyResolver _namespacePolicyResolver;
		private readonly ILastAccessTracker<LastAccessRecord> _lastAccessTracker;
		private readonly Tracer _tracer;
		private readonly ILogger _logger;
		private readonly IOptionsMonitor<UnrealCloudDDCSettings> _cloudDDCSettings;

		public ObjectService(IHttpContextAccessor httpContextAccessor, IReferencesStore referencesStore, IBlobService blobService, IReferenceResolver referenceResolver, IReplicationLog replicationLog, IBlobIndex blobIndex, INamespacePolicyResolver namespacePolicyResolver, ILastAccessTracker<LastAccessRecord> lastAccessTracker, Tracer tracer, ILogger<ObjectService> logger, IOptionsMonitor<UnrealCloudDDCSettings> cloudDDCSettings)
		{
			_httpContextAccessor = httpContextAccessor;
			_referencesStore = referencesStore;
			_blobService = blobService;
			_referenceResolver = referenceResolver;
			_replicationLog = replicationLog;
			_blobIndex = blobIndex;
			_namespacePolicyResolver = namespacePolicyResolver;
			_lastAccessTracker = lastAccessTracker;
			_tracer = tracer;
			_logger = logger;
			_cloudDDCSettings = cloudDDCSettings;
		}

		public Task<(RefRecord, BlobContents?)> GetAsync(NamespaceId ns, BucketId bucket, RefId key, string[]? fields = null, bool doLastAccessTracking = true)
		{
			return GetAsync(ns, bucket, key, fields, doLastAccessTracking, skipCache: false);
		}

		// ReSharper disable once MethodOverloadWithOptionalParameter - this private overload exists only for bypassing cache for internal use within this service
		private async Task<(RefRecord, BlobContents?)> GetAsync(NamespaceId ns, BucketId bucket, RefId key, string[]? fields = null, bool doLastAccessTracking = true, bool skipCache = false)
		{
			// if no field filtering is being used we assume everything is needed
			IReferencesStore.FieldFlags flags = IReferencesStore.FieldFlags.All;
			if (fields != null)
			{
				// empty array means fetch all fields
				if (fields.Length == 0)
				{
					flags = IReferencesStore.FieldFlags.All;
				}
				else
				{
					flags = fields.Contains("payload")
						? IReferencesStore.FieldFlags.IncludePayload
						: IReferencesStore.FieldFlags.None;
				}
			}

			IReferencesStore.OperationFlags opFlags = IReferencesStore.OperationFlags.None;
			if (skipCache)
			{
				opFlags |= IReferencesStore.OperationFlags.BypassCache;
			}

			IServerTiming? serverTiming = _httpContextAccessor.HttpContext?.RequestServices.GetService<IServerTiming>();

			RefRecord o;
			{
				using ServerTimingMetricScoped? serverTimingScope = serverTiming?.CreateServerTimingMetricScope("ref.get", "Fetching Ref from DB");

				o = await _referencesStore.GetAsync(ns, bucket, key, flags, opFlags);
			}

			if (doLastAccessTracking)
			{
				NamespacePolicy.StoragePoolGCMethod gcPolicy = _namespacePolicyResolver.GetPoliciesForNs(ns).GcMethod ?? NamespacePolicy.StoragePoolGCMethod.LastAccess;
				if (gcPolicy == NamespacePolicy.StoragePoolGCMethod.LastAccess)
				{
					// we do not wait for the last access tracking as it does not matter when it completes
					Task lastAccessTask = _lastAccessTracker.TrackUsed(new LastAccessRecord(ns, bucket, key)).ContinueWith((task, _) =>
					{
						if (task.Exception != null)
						{
							_logger.LogError(task.Exception, "Exception when tracking last access record");
						}
					}, null, TaskScheduler.Current);
				}
			}

			BlobContents? blobContents = null;
			if ((flags & IReferencesStore.FieldFlags.IncludePayload) != 0)
			{
				if (o.InlinePayload != null && o.InlinePayload.Length != 0)
				{
#pragma warning disable CA2000 // Dispose objects before losing scope , ownership is transfered to caller
					blobContents = new BlobContents(o.InlinePayload);
#pragma warning restore CA2000 // Dispose objects before losing scope
				}
				else
				{
					using ServerTimingMetricScoped? serverTimingScope = serverTiming?.CreateServerTimingMetricScope("blob.get", "Downloading blob from store");

					blobContents = await _blobService.GetObjectAsync(ns, o.BlobIdentifier);
				}
			}

			return (o, blobContents);
		}

		public async Task<(ContentId[], BlobId[])> PutAsync(NamespaceId ns, BucketId bucket, RefId key, BlobId blobHash, CbObject payload)
		{
			IServerTiming? serverTiming = _httpContextAccessor.HttpContext?.RequestServices.GetService<IServerTiming>();
			using ServerTimingMetricScoped? serverTimingScope = serverTiming?.CreateServerTimingMetricScope("ref.put", "Inserting ref");

			bool hasReferences = HasAttachments(payload);

			// if we have no references we are always finalized, e.g. there are no referenced blobs to upload
			bool isFinalized = !hasReferences;

			Task objectStorePut = _referencesStore.PutAsync(ns, bucket, key, blobHash, payload.GetView().ToArray(), isFinalized);

			Task<BlobId>? blobStorePut = null;
			if (_cloudDDCSettings.CurrentValue.EnablePutRefBodyIntoBlobStore)
			{
				blobStorePut = _blobService.PutObjectAsync(ns, payload.GetView().ToArray(), blobHash);
			}

			await objectStorePut;
			if (blobStorePut != null)
			{
				await blobStorePut;
			}

			return await DoFinalizeAsync(ns, bucket, key, blobHash, payload);
		}

		private bool HasAttachments(CbObject payload)
		{
			bool FieldHasAttachments(CbField field)
			{
				if (field.IsObject())
				{
					bool hasAttachment = HasAttachments(field.AsObject());
					if (hasAttachment)
					{
						return true;
					}
				}

				if (field.IsArray())
				{
					foreach (CbField subField in field.AsArray())
					{
						bool hasAttachment = FieldHasAttachments(subField);
						if (hasAttachment)
						{
							return true;
						}
					}
				}

				return field.IsAttachment();
			}

			return payload.Any(FieldHasAttachments);
		}

		public async Task<(ContentId[], BlobId[])> FinalizeAsync(NamespaceId ns, BucketId bucket, RefId key, BlobId blobHash)
		{
			// finalize is intended to verify the state of the object in the db, so we bypass any caches to make sure the object we are working on is not stale
			(RefRecord o, BlobContents? blob) = await GetAsync(ns, bucket, key, skipCache: true);
			if (blob == null)
			{
				throw new InvalidOperationException("No blob when attempting to finalize");
			}

			byte[] blobContents = await blob.Stream.ToByteArrayAsync();
			CbObject payload = new CbObject(blobContents);

			if (!o.BlobIdentifier.Equals(blobHash))
			{
				throw new ObjectHashMismatchException(ns, bucket, key, blobHash, o.BlobIdentifier);
			}

			return await DoFinalizeAsync(ns, bucket, key, blobHash, payload);
		}

		
		private async Task<(ContentId[], BlobId[])> DoFinalizeAsync(NamespaceId ns, BucketId bucket, RefId key, BlobId blobHash, CbObject payload)
		{
			IServerTiming? serverTiming = _httpContextAccessor.HttpContext?.RequestServices.GetService<IServerTiming>();
			using ServerTimingMetricScoped? serverTimingScope = serverTiming?.CreateServerTimingMetricScope("ref.finalize", "Finalizing the ref");

			Task addRefToBlobsTask = _blobIndex.AddRefToBlobsAsync(ns, bucket, key, new [] {blobHash});
			Task addToBucketListTask = _cloudDDCSettings.CurrentValue.EnableBucketStatsTracking
				? _blobIndex.AddBlobToBucketListAsync(ns, bucket, key, blobHash, (long)payload.GetView().Length)
				: Task.CompletedTask;
			ContentId[] missingReferences = Array.Empty<ContentId>();
			BlobId[] missingBlobs = Array.Empty<BlobId>();

			List<Task> addToBucketTasks = new List<Task>();
			List<Task> addRefMappingTasks = new List<Task>();

			bool hasReferences = HasAttachments(payload);
			if (hasReferences)
			{
				using TelemetrySpan _ = _tracer.StartActiveSpan("ObjectService.ResolveReferences").SetAttribute("operation.name", "ObjectService.ResolveReferences");
				try
				{
					IAsyncEnumerable<BlobId> references = _referenceResolver.GetReferencedBlobs(ns, payload);

					await foreach (BlobId blobId in references)
					{
						if (_cloudDDCSettings.CurrentValue.EnableBucketStatsTracking)
						{
							addToBucketTasks.Add(Task.Run( async () =>
							{
								// if a blob is missing its not a error, the finalize will report this as missing and it will be uploaded and finalize ran again
								try
								{
									BlobMetadata result = await _blobService.GetObjectMetadataAsync(ns, blobId);
									await _blobIndex.AddBlobToBucketListAsync(ns, bucket, key, blobId, result.Length);
								}
								catch (BlobNotFoundException)
								{
								}
							}));
						}

						addRefMappingTasks.Add(_blobIndex.AddRefToBlobsAsync(ns, bucket, key, new BlobId[] {blobId}));
					}
				}
				catch (PartialReferenceResolveException e)
				{
					missingReferences = e.UnresolvedReferences.ToArray();
				}
				catch (ReferenceIsMissingBlobsException e)
				{
					missingBlobs = e.MissingBlobs.ToArray();
				}
			}

			await Task.WhenAll(addRefToBlobsTask, addToBucketListTask);

			if (missingReferences.Length == 0 && missingBlobs.Length == 0)
			{
				await _referencesStore.FinalizeAsync(ns, bucket, key, blobHash);
				await _replicationLog.InsertAddEventAsync(ns, bucket, key, blobHash);
			}

			await Task.WhenAll(Task.WhenAll(addToBucketTasks), Task.WhenAll(addRefMappingTasks));

			return (missingReferences, missingBlobs);
		}

		public IAsyncEnumerable<NamespaceId> GetNamespacesAsync()
		{
			return _referencesStore.GetNamespacesAsync();
		}

		public Task<bool> DeleteAsync(NamespaceId ns, BucketId bucket, RefId key)
		{
			return _referencesStore.DeleteAsync(ns, bucket, key);
		}

		public Task<long> DropNamespaceAsync(NamespaceId ns)
		{
			return _referencesStore.DropNamespaceAsync(ns);
		}

		public Task<long> DeleteBucketAsync(NamespaceId ns, BucketId bucket)
		{
			return _referencesStore.DeleteBucketAsync(ns, bucket);
		}

		public async Task<bool> ExistsAsync(NamespaceId ns, BucketId bucket, RefId key)
		{
			try
			{
				(RefRecord, BlobContents?) _ = await GetAsync(ns, bucket, key, new string[] {"name"}, doLastAccessTracking: false, skipCache: true);
			}
			catch (NamespaceNotFoundException)
			{
				return false;
			}
			catch (BlobNotFoundException)
			{
				return false;
			}
			catch (RefNotFoundException)
			{
				return false;
			}
			catch (PartialReferenceResolveException)
			{
				return false;
			}
			catch (ReferenceIsMissingBlobsException)
			{
				return false;
			}

			return true;
		}

		public async Task<List<BlobId>> GetReferencedBlobsAsync(NamespaceId ns, BucketId bucket, RefId name, bool ignoreMissingBlobs = false)
		{
			byte[] blob;
			RefRecord o = await _referencesStore.GetAsync(ns, bucket, name, IReferencesStore.FieldFlags.IncludePayload, IReferencesStore.OperationFlags.None);
			if (o.InlinePayload != null && o.InlinePayload.Length != 0)
			{
				blob = o.InlinePayload;
			}
			else
			{
				BlobContents blobContents = await _blobService.GetObjectAsync(ns, o.BlobIdentifier);
				blob = await blobContents.Stream.ToByteArrayAsync();
			}

			CbObject cbObject = new CbObject(blob);

			List<BlobId> referencedBlobs = await _referenceResolver.GetReferencedBlobs(ns, cbObject, ignoreMissingBlobs).ToListAsync();
			return referencedBlobs;
		}
	}
}
