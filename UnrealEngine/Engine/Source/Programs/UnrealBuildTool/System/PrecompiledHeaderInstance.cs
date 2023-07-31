// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
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
		public List<UEBuildModuleCPP> Modules = new List<UEBuildModuleCPP>();

		/// <summary>
		/// Constructor
		/// </summary>
		public PrecompiledHeaderInstance(FileItem HeaderFile, CppCompileEnvironment CompileEnvironment, CPPOutput Output)
		{
			this.HeaderFile = HeaderFile;
			this.CompileEnvironment = CompileEnvironment;
			this.Output = Output;
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
