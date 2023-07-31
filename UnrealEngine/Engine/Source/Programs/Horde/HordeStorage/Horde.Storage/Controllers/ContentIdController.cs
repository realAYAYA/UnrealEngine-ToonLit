// Copyright Epic Games, Inc. All Rights Reserved.

using System.Net.Mime;
using System.Threading.Tasks;
using EpicGames.Horde.Storage;
using Horde.Storage.Implementation;
using Jupiter;
using Jupiter.Implementation;
using Microsoft.AspNetCore.Authorization;
using Microsoft.AspNetCore.Mvc;
using ContentId = Jupiter.Implementation.ContentId;

namespace Horde.Storage.Controllers
{
    [ApiController]
    [FormatFilter]
    [Produces(MediaTypeNames.Application.Json, MediaTypeNames.Application.Octet, CustomMediaTypeNames.UnrealCompactBinary)]
    [Route("api/v1/content-id")]
    [Authorize]
    public class ContentIdController : ControllerBase
    {
        private readonly RequestHelper _requestHelper;
        private readonly IContentIdStore _contentIdStore;

        public ContentIdController(RequestHelper requestHelper, IContentIdStore contentIdStore)
        {
            _requestHelper = requestHelper;
            _contentIdStore = contentIdStore;
        }

        /// <summary>
        /// Returns which blobs a content id maps to
        /// </summary>
        /// <param name="ns">Namespace. Each namespace is completely separated from each other. Use for different types of data that is never expected to be similar (between two different games for instance). Example: `uc4.ddc`</param>
        /// <param name="contentId">The content id to resolve </param>
        [HttpGet("{ns}/{contentId}.{format?}", Order = 500)]
        [ProducesDefaultResponseType]
        [ProducesResponseType(type: typeof(ProblemDetails), 400)]
        public async Task<IActionResult> Resolve(NamespaceId ns, ContentId contentId)
        {
            ActionResult? result = await _requestHelper.HasAccessToNamespace(User, Request, ns, new [] { AclAction.ReadObject });
            if (result != null)
            {
                return result;
            }
            BlobIdentifier[]? blobs = await _contentIdStore.Resolve(ns, contentId, mustBeContentId: true);

            if (blobs == null)
            {
                return NotFound(new ProblemDetails { Title = $"Unable to resolve content id {contentId} ({ns})." });
            }

            return Ok(new ResolvedContentIdResponse(blobs));
        }

        /// <summary>
        /// Update a content id mapping
        /// </summary>
        /// <param name="ns">Namespace. Each namespace is completely separated from each other. Use for different types of data that is never expected to be similar (between two different games for instance). Example: `uc4.ddc`</param>
        /// <param name="contentId">The content id to resolve </param>
        /// <param name="blobIdentifier">The blob identifier to map the content id to</param>
        /// <param name="contentWeight">The weight of this mapping, higher means more important</param>
        [HttpPut("{ns}/{contentId}/update/{blobIdentifier}/{contentWeight}", Order = 500)]
        [ProducesDefaultResponseType]
        [ProducesResponseType(type: typeof(ProblemDetails), 400)]
        public async Task<IActionResult> UpdateContentIdMapping(NamespaceId ns, ContentId contentId, BlobIdentifier blobIdentifier, int contentWeight)
        {
            ActionResult? result = await _requestHelper.HasAccessToNamespace(User, Request, ns, new [] { AclAction.WriteObject });
            if (result != null)
            {
                return result;
            }
            await _contentIdStore.Put(ns, contentId, blobIdentifier, contentWeight);

            return Ok();
        }
    }

    public class ResolvedContentIdResponse
    {
        public BlobIdentifier[] Blobs { get; set; }

        public ResolvedContentIdResponse(BlobIdentifier[] blobs)
        {
            Blobs = blobs;
        }
    }
}
