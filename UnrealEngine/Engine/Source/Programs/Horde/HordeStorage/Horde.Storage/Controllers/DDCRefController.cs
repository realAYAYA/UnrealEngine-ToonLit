// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.ComponentModel.DataAnnotations;
using System.IO;
using System.Linq;
using System.Net.Mime;
using System.Threading.Tasks;
using Dasync.Collections;
using Datadog.Trace;
using EpicGames.Horde.Storage;
using Horde.Storage.Implementation;
using Jupiter;
using Jupiter.Implementation;
using Jupiter.Utils;
using Microsoft.AspNetCore.Authorization;
using Microsoft.AspNetCore.Http;
using Microsoft.AspNetCore.Http.Extensions;
using Microsoft.AspNetCore.Mvc;
using Microsoft.Extensions.Options;
using Newtonsoft.Json;
using Serilog;

namespace Horde.Storage.Controllers
{
    using BlobNotFoundException = Horde.Storage.Implementation.BlobNotFoundException;

    public class RefRequest
    {
        [JsonConstructor]
        public RefRequest(BlobIdentifier[] blobReferences, Dictionary<string, object>? metadata, ContentHash contentHash)
        {
            BlobReferences = blobReferences;
            Metadata = metadata;
            ContentHash = contentHash;
        }

        public Dictionary<string, object>? Metadata { get; }

        [Required] public BlobIdentifier[] BlobReferences { get; }

        [Required] public ContentHash ContentHash { get; }
    }

    [ApiController]
    [FormatFilter]
    [Route("api/v1/c")]
    [Authorize]
    public class DDCRefController : ControllerBase
    {
        private readonly IRefsStore _refsStore;
        private readonly IDiagnosticContext _diagnosticContext;
		private readonly RequestHelper _requestHelper;
        private readonly IOptionsMonitor<HordeStorageSettings> _settings;

        private readonly ILogger _logger = Log.ForContext<DDCRefController>();
        private readonly IDDCRefService _ddcRefService;

        public DDCRefController(IDDCRefService ddcRefService, IRefsStore refsStore, IDiagnosticContext diagnosticContext, RequestHelper requestHelper, IOptionsMonitor<HordeStorageSettings> settings)
        {
            _refsStore = refsStore;
            _ddcRefService = ddcRefService;
            _diagnosticContext = diagnosticContext;
            _requestHelper = requestHelper;
            _settings = settings;
        }

        /// <summary>
        /// Returns all the known namespace the token has access to
        /// </summary>
        /// <returns></returns>
        [HttpGet("ddc")]
        [ProducesDefaultResponseType]
        [ProducesResponseType(type: typeof(ProblemDetails), 400)]
        public async Task<IActionResult> GetNamespaces()
        {
            NamespaceId[] namespaces = await _refsStore.GetNamespaces().ToArrayAsync();

            // filter namespaces down to only the namespaces the user has access to
            List<NamespaceId> validNamespaces = new List<NamespaceId>();
            foreach (NamespaceId ns in namespaces)
            {
                ActionResult? result = await _requestHelper.HasAccessToNamespace(User, Request, ns, new [] { AclAction.ReadObject });
                if (result == null)
                {
                    validNamespaces.Add(ns);
                }
            }

            return Ok(new GetNamespacesResponse(validNamespaces.ToArray()));
        }

