// Copyright Epic Games, Inc. All Rights Reserved.

using System.Collections.Generic;
using UnrealBuildBase;

namespace UnrealBuildTool
{
	/// <summary>
	/// Information about a PCH instance
	/// </summary>
	class PrecompiledHeaderInstance
	{
		/// <summary>
		/// The file to include to use this shared PCH
		/// </summary>
		public FileItem HeaderFile;

		/// <summary>
		/// The definitions file
		/// </summary>
		public FileItem DefinitionsFile;

		/// <summary>
		/// The compile environment for this shared PCH
		/// </summary>
		public CppCompileEnvironment CompileEnvironment;

		/// <summary>
		/// The output files for the shared PCH
		/// </summary>
		public CPPOutput Output;

		/// <summary>
		/// List of modules using this instance
		/// </summary>
		public HashSet<UEBuildModuleCPP> Modules = new HashSet<UEBuildModuleCPP>();

		/// <summary>
		/// These are definitions that are immutable and should never be #undef. There are a few exceptions and we make sure those are not ending up in this list
		/// </summary>
		public HashSet<string> ImmutableDefinitions;

		/// <summary>
		/// Parent PCH instance used in PCH chaining
		/// </summary>
		public PrecompiledHeaderInstance? ParentPCHInstance;

		/// <summary>
		/// Dictionary of definitions use in the CppCompileEnvironment
		/// </summary>
		public Dictionary<string, string>? DefinitionsDictionary;

		/// <summary>
		/// Constructor
		/// </summary>
		public PrecompiledHeaderInstance(FileItem HeaderFile, FileItem DefinitionsFile, CppCompileEnvironment CompileEnvironment, CPPOutput Output, HashSet<string> ImmutableDefinitions)
		{
			this.HeaderFile = HeaderFile;
			this.DefinitionsFile = DefinitionsFile;
			this.CompileEnvironment = CompileEnvironment;
			this.Output = Output;
			this.ImmutableDefinitions = ImmutableDefinitions;
		}

		/// <summary>
		/// Return a string representation of this object for debugging
		/// </summary>
		/// <returns>String representation of the object</returns>
		public override string ToString()
		{
			return HeaderFile.Location.GetFileName();
		}
	}
}
