// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Concurrent;
using System.Collections.Generic;
using System.ComponentModel.DataAnnotations;
using System.Diagnostics;
using System.Globalization;
using System.IO;
using System.Linq;
using System.Net;
using System.Net.Mime;
using System.Text;
using System.Threading;
using System.Threading.Tasks;
using EpicGames.AspNet;
using EpicGames.Core;
using EpicGames.Horde.Storage;
using EpicGames.Serialization;
using Jupiter.Implementation;
using JetBrains.Annotations;
using Jupiter.Common.Implementation;
using Jupiter.Utils;
using Microsoft.AspNetCore.Authorization;
using Microsoft.AspNetCore.Http;
using Microsoft.AspNetCore.Http.Extensions;
using Microsoft.AspNetCore.Mvc;
using Microsoft.Extensions.DependencyInjection;
using Microsoft.Extensions.Logging;
using Newtonsoft.Json;
using OpenTelemetry.Trace;

using ContentHash = Jupiter.Implementation.ContentHash;
using ContentId = Jupiter.Implementation.ContentId;

namespace Jupiter.Controllers
{
	using IDiagnosticContext = Serilog.IDiagnosticContext;
    using BlobNotFoundException = Jupiter.Implementation.BlobNotFoundException;

    [ApiController]
    [FormatFilter]
    [Produces(MediaTypeNames.Application.Json, MediaTypeNames.Application.Octet, CustomMediaTypeNames.UnrealCompactBinary)]
    [Route("api/v1/refs")]
    [Authorize]
    public class ReferencesController : ControllerBase
    {
        private readonly IDiagnosticContext _diagnosticContext;
        private readonly FormatResolver _formatResolver;
        private readonly BufferedPayloadFactory _bufferedPayloadFactory;
        private readonly IReferenceResolver _referenceResolver;
        private readonly RequestHelper _requestHelper;
        private readonly Tracer _tracer;

        private readonly ILogger _logger;
        private readonly IObjectService _objectService;
        private readonly IBlobService _blobStore;

        public ReferencesController(IObjectService objectService, IBlobService blobStore, IDiagnosticContext diagnosticContext, FormatResolver formatResolver, BufferedPayloadFactory bufferedPayloadFactory, IReferenceResolver referenceResolver, RequestHelper requestHelper, Tracer tracer, ILogger<ReferencesController> logger)
        {
            _objectService = objectService;
            _blobStore = blobStore;
            _diagnosticContext = diagnosticContext;
            _formatResolver = formatResolver;
            _bufferedPayloadFactory = bufferedPayloadFactory;
            _referenceResolver = referenceResolver;
            _requestHelper = requestHelper;
            _tracer = tracer;
            _logger = logger;
        }

        /// <summary>
        /// Returns all the known namespace the token has access to
        /// </summary>
        /// <returns></returns>
        [HttpGet("")]
        [ProducesDefaultResponseType]
        [ProducesResponseType(type: typeof(ProblemDetails), 400)]
        public async Task<IActionResult> GetNamespaces()
        {
            NamespaceId[] namespaces = await _objectService.GetNamespaces().ToArrayAsync();

            // filter namespaces down to only the namespaces the user has access to
            List<NamespaceId> namespacesWithAccess = new();
            foreach (NamespaceId ns in namespaces)
            {
                ActionResult? accessResult = await _requestHelper.HasAccessToNamespace(User, Request, ns, new [] { AclAction.ReadObject });
                if (accessResult == null)
                {
                    namespacesWithAccess.Add(ns);
                }
            }

            return Ok(new GetNamespacesResponse(namespacesWithAccess.ToArray()));
        }

