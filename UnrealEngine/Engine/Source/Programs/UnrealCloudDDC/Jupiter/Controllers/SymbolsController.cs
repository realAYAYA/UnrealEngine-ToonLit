// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.ComponentModel.DataAnnotations;
using System.IO;
using System.Linq;
using System.Net.Mime;
using System.Text;
using System.Threading.Tasks;
using EpicGames.AspNet;
using EpicGames.Core;
using EpicGames.Horde.Storage;
using EpicGames.Serialization;
using Jupiter.Common.Implementation;
using Jupiter.Implementation;
using Jupiter.Utils;
using Microsoft.AspNetCore.Authorization;
using Microsoft.AspNetCore.Http;
using Microsoft.AspNetCore.Http.Extensions;
using Microsoft.AspNetCore.Mvc;
using Microsoft.Extensions.Logging;
using OpenTelemetry.Trace;
using Serilog;
using Microsoft.Extensions.DependencyInjection;
using System.Collections.Generic;
using BinaryReader = System.IO.BinaryReader;

namespace Jupiter.Controllers
{
	[ApiController]
	[Produces(MediaTypeNames.Application.Json, MediaTypeNames.Application.Octet, CustomMediaTypeNames.UnrealCompactBinary)]
	[Route("api/v1/symbols")]
	[Authorize]
	public class SymbolsController : ControllerBase
	{
		private readonly IRefService _refService;
		private readonly IBlobService _blobStore;
		private readonly IDiagnosticContext _diagnosticContext;
		private readonly BufferedPayloadFactory _bufferedPayloadFactory;
		private readonly FormatResolver _formatResolver;
		private readonly NginxRedirectHelper _nginxRedirectHelper;
		private readonly IRequestHelper _requestHelper;
		private readonly Tracer _tracer;
		private readonly ILogger<SymbolsController> _logger;

		public SymbolsController(IRefService refService, IBlobService blobStore, IDiagnosticContext diagnosticContext, BufferedPayloadFactory bufferedPayloadFactory, FormatResolver formatResolver, NginxRedirectHelper nginxRedirectHelper, IRequestHelper requestHelper, Tracer tracer, ILogger<SymbolsController> logger)
		{
			_refService = refService;
			_blobStore = blobStore;
			_diagnosticContext = diagnosticContext;
			_bufferedPayloadFactory = bufferedPayloadFactory;
			_formatResolver = formatResolver;
			_nginxRedirectHelper = nginxRedirectHelper;
			_requestHelper = requestHelper;
			_tracer = tracer;
			_logger = logger;
		}

		/// <summary>
		///  Fetch a symbol file
		/// </summary>
		/// <param name="ns">Namespace. Each namespace is completely separated from each other. Use for different types of data that is never expected to be similar (between two different games for instance). Example: `uc4.ddc`</param>
		/// <param name="moduleName">The name of the module, so Foo.pdb for instance</param>
		/// <param name="identifier">The pdb identifier and age</param>
		/// <param name="fileName">The specific file to fetch, either pdb or ptrs</param>
		[HttpGet("{ns}/{moduleName}/{identifier}/{fileName}", Order = 500)]
		public async Task<IActionResult> GetAsync(
			[FromRoute] [Required] NamespaceId ns,
			[FromRoute] [Required] string moduleName,
			[FromRoute] [Required] string identifier,
			[FromRoute] [Required] string fileName)
		{
			ActionResult? accessResult = await _requestHelper.HasAccessToNamespaceAsync(User, Request, ns, new[] { JupiterAclAction.ReadObject });
			if (accessResult != null)
			{
				return accessResult;
			}

			BlobContents? refContents;
			try
			{
				(RefRecord _, refContents) = await _refService.GetAsync(ns, new BucketId(moduleName), RefId.FromName($"{moduleName}.{identifier}.{fileName}"), Array.Empty<string>());
			}
			catch (RefNotFoundException)
			{
				return NotFound("Symbol not found");
			}
	
			if (refContents == null)
			{
				// TODO: is a large blob that is not inlined, we need to read this back from blob storage
				return BadRequest("No Blob Contents found");
			}

			byte[] blobMemory = await refContents.Stream.ToByteArrayAsync();
			CbObject cb = new CbObject(blobMemory);
			IoHash payloadHash = cb["pdbPayload"].AsBinaryAttachment().Hash;

			BlobContents referencedBlobContents = await _blobStore.GetObjectAsync(ns, BlobId.FromIoHash(payloadHash), null, supportsRedirectUri: true);

			if (referencedBlobContents.RedirectUri != null)
			{
				return Redirect(referencedBlobContents.RedirectUri.ToString());
			}

			if (_nginxRedirectHelper.CanRedirect(Request, referencedBlobContents))
			{
				return _nginxRedirectHelper.CreateActionResult(referencedBlobContents, CustomMediaTypeNames.UnrealCompressedBuffer);
			}

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

			string responseType = _formatResolver.GetResponseType(Request, null, MediaTypeNames.Application.Octet);
			Tracer.CurrentSpan.SetAttribute("response-type", responseType);
			switch (responseType)
			{
				case CustomMediaTypeNames.UnrealCompressedBuffer:
					{
						await WriteBody(referencedBlobContents, CustomMediaTypeNames.UnrealCompressedBuffer);
						break;
					}
				case MediaTypeNames.Application.Octet:
					{
						CompressedBufferUtils utils = new CompressedBufferUtils(_tracer, _bufferedPayloadFactory);
						using IBufferedPayload payload = await utils.DecompressContentAsync(referencedBlobContents.Stream, (ulong)referencedBlobContents.Length);
						await using Stream s = payload.GetStream();
						await using BlobContents contents = new BlobContents(s, s.Length);
						await WriteBody(contents, MediaTypeNames.Application.Octet);
						break;
					}

				default:
					throw new NotImplementedException($"Unknown expected response type {responseType}");
			}

			// content was written above
			return new EmptyResult();
		}

