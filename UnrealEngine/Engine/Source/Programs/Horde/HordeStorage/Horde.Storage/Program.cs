// Copyright Epic Games, Inc. All Rights Reserved.

using Jupiter;
using Microsoft.Extensions.Hosting;

namespace Horde.Storage
{
    // ReSharper disable once ClassNeverInstantiated.Global
    public static class Program
    {
        public static int Main(string[] args)
        {
            return BaseProgram<HordeStorageStartup>.BaseMain(args);
        }

        public static IHostBuilder CreateHostBuilder(string[] Args)
        {
            return BaseProgram<HordeStorageStartup>.CreateHostBuilder(Args);
        }
    }
}