        /// <summary>
        /// Returns a refs key
        /// </summary>
        /// <param name="ns">Namespace. Each namespace is completely separated from each other. Use for different types of data that is never expected to be similar (between two different games for instance). Example: `uc4.ddc`</param>
        /// <param name="bucket">The category/type of record you are caching. Is a clustered key together with the actual key, but all records in the same bucket can be dropped easily. Example: `terrainTexture` </param>
        /// <param name="key">The unique name of this particular key. `iAmAVeryValidKey`</param>
        /// <param name="format">Optional specifier to set which output format is used json/raw/cb</param>
        [HttpGet("{ns}/{bucket}/{key}.{format?}", Order = 500)]
        [Produces(MediaTypeNames.Application.Json, MediaTypeNames.Application.Octet, CustomMediaTypeNames.UnrealCompactBinary, CustomMediaTypeNames.JupiterInlinedPayload, CustomMediaTypeNames.UnrealCompactBinaryPackage)]
        public async Task<IActionResult> Get(
            [FromRoute] [Required] NamespaceId ns,
            [FromRoute] [Required] BucketId bucket,
            [FromRoute] [Required] IoHashKey key,
            [FromRoute] string? format = null)
        {
            ActionResult? accessResult = await _requestHelper.HasAccessToNamespace(User, Request, ns, new [] { AclAction.ReadObject });
            if (accessResult != null)
            {
                return accessResult;
            }

            try
            {
                (ObjectRecord objectRecord, BlobContents? blob) = await _objectService.Get(ns, bucket, key, Array.Empty<string>());

                if (blob == null)
                {
                    throw new InvalidOperationException($"Blob was null when attempting to fetch {ns} {bucket} {key}");
                }

                if (!objectRecord.IsFinalized)
                {
                    // we do not consider un-finalized objects as valid
                    return BadRequest(new ProblemDetails { Title = $"Object {objectRecord.Bucket} {objectRecord.Name} is not finalized." });
                }

                Response.Headers[CommonHeaders.HashHeaderName] = objectRecord.BlobIdentifier.ToString();
                Response.Headers[CommonHeaders.LastAccessHeaderName] = objectRecord.LastAccess.ToString(CultureInfo.InvariantCulture);

                async Task WriteBody(BlobContents blobContents, string contentType)
                {
                    IServerTiming? serverTiming = Request.HttpContext.RequestServices.GetService<IServerTiming>();
                    using ServerTimingMetricScoped? serverTimingScope = serverTiming?.CreateServerTimingMetricScope("body.write", "Time spent writing body");

                    long contentLength = blobContents.Length;
                    using TelemetrySpan scope = _tracer.StartActiveSpan("body.write").SetAttribute("operation.name", "body.write");
                    scope.SetAttribute("content-length", contentLength);
                    const int BufferSize = 64 * 1024;
                    Stream outputStream = Response.Body;
                    Response.ContentLength = contentLength;
                    Response.ContentType = contentType;
                    Response.StatusCode = StatusCodes.Status200OK;
                    try
                    {
                        await StreamCopyOperation.CopyToAsync(blobContents.Stream, outputStream, count: null, bufferSize: BufferSize, cancel: Response.HttpContext.RequestAborted);
                    }
                    catch (OperationCanceledException)
                    {
                        // do not raise exceptions for cancelled writes
                        // as we have already started writing a response we can not change the status code
                        // so we just drop a warning and proceed
                        _logger.LogWarning("The operation was canceled while writing the body");
                    }
                }

                string responseType = _formatResolver.GetResponseType(Request, format, CustomMediaTypeNames.UnrealCompactBinary);

                switch (responseType)
                {
                    case CustomMediaTypeNames.UnrealCompactBinary:
                    {
                        // for compact binary we can just serialize our internal object
                        await WriteBody(blob, CustomMediaTypeNames.UnrealCompactBinary);

                        break;
                    }
                    case MediaTypeNames.Application.Octet:
                    {
                        byte[] blobMemory = await blob.Stream.ToByteArray();
                        CbObject cb = new CbObject(blobMemory);

                        (int,CbField?) CountFields(CbObject o)
                        {
                            int count = 0;
                            CbField? foundField = null;
                            cb.IterateAttachments(field =>
                            {
                                ++count;
                                if (field.IsBinaryAttachment())
                                {
                                    foundField = field;
                                }
                            });

                            return (count, foundField);
                        }

                        // breaking lambda call into private method to workaround incorrect triggering of CA1508 - https://github.com/dotnet/roslyn-analyzers/issues/5254
                        (int countOfAttachmentFields,CbField? binaryAttachmentField) = CountFields(cb);

                        if (countOfAttachmentFields == 1 && binaryAttachmentField != null)
                        {
                            // there is a single attachment field and that is of the binary attachment type, fetch that attachment and return it instead of the compact binary
                            // this is so that we match the uploaded that happened as a octet-stream which generates a small cb object with a single attachment

                            IoHash hash = binaryAttachmentField.AsBinaryAttachment();

                            BlobContents referencedBlobContents = await _blobStore.GetObject(ns, BlobIdentifier.FromIoHash(hash));
                            await WriteBody(referencedBlobContents, MediaTypeNames.Application.Octet);
                            break;
                        }

                        // this doesn't look like the generated compact binary so we just return the payload
                        await using BlobContents contents = new(blobMemory);
                        await WriteBody(contents, MediaTypeNames.Application.Octet);
                        break;
                    }
                    case MediaTypeNames.Application.Json:
                    {
                        byte[] blobMemory;
                        {
                            using TelemetrySpan scope = _tracer.StartActiveSpan("json.readblob").SetAttribute("operation.name", "authorize");
                            blobMemory = await blob.Stream.ToByteArray();
                        }
                        CbObject cb = new CbObject(blobMemory);
                        string s = cb.ToJson();
                        await using BlobContents contents = new BlobContents(Encoding.UTF8.GetBytes(s));
                        await WriteBody(contents, MediaTypeNames.Application.Json);
                        break;

                    }
                    case CustomMediaTypeNames.UnrealCompactBinaryPackage:
                    {
                        byte[] blobMemory = await blob.Stream.ToByteArray();
                        CbObject cb = new CbObject(blobMemory);

                        IAsyncEnumerable<Attachment> attachments = _referenceResolver.GetAttachments(ns, cb);

                        using CbPackageBuilder writer = new CbPackageBuilder();
                        await writer.AddAttachment(objectRecord.BlobIdentifier.AsIoHash(), CbPackageAttachmentFlags.IsObject, blobMemory);

                        await foreach (Attachment attachment in attachments)
                        {
                            IoHash attachmentHash = attachment.AsIoHash();
                            CbPackageAttachmentFlags flags = 0;

                            try
                            {
                                BlobContents attachmentContents;
                                if (attachment is BlobAttachment blobAttachment)
                                {
                                    BlobIdentifier referencedBlob = blobAttachment.Identifier;
                                    attachmentContents = await _blobStore.GetObject(ns, referencedBlob);
                                }
                                else if (attachment is ObjectAttachment objectAttachment)
                                {
                                    flags &= CbPackageAttachmentFlags.IsObject;
                                    BlobIdentifier referencedBlob = objectAttachment.Identifier;
                                    attachmentContents = await _blobStore.GetObject(ns, referencedBlob);
                                }
                                else if (attachment is ContentIdAttachment contentIdAttachment)
                                {

                                    ContentId contentId = contentIdAttachment.Identifier;
                                    (attachmentContents, string mime) = await _blobStore.GetCompressedObject(ns, contentId, HttpContext.RequestServices);
                                    if (mime == CustomMediaTypeNames.UnrealCompressedBuffer)
                                    {
                                        flags &= CbPackageAttachmentFlags.IsCompressed;
                                    }
                                    else
                                    {
                                        // this resolved to a uncompressed blob, the content id existed the the compressed blob didn't
                                        // so resetting flags to indicate this.
                                        flags = 0;
                                    }
                                }
                                else
                                {
                                    throw new NotSupportedException($"Unknown attachment type {attachment.GetType()}");
                                }

                                await writer.AddAttachment(attachmentHash, flags, attachmentContents.Stream, (ulong)attachmentContents.Length);
                            }
                            catch (Exception e)
                            {
                                (CbObject errorObject, HttpStatusCode _) = ToErrorResult(e);
                                await writer.AddAttachment(attachmentHash, CbPackageAttachmentFlags.IsError | CbPackageAttachmentFlags.IsObject, errorObject.GetView().ToArray());
                            }
                        }
                        await using BlobContents contents = new BlobContents(writer.ToByteArray());
                        await WriteBody(contents, CustomMediaTypeNames.UnrealCompactBinaryPackage);
                        break;
                    }
                    case CustomMediaTypeNames.JupiterInlinedPayload:
                    {
                        byte[] blobMemory = await blob.Stream.ToByteArray();
                        CbObject cb = new CbObject(blobMemory);

                        static (int, int) CountFields(CbObject o)
                        {
                            int countOfBinaryAttachmentFields = 0;
                            int countOfAttachmentFields = 0;

                            o.IterateAttachments(field =>
                            {
                                if (field.IsBinaryAttachment())
                                {
                                    ++countOfBinaryAttachmentFields;
                                }

                                if (field.IsAttachment())
                                {
                                    ++countOfAttachmentFields;
                                }
                            });

                            return (countOfBinaryAttachmentFields, countOfAttachmentFields);
                        }
                        // breaking lambda call into private method to workaround incorrect triggering of CA1508 - https://github.com/dotnet/roslyn-analyzers/issues/5254
                        (int countOfAttachmentFields, int countOfBinaryAttachmentFields) = CountFields(cb);

                        // if the object consists of a single attachment field we return this attachment field instead
                        if (countOfBinaryAttachmentFields == 1 && countOfAttachmentFields == 1)
                        {
                            // fetch the blob so we can resolve any content ids in it
                            List<BlobIdentifier> referencedBlobs;
                            try
                            {
                                IAsyncEnumerable<BlobIdentifier> referencedBlobsEnumerable = _referenceResolver.GetReferencedBlobs(ns, cb);
                                referencedBlobs = await referencedBlobsEnumerable.ToListAsync();
                            }
                            catch (PartialReferenceResolveException)
                            {
                                return NotFound(new ProblemDetails {Title = $"Object {bucket} {key} in namespace {ns} was missing some content ids"});
                            }
                            catch (ReferenceIsMissingBlobsException)
                            {
                                return NotFound(new ProblemDetails {Title = $"Object {bucket} {key} in namespace {ns} was missing some blobs"});
                            }

                            if (referencedBlobs.Count == 1)
                            {
                                BlobIdentifier attachmentToSend = referencedBlobs.First();
                                try
                                {
                                    BlobContents referencedBlobContents = await _blobStore.GetObject(ns, attachmentToSend);
                                    Response.Headers[CommonHeaders.InlinePayloadHash] = attachmentToSend.ToString();

                                    await WriteBody(referencedBlobContents, CustomMediaTypeNames.JupiterInlinedPayload);
                                }
                                catch (BlobNotFoundException)
                                {
                                    return NotFound(new ProblemDetails {Title = $"Object {bucket} {key} in namespace {ns} was missing blob {attachmentToSend}"});
                                }
                                catch (Exception ex)
                                {
                                    _logger.LogError(ex, "Unknown exception encountered while writing body for jupiter inlined payload.");
                                    throw;
                                }
                                return new EmptyResult();
                            }
                            else if (referencedBlobs.Count == 0)
                            {
                                return NotFound(new ProblemDetails
                                {
                                    Title =
                                        $"Object {objectRecord.Bucket} {objectRecord.Name} did not resolve into any objects that we could find."
                                });
                            }

                            return BadRequest(new ProblemDetails
                            {
                                Title =
                                    $"Object {objectRecord.Bucket} {objectRecord.Name} contained a content id which resolved to more then 1 blob, unable to inline this object. Use compact object response instead."
                            });
                        }
                        else if (countOfBinaryAttachmentFields == 0 && countOfAttachmentFields == 0)
                        {
                            // no attachments so we just return the compact object instead
                            await using BlobContents contents = new BlobContents(blobMemory);
                            await WriteBody(contents, CustomMediaTypeNames.JupiterInlinedPayload);
                            return new EmptyResult();
                        }

                        return BadRequest(new ProblemDetails
                        {
                            Title =
                                $"Object {objectRecord.Bucket} {objectRecord.Name} had more then 1 binary attachment field, unable to inline this object. Use compact object response instead."
                        });
                    }
                    default:
                        throw new NotImplementedException($"Unknown expected response type {responseType}");
                }
                
                // this result is ignored as we write to the body explicitly
                return new EmptyResult();

            }
            catch (NamespaceNotFoundException e)
            {
                return NotFound(new ProblemDetails {Title = $"Namespace {e.Namespace} did not exist"});
            }
            catch (ObjectNotFoundException e)
            {
                return NotFound(new ProblemDetails { Title = $"Object {e.Bucket} {e.Key} did not exist" });
            }
            catch (BlobNotFoundException e)
            {
                return NotFound(new ProblemDetails { Title = $"Object {e.Blob} in {e.Ns} not found" });
            }
        }

       

