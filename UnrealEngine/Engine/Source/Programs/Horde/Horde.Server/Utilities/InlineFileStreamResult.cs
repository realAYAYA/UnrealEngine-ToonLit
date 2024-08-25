// Copyright Epic Games, Inc. All Rights Reserved.

using System.Net.Mime;
using System.Threading.Tasks;
using Microsoft.AspNetCore.Mvc;

namespace Horde.Server.Utilities
{
	/// <summary>
	/// Class to return a file stream without the "content-disposition: attachment" header
	/// </summary>
	class InlineFileStreamResult : FileStreamResult
	{
		/// <summary>
		/// The suggested download filename
		/// </summary>
		readonly string _fileName;

		public InlineFileStreamResult(System.IO.Stream stream, string mimeType, string fileName)
			: base(stream, mimeType)
		{
			_fileName = fileName;
		}

		/// <inheritdoc/>
		public override Task ExecuteResultAsync(ActionContext context)
		{
			ContentDisposition contentDisposition = new ContentDisposition();
			contentDisposition.Inline = true;
			contentDisposition.FileName = _fileName;
			context.HttpContext.Response.Headers["Content-Disposition"] = contentDisposition.ToString();

			return base.ExecuteResultAsync(context);
		}
	}
}
