// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using Microsoft.AspNetCore.Authorization;
using Microsoft.AspNetCore.Mvc;

namespace Jupiter.Controllers
{
	[ApiController]
	[Route("api/v1/c/_debug")]
	public class DebugController : Controller
	{
		/// <summary>
		/// Return bytes of specified length with auth, used for testing only
		/// </summary>
		/// <returns></returns>
		[HttpGet("getBytes")]
		[Authorize]
		public IActionResult GetBytes([FromQuery] int length = 1)
		{
			return GenerateByteResponse(length);
		}
		
		/// <summary>
		/// Return bytes of specified length without auth, used for testing only
		/// </summary>
		/// <returns></returns>
		[HttpGet("getBytesWithoutAuth")]
		public IActionResult GetBytesWithoutAuth([FromQuery] int length = 1)
		{
			return GenerateByteResponse(length);
		}

		private FileContentResult GenerateByteResponse(int length)
		{
			byte[] generatedData = new byte[length];
			Array.Fill(generatedData, (byte)'J');
			return File(generatedData, "application/octet-stream");
		}
	}
}