        /// <summary>
        /// Returns a refs key
        /// </summary>
        /// <remarks>If you use the .raw format specifier the raw object uploaded will be returned. If using json or another structured object it is instead base64 encoded.</remarks>
        /// <param name="ns">Namespace. Each namespace is completely separated from each other. Use for different types of data that is never expected to be similar (between two different games for instance). Example: `uc4.ddc`</param>
        /// <param name="bucket">The category/type of record you are caching. Is a clustered key together with the actual key, but all records in the same bucket can be dropped easily. Example: `terrainTexture` </param>
        /// <param name="key">The unique name of this particular key. `iAmAVeryValidKey`</param>
        /// <param name="fields">The fields to include in the response, omit this to include everything.</param>
        /// <param name="format">Optional specifier to set which output format is used json/raw</param>
        /// <returns>A json blob with the base 64 encoded object, or if octet-stream is used just the raw object</returns>
        [HttpGet("ddc/{ns}/{bucket}/{key}.{format?}", Order = 500)]
        [ProducesResponseType(type: typeof(RefResponse), 200)]
        [ProducesResponseType(type: typeof(ValidationProblemDetails), 400)]
        [Produces(MediaTypeNames.Application.Json, MediaTypeNames.Application.Octet, CustomMediaTypeNames.UnrealCompactBinary)]
        public async Task<IActionResult> Get(
            [FromRoute] [Required] NamespaceId ns,
            [FromRoute] [Required] BucketId bucket,
            [FromRoute] [Required] KeyId key,
            [FromQuery] string[] fields,
            [FromRoute] string? format = null)
        {
            ActionResult? result = await _requestHelper.HasAccessToNamespace(User, Request, ns, new [] { AclAction.ReadObject });
            if (result != null)
            {
                return result;
            }

            if (_settings.CurrentValue.DisableLegacyApi)
            {
                return NotFound("This api has been removed, you will need to update your code.");
            }

            try
            {
                bool isRaw = format?.ToUpperInvariant() == "RAW";
                // if the raw format is used only the blob itself will be output so no need to fetch anything else
                if (isRaw)
                {
                    fields = new[] {"blob"};
                }
                (RefResponse record, BlobContents? blob) = await _ddcRefService.Get(ns, bucket, key, fields);
                
                Response.Headers[CommonHeaders.HashHeaderName] = record.ContentHash.ToString();

                if (blob != null)
                {
                    Response.Headers["Content-Length"] = blob.Length.ToString();
   
                    if (isRaw)
                    {
                        using (IScope _ = Tracer.Instance.StartActive("body.write"))
                        {
                            const int BufferSize = 64 * 1024;
                            Stream outputStream = Response.Body;
                            Response.ContentLength = blob.Length;
                            Response.ContentType = MediaTypeNames.Application.Octet;
                            Response.StatusCode = StatusCodes.Status200OK;
                            try
                            {
                                await StreamCopyOperation.CopyToAsync(blob.Stream, outputStream, count: null, bufferSize: BufferSize,
                                    cancel: Response.HttpContext.RequestAborted);
                            }
                            catch (OperationCanceledException e)
                            {
                                _logger.Error(e, "Copy operation cancelled due to http request being cancelled.");
                            }
                            catch (Exception e)
                            {
                                // catch all exceptions because the unhandled exception filter will try to update the response which we have already started writing
                                _logger.Error(e, "Exception while writing response");
                            }
                        }

                        return new EmptyResult();
                    }

                    await using Stream blobStream = blob.Stream;
                    // convert to byte array in preparation for json serialization
                    record.Blob = await blobStream.ToByteArray();
                }

                return Ok(record);

            }
            catch (NamespaceNotFoundException e)
            {
                return BadRequest(new ProblemDetails {Title = $"Namespace {e.Namespace} did not exist"});
            }
            catch (RefRecordNotFoundException e)
            {
                return BadRequest(new ProblemDetails { Title = $"Object {e.Bucket} {e.Key} did not exist" });
            }
            catch (BlobNotFoundException e)
            {
                return BadRequest(new ProblemDetails { Title = $"Object {e.Blob} in {e.Ns} not found" });
            }
        }

