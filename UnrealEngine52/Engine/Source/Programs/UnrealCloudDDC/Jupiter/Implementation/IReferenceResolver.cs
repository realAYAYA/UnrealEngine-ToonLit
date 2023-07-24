// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Linq;
using System.Threading.Tasks;
using EpicGames.Core;
using EpicGames.Horde.Storage;
using EpicGames.Serialization;
using Jupiter.Utils;
using OpenTelemetry.Trace;

namespace Jupiter.Implementation
{
    public abstract class Attachment
    {
        public abstract IoHash AsIoHash();
    }

    public class BlobAttachment : Attachment
    {
        public BlobIdentifier Identifier { get; }

        public BlobAttachment(BlobIdentifier blobIdentifier)
        {
            Identifier = blobIdentifier;
        }

        public override IoHash AsIoHash()
        {
            return Identifier.AsIoHash();
        }
    }

    public class ObjectAttachment : Attachment
    {
        public BlobIdentifier Identifier { get; }

        public ObjectAttachment(BlobIdentifier blobIdentifier)
        {
            Identifier = blobIdentifier;
        }

        public override IoHash AsIoHash()
        {
            return Identifier.AsIoHash();
        }
    }

    public class ContentIdAttachment : Attachment
    {
        public ContentId Identifier { get; }
        public BlobIdentifier[] ReferencedBlobs { get; }

        public ContentIdAttachment(ContentId contentId, BlobIdentifier[] referencedBlobs)
        {
            Identifier = contentId;
            ReferencedBlobs = referencedBlobs;
        }

        public override IoHash AsIoHash()
        {
            return Identifier.AsIoHash();
        }
    }

    public interface IReferenceResolver
    {
        /// <summary>
        /// Returns the blobs referenced from the cb object and any children
        /// These blobs are guaranteed to exist, if any blob is missing
        /// PartialReferenceResolveException or ReferenceIsMissingBlobsException are raised
        /// </summary>
        /// <param name="ns">The namespace to check</param>
        /// <param name="cb">The compact binary object to resolve references for</param>
        /// <returns></returns>
        IAsyncEnumerable<BlobIdentifier> GetReferencedBlobs(NamespaceId ns, CbObject cb);

        /// <summary>
        /// Returns which attachments exist in the cb object or any children
        /// These attachments are not guaranteed to reference blobs that actually exist
        /// </summary>
        /// <param name="ns">The namespace to check</param>
        /// <param name="cb">The compact binary object to resolve references for</param>
        /// <returns></returns>
        IAsyncEnumerable<Attachment> GetAttachments(NamespaceId ns, CbObject cb);
    }

    public class ReferenceResolver : IReferenceResolver
    {
        private readonly IBlobService _blobStore;
        private readonly IContentIdStore _contentIdStore;
        private readonly Tracer _tracer;

        public ReferenceResolver(IBlobService blobStore, IContentIdStore contentIdStore, Tracer tracer)
        {
            _blobStore = blobStore;
            _contentIdStore = contentIdStore;
            _tracer = tracer;
        }