    /// <summary>
    /// Returns the metadata about a ref key
    /// </summary>
    /// <param name="ns">Namespace. Each namespace is completely separated from each other. Use for different types of data that is never expected to be similar (between two different games for instance). Example: `uc4.ddc`</param>
    /// <param name="bucket">The category/type of record you are caching. Is a clustered key together with the actual key, but all records in the same bucket can be dropped easily. Example: `terrainTexture` </param>
    /// <param name="key">The unique name of this particular key. `iAmAVeryValidKey`</param>
    /// <param name="fields">The fields to include in the response, omit this to include everything.</param>
    [HttpGet("{ns}/{bucket}/{key}/metadata", Order = 500)]
    public async Task<IActionResult> GetMetadata(
        [FromRoute] [Required] NamespaceId ns,
        [FromRoute] [Required] BucketId bucket,
        [FromRoute] [Required] IoHashKey key,
        [FromQuery] string[] fields)
    {
        ActionResult? accessResult = await _requestHelper.HasAccessToNamespace(User, Request, ns, new [] { AclAction.ReadObject });
        if (accessResult != null)
        {
            return accessResult;
        }

        try
        {
            (ObjectRecord objectRecord, BlobContents? _) = await _objectService.Get(ns, bucket, key, fields);

            return Ok(new RefMetadataResponse(objectRecord));
        }
        catch (NamespaceNotFoundException e)
        {
            return NotFound(new ProblemDetails {Title = $"Namespace {e.Namespace} did not exist"});
        }
        catch (ObjectNotFoundException e)
        {
            return NotFound(new ProblemDetails { Title = $"Object {e.Bucket} {e.Key} did not exist" });
        }
        catch (BlobNotFoundException e)
        {
            return NotFound(new ProblemDetails { Title = $"Object {e.Blob} in {e.Ns} not found" });
        }
    }

