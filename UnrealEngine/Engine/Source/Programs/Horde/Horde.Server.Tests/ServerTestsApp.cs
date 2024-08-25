// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Threading.Tasks;

namespace Horde.Server.Tests
{
	class ServerTestsApp
	{
		/// <summary>
		/// Entry point
		/// </summary>
		public static Task Main()
		{
			// This custom entry point allows running custom code for debugging/profiling test runs.
			Console.WriteLine("Run tests through the MSTest framework.");
			return Task.CompletedTask;
		}
	}
}
