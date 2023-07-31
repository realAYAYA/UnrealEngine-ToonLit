// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Concurrent;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using EpicGames.Core;
using UnrealBuildBase;

namespace UnrealBuildTool
{
	/// <summary>
	/// Encapsulates metadata for a C++ source file
	/// </summary>
	[Serializable]
	class SourceFile
	{
		/// <summary>
		/// The corresponding file item
		/// </summary>
		public readonly FileItem File;

		/// <summary>
		/// Hash of the source file data
		/// </summary>
		public readonly ContentHash Hash;

		/// <summary>
		/// Last write time of this file when constructed
		/// </summary>
		public readonly long LastWriteTimeUtc;

		/// <summary>
		/// Markup dividing up this source file
		/// </summary>
		public readonly SourceFileMarkup[] Markup;

		/// <summary>
		/// Macro which is defined as part of the header guard. If this macro is defined, we don't need to include the header more than once.
		/// </summary>
		public Identifier? HeaderGuardMacro;

		/// <summary>
		/// Parsed fragments in this file. Only set for files which are being optimized.
		/// </summary>
		public SourceFileFragment[] Fragments;

		/// <summary>
		/// Gets the location of this file
		/// </summary>
		public FileReference Location
		{
			get { return File.Location; }
		}

		/// <summary>
		/// Construct a SourceFile from a file on disk
		/// </summary>
		/// <param name="File">Location of the file</param>
		public SourceFile(FileItem File)
			: this(File, ReadFileWithNullTerminator(File.Location))
		{
			this.LastWriteTimeUtc = File.LastWriteTimeUtc.Ticks;
		}

		/// <summary>
		/// Construct a SourceFile from a raw byte array
		/// </summary>
		/// <param name="File">Location of the file</param>
		/// <param name="Data">Contents of the file</param>
		public SourceFile(FileItem File, byte[] Data)
		{
			this.File = File;

			// Compute the hash
			Hash = ContentHash.MD5(Data);

			// Read the preprocessor markup
			TokenReader Reader = new TokenReader(Data);
			Markup = SourceFileMarkup.Parse(Reader);

			// Try to parse a header guard from this file
			ParseHeaderGuard();

			// Create the fragments
			List<SourceFileFragment> FragmentsList = new List<SourceFileFragment>();
			for(int MaxIdx = 0; MaxIdx < Markup.Length; MaxIdx++)
			{
				if(Markup[MaxIdx].Type != SourceFileMarkupType.Include)
				{
					int MinIdx = MaxIdx++;
					while(MaxIdx < Markup.Length && Markup[MaxIdx].Type != SourceFileMarkupType.Include)
					{
						MaxIdx++;
					}
					FragmentsList.Add(new SourceFileFragment(this, MinIdx, MaxIdx));
				}
			}
			Fragments = FragmentsList.ToArray();
		}

		/// <summary>
		/// Read a source file from a binary reader
		/// </summary>
		/// <param name="Reader">The reader to serialize from</param>
		public SourceFile(BinaryArchiveReader Reader)
		{
			File = Reader.ReadFileItem()!;
			Hash = Reader.ReadContentHash()!;
			LastWriteTimeUtc = Reader.ReadLong();
			Markup = Reader.ReadArray(() => new SourceFileMarkup(Reader))!;
			HeaderGuardMacro = Reader.ReadIdentifier();
			Fragments = Reader.ReadArray(() => new SourceFileFragment(this, Reader))!;
		}

		/// <summary>
		/// Read a source file from a binary reader
		/// </summary>
		/// <param name="Writer">The reader to serialize to</param>
		public void Write(BinaryArchiveWriter Writer)
		{
			Writer.WriteFileItem(File);
			Writer.WriteContentHash(Hash);
			Writer.WriteLong(LastWriteTimeUtc);
			Writer.WriteArray(Markup, x => x.Write(Writer));
			Writer.WriteIdentifier(HeaderGuardMacro);
			Writer.WriteArray(Fragments, x => x.Write(Writer));
		}

		/// <summary>
		/// Reads the contents of a file into a buffer, with a null terminator
		/// </summary>
		/// <param name="Location">Location of the file</param>
		/// <returns>Array of bytes, with a null terminator</returns>
		public static byte[] ReadFileWithNullTerminator(FileReference Location)
		{
			using(FileStream Stream = FileReference.Open(Location, FileMode.Open, FileAccess.Read, FileShare.Read))
			{
				int Length = (int)Stream.Length;

				byte[] Data = new byte[Length + 1];
				Stream.Read(Data, 0, Length);

				return ConvertToNullTerminatedUtf8(Data, Length);
			}
		}