        /// <summary>
        /// Checks if a object exists
        /// </summary>
        /// <param name="ns">Namespace. Each namespace is completely separated from each other. Use for different types of data that is never expected to be similar (between two different games for instance). Example: `uc4.ddc`</param>
        /// <param name="bucket">The category/type of record you are caching. Is a clustered key together with the actual key, but all records in the same bucket can be dropped easily. Example: `terrainTexture` </param>
        /// <param name="key">The unique name of this particular key. `iAmAVeryValidKey`</param>
        /// <returns>200 if it existed, 400 otherwise</returns>
        [HttpHead("{ns}/{bucket}/{key}", Order = 500)]
        [ProducesResponseType(type: typeof(OkResult), 200)]
        [ProducesResponseType(type: typeof(ValidationProblemDetails), 400)]
        public async Task<IActionResult> Head(
            [FromRoute] [Required] NamespaceId ns,
            [FromRoute] [Required] BucketId bucket,
            [FromRoute] [Required] IoHashKey key)
        {
            ActionResult? accessResult = await _requestHelper.HasAccessToNamespace(User, Request, ns, new [] { AclAction.ReadObject });
            if (accessResult != null)
            {
                return accessResult;
            }

            try
            {
                (ObjectRecord record, BlobContents? blob) = await _objectService.Get(ns, bucket, key, new string[] {"blobIdentifier", "IsFinalized"});
                Response.Headers[CommonHeaders.HashHeaderName] = record.BlobIdentifier.ToString();

                if (!record.IsFinalized)
                {
                    return NotFound(new ProblemDetails {Title = $"Object {bucket} {key} in namespace {ns} is not finalized."});
                }

                blob ??= await _blobStore.GetObject(ns, record.BlobIdentifier);

                // we have to verify the blobs are available locally, as the record of the key is replicated a head of the content
                // TODO: Once we support inline replication this step is not needed as at least one region as this blob, just maybe not this current one
                byte[] blobContents = await blob.Stream.ToByteArray();
                CbObject compactBinaryObject = new CbObject(blobContents);
                // the reference resolver will throw if any blob is missing, so no need to do anything other then process each reference
                IAsyncEnumerable<BlobIdentifier> references = _referenceResolver.GetReferencedBlobs(ns, compactBinaryObject);
                List<BlobIdentifier>? _ = await references.ToListAsync();

                // we have to verify the blobs are available locally, as the record of the key is replicated a head of the content
                // TODO: Once we support inline replication this step is not needed as at least one region as this blob, just maybe not this current one
                BlobIdentifier[] unknownBlobs = await _blobStore.FilterOutKnownBlobs(ns, new BlobIdentifier[] { record.BlobIdentifier });
                if (unknownBlobs.Length != 0)
                {
                    return NotFound(new ProblemDetails {Title = $"Object {bucket} {key} in namespace {ns} had at least one missing blob."});
                }
            }
            catch (NamespaceNotFoundException e)
            {
                return NotFound(new ProblemDetails {Title = $"Namespace {e.Namespace} did not exist"});
            }
            catch (BlobNotFoundException e)
            {
                return NotFound(new ProblemDetails { Title = $"Blob {e.Blob} in namespace {ns} did not exist" });
            }
            catch (ObjectNotFoundException e)
            {
                return NotFound(new ProblemDetails {Title = $"Object {e.Bucket} {e.Key} in namespace {e.Namespace} did not exist"});
            }
            catch (PartialReferenceResolveException)
            {
                return NotFound(new ProblemDetails {Title = $"Object {bucket} {key} in namespace {ns} was missing some content ids"});
            }
            catch (ReferenceIsMissingBlobsException)
            {
                return NotFound(new ProblemDetails {Title = $"Object {bucket} {key} in namespace {ns} was missing some blobs"});
            }

            return Ok();
        }

        [HttpGet("{ns}/exists")]
        [ProducesDefaultResponseType]
        public async Task<IActionResult> ExistsMultiple(
            [FromRoute] [Required] NamespaceId ns,
            [FromQuery] [Required] List<string> names)
        {
            ActionResult? accessResult = await _requestHelper.HasAccessToNamespace(User, Request, ns, new [] { AclAction.ReadObject });
            if (accessResult != null)
            {
                return accessResult;
            }

            ConcurrentBag<(BucketId, IoHashKey)> missingObject = new ();

            List<(BucketId, IoHashKey)> requestedNames = new List<(BucketId, IoHashKey)>();
            foreach (string name in names)
            {
                int separatorIndex = name.IndexOf(".", StringComparison.Ordinal);
                if (separatorIndex == -1)
                {
                    return BadRequest(new ProblemDetails() { Title = $"Key {name} did not contain a '.' separator" });
                }

                BucketId bucket = new BucketId(name.Substring(0, separatorIndex));
                IoHashKey key = new IoHashKey(name.Substring(separatorIndex + 1));
                requestedNames.Add((bucket, key));
            }

            IEnumerable<Task> tasks = requestedNames.Select(async pair =>
            {
                (BucketId bucket, IoHashKey key) = pair;
                try
                {
                    (ObjectRecord record, BlobContents? blob) =
                        await _objectService.Get(ns, bucket, key, new string[] { "blobIdentifier" });

                    blob ??= await _blobStore.GetObject(ns, record.BlobIdentifier);

                    // we have to verify the blobs are available locally, as the record of the key is replicated a head of the content
                    // TODO: Once we support inline replication this step is not needed as at least one region as this blob, just maybe not this current one
                    byte[] blobContents = await blob.Stream.ToByteArray();
                    CbObject cb = new CbObject(blobContents);
                    // the reference resolver will throw if any blob is missing, so no need to do anything other then process each reference
                    IAsyncEnumerable<BlobIdentifier> references = _referenceResolver.GetReferencedBlobs(ns, cb);
                    List<BlobIdentifier>? _ = await references.ToListAsync();
                }
                catch (ObjectNotFoundException)
                {
                    missingObject.Add((bucket, key));
                }
                catch (PartialReferenceResolveException)
                {
                    missingObject.Add((bucket, key));
                }
                catch (ReferenceIsMissingBlobsException)
                {
                    missingObject.Add((bucket, key));
                }
                catch (BlobNotFoundException)
                {
                    missingObject.Add((bucket, key));
                }
            });
            await Task.WhenAll(tasks);

            return Ok(new ExistCheckMultipleRefsResponse(missingObject.ToList()));
        }