        /// <summary>
        /// Checks if a refs key exists
        /// </summary>
        /// <param name="ns">Namespace. Each namespace is completely separated from each other. Use for different types of data that is never expected to be similar (between two different games for instance). Example: `uc4.ddc`</param>
        /// <param name="bucket">The category/type of record you are caching. Is a clustered key together with the actual key, but all records in the same bucket can be dropped easily. Example: `terrainTexture` </param>
        /// <param name="key">The unique name of this particular key. `iAmAVeryValidKey`</param>
        /// <returns>200 if it existed, 400 otherwise</returns>
        [HttpHead("ddc/{ns}/{bucket}/{key}", Order = 500)]
        [ProducesResponseType(type: typeof(RefResponse), 200)]
        [ProducesResponseType(type: typeof(ValidationProblemDetails), 400)]
        public async Task<IActionResult> Head(
            [FromRoute] [Required] NamespaceId ns,
            [FromRoute] [Required] BucketId bucket,
            [FromRoute] [Required] KeyId key)
        {
            ActionResult? result = await _requestHelper.HasAccessToNamespace(User, Request, ns, new [] { AclAction.ReadObject });
            if (result != null)
            {
                return result;
            }

            if (_settings.CurrentValue.DisableLegacyApi)
            {
                return NotFound("This api has been removed, you will need to update your code.");
            }

            try
            {
                RefRecord record = await _ddcRefService.Exists(ns, bucket, key);
                Response.Headers[CommonHeaders.HashHeaderName] = record.ContentHash.ToString();
            }
            catch (NamespaceNotFoundException e)
            {
                return BadRequest(new ProblemDetails {Title = $"Namespace {e.Namespace} did not exist"});
            }
            catch (RefRecordNotFoundException e)
            {
                return BadRequest(new ProblemDetails {Title = $"Object {e.Bucket} {e.Key} in namespace {e.Namespace} did not exist"});
            }
            catch (MissingBlobsException e)
            {
                return BadRequest(new ProblemDetails { Title = $"Blobs {e.Blobs} from object {e.Bucket} {e.Key} in namespace {e.Namespace} did not exist" });
            }

            return NoContent();
        }

        /// <summary>
        /// Insert a new refs key.
        /// </summary>
        /// <remarks>The raw object can also be sent as a octet-stream in which case the refRequest should not be sent.</remarks>
        /// <param name="ns">Namespace. Each namespace is completely separated from each other. Use for different types of data that is never expected to be similar (between two different games for instance). Example: `uc4.ddc`</param>
        /// <param name="bucket">The category/type of record you are caching. Is a clustered key together with the actual key, but all records in the same bucket can be dropped easily. Example: `terrainTexture` </param>
        /// <param name="key">The unique name of this particular key. `iAmAVeryValidKey`</param>
        /// <param name="refRequest">Json object containing which blobs you have already inserted and metadata about them. Instead of sending this you can also send a octet-stream of the object you want to refs.</param>
        /// <returns>The transaction id of the created object</returns>
        [HttpPut("ddc/{ns}/{bucket}/{key}", Order = 500)]
        [Consumes(MediaTypeNames.Application.Json)]
        [ProducesResponseType(type: typeof(PutRequestResponse), 200)]
        [ProducesResponseType(type: typeof(ValidationProblemDetails), 400)]
        public async Task<IActionResult> PutStructured(
            [FromRoute] [Required] NamespaceId ns,
            [FromRoute] [Required] BucketId bucket,
            [FromRoute] [Required] KeyId key,
            [FromBody] RefRequest refRequest)
        {
            ActionResult? result = await _requestHelper.HasAccessToNamespace(User, Request, ns, new [] { AclAction.WriteObject });
            if (result != null)
            {
                return result;
            }

            if (_settings.CurrentValue.DisableLegacyApi)
            {
                return NotFound("This api has been removed, you will need to update your code.");
            }

            try
            {
                _diagnosticContext.Set("Content-Length", Request.ContentLength ?? -1);

                long transactionId = await _ddcRefService.PutIndirect(ns, bucket, key, refRequest);
                return Ok(new PutRequestResponse(transactionId));
            }
            catch (HashMismatchException e)
            {
                return BadRequest(new ProblemDetails
                {
                    Title = $"Incorrect hash, got hash \"{e.SuppliedHash}\" but hash of content was determined to be \"{e.ContentHash}\""
                });
            }
            catch (MissingBlobsException e)
            {
                return BadRequest(new ProblemDetails { Title = "Some blobs were not uploaded", Detail = string.Join(" ", e.Blobs.Select(identifier => identifier.ToString())) });
            }
        }

