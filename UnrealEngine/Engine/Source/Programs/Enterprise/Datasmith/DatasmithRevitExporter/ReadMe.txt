// Copyright Epic Games, Inc. All Rights Reserved.

Since the Datasmith Revit Exporter is programmed in C#, the standard build process just runs a post-build step to build
in batch the release configuration of Visual Studio C# project file DatasmithRevit<year>\DatasmithRevit<year>.csproj.

To develop the C# code, it is preferable to directly open DatasmithRevit<year>\DatasmithRevit<year>.csproj in Visual Studio.

The Visual Studio C# project automatically includes all the Datasmith Revit Exporter C# files from project directory Private
and all the Datasmith Facade C# files from directory Enterprise\Binaries\Win64\DatasmithFacadeCSharp\Public.

Outside of Epic Games, environment variable Revit_<year>_API must be set to the Revit API third party directory
on the developer's workstation.

Inside of Epic Games, when opening the Visual Studio C# project directly, environment variable Revit_<year>_API must be set
to the developer's Perforce workspace directory Enterprise\Source\ThirdParty\NotForLicensees\Revit\Revit_<year>_API.

At this time, <year> can be in the range [2018..2022].