        [HttpPut("{ns}/{bucket}/{key}.{format?}", Order = 500)]
        [DisableRequestSizeLimit]
        public async Task<IActionResult> PutObject(
            [FromRoute] [Required] NamespaceId ns,
            [FromRoute] [Required] BucketId bucket,
            [FromRoute] [Required] IoHashKey key)
        {
            ActionResult? accessResult = await _requestHelper.HasAccessToNamespace(User, Request, ns, new [] { AclAction.WriteObject });
            if (accessResult != null)
            {
                return accessResult;
            }

            _diagnosticContext.Set("Content-Length", Request.ContentLength ?? -1);

            CbObject payloadObject;
            BlobIdentifier blobHeader;

            try
            {
                using IBufferedPayload payload = await _bufferedPayloadFactory.CreateFromRequest(Request);

                BlobIdentifier headerHash;
                if (Request.Headers.ContainsKey(CommonHeaders.HashHeaderName))
                {
                    headerHash = new BlobIdentifier(Request.Headers[CommonHeaders.HashHeaderName]);
                }
                else
                {
                    return BadRequest(new ProblemDetails
                    {
                        Title = $"Missing expected header {CommonHeaders.HashHeaderName}"
                    });
                }

                blobHeader = headerHash;

                switch (Request.ContentType)
                {
                    case MediaTypeNames.Application.Json:
                    {
                        // TODO: define a scheme for how a json object specifies references

                        blobHeader = await _blobStore.PutObject(ns, payload, headerHash);

                        // TODO: convert the json object into a compact binary instead
                        CbWriter writer = new CbWriter();
                        writer.BeginObject();
                        writer.WriteBinaryAttachmentValue(blobHeader.AsIoHash());
                        writer.EndObject();

                        byte[] blob = writer.ToByteArray();
                        payloadObject = new CbObject(blob);
                        blobHeader = BlobIdentifier.FromBlob(blob);
                        break;
                    }
                    case CustomMediaTypeNames.UnrealCompactBinary:
                    {
                        await using MemoryStream ms = new MemoryStream();
                        await using Stream payloadStream = payload.GetStream();
                        await payloadStream.CopyToAsync(ms);
                        payloadObject = new CbObject(ms.ToArray());
                        break;
                    }
                    case MediaTypeNames.Application.Octet:
                    {
                        blobHeader = await _blobStore.PutObject(ns, payload, headerHash);

                        CbWriter writer = new CbWriter();
                        writer.BeginObject();
                        writer.WriteBinaryAttachment("RawHash", blobHeader.AsIoHash());
                        writer.WriteInteger("RawSize", payload.Length);
                        writer.EndObject();

                        byte[] blob = writer.ToByteArray();
                        payloadObject = new CbObject(blob);
                        blobHeader = BlobIdentifier.FromBlob(blob);
                        break;
                    }
                    default:
                        throw new Exception($"Unknown request type {Request.ContentType}, if submitting a blob please use {MediaTypeNames.Application.Octet}");
                }
            }
            catch (HashMismatchException e)
            {
                return BadRequest(new ProblemDetails
                {
                    Title = $"Incorrect hash, got hash \"{e.SuppliedHash}\" but hash of content was determined to be \"{e.ContentHash}\""
                });
            }
            catch (ClientSendSlowException e)
            {
                return Problem(e.Message, null, (int)HttpStatusCode.RequestTimeout);
            }

            (ContentId[] missingReferences, BlobIdentifier[] missingBlobs) = await _objectService.Put(ns, bucket, key, blobHeader, payloadObject);

            List<ContentHash> missingHashes = new List<ContentHash>(missingReferences);
            missingHashes.AddRange(missingBlobs);
            return Ok(new PutObjectResponse(missingHashes.ToArray()));
        }

        [HttpPut("{ns}/{bucket}/{key}", Order = 300)]
        [DisableRequestSizeLimit]
        [RequiredContentType(CustomMediaTypeNames.UnrealCompactBinaryPackage)]
        public async Task<IActionResult> PutPackage(
            [FromRoute][Required] NamespaceId ns,
            [FromRoute][Required] BucketId bucket,
            [FromRoute][Required] IoHashKey key)
        {
            ActionResult? accessResult = await _requestHelper.HasAccessToNamespace(User, Request, ns, new [] { AclAction.WriteObject });
            if (accessResult != null)
            {
                return accessResult;
            }

            _diagnosticContext.Set("Content-Length", Request.ContentLength ?? -1);

            CbPackageReader packageReader = await CbPackageReader.Create(Request.Body);

            try
            {
                await foreach ((CbPackageAttachmentEntry entry, byte[] blob) in packageReader.IterateAttachments())
                {
                    if (entry.Flags.HasFlag(CbPackageAttachmentFlags.IsError))
                    {
                        return BadRequest(new ProblemDetails
                        {
                            Title = $"Package contained attachment with error {entry.AttachmentHash}\""
                        });
                    }
                    if (entry.Flags.HasFlag(CbPackageAttachmentFlags.IsCompressed))
                    {
                        using MemoryBufferedPayload payload = new MemoryBufferedPayload(blob);
                        await _blobStore.PutCompressedObject(ns, payload, ContentId.FromIoHash(entry.AttachmentHash), HttpContext.RequestServices);
                    }
                    else
                    {
                        await _blobStore.PutObject(ns, blob, BlobIdentifier.FromIoHash(entry.AttachmentHash));
                    }
                }
            }
            catch (HashMismatchException e)
            {
                return BadRequest(new ProblemDetails
                {
                    Title = $"Incorrect hash, got hash \"{e.SuppliedHash}\" but hash of content was determined to be \"{e.ContentHash}\""
                });
            }
            
            CbObject rootObject = packageReader.RootObject;
            BlobIdentifier rootObjectHash = BlobIdentifier.FromIoHash(packageReader.RootHash);

            (ContentId[] missingReferences, BlobIdentifier[] missingBlobs) = await _objectService.Put(ns, bucket, key, rootObjectHash, rootObject);

            List<ContentHash> missingHashes = new List<ContentHash>(missingReferences);
            missingHashes.AddRange(missingBlobs);
            return Ok(new PutObjectResponse(missingHashes.ToArray()));
        }