        // that structured data will match the route above first
        [RequiredContentType(MediaTypeNames.Application.Octet)]
        [HttpPut("ddc/{ns}/{bucket}/{key}", Order = 500)]
        // TODO: Investigate if we can resolve the conflict between this and the other put endpoint in open api
        [ApiExplorerSettings(IgnoreApi = true)]
        [DisableRequestSizeLimit]
        public async Task<IActionResult> PutBlob(
            [FromRoute] [Required] NamespaceId ns,
            [FromRoute] [Required] BucketId bucket,
            [FromRoute] [Required] KeyId key)
        {
            ActionResult? result = await _requestHelper.HasAccessToNamespace(User, Request, ns, new [] { AclAction.WriteObject });
            if (result != null)
            {
                return result;
            }

            
            if (_settings.CurrentValue.DisableLegacyApi)
            {
                return NotFound("This api has been removed, you will need to update your code.");
            }

            byte[] blob;
            try
            {
                blob = await RequestUtil.ReadRawBody(Request);
            }
            catch (BadHttpRequestException e)
            {
                const string msg = "Partial content transfer when reading request body.";
                _logger.Warning(e, msg);
                return BadRequest(msg);
            }
            _logger.Debug("Received PUT for {Namespace} {Bucket} {Key}", ns, bucket, key);
            _diagnosticContext.Set("Content-Length", Request.ContentLength ?? -1);

            ContentHash headerHash;
            ContentHash blobHash = ContentHash.FromBlob(blob);
            ContentHash headerVerificationHash = blobHash;
            if (Request.Headers.ContainsKey(CommonHeaders.HashHeaderName))
            {
                headerHash = new ContentHash(Request.Headers[CommonHeaders.HashHeaderName]);
            }
            else if (Request.Headers.ContainsKey(CommonHeaders.HashHeaderSHA1Name))
            {
                headerHash = new ContentHash(Request.Headers[CommonHeaders.HashHeaderSHA1Name]);
                byte[] sha1Hash = Sha1Utils.GetSHA1(blob);
                headerVerificationHash = new ContentHash(sha1Hash);
            }
            else
            {
                return BadRequest(new ProblemDetails
                {
                    Title = $"Missing expected header {CommonHeaders.HashHeaderName} or {CommonHeaders.HashHeaderSHA1Name}"
                });
            }

            if (!headerVerificationHash.Equals(headerHash))
            {
                return BadRequest(new ProblemDetails
                {
                    Title = $"Incorrect hash, got hash \"{headerHash}\" but hash of content was determined to be \"{headerVerificationHash}\""
                });
            }

            PutRequestResponse response = await _ddcRefService.Put(ns, bucket, key, blobHash, blob);
            return Ok(response);

        }

        /// <summary>
        /// Drop all refs records in the namespace
        /// </summary>
        /// <param name="ns">Namespace. Each namespace is completely separated from each other. Use for different types of data that is never expected to be similar (between two different games for instance)</param>
        [HttpDelete("ddc/{ns}", Order = 500)]
        [ProducesResponseType(204)]
        public async Task<IActionResult> DeleteNamespace(
            [FromRoute] [Required] NamespaceId ns
        )
        {
            ActionResult? result = await _requestHelper.HasAccessToNamespace(User, Request, ns, new [] { AclAction.DeleteNamespace });
            if (result != null)
            {
                return result;
            }

            try
            {
                await _refsStore.DropNamespace(ns);
            }
            catch (NamespaceNotFoundException e)
            {
                return BadRequest(new ProblemDetails {Title = $"Namespace {e.Namespace} did not exist"});
            }

            return NoContent();
        }

