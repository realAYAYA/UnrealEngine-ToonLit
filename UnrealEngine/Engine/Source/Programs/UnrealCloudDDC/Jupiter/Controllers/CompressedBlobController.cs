// Copyright Epic Games, Inc. All Rights Reserved.

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
using Jupiter.Implementation;
using Jupiter.Common.Implementation;
using Microsoft.AspNetCore.Authorization;
using Microsoft.AspNetCore.Mvc;
using Microsoft.Extensions.Primitives;
using ContentId = Jupiter.Implementation.ContentId;

namespace Jupiter.Controllers
{
	using IDiagnosticContext = Serilog.IDiagnosticContext;
	using BlobNotFoundException = BlobNotFoundException;

	[ApiController]
	[Authorize]
	[Route("api/v1/compressed-blobs")]
	public class CompressedBlobController : ControllerBase
	{
		private readonly IBlobService _storage;
		private readonly IContentIdStore _contentIdStore;
		private readonly IDiagnosticContext _diagnosticContext;
		private readonly IRequestHelper _requestHelper;
		private readonly BufferedPayloadFactory _bufferedPayloadFactory;
		private readonly NginxRedirectHelper _nginxRedirectHelper;

		public CompressedBlobController(IBlobService storage, IContentIdStore contentIdStore, IDiagnosticContext diagnosticContext, IRequestHelper requestHelper, BufferedPayloadFactory bufferedPayloadFactory, NginxRedirectHelper nginxRedirectHelper)
		{
			_storage = storage;
			_contentIdStore = contentIdStore;
			_diagnosticContext = diagnosticContext;
			_requestHelper = requestHelper;
			_bufferedPayloadFactory = bufferedPayloadFactory;
			_nginxRedirectHelper = nginxRedirectHelper;
		}

		[HttpGet("{ns}/{id}")]
		[ProducesResponseType(type: typeof(byte[]), 200)]
		[ProducesResponseType(type: typeof(ValidationProblemDetails), 400)]
		[Produces(CustomMediaTypeNames.UnrealCompressedBuffer, MediaTypeNames.Application.Octet)]

		public async Task<IActionResult> GetAsync(
			[Required] NamespaceId ns,
			[Required] ContentId id)
		{
			ActionResult? result = await _requestHelper.HasAccessToNamespaceAsync(User, Request, ns, new [] { JupiterAclAction.ReadObject });
			if (result != null)
			{
				return result;
			}

			try
			{
				(BlobContents blobContents, string mediaType) = await _storage.GetCompressedObjectAsync(ns, id, HttpContext.RequestServices, supportsRedirectUri: true);

				StringValues acceptHeader = Request.Headers["Accept"];
				if (!acceptHeader.Contains("*/*") && acceptHeader.Count != 0 && !acceptHeader.Contains(mediaType))
				{
					return new UnsupportedMediaTypeResult();
				}

				if (blobContents.RedirectUri != null)
				{
					return Redirect(blobContents.RedirectUri.ToString());
				}

				if (_nginxRedirectHelper.CanRedirect(Request, blobContents))
				{
					return _nginxRedirectHelper.CreateActionResult(blobContents, mediaType);
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
		public async Task<IActionResult> HeadAsync(
			[Required] NamespaceId ns,
			[Required] ContentId id)
		{
			ActionResult? result = await _requestHelper.HasAccessToNamespaceAsync(User, Request, ns, new [] { JupiterAclAction.ReadObject });
			if (result != null)
			{
				return result;
			}

			BlobId[]? chunks = await _contentIdStore.ResolveAsync(ns, id, mustBeContentId: false);
			if (chunks == null || chunks.Length == 0)
			{
				return NotFound();
			}

			Task<bool>[] tasks = new Task<bool>[chunks.Length];
			for (int i = 0; i < chunks.Length; i++)
			{
				tasks[i] = _storage.ExistsAsync(ns, chunks[i]);
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
		public async Task<IActionResult> ExistsMultipleAsync(
			[Required] NamespaceId ns,
			[Required] [FromQuery] List<ContentId> id)
		{
			ActionResult? result = await _requestHelper.HasAccessToNamespaceAsync(User, Request, ns, new [] { JupiterAclAction.ReadObject });
			if (result != null)
			{
				return result;
			}

			ConcurrentBag<ContentId> partialContentIds = new ConcurrentBag<ContentId>();
			ConcurrentBag<ContentId> invalidContentIds = new ConcurrentBag<ContentId>();

			IEnumerable<Task> tasks = id.Select(async blob =>
			{
				BlobId[]? chunks = await _contentIdStore.ResolveAsync(ns, blob, mustBeContentId: false);

				if (chunks == null)
				{
					invalidContentIds.Add(blob);
					return;
				}

				foreach (BlobId chunk in chunks)
				{
					if (!await _storage.ExistsAsync(ns, chunk))
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
		public async Task<IActionResult> ExistsBodyAsync(
			[Required] NamespaceId ns,
			[FromBody] ContentId[] bodyIds)
		{
			ActionResult? result = await _requestHelper.HasAccessToNamespaceAsync(User, Request, ns, new [] { JupiterAclAction.ReadObject });
			if (result != null)
			{
				return result;
			}

			ConcurrentBag<ContentId> partialContentIds = new ConcurrentBag<ContentId>();
			ConcurrentBag<ContentId> invalidContentIds = new ConcurrentBag<ContentId>();

			IEnumerable<Task> tasks = bodyIds.Select(async blob =>
			{
				BlobId[]? chunks = await _contentIdStore.ResolveAsync(ns, blob, mustBeContentId: false);

				if (chunks == null)
				{
					invalidContentIds.Add(blob);
					return;
				}

				foreach (BlobId chunk in chunks)
				{
					if (!await _storage.ExistsAsync(ns, chunk))
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
		public async Task<IActionResult> PutAsync(
			[Required] NamespaceId ns,
			[Required] ContentId id)
		{
			ActionResult? result = await _requestHelper.HasAccessToNamespaceAsync(User, Request, ns, new [] { JupiterAclAction.WriteObject });
			if (result != null)
			{
				return result;
			}

			_diagnosticContext.Set("Content-Length", Request.ContentLength ?? -1);

			try
			{
				using IBufferedPayload payload = await _bufferedPayloadFactory.CreateFromRequest(Request);

				ContentId identifier = await _storage.PutCompressedObjectAsync(ns, payload, id, HttpContext.RequestServices);

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
		public async Task<IActionResult> PostAsync(
			[Required] NamespaceId ns)
		{
			ActionResult? result = await _requestHelper.HasAccessToNamespaceAsync(User, Request, ns, new [] { JupiterAclAction.WriteObject });
			if (result != null)
			{
				return result;
			}

			_diagnosticContext.Set("Content-Length", Request.ContentLength ?? -1);

			try
			{
				using IBufferedPayload payload = await _bufferedPayloadFactory.CreateFromRequest(Request);

				ContentId identifier = await _storage.PutCompressedObjectAsync(ns, payload, null, HttpContext.RequestServices);

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
