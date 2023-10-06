// Copyright Epic Games, Inc. All Rights Reserved.

// Default AssemlbyInfo.cs used when developping the plugin with the DatasmithRhino6.sln 
// for fast iteration. Is not used to ship the plugin, please change GeneratedAssemblyInfo.cs.template instead.

using System.Reflection;
using System.Runtime.CompilerServices;
using System.Runtime.InteropServices;
using Rhino.PlugIns;

// Plug-in Description Attributes - all of these are optional.
// These will show in Rhino's option dialog, in the tab Plug-ins.
[assembly: PlugInDescription(DescriptionType.Address, "-")]
[assembly: PlugInDescription(DescriptionType.Country, "-")]
[assembly: PlugInDescription(DescriptionType.Email, "-")]
[assembly: PlugInDescription(DescriptionType.Phone, "-")]
[assembly: PlugInDescription(DescriptionType.Fax, "-")]
[assembly: PlugInDescription(DescriptionType.Organization, "Epic Games, Inc.")]
[assembly: PlugInDescription(DescriptionType.UpdateUrl, "-")]
[assembly: PlugInDescription(DescriptionType.WebSite, "-")]


// Icons should be Windows .ico files and contain 32-bit images in the following sizes: 16, 24, 32, 48, and 256.
// This is a Rhino 6-only description.
[assembly: PlugInDescription(DescriptionType.Icon, "DatasmithRhino6.EmbeddedResources.UnrealEngine.ico")]

// General Information about an assembly is controlled through the following 
// set of attributes. Change these attribute values to modify the information
// associated with an assembly.
[assembly: AssemblyTitle("Datasmith Exporter")]

// This will be used also for the plug-in description.
[assembly: AssemblyDescription("Export a Rhino 3D View to Unreal Datasmith")]

[assembly: AssemblyConfiguration("")]
[assembly: AssemblyCompany("Epic Games, Inc.")]
[assembly: AssemblyProduct("Datasmith Exporter Add-In for Rhino")]
[assembly: AssemblyCopyright("Copyright Epic Games, Inc. All Rights Reserved.")]
[assembly: AssemblyTrademark("")]
[assembly: AssemblyCulture("")]
[assembly: System.Resources.NeutralResourcesLanguage("en-US")]

// Setting ComVisible to false makes the types in this assembly not visible 
// to COM components.  If you need to access a type in this assembly from 
// COM, set the ComVisible attribute to true on that type.
[assembly: ComVisible(false)]

// The following GUID is for the ID of the typelib if this project is exposed to COM
[assembly: Guid("d1fdc795-b334-4933-b680-088119cdc6bb")] // This will also be the Guid of the Rhino plug-in

// Version information for an assembly consists of the following four values:
//
//      Major Version
//      Minor Version 
//      Build Number
//      Revision
//
// You can specify all the values or you can default the Build and Revision Numbers 
// by using the '*' as shown below:
// [assembly: AssemblyVersion("1.0.*")]

// Make compatible with Rhino Installer Engine
[assembly: AssemblyInformationalVersion("2")]
