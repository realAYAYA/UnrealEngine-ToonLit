// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using EpicGames.Core;

namespace UnrealBuildTool
{
	/// <summary>
	/// Tracks dependencies for transforming the current preprocessor state
	/// </summary>
	class PreprocessorTransform
	{
		/// <summary>
		/// Whether the preprocessor must be in an active state after the branches below are popped. This cannot be treated as a 'pop', because it might be the global scope.
		/// </summary>
		public bool? bRequireTopmostActive;

		/// <summary>
		/// List of branch states that will be popped from the stack
		/// </summary>
		public List<PreprocessorBranch> RequiredBranches;

		/// <summary>
		/// Map of macro name to their required value
		/// </summary>
		public Dictionary<Identifier, PreprocessorMacro?> RequiredMacros;

		/// <summary>
		/// List of new branches that will be pushed onto the stack
		/// </summary>
		public List<PreprocessorBranch> NewBranches;

		/// <summary>
		/// List of new macros that will be defined
		/// </summary>
		public Dictionary<Identifier, PreprocessorMacro?> NewMacros;

		/// <summary>
		/// This fragment contains a pragma once directive
		/// </summary>
		public bool bHasPragmaOnce;

		/// <summary>
		/// Constructor
		/// </summary>
		public PreprocessorTransform()
		{
			RequiredBranches = new List<PreprocessorBranch>();
			RequiredMacros = new Dictionary<Identifier, PreprocessorMacro?>();
			NewBranches = new List<PreprocessorBranch>();
			NewMacros = new Dictionary<Identifier, PreprocessorMacro?>();
		}

		/// <summary>
		/// Reads a transform from disk
		/// </summary>
		/// <param name="Reader">Archive to serialize from</param>
		public PreprocessorTransform(BinaryArchiveReader Reader)
		{
			if(Reader.ReadBool())
			{
				bRequireTopmostActive = Reader.ReadBool();
			}

			RequiredBranches = Reader.ReadList(() => (PreprocessorBranch)Reader.ReadByte())!;
			RequiredMacros = Reader.ReadDictionary(() => Reader.ReadIdentifier(), () => (PreprocessorMacro?)Reader.ReadObjectReference(() => new PreprocessorMacro(Reader)))!;
			NewBranches = Reader.ReadList(() => (PreprocessorBranch)Reader.ReadByte())!;
			NewMacros = Reader.ReadDictionary(() => Reader.ReadIdentifier(), () => (PreprocessorMacro?)Reader.ReadObjectReference(() => new PreprocessorMacro(Reader)))!;
			bHasPragmaOnce = Reader.ReadBool();
		}

		/// <summary>
		/// Writes a transform to disk
		/// </summary>
		/// <param name="Writer">Archive to serialize to</param>
		public void Write(BinaryArchiveWriter Writer)
		{
			Writer.WriteBool(bRequireTopmostActive.HasValue);
			if(bRequireTopmostActive.HasValue)
			{
				Writer.WriteBool(bRequireTopmostActive.Value);
			}

			Writer.WriteList(RequiredBranches, x => Writer.WriteByte((byte)x));
			Writer.WriteDictionary(RequiredMacros, k => Writer.WriteIdentifier(k), v => Writer.WriteObjectReference(v, () => v!.Write(Writer)));
			Writer.WriteList(NewBranches, x => Writer.WriteByte((byte)x));
			Writer.WriteDictionary(NewMacros, k => Writer.WriteIdentifier(k), v => Writer.WriteObjectReference(v, () => v!.Write(Writer)));
			Writer.WriteBool(bHasPragmaOnce);
		}
	}
}
