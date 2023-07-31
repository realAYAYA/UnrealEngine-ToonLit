// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Reflection;
using System.IO;

namespace Horde.Storage.Implementation;

public static class PathUtil
{
    public static string ResolvePath(string path)
    {
        return Environment.ExpandEnvironmentVariables(path)
            .Replace("$(ExecutableLocation)", Path.GetDirectoryName(Assembly.GetEntryAssembly()!.Location), StringComparison.OrdinalIgnoreCase);
    }
}