        [HttpPost("{ns}/{bucket}/{key}/finalize/{hash}.{format?}")]
        public async Task<IActionResult> FinalizeObject(
            [FromRoute] [Required] NamespaceId ns,
            [FromRoute] [Required] BucketId bucket,
            [FromRoute] [Required] IoHashKey key,
            [FromRoute] [Required] BlobIdentifier hash)
        {
            ActionResult? accessResult = await _requestHelper.HasAccessToNamespace(User, Request, ns, new [] { AclAction.WriteObject });
            if (accessResult != null)
            {
                return accessResult;
            }

            try
            {
                (ContentId[] missingReferences, BlobIdentifier[] missingBlobs) = await _objectService.Finalize(ns, bucket, key, hash);
                List<ContentHash> missingHashes = new List<ContentHash>(missingReferences);
                missingHashes.AddRange(missingBlobs);

                return Ok(new PutObjectResponse(missingHashes.ToArray()));
            }
            catch (ObjectHashMismatchException e)
            {
                return BadRequest(e.Message);
            }
        }

        [HttpPost("{ns}")]
        [Consumes(CustomMediaTypeNames.UnrealCompactBinary)]
        [Produces(CustomMediaTypeNames.UnrealCompactBinary)]
        public async Task<IActionResult> Batch(
            [FromRoute] [Required] NamespaceId ns,
            [FromBody] [Required] BatchOps ops)
        {
            AclAction ActionForOp(BatchOps.BatchOp.Operation op)
            {
                switch (op)
                {
                    case BatchOps.BatchOp.Operation.GET:
                        return AclAction.ReadObject;
                    case BatchOps.BatchOp.Operation.PUT:
                        return AclAction.WriteObject;
                    case BatchOps.BatchOp.Operation.HEAD:
                        return AclAction.ReadObject;
                    default:
                        throw new ArgumentOutOfRangeException(nameof(op), op, null);
                }
            }

            AclAction[] requiredActions = ops.Ops.Select(op => ActionForOp(op.Op)).ToArray();

            ActionResult? accessResult = await _requestHelper.HasAccessToNamespace(User, Request, ns, requiredActions);
            if (accessResult != null)
            {
                return accessResult;
            }

            HashSet<uint> usedOpIds = new HashSet<uint>();
            foreach (BatchOps.BatchOp batchOp in ops.Ops)
            {
                bool added = usedOpIds.Add(batchOp.OpId);
                if (!added)
                {
                    return BadRequest(new ProblemDetails { Title = $"Duplicate op ids used for id: {batchOp.OpId}" });
                }
            }
            ConcurrentDictionary<uint, (CbObject, HttpStatusCode)> results = new();

            async Task<(CbObject, HttpStatusCode)> BatchGetOp(BatchOps.BatchOp op)
            {
                try
                {
                    (ObjectRecord objectRecord, BlobContents? blob) = await _objectService.Get(ns, op.Bucket, op.Key, Array.Empty<string>());

                    if (!objectRecord.IsFinalized)
                    {
                        throw new Exception("Object is not finalized");
                    }

                    if (blob == null)
                    {
                        throw new Exception();
                    }

                    CbObject cb = new CbObject(await blob.Stream.ToByteArray());

                    if (op.ResolveAttachments ?? false)
                    {
                        IAsyncEnumerable<BlobIdentifier> references = _referenceResolver.GetReferencedBlobs(ns, cb);
                        List<BlobIdentifier>? _ = await references.ToListAsync();
                    }

                    return (cb, HttpStatusCode.OK);
                }
                catch (Exception ex) when( ex is ObjectNotFoundException or PartialReferenceResolveException or ReferenceIsMissingBlobsException)
                {
                    return ToErrorResult(ex, HttpStatusCode.NotFound);
                }
                catch (Exception e)
                {
                    return ToErrorResult(e);
                }
            }

            async Task<(CbObject, HttpStatusCode)> BatchHeadOp(BatchOps.BatchOp op)
            {
                try
                {
                    (ObjectRecord record, BlobContents? blob) = await _objectService.Get(ns, op.Bucket, op.Key, new string[] { "blobIdentifier" });

                    if (!record.IsFinalized)
                    {
                        return (CbObject.Build(writer => writer.WriteBool("exists", false)), HttpStatusCode.NotFound);
                    }

                    blob ??= await _blobStore.GetObject(ns, record.BlobIdentifier);

                    if (op.ResolveAttachments ?? false)
                    {
                        byte[] blobContents = await blob.Stream.ToByteArray();
                        CbObject cb = new CbObject(blobContents);
                        // the reference resolver will throw if any blob is missing, so no need to do anything other then process each reference
                        IAsyncEnumerable<BlobIdentifier> references = _referenceResolver.GetReferencedBlobs(ns, cb);
                        List<BlobIdentifier>? _ = await references.ToListAsync();
                    }

                    if (blob == null)
                    {
                        throw new Exception();
                    }

                    return (CbObject.Build(writer => writer.WriteBool("exists", true)), HttpStatusCode.OK);
                }
                catch (Exception ex) when( ex is ObjectNotFoundException or PartialReferenceResolveException or ReferenceIsMissingBlobsException)
                {
                    return (CbObject.Build(writer => writer.WriteBool("exists", false)), HttpStatusCode.NotFound);
                }
                catch (Exception e)
                {
                    return ToErrorResult(e);
                }
            }

            async Task<(CbObject, HttpStatusCode)> BatchPutOp(BatchOps.BatchOp op)
            {
                try
                {
                    if (op.Payload == null || op.Payload.Equals(CbObject.Empty))
                    {
                        throw new Exception($"Missing payload for operation: {op.OpId}");
                    }

                    if (op.PayloadHash == null)
                    {
                        throw new Exception($"Missing payload hash for operation: {op.OpId}");
                    }
                    BlobIdentifier headerHash = BlobIdentifier.FromContentHash(op.PayloadHash);
                    BlobIdentifier objectHash = BlobIdentifier.FromBlob(op.Payload.GetView().ToArray());

                    if (!headerHash.Equals(objectHash))
                    {
                        throw new HashMismatchException(headerHash, objectHash);
                    }

                    (ContentId[] missingReferences, BlobIdentifier[] missingBlobs) = await _objectService.Put(ns, op.Bucket, op.Key, objectHash, op.Payload);
                    List<ContentHash> missingHashes = new List<ContentHash>(missingReferences);

                    return (CbSerializer.Serialize(new PutObjectResponse(missingHashes.ToArray())), HttpStatusCode.OK);
                }
                catch (Exception e)
                {
                    return ToErrorResult(e);
                }
            }

            await Parallel.ForEachAsync(ops.Ops, CancellationToken.None, async (op, token) =>
            {
                switch (op.Op)
                {
                    case BatchOps.BatchOp.Operation.GET:
                        results.TryAdd(op.OpId, await BatchGetOp(op));
                        break;
                    case BatchOps.BatchOp.Operation.PUT:
                        results.TryAdd(op.OpId, await BatchPutOp(op));
                        break;
                    case BatchOps.BatchOp.Operation.HEAD:
                        results.TryAdd(op.OpId, await BatchHeadOp(op));
                        break;
                    case BatchOps.BatchOp.Operation.INVALID:
                    default:
                        throw new NotImplementedException($"Unknown op type {op.Op}");
                }

                await Task.CompletedTask;
            });

            return Ok(new BatchOpsResponse()
            {
                Results = results.Select(result =>
                {
                    return new BatchOpsResponse.OpResponses()
                    {
                        OpId = result.Key,
                        Response = result.Value.Item1,
                        StatusCode = (int)result.Value.Item2
                    };
                }).ToList()
            });
        }

