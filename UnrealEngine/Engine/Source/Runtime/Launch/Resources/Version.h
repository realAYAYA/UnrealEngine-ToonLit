// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

//=============================================================================================================================================================
// This file defines constants used for versioning packages, modules, and various subsystems in UE. In general, it should not be necessary to include this
// file and access these values directly - they are all wrapped behind the higher-level abstractions in FEngineVersion and the FApp class.
//
// The following concepts are used for versioning in UE:
//
//  -  The *engine version* defines the explicit major/minor/patch version of the engine, plus the changelist and branch name that it was built from. The 
//     changelist is assumed to be a monotonically increasing number in the current branch, and is used both as a unique identifier and to infer that one engine
//     was later than another. Tagged property serialization in UE is tolerant to properties being added or removed, so we always want to prevent an older 
//     build of the engine loading assets created with a newer build, discarding properties which have recently been added, and silently losing data
//     when the asset is saved out. The changelist allows ordering versions in such cases. The engine version is encapsulated by the FEngineVersion class, of 
//     which there are two commonly referenced instances:
//
//         FEngineVersion::Current()        normally uses ENGINE_CURRENT_CL_VERSION for the changelist component, and indicates the code the engine was built 
//                                          from. This is typically only used for diagnostic and display purposes.
//
//         FEngineVersion::CompatibleWith() normally uses ENGINE_COMPATIBLE_CL_VERSION for the changelist component and '0' for the patch component, and 
//                                          indicates the baseline version of the engine that this build maintains strict binary compatibility with. 
//                                          By default, this compatibility extends to assets, executable modules, and any network data transmitted between 
//                                          two builds, and is used when creating patches and hotfixes that can be used interchangeably with another build.
//                                          This should be used for versioning in the majority of cases in the engine.
//
//     Both the ENGINE_CURRENT_CL_VERSION and ENGINE_COMPATIBLE_CL_VERSION macros can be updated systemically by build systems using the UpdateLocalVersion 
//     AutomationTool command (as well as the ENGINE_IS_LICENSEE_VERSION and BRANCH_NAME macros).
//
//  -  The *object version* (aka serialization version) is as a monotonically incrementing (but manually updated) integer, and is used to write one-way 
//     upgrade code in custom UObject serialization functions. It is set by the enum in ObjectVersion.h, and is global to the whole engine. This version number 
//     is saved as a raw integer value in package headers, so it cannot be safely reordered or merged between branches. It should ONLY be updated 
//     by Epic, otherwise future engine merges may corrupt content.
//
//  -  The *licensee object version* is provided for licensees to create their own one-way upgrade paths akin to the regular object version. Epic will never
//     add entries to this enumeration. It is defined by the enum in ObjectVersion.h
//
//  -  Any number of *custom object version* objects may be registered to create orthogonal incrementing version numbers similar to the object version and 
//     licensee version enums (see FCustomVersion). Each one is registered with a GUID, ensuring uniqueness and allowing the FArchive to quickly store and 
//     retrieve them without any context of what they represent. Custom versions may be created for individual projects, subsystems, or branches.
//
//  -  The *build version* is an opaque string specific to the product being built, and should be used for identifying the current application 
//     (as opposed to distinct applications built with the same engine version). It is set by the BUILD_VERSION macro, which can be updated using the 
//     UpdateLocalVersion AutomationTool command.
//
//  -  The *network version* and *replay version* are used for versioning the network and replay subsystems, and default to the compatible engine version.
//
//  -  The *engine association* in a .uproject file often takes the appearance of a version number for launcher-installed binary UE releases, but may be 
//     other identifiers as well. See ProjectDescriptor.h for a description of how this technique works.
//
// Constants in this file are updated by AutomationTool and UnrealGameSync. Be careful when changing formatting for the submitted version of this file that 
// these tools can still parse it.
//=============================================================================================================================================================

// These numbers define the banner UE version, and are the most significant numbers when ordering two engine versions (that is, a 5.12.* version is always 
// newer than a 5.11.* version, regardless of the changelist that it was built with)
// When updating these, also update the static_assert below
#define ENGINE_MAJOR_VERSION	5
#define ENGINE_MINOR_VERSION	4
#define ENGINE_PATCH_VERSION	4

// If this static_assert fires then Version.h has been updated without updating this code.
// This line exists to cause conflicts when merging Version.h between streams so if one stream updates
// the minor version while another updates the patch, you do not get a silently combined version that
// was unintended.
#ifdef __cplusplus
static_assert(ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION == 4 && ENGINE_PATCH_VERSION == 4); //-V501
#endif // __cplusplus

// Macros for encoding strings
#define VERSION_TEXT(x) TEXT(x)
#define VERSION_STRINGIFY_2(x) VERSION_TEXT(#x)
#define VERSION_STRINGIFY(x) VERSION_STRINGIFY_2(x)

// Various strings used for engine resources
#define EPIC_COMPANY_NAME  "Epic Games, Inc."
#define EPIC_COPYRIGHT_STRING "Copyright Epic Games, Inc. All Rights Reserved."
#define EPIC_PRODUCT_NAME "Unreal Engine"
#define EPIC_PRODUCT_IDENTIFIER "UnrealEngine"

#if defined(BUILT_FROM_CHANGELIST) && defined(BRANCH_NAME)
#define ENGINE_VERSION_STRING \
	VERSION_STRINGIFY(ENGINE_MAJOR_VERSION) \
	VERSION_TEXT(".") \
	VERSION_STRINGIFY(ENGINE_MINOR_VERSION) \
	VERSION_TEXT(".") \
	VERSION_STRINGIFY(ENGINE_PATCH_VERSION) \
	VERSION_TEXT("-") \
	VERSION_STRINGIFY(BUILT_FROM_CHANGELIST) \
	VERSION_TEXT("+") \
	VERSION_TEXT(BRANCH_NAME)
#else
#define ENGINE_VERSION_STRING \
	VERSION_STRINGIFY(ENGINE_MAJOR_VERSION) \
	VERSION_TEXT(".") \
	VERSION_STRINGIFY(ENGINE_MINOR_VERSION) \
	VERSION_TEXT(".") \
	VERSION_STRINGIFY(ENGINE_PATCH_VERSION)
#endif
