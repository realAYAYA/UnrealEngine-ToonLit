// Copyright Epic Games, Inc. All Rights Reserved.

// djh using System.IO;
using UnrealBuildTool;
using System.Diagnostics;
using System.Collections.Generic;
using System.IO; // This is for the directory check.


public class DirectXMesh : ModuleRules
{
    public DirectXMesh(ReadOnlyTargetRules Target) : base(Target)
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
            string HeaderDir = ModuleDirectory + "/DirectXMeshCode";


            if (bDebugPaths)
            {
                if (!Directory.Exists(HeaderDir))
                {
                    string Err = string.Format("UVAtlas SDK not found in {0}", HeaderDir);
                    System.Console.WriteLine(Err);
                    throw new BuildException(Err);
                }
            }

            PublicIncludePaths.Add(HeaderDir);

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
                if (!File.Exists(LibDirName + "DirectXMesh.lib"))
                {
                    string Err = string.Format("DirectXMesh.lib not found in {0}", LibDirName);
                    System.Console.WriteLine(Err);
                    throw new BuildException(Err);
                }
            }
            PublicAdditionalLibraries.Add(LibDirName + "DirectXMesh.lib");
        }
        else
        {
            string Err = "Wrong build env!";
            System.Console.WriteLine(Err);
            throw new BuildException(Err);
        }
    }
}