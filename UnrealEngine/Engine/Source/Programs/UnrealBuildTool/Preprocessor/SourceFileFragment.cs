// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using EpicGames.Core;

namespace UnrealBuildTool
{
	/// <summary>
	/// Represents an fragment of a source file consisting of a sequence of includes followed by some arbitrary directives or source code.
	/// </summary>
	[Serializable]
	class SourceFileFragment
	{
		/// <summary>
		/// The file that this fragment is part of
		/// </summary>
		public readonly SourceFile File;

		/// <summary>
		/// Index into the file's markup array of the start of this fragment (inclusive)
		/// </summary>
		public readonly int MarkupMin;

		/// <summary>
		/// Index into the file's markup array of the end of this fragment (exclusive). Set to zero for external files.
		/// </summary>
		public readonly int MarkupMax;

		/// <summary>
		/// List of known transforms for this fragment
		/// </summary>
		public PreprocessorTransform[] Transforms = new PreprocessorTransform[0];

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="File">The source file that this fragment is part of</param>
		/// <param name="MarkupMin">Index into the file's markup array of the start of this fragment (inclusive)</param>
		/// <param name="MarkupMax">Index into the file's markup array of the end of this fragment (exclusive)</param>
		public SourceFileFragment(SourceFile File, int MarkupMin, int MarkupMax)
		{
			this.File = File;
			this.MarkupMin = MarkupMin;
			this.MarkupMax = MarkupMax;
		}

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="File">The source file that this fragment is part of</param>
		/// <param name="Reader">Reader to deserialize this object from</param>
		public SourceFileFragment(SourceFile File, BinaryArchiveReader Reader)
		{
			this.File = File;
			this.MarkupMin = Reader.ReadInt();
			this.MarkupMax = Reader.ReadInt();
			this.Transforms = Reader.ReadArray(() => new PreprocessorTransform(Reader))!;
		}

		/// <summary>
		/// Write this fragment to an archive
		/// </summary>
		/// <param name="Writer">The writer to serialize to</param>
		public void Write(BinaryArchiveWriter Writer)
		{
			Writer.WriteInt(MarkupMin);
			Writer.WriteInt(MarkupMax);
			Writer.WriteArray(Transforms, x => x.Write(Writer));
		}

		/// <summary>
		/// Summarize this fragment for the debugger
		/// </summary>
		/// <returns>String representation of this fragment for the debugger</returns>
		public override string ToString()
		{
			if(MarkupMax == 0)
			{
				return File.ToString();
			}
			else
			{
				return String.Format("{0}: {1}-{2}", File.ToString(), File.Markup[MarkupMin].LineNumber, File.Markup[MarkupMax - 1].LineNumber + 1);
			}
		}
	}
}