        /// <summary>
        /// Drop all refs records in the bucket
        /// </summary>
        /// <param name="ns">Namespace. Each namespace is completely separated from each other. Use for different types of data that is never expected to be similar (between two different games for instance)</param>
        /// <param name="bucket">The category/type of record you are caching. Is a clustered key together with the actual key, but all records in the same bucket can be dropped easily.</param>
        [HttpDelete("ddc/{ns}/{bucket}", Order = 500)]
        [ProducesResponseType(204)]
        public async Task<IActionResult> DeleteBucket(
            [FromRoute] [Required] NamespaceId ns,
            [FromRoute] [Required] BucketId bucket)
        {
            ActionResult? result = await _requestHelper.HasAccessToNamespace(User, Request, ns, new [] { AclAction.DeleteBucket });
            if (result != null)
            {
                return result;
            }

            try
            {
                await _refsStore.DeleteBucket(ns, bucket);
            }
            catch (NamespaceNotFoundException e)
            {
                return BadRequest(new ProblemDetails {Title = $"Namespace {e.Namespace} did not exist"});
            }

            return NoContent();
        }

        /// <summary>
        /// Delete a individual refs key
        /// </summary>
        /// <param name="ns">Namespace. Each namespace is completely separated from each other. Use for different types of data that is never expected to be similar (between two different games for instance)</param>
        /// <param name="bucket">The category/type of record you are caching. Is a clustered key together with the actual key, but all records in the same bucket can be dropped easily.</param>
        /// <param name="key">The unique name of this particular key</param>
        [HttpDelete("ddc/{ns}/{bucket}/{key}", Order = 500)]
        [ProducesResponseType(204)]
        [ProducesResponseType(400)]
        public async Task<IActionResult> Delete(
            [FromRoute] [Required] NamespaceId ns,
            [FromRoute] [Required] BucketId bucket,
            [FromRoute] [Required] KeyId key)
        {
            ActionResult? result = await _requestHelper.HasAccessToNamespace(User, Request, ns, new [] { AclAction.DeleteObject });
            if (result != null)
            {
                return result;
            }

            try
            {
                long deleteCount = await _ddcRefService.Delete(ns, bucket, key);

                if (deleteCount == 0)
                {
                    return BadRequest("Deleted 0 records, most likely the object did not exist");
                }
            }
            catch (NamespaceNotFoundException e)
            {
                return BadRequest(new ProblemDetails {Title = $"Namespace {e.Namespace} did not exist"});
            }
            return NoContent();
        }

        // ReSharper disable UnusedAutoPropertyAccessor.Global
        // ReSharper disable once ClassNeverInstantiated.Global
        [System.Diagnostics.CodeAnalysis.SuppressMessage("Design", "CA1034:Nested types should not be visible", Justification = "Required by serialization")]
        public class DdcBatchOp
        {
            // ReSharper disable InconsistentNaming
            public enum DdcOperation
            {
                INVALID,
                GET,
                PUT,
                DELETE,
                HEAD
            }
            // ReSharper restore InconsistentNaming

            [Required] public NamespaceId? Namespace { get; set; }

            [Required]
            public BucketId? Bucket { get; set; }

            [Required] public KeyId? Id { get; set; }

            [Required] public DdcOperation Op { get; set; }

            // used for put ops
            public ContentHash? ContentHash { get; set; }
            public byte[]? Content { get; set; }
        }

        [System.Diagnostics.CodeAnalysis.SuppressMessage("Design", "CA1034:Nested types should not be visible", Justification = "Required by serialization")]
        public class DdcBatchCall
        {
            public DdcBatchOp[]? Operations { get; set; }
        }