		[HttpPut("{ns}/{moduleName}", Order = 500)]
		[DisableRequestSizeLimit]
		[RequiredContentType(CustomMediaTypeNames.UnrealCompressedBuffer, MediaTypeNames.Application.Octet)]
		public async Task<IActionResult> PutSymbolsAsync(
			[FromRoute] [Required] NamespaceId ns,
			[FromRoute] [Required] string moduleName)
		{
			ActionResult? accessResult = await _requestHelper.HasAccessToNamespaceAsync(User, Request, ns, new[] { JupiterAclAction.WriteObject });
			if (accessResult != null)
			{
				return accessResult;
			}

			_diagnosticContext.Set("Content-Length", Request.ContentLength ?? -1);
			CompressedBufferUtils utils = new CompressedBufferUtils(_tracer, _bufferedPayloadFactory);

			IBufferedPayload payloadToUse = await _bufferedPayloadFactory.CreateFromRequest(Request);
			if (Request.ContentType == MediaTypeNames.Application.Octet)
			{
				await using Stream s = payloadToUse.GetStream();
				using MemoryStream compressedStream = new MemoryStream();

				// compress the content so we can use our normal path for processing content
				utils.CompressContent(compressedStream, OoodleCompressorMethod.Kraken, OoodleCompressionLevel.VeryFast, await s.ToByteArrayAsync());

				compressedStream.Seek(0, SeekOrigin.Begin);
				payloadToUse = await _bufferedPayloadFactory.CreateFromStreamAsync(compressedStream, compressedStream.Length);
			}

			using IBufferedPayload payload = payloadToUse;
			await using Stream hashStream = payload.GetStream();
			BlobId attachmentHash = await BlobId.FromStreamAsync(hashStream);
			IBufferedPayload decompressedContent = await utils.DecompressContentAsync(payload.GetStream(), (ulong)payload.Length);

			(string pdbIdentifier, int pdbAge) = ExtractModuleInformation(moduleName, decompressedContent);

			string filename = moduleName;
			IoHash attachmentIoHash = attachmentHash.AsIoHash();
			CbWriter writer = new CbWriter();
			writer.BeginObject();
			writer.WriteBinaryAttachment("pdbPayload", attachmentIoHash);
			writer.WriteString("moduleName", moduleName);
			writer.WriteString("pdbIdentifier", pdbIdentifier);
			writer.WriteInteger("pdbAge", pdbAge);
			writer.EndObject();

			byte[] blob = writer.ToByteArray();
			CbObject o = new CbObject(blob);
			BlobId blobHeader = BlobId.FromBlob(blob);
			await _blobStore.PutObjectKnownHashAsync(ns, payload, attachmentHash);

			(ContentId[], BlobId[]) missingHashes = await _refService.PutAsync(ns, new BucketId(moduleName), RefId.FromName($"{moduleName}.{pdbIdentifier}{pdbAge}.{filename}"), blobHeader, o);

			if (missingHashes.Item1.Any() || missingHashes.Item2.Any())
			{
				// this shouldn't be able to handle as we upload the object just before adding the ref
				return BadRequest("Missing references from object");
			}

			return Ok(new PutSymbolResponse(moduleName, pdbIdentifier, pdbAge, attachmentIoHash));
		}