        private static (CbObject, HttpStatusCode) ToErrorResult(Exception exception, HttpStatusCode statusCode = HttpStatusCode.InternalServerError)
        {
            Exception e = exception;
            CbWriter writer = new CbWriter();
            writer.BeginObject();
            writer.WriteString("title", e.Message);
            writer.WriteInteger("status", (int)statusCode);
            if (e.StackTrace != null)
            {
                writer.WriteString("stackTrace", e.StackTrace);
            }
            writer.EndObject();
            return (writer.ToObject(), statusCode);
        }

        /// <summary>
        /// Drop all refs records in the namespace
        /// </summary>
        /// <param name="ns">Namespace. Each namespace is completely separated from each other. Use for different types of data that is never expected to be similar (between two different games for instance)</param>
        [HttpDelete("{ns}", Order = 500)]
        [ProducesResponseType(204)]
        public async Task<IActionResult> DeleteNamespace(
            [FromRoute] [Required] NamespaceId ns
        )
        {
            ActionResult? accessResult = await _requestHelper.HasAccessToNamespace(User, Request, ns, new [] { AclAction.DeleteNamespace });
            if (accessResult != null)
            {
                return accessResult;
            }

            try
            {
                await _objectService.DropNamespace(ns);
            }
            catch (NamespaceNotFoundException e)
            {
                return NotFound(new ProblemDetails {Title = $"Namespace {e.Namespace} did not exist"});
            }

            return NoContent();
        }

        /// <summary>
        /// Drop all refs records in the bucket
        /// </summary>
        /// <param name="ns">Namespace. Each namespace is completely separated from each other. Use for different types of data that is never expected to be similar (between two different games for instance)</param>
        /// <param name="bucket">The category/type of record you are caching. Is a clustered key together with the actual key, but all records in the same bucket can be dropped easily.</param>
        [HttpDelete("{ns}/{bucket}", Order = 500)]
        [ProducesResponseType(200)]
        public async Task<IActionResult> DeleteBucket(
            [FromRoute] [Required] NamespaceId ns,
            [FromRoute] [Required] BucketId bucket)
        {
            ActionResult? accessResult = await _requestHelper.HasAccessToNamespace(User, Request, ns, new [] { AclAction.DeleteBucket });
            if (accessResult != null)
            {
                return accessResult;
            }

            long countOfDeletedRecords;
            try
            {
                countOfDeletedRecords = await _objectService.DeleteBucket(ns, bucket);
            }
            catch (NamespaceNotFoundException e)
            {
                return NotFound(new ProblemDetails {Title = $"Namespace {e.Namespace} did not exist"});
            }

            return Ok(new BucketDeletedResponse(countOfDeletedRecords));
        }

        /// <summary>
        /// Delete a individual refs key
        /// </summary>
        /// <param name="ns">Namespace. Each namespace is completely separated from each other. Use for different types of data that is never expected to be similar (between two different games for instance)</param>
        /// <param name="bucket">The category/type of record you are caching. Is a clustered key together with the actual key, but all records in the same bucket can be dropped easily.</param>
        /// <param name="key">The unique name of this particular key</param>
        [HttpDelete("{ns}/{bucket}/{key}", Order = 500)]
        [ProducesResponseType(200)]
        [ProducesResponseType(404)]
        public async Task<IActionResult> Delete(
            [FromRoute] [Required] NamespaceId ns,
            [FromRoute] [Required] BucketId bucket,
            [FromRoute] [Required] IoHashKey key)
        {
            ActionResult? accessResult = await _requestHelper.HasAccessToNamespace(User, Request, ns, new [] { AclAction.DeleteObject });
            if (accessResult != null)
            {
                return accessResult;
            }

            try
            {
                bool deleted = await _objectService.Delete(ns, bucket, key);
                if (!deleted)
                {
                    return NotFound(new ProblemDetails { Title = $"Object {key} in bucket {bucket} and namespace {ns} did not exist" });
                }
                return Ok(new RefDeletedResponse(deleted ? 1 : 0));
            }
            catch (NamespaceNotFoundException e)
            {
                return NotFound(new ProblemDetails { Title = $"Namespace {e.Namespace} did not exist" });
            }
            catch (ObjectNotFoundException)
            {
                return NotFound(new ProblemDetails { Title = $"Object {key} in bucket {bucket} and namespace {ns} did not exist" });
            }
        }
    }

    public class RefDeletedResponse
    {
        public RefDeletedResponse()
        {

        }

        public RefDeletedResponse(int deletedCount)
        {
            DeletedCount = deletedCount;
        }

        [CbField("deletedCount")]
        public int DeletedCount { get; set; }
    }