        [HttpPost("ddc-rpc")]
        public async Task<IActionResult> Post([FromBody] DdcBatchCall batch)
        {
            AclAction MapToAclAction(DdcBatchOp.DdcOperation op)
            {
                switch (op)
                {
                    case DdcBatchOp.DdcOperation.GET:
                        return AclAction.ReadObject;
                    case DdcBatchOp.DdcOperation.PUT:
                        return AclAction.WriteObject;
                    case DdcBatchOp.DdcOperation.DELETE:
                        return AclAction.DeleteObject;
                    case DdcBatchOp.DdcOperation.HEAD:
                        return AclAction.ReadObject;
                    default:
                        throw new ArgumentOutOfRangeException(nameof(op), op, null);
                }
            }

            if (batch.Operations == null)
            {
                throw new InvalidOperationException("No operations specified, this is a required field");
            }

            if (_settings.CurrentValue.DisableLegacyApi)
            {
                return NotFound("This api has been removed, you will need to update your code.");
            }

            Task<object?>[] tasks = new Task<object?>[batch.Operations.Length];
            for (int index = 0; index < batch.Operations.Length; index++)
            {
                DdcBatchOp op = batch.Operations[index];

                ActionResult? result = await _requestHelper.HasAccessToNamespace(User, Request, op.Namespace!.Value, new [] { MapToAclAction(op.Op) });

                if (result != null)
                {
                    tasks[index] = Task.FromResult((object?)new ProblemDetails { Title = "Forbidden" });
                }
                else
                {
                    tasks[index] = ProcessOp(op).ContinueWith((task, _) => task.IsFaulted ? (object?)new ProblemDetails { Title = "Exception thrown", Detail = task!.Exception!.ToString()} : task.Result, null, TaskScheduler.Current);
                }
            }

            await Task.WhenAll(tasks);

            object?[] results = tasks.Select(t => t.Result).ToArray();

            return Ok(results);
        }

        private Task<object?> ProcessOp(DdcBatchOp op)
        {
            if (op.Namespace == null)
            {
                throw new InvalidOperationException("namespace was null, is required");
            }

            if (op.Bucket == null)
            {
                throw new InvalidOperationException("bucket was null, is required");
            }

            if (op.Id == null)
            {
                throw new InvalidOperationException("id was null, is required");
            }

            switch (op.Op)
            {
                case DdcBatchOp.DdcOperation.INVALID:
                    throw new NotImplementedException($"Unsupported batch op {DdcBatchOp.DdcOperation.INVALID} used in op {op.Id}");
                case DdcBatchOp.DdcOperation.GET:
                    // TODO: support field filtering
                    return _ddcRefService.Get(op.Namespace.Value, op.Bucket.Value, op.Id.Value, Array.Empty<string>()).ContinueWith((t,_) => 
                    {
                        // we are serializing this to json, so we just stream the blob into memory to return it.
                        (RefResponse response, BlobContents? blob) = t.Result;
                        if (blob != null)
                        {
                            using BlobContents blobContents = blob;
                            response.Blob = blobContents.Stream.ToByteArray().Result;
                        }
                        return (object?)response;
                    }, null, TaskScheduler.Current);
                case DdcBatchOp.DdcOperation.HEAD:
                    return _ddcRefService.Exists(op.Namespace.Value, op.Bucket.Value, op.Id.Value).ContinueWith((t,_) => t.Result == null ? (object?)null : op.Id, null, TaskScheduler.Current);
                case DdcBatchOp.DdcOperation.PUT:
                {
                    if (op.ContentHash == null)
                    {
                        throw new InvalidOperationException("ContentHash was null, is required");
                    }

                    if (op.Content == null)
                    {
                        throw new InvalidOperationException("Content was null, is required");
                    }

                    ContentHash blobHash = ContentHash.FromBlob(op.Content);
                    if (!blobHash.Equals(op.ContentHash))
                    {
                        throw new HashMismatchException(op.ContentHash, blobHash);
                    }
                    return _ddcRefService.Put(op.Namespace.Value, op.Bucket.Value, op.Id.Value, blobHash, op.Content).ContinueWith((t,_) => (object?)t.Result, null, TaskScheduler.Current);
                }
                case DdcBatchOp.DdcOperation.DELETE:
                    return _ddcRefService.Delete(op.Namespace.Value, op.Bucket.Value, op.Id.Value).ContinueWith((t,_) => (object?)null, null, TaskScheduler.Current);
                default:
                    throw new NotImplementedException($"Unknown op used {op.Op}");
            }
        }

        [System.Diagnostics.CodeAnalysis.SuppressMessage("Design", "CA1034:Nested types should not be visible", Justification = "Used by serialization")]
        public class BatchGetOp
        {
            public class GetOp
            {
                public BucketId? Bucket { get; set; } = null!;
                public KeyId? Key { get; set; } = null!;
                public BatchWriter.OpVerb Verb { get; set; } = BatchWriter.OpVerb.GET;
            }