		private static (string, int) ExtractModuleInformation(string moduleName, IBufferedPayload decompressedContent)
		{
			using Stream s = decompressedContent.GetStream();
			string extension = Path.GetExtension(moduleName);
			switch (extension)
			{
				case ".pdb":
					return ExtractModuleInformationPdb(s);
				default:
					throw new NotImplementedException($"Unhandled extension type: {extension}");
			}
		}

		private static (string, int) ExtractModuleInformationPdb(Stream s)
		{
			// the pdb format is somewhat documented here: 
			// https://llvm.org/docs/PDB/MsfFile.html
			// as well as here
			// https://github.com/microsoft/microsoft-pdb

			using BinaryReader reader = new BinaryReader(s);
			// extract magic
			const string MagicHeader= "Microsoft C/C++ MSF 7.00\r\n\u001aDS\0\0\0";
			byte[] magicBytes = reader.ReadBytes(MagicHeader.Length);
			string magicString = Encoding.ASCII.GetString(magicBytes);
			if (!string.Equals(magicString, MagicHeader, StringComparison.OrdinalIgnoreCase))
			{
				throw new Exception("Failed to find expected magic for pdb header");
			}

			// parse the super block
			int blockSize = reader.ReadInt32();
			int freeBlockMapIndex = reader.ReadInt32();
			int blockCount = reader.ReadInt32();
			int directoryStreamLength = reader.ReadInt32();
			int reserved = reader.ReadInt32();
			int hintBlock = reader.ReadInt32();

			// read the free block maps
			int freeBlockOffset = blockSize * hintBlock;
			reader.BaseStream.Seek(freeBlockOffset, SeekOrigin.Begin);
			int directoryBlock = reader.ReadInt32();

			// read the stream directory
			int streamDirectoryOffset = blockSize * directoryBlock;
			reader.BaseStream.Seek(streamDirectoryOffset, SeekOrigin.Begin);
			int streamCount = reader.ReadInt32();

			int[] streamLengths = new int[streamCount];
			for (int i = 0; i < streamCount; ++i)
			{
				streamLengths[i] = reader.ReadInt32();
			}

			// calculate the blocks for each size
			List<List<int>> blocks = new List<List<int>>();
			for (int i = 0; i < streamCount; ++i)
			{
				int extraBlocks = streamLengths[i] / blockSize;

				List<int> blocklist = new List<int>();
				if (streamLengths[i] % blockSize > 0)
				{
					blocklist.Add(reader.ReadInt32());
				}

				for (int j = 0; j < extraBlocks; ++j)
				{
					blocklist.Add(reader.ReadInt32());
				}

				blocks.Add(blocklist);
			}

			int? pdbVersion = null;
			int? pdbSignature = null;
			int? pdbAge = null;
			Guid? pdbGuid = null;

			bool infoFound = false;
			// start reading each stream
			for (int i = 0; i < streamCount; ++i)
			{
				if (infoFound)
				{
					break;
				}
				byte[] streamBuffer = new byte[streamLengths[i]];
				int destinationIndex = 0;

				List<int> streamBlocks = blocks[i];
				for (int index = 0; index < streamBlocks.Count - 1; ++index)
				{
					reader.BaseStream.Seek(streamBlocks[index] * blockSize, SeekOrigin.Begin);
					byte[] buf = reader.ReadBytes(blockSize);
					
					Array.Copy(buf, 0, streamBuffer, destinationIndex, buf.Length);
					destinationIndex += blockSize;
				}

				// all blocks are combined into a contiguous stream
				MemoryStream ms = new MemoryStream(streamBuffer);
				using BinaryReader streamReader = new BinaryReader(ms);
				switch (i)
				{
					case 1:
						// PdbInfoStream
						pdbVersion = streamReader.ReadInt32();
						pdbSignature = streamReader.ReadInt32();
						pdbAge = streamReader.ReadInt32();
						
						pdbGuid = new Guid(streamReader.ReadBytes(16));

						infoFound = true;
						break;

					case 3:
						// DBIStream
						break;
					default:
						break;
				}
			}

			if (pdbGuid == null || pdbAge == null)
			{
				throw new Exception("No PDBInfoStream found, was this really a pdb?");
			}

			return (pdbGuid.Value.ToString("N").ToUpperInvariant(), pdbAge.Value);
		}
	}

	public class PutSymbolResponse
	{
		public PutSymbolResponse(string moduleName, string pdbIdentifier, int pdbAge, IoHash pdbPayload)
		{
			ModuleName = moduleName;
			PdbIdentifier = pdbIdentifier;
			PdbAge = pdbAge;
			PdbPayload = pdbPayload;
		}

		public string ModuleName { get; set; }
		public string PdbIdentifier { get; set; }
		public int PdbAge { get; set; }
		public IoHash PdbPayload { get; set; }
	}
}