		/// <summary>
		/// Converts a buffer from a given encoding to UTF8, appending a null terminator
		/// </summary>
		/// <param name="Data">The source data</param>
		/// <param name="Length">Length of the data in the source buffer</param>
		/// <returns>Array of utf-8 encoded characters, with a null terminator</returns>
		public static byte[] ConvertToNullTerminatedUtf8(byte[] Data, int Length)
		{
			if(Length >= 2)
			{
				if(Data[0] == 0xfe && Data[1] == 0xff)
				{
					return ConvertToNullTerminatedUtf8(Encoding.BigEndianUnicode, Data, Length);
				}
				else if(Data[0] == 0xff && Data[1] == 0xfe)
				{
					return ConvertToNullTerminatedUtf8(Encoding.Unicode, Data, Length);
				}
			}
			return Data;
		}

		/// <summary>
		/// Converts a buffer from a given encoding to UTF8, appending a null terminator
		/// </summary>
		/// <param name="SourceEncoding">The encoding to convert from</param>
		/// <param name="Data">The source data</param>
		/// <param name="Length">Length of the data in the source buffer</param>
		/// <returns>Array of utf-8 encoded characters, with a null terminator</returns>
		public static byte[] ConvertToNullTerminatedUtf8(Encoding SourceEncoding, byte[] Data, int Length)
		{
			byte[] NewBytes = Data;
			if(!(SourceEncoding is UTF8Encoding))
			{
				char[] Chars = SourceEncoding.GetChars(Data, 0, Length);

				int NewLength = Encoding.UTF8.GetByteCount(Chars);
				NewBytes = new byte[NewLength + 1];

				Encoding.UTF8.GetBytes(Chars, 0, Chars.Length, NewBytes, 0);
			}
			return NewBytes;
		}

		/// <summary>
		/// Checks whether the markup matches a C-style header guard
		/// </summary>
		void ParseHeaderGuard()
		{
			// Make sure there are enough markup entries in this list
			if(Markup.Length >= 3)
			{
				int MinIdx = 0;

				// Get the define used for the header guard
				Identifier? PossibleHeaderGuardMacro;
				if(Markup[MinIdx].Type == SourceFileMarkupType.Ifndef && Markup[MinIdx].Tokens!.Count == 1 && Markup[MinIdx].Tokens![0].Type == TokenType.Identifier)
				{
					PossibleHeaderGuardMacro = Markup[MinIdx].Tokens![0].Identifier!;
				}
				else if(Markup[MinIdx].Type == SourceFileMarkupType.If && Markup[MinIdx].Tokens!.Count == 3 && Markup[MinIdx].Tokens![0].Type == TokenType.LogicalNot && Markup[MinIdx].Tokens![1].Identifier == Identifiers.Defined && Markup[MinIdx].Tokens![2].Type == TokenType.Identifier)
				{
					PossibleHeaderGuardMacro = Markup[MinIdx].Tokens![2].Identifier!;
				}
				else
				{
					PossibleHeaderGuardMacro = null;
				}

				// Check it starts with an #if or #ifdef
				if (Markup[MinIdx + 1].Type == SourceFileMarkupType.Define && Markup[MinIdx + 1].Tokens!.Count == 1 && Markup[MinIdx + 1].Tokens![0].Identifier == PossibleHeaderGuardMacro)
				{
					// Find the point at which the include depth goes back to zero
					int MaxIdx = MinIdx + 1;
					for (int Depth = 1; MaxIdx < Markup.Length; MaxIdx++)
					{
						Depth += Markup[MaxIdx].GetConditionDepthDelta();
						if (Depth == 0)
						{
							break;
						}
					}

					// Check it matched the end of the file
					if (MaxIdx == Markup.Length - 1)
					{
						HeaderGuardMacro = PossibleHeaderGuardMacro;
					}
				}
			}
		}

		/// <summary>
		/// Converts a source file to a string for debugging
		/// </summary>
		/// <returns>The full path to the file</returns>
		public override string ToString()
		{
			return File.ToString();
		}
	}
}