    public class BucketDeletedResponse
    {
        public BucketDeletedResponse()
        {

        }

        public BucketDeletedResponse(long countOfDeletedRecords)
        {
            CountOfDeletedRecords = countOfDeletedRecords;
        }

        [CbField("countOfDeletedRecords")]
        public long CountOfDeletedRecords { get; set; }
    }

    public class BatchOps
    {
        public BatchOps()
        {
            Ops = Array.Empty<BatchOp>();
        }

        [System.Diagnostics.CodeAnalysis.SuppressMessage("Design", "CA1034:Nested types should not be visible", Justification = "For serialization only")]
        public class BatchOp
        {
            public BatchOp()
            {
                Payload = null;
                PayloadHash = null;
            }

            public enum Operation
            {
                INVALID,
                GET,
                PUT,
                HEAD,
            }

            [Required]
            [CbField("opId")]
            public uint OpId { get; set; }

            [CbField("op")]
            [JsonIgnore]
            [UsedImplicitly]
            public string OpString
            {
                get => Op.ToString();
                set => Op = Enum.Parse<Operation>(value);
            }

            [Required]
            public Operation Op { get; set; } = Operation.INVALID;

            [Required]
            [CbField("bucket")]
            public BucketId Bucket { get; set; }

            [Required]
            [CbField("key")]
            public IoHashKey Key { get; set; }

            [CbField("resolveAttachments")]
            public bool? ResolveAttachments { get; set; } = null;

            [CbField("payload")]
            public CbObject? Payload { get; set; } = null;

            [CbField("payloadHash")] 
            public ContentHash? PayloadHash { get; set; } = null;
        }

        [CbField("ops")]
        public BatchOp[] Ops { get; set; }
    }

    public class BatchOpsResponse
    {
        public BatchOpsResponse()
        {

        }

        [System.Diagnostics.CodeAnalysis.SuppressMessage("Design", "CA1034:Nested types should not be visible", Justification = "For serialization only")]
        public class OpResponses
        {
            public OpResponses()
            {

            }
            [CbField("opId")]
            public uint OpId { get; set; }

            [CbField("response")]
            public CbObject Response { get; set; } = null!;

            [CbField("statusCode")]
            public int StatusCode { get; set; }
        }

        [CbField("results")]
        [System.Diagnostics.CodeAnalysis.SuppressMessage("Usage", "CA2227:Collection properties should be read only", Justification = "Used by serialization")]
        public List<OpResponses> Results { get; set; } = new List<OpResponses>();
    }

    public class RefMetadataResponse
    {
        public RefMetadataResponse()
        {
            PayloadIdentifier = null!;
            InlinePayload = null!;
        }

        [JsonConstructor]
        public RefMetadataResponse(NamespaceId ns, BucketId bucket, IoHashKey name, BlobIdentifier payloadIdentifier, DateTime lastAccess, bool isFinalized, byte[]? inlinePayload)
        {
            Ns = ns;
            Bucket = bucket;
            Name = name;
            PayloadIdentifier = payloadIdentifier;
            LastAccess = lastAccess;
            IsFinalized = isFinalized;
            InlinePayload = inlinePayload;
        }

        public RefMetadataResponse(ObjectRecord objectRecord)
        {
            Ns = objectRecord.Namespace;
            Bucket = objectRecord.Bucket;
            Name = objectRecord.Name;
            PayloadIdentifier = objectRecord.BlobIdentifier;
            LastAccess = objectRecord.LastAccess;
            IsFinalized = objectRecord.IsFinalized;
            InlinePayload = objectRecord.InlinePayload;
        }

        [CbField("ns")]
        public NamespaceId Ns { get; set; }
        
        [CbField("bucket")]
        public BucketId Bucket { get; set; }

        [CbField("name")]
        public IoHashKey Name { get; set; }

        [CbField("payloadIdentifier")]
        public BlobIdentifier PayloadIdentifier { get; set; }

        [CbField("lastAccess")]
        public DateTime LastAccess { get; set; }

        [CbField("isFinalized")]
        public bool IsFinalized { get; set; }

        [CbField("inlinePayload")]
        public byte[]? InlinePayload { get; set; }
    }

    public class PutObjectResponse
    {
        public PutObjectResponse()
        {
            Needs = null!;
        }

        public PutObjectResponse(ContentHash[] missingReferences)
        {
            Needs = missingReferences;
        }

        [CbField("needs")]
        public ContentHash[] Needs { get; set; }
    }

    public class ExistCheckMultipleRefsResponse
    {
        public ExistCheckMultipleRefsResponse(List<(BucketId,IoHashKey)> missingNames)
        {
            Missing = missingNames.Select(pair =>
            {
                (BucketId bucketId, IoHashKey ioHashKey) = pair;
                return new MissingReference()
                {
                    Bucket = bucketId,
                    Key = ioHashKey,
                };
            }).ToList();
        }

        [JsonConstructor]
        public ExistCheckMultipleRefsResponse(List<MissingReference> missingNames)
        {
            Missing = missingNames;
        }

        [CbField("missing")]
        [System.Diagnostics.CodeAnalysis.SuppressMessage("Usage", "CA2227:Collection properties should be read only", Justification = "Used by serialization")]
        public List<MissingReference> Missing { get; set; }

        [System.Diagnostics.CodeAnalysis.SuppressMessage("Design", "CA1034:Nested types should not be visible", Justification = "For serialization only")]
        public class MissingReference
        {
            [CbField("bucket")]
            public BucketId Bucket { get; set; }

            [CbField("key")]
            public IoHashKey Key { get; set; }
        }
    }

    public class GetNamespacesResponse
    {
        [JsonConstructor]
        public GetNamespacesResponse(NamespaceId[] namespaces)
        {
            Namespaces = namespaces;
        }

        public NamespaceId[] Namespaces { get; set; }
    }

    public class HashMismatchException : Exception
    {
        public ContentHash SuppliedHash { get; }
        public ContentHash ContentHash { get; }

        public HashMismatchException(ContentHash suppliedHash, ContentHash contentHash) : base($"ID was not a hash of the content uploaded. Supplied hash was: {suppliedHash} but hash of content was {contentHash}")
        {
            SuppliedHash = suppliedHash;
            ContentHash = contentHash;
        }
    }
}
