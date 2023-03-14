// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Concurrent;
using System.Collections.Generic;
using System.ComponentModel.DataAnnotations;
using System.Linq;
using System.Net;
using System.Net.Mime;
using System.Threading.Tasks;
using EpicGames.AspNet;
using EpicGames.Horde.Storage;
using EpicGames.Serialization;
using Horde.Storage.Implementation;
using Jupiter;
using Jupiter.Common.Implementation;
using Jupiter.Implementation;
using Microsoft.AspNetCore.Authorization;
using Microsoft.AspNetCore.Mvc;
using Microsoft.Extensions.Primitives;
using Serilog;
using ContentId = Jupiter.Implementation.ContentId;
using CustomMediaTypeNames = Jupiter.CustomMediaTypeNames;

namespace Horde.Storage.Controllers
{
    using BlobNotFoundException = Horde.Storage.Implementation.BlobNotFoundException;

    [ApiController]
    [Authorize]
    [Route("api/v1/compressed-blobs")]
    public class CompressedBlobController : ControllerBase
    {
        private readonly IBlobService _storage;
        private readonly IContentIdStore _contentIdStore;
        private readonly IDiagnosticContext _diagnosticContext;
        private readonly RequestHelper _requestHelper;
        private readonly CompressedBufferUtils _compressedBufferUtils;
        private readonly BufferedPayloadFactory _bufferedPayloadFactory;

        public CompressedBlobController(IBlobService storage, IContentIdStore contentIdStore, IDiagnosticContext diagnosticContext, RequestHelper requestHelper, CompressedBufferUtils compressedBufferUtils, BufferedPayloadFactory bufferedPayloadFactory)
        {
            _storage = storage;
            _contentIdStore = contentIdStore;
            _diagnosticContext = diagnosticContext;
            _requestHelper = requestHelper;
            _compressedBufferUtils = compressedBufferUtils;
            _bufferedPayloadFactory = bufferedPayloadFactory;
        }

        [HttpGet("{ns}/{id}")]
        [ProducesResponseType(type: typeof(byte[]), 200)]
        [ProducesResponseType(type: typeof(ValidationProblemDetails), 400)]
        [Produces(CustomMediaTypeNames.UnrealCompressedBuffer, MediaTypeNames.Application.Octet)]

        public async Task<IActionResult> Get(
            [Required] NamespaceId ns,
            [Required] ContentId id)
        {
            ActionResult? result = await _requestHelper.HasAccessToNamespace(User, Request, ns, new [] { AclAction.ReadObject });
            if (result != null)
            {
                return result;
            }

            try
            {
                (BlobContents blobContents, string mediaType) = await _storage.GetCompressedObject(ns, id, _contentIdStore);

                StringValues acceptHeader = Request.Headers["Accept"];
                if (!acceptHeader.Contains("*/*") && acceptHeader.Count != 0 && !acceptHeader.Contains(mediaType))
                {
                    return new UnsupportedMediaTypeResult();
                }

                return File(blobContents.Stream, mediaType, enableRangeProcessing: true);
            }
            catch (BlobNotFoundException e)
            {
                return NotFound(new ValidationProblemDetails { Title = $"Object {e.Blob} not found" });
            }
            catch (ContentIdResolveException e)
            {
                return NotFound(new ValidationProblemDetails { Title = $"Content Id {e.ContentId} not found" });
            }
        }
        
        [HttpHead("{ns}/{id}")]
        [ProducesDefaultResponseType]
        public async Task<IActionResult> Head(
            [Required] NamespaceId ns,
            [Required] ContentId id)
        {
            ActionResult? result = await _requestHelper.HasAccessToNamespace(User, Request, ns, new [] { AclAction.ReadObject });
            if (result != null)
            {
                return result;
            }

            BlobIdentifier[]? chunks = await _contentIdStore.Resolve(ns, id, mustBeContentId: false);
            if (chunks == null || chunks.Length == 0)
            {
                return NotFound();
            }

            Task<bool>[] tasks = new Task<bool>[chunks.Length];
            for (int i = 0; i < chunks.Length; i++)
            {
                tasks[i] = _storage.Exists(ns, chunks[i]);
            }

            await Task.WhenAll(tasks);

            bool exists = tasks.All(task => task.Result);

            if (!exists)
            {
                return NotFound();
            }

            return Ok();
        }

        [HttpPost("{ns}/exists")]
        [ProducesDefaultResponseType]
        public async Task<IActionResult> ExistsMultiple(
            [Required] NamespaceId ns,
            [Required] [FromQuery] List<ContentId> id)
        {
            ActionResult? result = await _requestHelper.HasAccessToNamespace(User, Request, ns, new [] { AclAction.ReadObject });
            if (result != null)
            {
                return result;
            }

            ConcurrentBag<ContentId> partialContentIds = new ConcurrentBag<ContentId>();
            ConcurrentBag<ContentId> invalidContentIds = new ConcurrentBag<ContentId>();

            IEnumerable<Task> tasks = id.Select(async blob =>
            {
                BlobIdentifier[]? chunks = await _contentIdStore.Resolve(ns, blob, mustBeContentId: false);

                if (chunks == null)
                {
                    invalidContentIds.Add(blob);
                    return;
                }

                foreach (BlobIdentifier chunk in chunks)
                {
                    if (!await _storage.Exists(ns, chunk))
                    {
                        partialContentIds.Add(blob);
                        break;
                    }
                }
            });
            await Task.WhenAll(tasks);

            List<ContentId> needs = new List<ContentId>(invalidContentIds);
            needs.AddRange(partialContentIds);
             
            return Ok(new ExistCheckMultipleContentIdResponse { Needs = needs.ToArray()});
        }

