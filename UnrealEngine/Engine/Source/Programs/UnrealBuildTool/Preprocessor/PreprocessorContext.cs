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
	/// Represents a context that the preprocessor is working in. Used to form error messages, and determine things like the __FILE__ and __LINE__ directives.
	/// </summary>
	abstract class PreprocessorContext
	{
		/// <summary>
		/// The outer context
		/// </summary>
		public readonly PreprocessorContext? Outer;

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="Outer">The outer context</param>
		public PreprocessorContext(PreprocessorContext? Outer)
		{
			this.Outer = Outer;
		}
	}

	/// <summary>
	/// Context for a command line argument
	/// </summary>
	class PreprocessorCommandLineContext : PreprocessorContext
	{
		/// <summary>
		/// Constructor
		/// </summary>
		public PreprocessorCommandLineContext()
			: base(null)
		{
		}

		/// <summary>
		/// Formats this context for error messages
		/// </summary>
		/// <returns>String describing this context</returns>
		public override string ToString()
		{
			return "From command line";
		}
	}

	/// <summary>
	/// Represents a context that the preprocessor is reading from
	/// </summary>
	class PreprocessorFileContext : PreprocessorContext
	{
		/// <summary>
		/// The source file being read
		/// </summary>
		public SourceFile SourceFile;

		/// <summary>
		/// The directory containing this file. When searching for included files, MSVC will check this directory.
		/// </summary>
		public DirectoryItem Directory;

		/// <summary>
		/// Index of the current markup object being processed
		/// </summary>
		public int MarkupIdx;

		/// <summary>
		/// Index of the next fragment to be read
		/// </summary>
		public int FragmentIdx;

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="SourceFile">The source file being parsed</param>
		/// <param name="Outer">The outer context</param>
		public PreprocessorFileContext(SourceFile SourceFile, PreprocessorContext? Outer)
			: base(Outer)
		{
			this.SourceFile = SourceFile;
			this.Directory = DirectoryItem.GetItemByDirectoryReference(SourceFile.Location.Directory);
			this.MarkupIdx = 0;
			this.FragmentIdx = 0;
		}

		/// <summary>
		/// Format this file for the debugger, and error messages
		/// </summary>
		/// <returns></returns>
		public override string ToString()
		{
			return String.Format("{0}({1})", SourceFile.Location, SourceFile.Markup[MarkupIdx].LineNumber);
		}
	}
}
