// Copyright Epic Games, Inc. All Rights Reserved.

using Microsoft.Extensions.Hosting;

namespace Jupiter
{
	// ReSharper disable once ClassNeverInstantiated.Global
	public static class Program
	{
		public static int Main(string[] args)
		{
			return BaseProgram<JupiterStartup>.BaseMain(args);
		}

		public static IHostBuilder CreateHostBuilder(string[] Args)
		{
			return BaseProgram<JupiterStartup>.CreateHostBuilder(Args);
		}
	}
}