        public async IAsyncEnumerable<Attachment> GetAttachments(NamespaceId ns, CbObject cb)
        {
            Queue<CbObject> objectsToVisit = new Queue<CbObject>();
            objectsToVisit.Enqueue(cb);
            List<BlobIdentifier> unresolvedBlobReferences = new List<BlobIdentifier>();

            List<Task<CbObject>> pendingCompactBinaryAttachments = new();
            List<Task< (ContentId, BlobIdentifier[]?)>> pendingContentIdResolves = new();

            while (pendingCompactBinaryAttachments.Count != 0 || pendingContentIdResolves.Count != 0 || objectsToVisit.Count != 0)
            {
                List<Attachment> attachments = new List<Attachment>();

                if (objectsToVisit.TryDequeue(out CbObject? parent))
                {
                    parent.IterateAttachments(field =>
                    {
                        IoHash attachmentHash = field.AsAttachment();

                        BlobIdentifier blobIdentifier = BlobIdentifier.FromIoHash(attachmentHash);
                        ContentId contentId = ContentId.FromIoHash(attachmentHash);

                        if (field.IsBinaryAttachment())
                        {
                            Task<(ContentId, BlobIdentifier[]?)> resolveContentId = ResolveContentId(ns, contentId);
                            pendingContentIdResolves.Add(resolveContentId);
                        }
                        else if (field.IsObjectAttachment())
                        {
                            attachments.Add(new ObjectAttachment(blobIdentifier));
                            pendingCompactBinaryAttachments.Add(ParseCompactBinaryAttachment(ns, blobIdentifier));
                        }
                        else
                        {
                            throw new NotImplementedException($"Unknown attachment type for field {field}");
                        }
                    });
                }

                // check for any content id resolves to finish
                List<Task<(ContentId, BlobIdentifier[]?)>> finishedContentIdResolves = new();
                foreach (Task<(ContentId, BlobIdentifier[]?)> pendingContentIdResolve in pendingContentIdResolves)
                {
                    if (pendingContentIdResolve.IsCompleted)
                    {
                        ContentId? contentId = null;
                        BlobIdentifier[]? resolvedBlobs = null;
                        BlobIdentifier? blobIdentifier = null;
                        bool wasContentId = false;
                        try
                        {
                            (contentId, resolvedBlobs) = await pendingContentIdResolve;
                            blobIdentifier = contentId.AsBlobIdentifier();
                            wasContentId = !(resolvedBlobs is { Length: 1 } && resolvedBlobs[0].Equals(blobIdentifier));
                        }
                        catch (InvalidContentIdException)
                        {
                            resolvedBlobs = null;
                        }
                        catch (BlobNotFoundException e)
                        {
                            unresolvedBlobReferences.Add(e.Blob);
                        }

                        if (wasContentId && resolvedBlobs != null)
                        {
                            attachments.Add(new ContentIdAttachment(contentId!, resolvedBlobs));
                        }
                        else
                        {
                            attachments.Add(new BlobAttachment(blobIdentifier!));
                        }

                        finishedContentIdResolves.Add(pendingContentIdResolve);
                    }
                }

                // cleanup finished tasks
                foreach (Task<(ContentId, BlobIdentifier[]?)> finishedTask in finishedContentIdResolves)
                {
                    pendingContentIdResolves.Remove(finishedTask);
                }

                // check for any compact binary attachment fetches and add those to the objects we are handling
                List<Task<CbObject>> finishedCompactBinaryResolves = new();
                foreach (Task<CbObject> pendingCompactBinaryAttachment in pendingCompactBinaryAttachments)
                {
                    if (pendingCompactBinaryAttachment.IsCompleted)
                    {
                        try
                        {
                            CbObject childBinaryObject = await pendingCompactBinaryAttachment;
                            objectsToVisit.Enqueue(childBinaryObject);
                        }
                        catch (BlobNotFoundException e)
                        {
                            unresolvedBlobReferences.Add(e.Blob);
                        }
                        finishedCompactBinaryResolves.Add(pendingCompactBinaryAttachment);
                    }
                }

                // cleanup finished tasks
                foreach (Task<CbObject> finishedTask in finishedCompactBinaryResolves)
                {
                    pendingCompactBinaryAttachments.Remove(finishedTask);
                }

                bool hasWaited = false;
                // if there are pending resolves left, wait for one of them to finish to avoid busy waiting
                if (pendingCompactBinaryAttachments.Any())
                {
                    hasWaited = true;
                    await Task.WhenAny(pendingCompactBinaryAttachments);
                }

                // if we did not waiting for compact binary attachment resolves we consider waiting for content id resolves if there are any
                if (!hasWaited && pendingContentIdResolves.Any())
                {
                    await Task.WhenAny(pendingContentIdResolves);
                }

                foreach (Attachment attachment in attachments)
                {
                    yield return attachment;
                }
            }

            if (unresolvedBlobReferences.Count != 0)
            {
                throw new ReferenceIsMissingBlobsException(unresolvedBlobReferences);
            }
        }