            public NamespaceId? Namespace { get; set; } = null!;

            public GetOp[] Operations { get; set; } = null!;
        }
        
        /// <summary>
        /// Custom batch get rpc to allows us to fetch multiple objects and returning them as a custom binary stream avoiding base64 encoding.
        /// Objects are returned in the order they have been fetched, not the order they were requested in.
        /// </summary>
        /// <param name="batch">Spec for which objects to return</param>
        /// <returns></returns>
        [HttpPost("ddc-rpc/batchGet")]
        public async Task<IActionResult> BatchGet([FromBody] BatchGetOp batch)
        {
            if (batch.Namespace == null)
            {
                throw new InvalidOperationException("A valid namespace must be specified");
            }

            ActionResult? result = await _requestHelper.HasAccessToNamespace(User, Request, batch.Namespace.Value, new [] { AclAction.ReadObject });
            if (result != null)
            {
                return result;
            }

            BatchWriter writer = new BatchWriter();
            await writer.WriteToStream(Response.Body, batch.Namespace.Value!, batch.Operations.Select(op => new Tuple<BatchWriter.OpVerb, BucketId, KeyId>(op.Verb, op.Bucket!.Value, op.Key!.Value)).ToList(),
                async (verb, ns, bucket, key, fields) =>
                {
                    BatchWriter.OpState opState;
                    RefResponse? refResponse = null;
                    BlobContents? blob = null;
                    try
                    {
                        switch (verb)
                        {
                            case BatchWriter.OpVerb.GET:
                                (refResponse, blob) = await _ddcRefService.Get(ns, bucket, key, fields);
                                opState = BatchWriter.OpState.OK;
                                break;
                            case BatchWriter.OpVerb.HEAD:
                                await _ddcRefService.Exists(ns, bucket, key);
                                opState = BatchWriter.OpState.Exists;
                                refResponse = null;
                                blob = null;
                                break;
                            default:
                                throw new ArgumentOutOfRangeException(nameof(verb), verb, null);
                        }
                    }
                    catch (RefRecordNotFoundException)
                    {
                        opState = BatchWriter.OpState.NotFound;
                    }
                    catch (BlobNotFoundException)
                    {
                        opState = BatchWriter.OpState.NotFound;
                    }
                    catch (MissingBlobsException)
                    {
                        opState = BatchWriter.OpState.NotFound;
                    }
                    catch (NamespaceNotFoundException)
                    {
                        opState = BatchWriter.OpState.NotFound;
                    }
                    catch (Exception e)
                    {
                        _logger.Error(e, "Unknown exception when executing batch get");

                        // we want to make sure that we always continue to write the results even when we get errors
                        opState = BatchWriter.OpState.Failed;
                    }

                    return new Tuple<ContentHash?, BlobContents?, BatchWriter.OpState>(refResponse?.ContentHash, blob, opState);
                });

            // we have already set the result by writing to Response
            return new EmptyResult();
        }
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

    public class RefRecordNotFoundException : Exception
    {
        public RefRecordNotFoundException(NamespaceId ns, BucketId bucket, KeyId key)
        {
            Namespace = ns;
            Bucket = bucket;
            Key = key;
        }

        public NamespaceId Namespace { get; }
        public BucketId Bucket { get; }
        public KeyId Key { get; }
    }

    public class MissingBlobsException : Exception
    {
        public MissingBlobsException(NamespaceId ns, BucketId bucket, KeyId key, BlobIdentifier[] blobs)
        {
            Namespace = ns;
            Bucket = bucket;
            Key = key;
            Blobs = blobs;
        }

        public NamespaceId Namespace { get; }
        public BucketId Bucket { get; }
        public KeyId Key { get; }
        public BlobIdentifier[] Blobs { get; }
    }

    public class PutRequestResponse
    {
        public PutRequestResponse(long transactionId)
        {
            TransactionId = transactionId;
        }

        public long TransactionId { get; }
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
}
