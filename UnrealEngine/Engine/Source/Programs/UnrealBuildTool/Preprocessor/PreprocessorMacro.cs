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
	/// Stores information about a defined preprocessor macro
	/// </summary>
	class PreprocessorMacro
	{
		/// <summary>
		/// Name of the macro
		/// </summary>
		public Identifier Name;

		/// <summary>
		/// Parameter names for the macro. The '...' placeholder is represented by the __VA_ARGS__ string.
		/// </summary>
		public List<Identifier>? Parameters;

		/// <summary>
		/// Raw list of tokens for this macro
		/// </summary>
		public List<Token> Tokens;

		/// <summary>
		/// Construct a preprocessor macro
		/// </summary>
		/// <param name="Name">Name of the macro</param>
		/// <param name="Parameters">Parameter list for the macro. Should be null for object macros. Ownership of this list is transferred.</param>
		/// <param name="Tokens">Tokens for the macro. Ownership of this list is transferred.</param>
		public PreprocessorMacro(Identifier Name, List<Identifier>? Parameters, List<Token> Tokens)
		{
			this.Name = Name;
			this.Parameters = Parameters;
			this.Tokens = Tokens;
		}

		/// <summary>
		/// Read a macro from a binary archive
		/// </summary>
		/// <param name="Reader">Reader to serialize from</param>
		public PreprocessorMacro(BinaryArchiveReader Reader)
		{
			Name = Reader.ReadIdentifier();
			Parameters = Reader.ReadList(() => Reader.ReadIdentifier());
			Tokens = Reader.ReadList(() => Reader.ReadToken())!;
		}

		/// <summary>
		/// Write a macro to a binary archive
		/// </summary>
		/// <param name="Writer">Writer to serialize to</param>
		public void Write(BinaryArchiveWriter Writer)
		{
			Writer.WriteIdentifier(Name);
			Writer.WriteList(Parameters, x => Writer.WriteIdentifier(x));
			Writer.WriteList(Tokens, x => Writer.WriteToken(x));
		}

		/// <summary>
		/// Finds the index of a parameter in the parameter list
		/// </summary>
		/// <param name="Parameter">Parameter name to look for</param>
		/// <returns>Index of the parameter, or -1 if it's not found.</returns>
		public int FindParameterIndex(Identifier Parameter)
		{
			for (int Idx = 0; Idx < Parameters!.Count; Idx++)
			{
				if (Parameters[Idx] == Parameter)
				{
					return Idx;
				}
			}
			return -1;
		}

		/// <summary>
		/// Checks whether this macro definition is equivalent to another macro definition
		/// </summary>
		/// <param name="Other">The macro definition to compare against</param>
		/// <returns>True if the macro definitions are equivalent</returns>
		public bool IsEquivalentTo(PreprocessorMacro Other)
		{
			if(this != Other)
			{
				if(Name != Other.Name || Tokens.Count != Other.Tokens.Count)
				{
					return false;
				}
				if(Parameters != null)
				{
					if(Other.Parameters == null || Other.Parameters.Count != Parameters.Count || !Enumerable.SequenceEqual(Parameters, Other.Parameters))
					{
						return false;
					}
				}
				else
				{
					if(Other.Parameters != null)
					{
						return false;
					}
				}
				if(!Enumerable.SequenceEqual(Tokens, Other.Tokens))
				{
					return false;
				}
			}
			return true;
		}

		/// <summary>
		/// True if the macro is an object macro
		/// </summary>
		public bool IsObjectMacro
		{
			get { return Parameters == null; }
		}

		/// <summary>
		/// True if the macro is a function macro
		/// </summary>
		public bool IsFunctionMacro
		{
			get { return Parameters != null; }
		}
		
		/// <summary>
		/// The number of required parameters. For variadic macros, the last parameter is optional.
		/// </summary>
		public int MinRequiredParameters
		{
			get { return HasVariableArgumentList? (Parameters!.Count - 1) : Parameters!.Count; }
		}

		/// <summary>
		/// True if the macro has a variable argument list
		/// </summary>
		public bool HasVariableArgumentList
		{
			get { return Parameters!.Count > 0 && Parameters[Parameters.Count - 1] == Identifiers.__VA_ARGS__; }
		}

		/// <summary>
		/// Converts this macro to a string for debugging
		/// </summary>
		/// <returns>The tokens in this macro</returns>
		public override string ToString()
		{
			StringBuilder Result = new StringBuilder(Name.ToString());
			if (Parameters != null)
			{
				Result.AppendFormat("({0})", String.Join(", ", Parameters));
			}
			Result.Append("=");
			if (Tokens.Count > 0)
			{
				Result.Append(Tokens[0].Text);
				for (int Idx = 1; Idx < Tokens.Count; Idx++)
				{
					if(Tokens[Idx].HasLeadingSpace)
					{
						Result.Append(" ");
					}
					Result.Append(Tokens[Idx].Text);
				}
			}
			return Result.ToString();
		}
	}
}