        public async IAsyncEnumerable<BlobIdentifier> GetReferencedBlobs(NamespaceId ns, CbObject cb)
        {
            List<Task<(BlobIdentifier, bool)>> pendingBlobExistsChecks = new();
            List<Task<(ContentIdAttachment, bool)>> pendingContentIdChecks = new();
            List<ContentId> unresolvedContentIdReferences = new List<ContentId>();
            List<BlobIdentifier> unresolvedBlobReferences = new List<BlobIdentifier>();

            // Resolve all the attachments
            await foreach (Attachment attachment in GetAttachments(ns, cb))
            {
                if (attachment is BlobAttachment blobAttachment)
                {
                    pendingBlobExistsChecks.Add(CheckBlobExists(ns, blobAttachment.Identifier));
                }
                else if (attachment is ContentIdAttachment contentIdAttachment)
                {
                    // If we find a content id we resolve that into the actual blobs it references
                    pendingContentIdChecks.Add(CheckContentIdExists(ns, contentIdAttachment));
                }
                else if (attachment is ObjectAttachment objectAttachment)
                {
                    // a object just references the same blob, traversing the object attachment is done in GetAttachments
                    pendingBlobExistsChecks.Add(CheckBlobExists(ns, objectAttachment.Identifier));
                }
                else
                {
                    throw new NotSupportedException($"Unknown attachment type {attachment.GetType()}");
                }
            }

            // return any verified blobs
            foreach (Task<(BlobIdentifier, bool)> pendingBlobExistsTask in pendingBlobExistsChecks)
            {
                (BlobIdentifier blob, bool exists) = await pendingBlobExistsTask;
                if (exists)
                {
                    yield return blob;
                }
                else
                {
                    unresolvedBlobReferences.Add(blob);
                }
            }

            foreach (Task<(ContentIdAttachment, bool)> pendingContentIdTask in pendingContentIdChecks)
            {
                (ContentIdAttachment contentIdAttachment, bool exists) = await pendingContentIdTask;

                if (!exists)
                {
                    unresolvedContentIdReferences.Add(contentIdAttachment.Identifier);
                    continue;
                }

                foreach (BlobIdentifier b in contentIdAttachment.ReferencedBlobs)
                {
                    yield return b;
                }
            }

            // if there were any content ids we did not recognize we throw a partial reference exception
            if (unresolvedContentIdReferences.Count != 0)
            {
                throw new PartialReferenceResolveException(unresolvedContentIdReferences);
            }
            // if there were any blobs missing we throw a partial reference exception
            if (unresolvedBlobReferences.Count != 0)
            {
                throw new ReferenceIsMissingBlobsException(unresolvedBlobReferences);
            }
        }

        private async Task<(ContentIdAttachment, bool)> CheckContentIdExists(NamespaceId ns, ContentIdAttachment contentIdAttachment)
        {
            bool allBlobsExist = true;
            foreach (BlobIdentifier b in contentIdAttachment.ReferencedBlobs)
            {
                (BlobIdentifier _, bool exists) = await CheckBlobExists(ns, b);

                if (!exists)
                {
                    allBlobsExist = false;
                    break;
                }
            }

            return (contentIdAttachment, allBlobsExist);
        }

        private async Task<CbObject> ParseCompactBinaryAttachment(NamespaceId ns, BlobIdentifier blobIdentifier)
        {
            BlobContents contents = await _blobStore.GetObject(ns, blobIdentifier);
            byte[] data = await contents.Stream.ToByteArray();
            CbObject childBinaryObject = new CbObject(data);

            return childBinaryObject;
        }

        private async Task<(ContentId, BlobIdentifier[]?)> ResolveContentId(NamespaceId ns, ContentId contentId)
        {
            using TelemetrySpan scope = _tracer.StartActiveSpan("ReferenceResolver.ResolveContentId")
                .SetAttribute("operation.name", "ReferenceResolver.ResolveContentId")
                .SetAttribute("resource.name", contentId.ToString());
            BlobIdentifier[]? resolvedBlobs = await _contentIdStore.Resolve(ns, contentId);
            return (contentId, resolvedBlobs);
        }

        private async Task<(BlobIdentifier, bool)> CheckBlobExists(NamespaceId ns, BlobIdentifier blob)
        {
            return (blob, await _blobStore.Exists(ns, blob));
        }
    }

    public class PartialReferenceResolveException : Exception
    {
        public List<ContentId> UnresolvedReferences { get; }

        public PartialReferenceResolveException(List<ContentId> unresolvedReferences) : base($"References missing: {string.Join(',', unresolvedReferences)}")
        {
            UnresolvedReferences = unresolvedReferences;
        }
    }

    public class ReferenceIsMissingBlobsException : Exception
    {
        public List<BlobIdentifier> MissingBlobs { get; }

        public ReferenceIsMissingBlobsException(List<BlobIdentifier> missingBlobs) : base($"References is missing these blobs: {string.Join(',', missingBlobs)}")
        {
            MissingBlobs = missingBlobs;
        }
    }
}
