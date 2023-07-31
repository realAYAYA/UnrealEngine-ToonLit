// Copyright Epic Games, Inc. All Rights Reserved.

// djh using System.IO;
using UnrealBuildTool;
using System.Diagnostics;
using System.Collections.Generic;
using System.IO; // This is for the directory check.


public class UVAtlas : ModuleRules
{
    public UVAtlas(ReadOnlyTargetRules Target) : base(Target)
    {
        // We are just setting up paths for pre-compiled binaries.
        Type = ModuleType.External;

        // For boost:: and TBB:: code
        bEnableUndefinedIdentifierWarnings = false;
        bUseRTTI = true;

        // For testing during developement 
        bool bDebugPaths = true;

        // Only building for Windows

        if (Target.Platform == UnrealTargetPlatform.Win64)
        {
            string UVAtlasHeaderDir = ModuleDirectory + "/UVAtlasCode/UVAtlas/inc";


            if (bDebugPaths)
            {
                if (!Directory.Exists(UVAtlasHeaderDir))
                {
                    string Err = string.Format("UVAtlas SDK not found in {0}", UVAtlasHeaderDir);
                    System.Console.WriteLine(Err);
                    throw new BuildException(Err);
                }
            }

            PublicIncludePaths.Add(UVAtlasHeaderDir);

            // Construct the OpenVDB directory name
            string LibDirName = ModuleDirectory + "/Deploy/";
            LibDirName += "VS" + Target.WindowsPlatform.GetVisualStudioCompilerVersionName() + "/lib/x64/";

            bool bDebug = (Target.Configuration == UnrealTargetConfiguration.Debug && Target.bDebugBuildsActuallyUseDebugCRT);

            if (bDebug)
            {
                LibDirName += "Debug/";
            }
            else
            {
                LibDirName += "Release/";
            }

            if (bDebugPaths)
            {
                // Look for the file
                if (!File.Exists(LibDirName + "UVAtlas.lib"))
                {
                    string Err = string.Format("UVAtlas.lib not found in {0}", LibDirName);
                    System.Console.WriteLine(Err);
                    throw new BuildException(Err);
                }
            }
            PublicAdditionalLibraries.Add(Path.Combine(LibDirName, "UVAtlas.lib"));
        }
        else
        {
            string Err = "Wrong build env!";
            System.Console.WriteLine(Err);
            throw new BuildException(Err);
        }
    }
}