        [HttpPost("{ns}/exist")]
        [ProducesDefaultResponseType]
        public async Task<IActionResult> ExistsBody(
            [Required] NamespaceId ns,
            [FromBody] ContentId[] bodyIds)
        {
            ActionResult? result = await _requestHelper.HasAccessToNamespace(User, Request, ns, new [] { AclAction.ReadObject });
            if (result != null)
            {
                return result;
            }

            ConcurrentBag<ContentId> partialContentIds = new ConcurrentBag<ContentId>();
            ConcurrentBag<ContentId> invalidContentIds = new ConcurrentBag<ContentId>();

            IEnumerable<Task> tasks = bodyIds.Select(async blob =>
            {
                BlobIdentifier[]? chunks = await _contentIdStore.Resolve(ns, blob, mustBeContentId: false);

                if (chunks == null)
                {
                    invalidContentIds.Add(blob);
                    return;
                }

                foreach (BlobIdentifier chunk in chunks)
                {
                    if (!await _storage.Exists(ns, chunk))
                    {
                        partialContentIds.Add(blob);
                        break;
                    }
                }
            });
            await Task.WhenAll(tasks);

            List<ContentId> needs = new List<ContentId>(invalidContentIds);
            needs.AddRange(partialContentIds);
             
            return Ok(new ExistCheckMultipleContentIdResponse { Needs = needs.ToArray()});
        }

        [HttpPut("{ns}/{id}")]
        [DisableRequestSizeLimit]
        [RequiredContentType(CustomMediaTypeNames.UnrealCompressedBuffer)]
        public async Task<IActionResult> Put(
            [Required] NamespaceId ns,
            [Required] ContentId id)
        {
            ActionResult? result = await _requestHelper.HasAccessToNamespace(User, Request, ns, new [] { AclAction.WriteObject });
            if (result != null)
            {
                return result;
            }

            _diagnosticContext.Set("Content-Length", Request.ContentLength ?? -1);

            try
            {
                using IBufferedPayload payload = await _bufferedPayloadFactory.CreateFromRequest(Request);

                ContentId identifier =
                    await _storage.PutCompressedObject(ns, payload, id, _contentIdStore, _compressedBufferUtils);

                return Ok(new { Identifier = identifier.ToString() });
            }
            catch (HashMismatchException e)
            {
                return BadRequest(new ProblemDetails
                {
                    Title =
                        $"Incorrect hash, got hash \"{e.SuppliedHash}\" but hash of content was determined to be \"{e.ContentHash}\""
                });
            }
            catch (ClientSendSlowException e)
            {
                return Problem(e.Message, null, (int)HttpStatusCode.RequestTimeout);
            }
        }

        [HttpPost("{ns}")]
        [DisableRequestSizeLimit]
        [RequiredContentType(CustomMediaTypeNames.UnrealCompressedBuffer)]
        public async Task<IActionResult> Post(
            [Required] NamespaceId ns)
        {
            ActionResult? result = await _requestHelper.HasAccessToNamespace(User, Request, ns, new [] { AclAction.WriteObject });
            if (result != null)
            {
                return result;
            }

            _diagnosticContext.Set("Content-Length", Request.ContentLength ?? -1);

            try
            {
                using IBufferedPayload payload = await _bufferedPayloadFactory.CreateFromRequest(Request);

                ContentId identifier = await _storage.PutCompressedObject(ns, payload, null, _contentIdStore, _compressedBufferUtils);

                return Ok(new
                {
                    Identifier = identifier.ToString()
                });
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
        }

        /*[HttpDelete("{ns}/{id}")]
        public async Task<IActionResult> Delete(
            [Required] string ns,
            [Required] BlobIdentifier id)
        {
            ActionResult? result = await _requestHelper.HasAccessToNamespace(User, Request, ns);
            if (result != null)
            {
                return result;
            }

            await DeleteImpl(ns, id);

            return Ok();
        }

        
        [HttpDelete("{ns}")]
        public async Task<IActionResult> DeleteNamespace(
            [Required] string ns)
        {
            ActionResult? result = await _requestHelper.HasAccessToNamespace(User, Request, ns);
            if (result != null)
            {
                return result;
            }

            int deletedCount = await  _storage.DeleteNamespace(ns);

            return Ok( new { Deleted = deletedCount });
        }


        private async Task DeleteImpl(string ns, BlobIdentifier id)
        {
            await _storage.DeleteObject(ns, id);
        }*/
    }

    public class ExistCheckMultipleContentIdResponse
    {
        [CbField("needs")]
        public ContentId[] Needs { get; set; } = null!;
    }
}
