// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using System.Diagnostics;
using System.IO;
using System.Collections;
using System.Collections.Concurrent;
using EpicGames.Core;
using System.Diagnostics.CodeAnalysis;
using UnrealBuildBase;
using Microsoft.Extensions.Logging;

namespace UnrealBuildTool
{
	/// <summary>
	/// Exception class for the preprocessor, which contains the file and position of the code causing an error
	/// </summary>
	class PreprocessorException : Exception
	{
		/// <summary>
		/// The context when the error was encountered
		/// </summary>
		PreprocessorContext? Context;

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="Context">The current preprocesor context</param>
		/// <param name="Format">Format string, to be passed to String.Format</param>
		/// <param name="Args">Optional argument list for the format string</param>
		public PreprocessorException(PreprocessorContext? Context, string Format, params object[] Args)
			: base(String.Format(Format, Args))
		{
			this.Context = Context;
		}
	}

	/// <summary>
	/// Implementation of a C++ preprocessor.
	/// </summary>
	class Preprocessor
	{
		/// <summary>
		/// Type of an include path
		/// </summary>
		public enum IncludePathType
		{
			/// <summary>
			/// Regular include path, enclosed by quotes
			/// </summary>
			Normal,

			/// <summary>
			/// System include path, enclosed by angle brackets
			/// </summary>
			System,
		}

		/// <summary>
		/// Include paths to look in
		/// </summary>
		List<DirectoryItem> IncludeDirectories = new List<DirectoryItem>();

		/// <summary>
        /// Framework paths to look in
        /// </summary>
		List<DirectoryItem> FrameworkDirectories = new List<DirectoryItem>();

		/// <summary>
		/// Set of all included files with the #pragma once directive
		/// </summary>
		HashSet<FileItem> PragmaOnceFiles = new HashSet<FileItem>();

		/// <summary>
		/// Set of any files that has been processed
		/// </summary>
		HashSet<FileItem> ProcessedFiles = new HashSet<FileItem>();

		/// <summary>
		/// The current state of the preprocessor
		/// </summary>
		PreprocessorState State = new PreprocessorState();

		/// <summary>
		/// Predefined token containing the constant "0"
		/// </summary>
		static readonly byte[] ZeroLiteral = Encoding.UTF8.GetBytes("0");

		/// <summary>
		/// Predefined token containing the constant "1"
		/// </summary>
		static readonly byte[] OneLiteral = Encoding.UTF8.GetBytes("1");

		/// <summary>
		/// Value of the __COUNTER__ variable
		/// </summary>
		int Counter;

		/// <summary>
		/// List of files included by the preprocessor
		/// </summary>
		/// <returns>Enumerable of processed files</returns>
		public IEnumerable<FileItem> GetProcessedFiles()
		{
			return ProcessedFiles.AsEnumerable();
		}

		/// <summary>
		/// Default constructor
		/// </summary>
		public Preprocessor()
		{
			DateTime Now = DateTime.Now;
			AddLiteralMacro("__DATE__", TokenType.String, String.Format("\"{0} {1,2} {2}\"", Now.ToString("MMM"), Now.Day, Now.Year));
			AddLiteralMacro("__TIME__", TokenType.String, "\"" + Now.ToString("HH:mm:ss") + "\"");
			AddLiteralMacro("__FILE__", TokenType.String, "\"<unknown>\"");
			AddLiteralMacro("__LINE__", TokenType.Number, "-1");
			AddLiteralMacro("__COUNTER__", TokenType.Number, "-1");
			AddLiteralMacro("CHAR_BIT", TokenType.Number, "8"); // Workaround for #include_next not being supported on Linux for limit.h
		}

		/// <summary>
		/// Determines whether the current preprocessor branch is active
		/// </summary>
		/// <returns>True if the current branch is active</returns>
		public bool IsCurrentBranchActive()
		{
			return State.IsCurrentBranchActive();
		}

		/// <summary>
		/// Defines a macro. May have an optional '=Value' suffix.
		/// </summary>
		/// <param name="Definition">Macro to define</param>
		public void AddDefinition(string Definition)
		{
			List<Token> Tokens = TokenReader.Tokenize(Definition);
			if(Tokens.Count == 0)
			{
				throw new PreprocessorException(null, "Missing macro name");
			}
			if(Tokens[0].Type != TokenType.Identifier)
			{
				throw new PreprocessorException(null, "'{0}' is not a valid macro name", Tokens[0].ToString()!);
			}

			List<Token> ValueTokens = new List<Token>();
			if(Tokens.Count == 1)
			{
				ValueTokens.Add(new Token(TokenType.Number, TokenFlags.None, OneLiteral));
			}
			else if(Tokens[1].Type != TokenType.Equals)
			{
				throw new PreprocessorException(null, "Unable to parse macro definition '{0}'", Definition);
			}
			else
			{
				ValueTokens.AddRange(Tokens.Skip(2));
			}

			PreprocessorMacro Macro = new PreprocessorMacro(Tokens[0].Identifier!, null, ValueTokens);
			State.DefineMacro(Macro);
		}

		/// <summary>
		/// Defines a macro
		/// </summary>
		/// <param name="Name">Name of the macro</param>
		/// <param name="Value">String to be parsed for the macro's value</param>
		public void AddDefinition(string Name, string Value)
		{
			List<Token> Tokens = new List<Token>();

			TokenReader Reader = new TokenReader(Value);
			while(Reader.MoveNext())
			{
				Tokens.Add(Reader.Current);
			}

			PreprocessorMacro Macro = new PreprocessorMacro(Identifier.FindOrAdd(Name), null, Tokens);
			State.DefineMacro(Macro);
		}

		/// <summary>
		/// Defines a macro
		/// </summary>
		/// <param name="Macro">The macro definition</param>
		public void AddDefinition(PreprocessorMacro Macro)
		{
			State.DefineMacro(Macro);
		}

		/// <summary>
		/// Adds an include path to the preprocessor
		/// </summary>
		/// <param name="Directory">The include path</param>
		public void AddIncludePath(DirectoryItem Directory)
		{
			if(!IncludeDirectories.Contains(Directory))
			{
				IncludeDirectories.Add(Directory);
			}
		}

		/// <summary>
		/// Adds an include path to the preprocessor
		/// </summary>
		/// <param name="Location">The include path</param>
		public void AddIncludePath(DirectoryReference Location)
		{
			DirectoryItem Directory = DirectoryItem.GetItemByDirectoryReference(Location);
			if(!Directory.Exists)
			{
				throw new FileNotFoundException("Unable to find " + Location.FullName);
			}
			AddIncludePath(Directory);
		}

		/// <summary>
		/// Adds an include path to the preprocessor
		/// </summary>
		/// <param name="DirectoryName">The include path</param>
		public void AddIncludePath(string DirectoryName)
		{
			AddIncludePath(new DirectoryReference(DirectoryName));
		}

		/// <summary>
		/// Adds a framework path to the preprocessor
		/// </summary>
		/// <param name="Directory">The framework path</param>
		public void AddFrameworkPath(DirectoryItem Directory)
		{
			if (!FrameworkDirectories.Contains(Directory))
			{
				FrameworkDirectories.Add(Directory);
			}
		}

		/// <summary>
		/// Adds a framework path to the preprocessor
		/// </summary>
		/// <param name="Location">The framework path</param>
		public void AddFrameworkPath(DirectoryReference Location)
		{
			DirectoryItem Directory = DirectoryItem.GetItemByDirectoryReference(Location);
			if (!Directory.Exists)
			{
				throw new FileNotFoundException("Unable to find " + Location.FullName);
			}
			AddFrameworkPath(Directory);
		}

		/// <summary>
		/// Adds a framework path to the preprocessor
		/// </summary>
		/// <param name="DirectoryName">The framework path</param>
		public void AddFrameworkPath(string DirectoryName)
		{
			AddFrameworkPath(new DirectoryReference(DirectoryName));
		}

		/// <summary>
		/// Try to resolve an quoted include against the list of include directories. Uses search order described by https://msdn.microsoft.com/en-us/library/36k2cdd4.aspx.
		/// </summary>
		/// <param name="Context">The current preprocessor context</param>
		/// <param name="IncludePath">The path appearing in an #include directive</param>
		/// <param name="Type">Specifies rules for how to resolve the include path (normal/system)</param>
		/// <param name="File">If found, receives the resolved file</param>
		/// <returns>True if the The resolved file</returns>
		public bool TryResolveIncludePath(PreprocessorContext Context, string IncludePath, IncludePathType Type, [NotNullWhen(true)] out FileItem? File)
		{
			// From MSDN (https://msdn.microsoft.com/en-us/library/36k2cdd4.aspx?f=255&MSPPError=-2147217396)
			//
			// The preprocessor searches for include files in this order:
			//
			// Quoted form:
			//   1) In the same directory as the file that contains the #include statement.
			//   2) In the directories of the currently opened include files, in the reverse order in which they were opened. 
			//      The search begins in the directory of the parent include file and continues upward through the directories of any grandparent include files.
			//   3) Along the path that's specified by each /I compiler option.
			//   4) Along the paths that are specified by the INCLUDE environment variable.
			//
			// Angle-bracket form:
			//   1) Along the path that's specified by each /I compiler option.
			//   2) Along the paths that are specified by the INCLUDE environment variable.

			// If it's an absolute path, return it immediately
			if(Path.IsPathRooted(IncludePath))
			{
				FileItem FileItem = FileItem.GetItemByPath(IncludePath);
				if(FileItem.Exists)
				{
					File = FileItem;
					return true;
				}
				else
				{
					File = null;
					return false;
				}
			}

			// Split the path into fragments
			string[] Fragments = IncludePath.Split(Path.DirectorySeparatorChar, Path.AltDirectorySeparatorChar);

			// Try to match the include path against any of the included directories
			if(Type == IncludePathType.Normal)
			{
				for(PreprocessorContext? OuterContext = Context; OuterContext != null; OuterContext = OuterContext.Outer)
				{
					PreprocessorFileContext? OuterFileContext = OuterContext as PreprocessorFileContext;
					if(OuterFileContext != null)
					{
						FileItem? ResolvedFile;
						if(TryResolveRelativeIncludePath(OuterFileContext.Directory, Fragments, out ResolvedFile))
						{
							File = ResolvedFile;
							return true;
						}
					}
				}
			}

			// Try to match the include path against any of the system directories
			foreach(DirectoryItem BaseDirectory in IncludeDirectories)
			{
				FileItem? ResolvedFile;
				if(TryResolveRelativeIncludePath(BaseDirectory, Fragments, out ResolvedFile))
				{
					File = ResolvedFile;
					return true;
				}
			}

			// Try to match the include path against any of the MacOS framework Header paths
			if (Fragments.Length > 1)
			{
				foreach (DirectoryItem BaseDirectory in FrameworkDirectories)
				{
					if (BaseDirectory.TryGetDirectory($"{Fragments[0]}.framework", out DirectoryItem? FrameworkBaseDirectory) &&
						FrameworkBaseDirectory.TryGetDirectory("Headers", out DirectoryItem? HeaderDirectory) &&
						TryResolveRelativeIncludePath(HeaderDirectory, Fragments.Skip(1).ToArray(), out FileItem? ResolvedFile))
					{
						File = ResolvedFile;
						return true;
					}
				}
			}

			// Failed to find the file
			File = null;
			return false;
		}

		/// <summary>
		/// Try to resolve an quoted include against the list of include directories. Uses search order described by https://msdn.microsoft.com/en-us/library/36k2cdd4.aspx.
		/// </summary>
		/// <param name="BaseDirectory">The base directory to search from</param>
		/// <param name="Fragments">Fragments of the relative path to follow</param>
		/// <param name="File">The file that was found, if successful</param>
		/// <returns>True if the The resolved file</returns>
		public bool TryResolveRelativeIncludePath(DirectoryItem BaseDirectory, string[] Fragments, [NotNullWhen(true)] out FileItem? File)
		{
			DirectoryItem? Directory = BaseDirectory;
			for(int Idx = 0; Idx < Fragments.Length - 1; Idx++)
			{
				if(!Directory.TryGetDirectory(Fragments[Idx], out Directory))
				{
					File = null;
					return false;
				}
			}
			return Directory.TryGetFile(Fragments[Fragments.Length - 1], out File);
		}

		/// <summary>
		/// Parses a file recursively 
		/// </summary>
		/// <param name="File">File to parse</param>
		/// <param name="Fragments">Lists of fragments that are parsed</param>
		/// <param name="OuterContext">Outer context information, for error messages</param>
		/// <param name="SourceFileCache">Cache for source files</param>
		/// <param name="Logger">Logger for output</param>
		/// <param name="bShowIncludes">Show all the included files, in order</param>
		/// <param name="bIgnoreMissingIncludes">Suppress exceptions if an include path can not be resolved</param>
		public void ParseFile(FileItem File, List<SourceFileFragment> Fragments, PreprocessorContext? OuterContext, SourceFileMetadataCache SourceFileCache, ILogger Logger, bool bShowIncludes = false, bool bIgnoreMissingIncludes = false)
		{
			// If the file has already been included and had a #pragma once directive, don't include it again
			if(PragmaOnceFiles.Contains(File))
			{
				return;
			}

			if (!ProcessedFiles.Contains(File))
			{
				ProcessedFiles.Add(File);
			}

			// Output a trace of the included files
			if(bShowIncludes)
			{
				Logger.LogInformation("Note: including file: {FileLocation}", File.Location);
			}

			// If the file had a header guard, and the macro is still defined, don't include it again
			SourceFile SourceFile = SourceFileCache.GetSourceFile(File);
			if(SourceFile.HeaderGuardMacro != null && State.IsMacroDefined(SourceFile.HeaderGuardMacro))
			{
				return;
			}

			// Create a context for this file
			PreprocessorFileContext Context = new PreprocessorFileContext(SourceFile, OuterContext);

			// Parse the markup for this file
			while(Context.MarkupIdx < SourceFile.Markup.Length)
			{
				SourceFileMarkup Markup = SourceFile.Markup[Context.MarkupIdx];
				if(Markup.Type == SourceFileMarkupType.Include)
				{
					if(State.IsCurrentBranchActive())
					{
						// Parse the directive
						FileItem? IncludedFile = ParseIncludeDirective(Markup, Context, bIgnoreMissingIncludes);

						// Parse the included file
						if (IncludedFile != null)
						{
							ParseFile(IncludedFile, Fragments, Context, SourceFileCache, Logger, bShowIncludes, bIgnoreMissingIncludes);
						}
					}
					Context.MarkupIdx++;
				}
				else
				{
					// Get the next fragment
					SourceFileFragment Fragment = SourceFile.Fragments[Context.FragmentIdx];
					Debug.Assert(Fragment.MarkupMin == Context.MarkupIdx);

					// Parse this fragment
					ParseFragment(SourceFile, Fragment, Context);

					// Add this fragment to the list
					Fragments.Add(Fragment);
					Context.FragmentIdx++;
				}
			}
		}

		/// <summary>
		/// Parse an include directive and resolve the file it references
		/// </summary>
		/// <param name="Markup">Markup for the include directive</param>
		/// <param name="Context">Current preprocessor context</param>
		/// <param name="bIgnoreMissingIncludes">Suppress exceptions if an include path can not be resolved</param>
		/// <returns>Included file</returns>
		FileItem? ParseIncludeDirective(SourceFileMarkup Markup, PreprocessorFileContext Context, bool bIgnoreMissingIncludes = false)
		{
			// Expand macros in the given tokens
			List<Token> ExpandedTokens = new List<Token>();
			ExpandMacros(Markup.Tokens!, ExpandedTokens, false, Context);
						
			// Convert the string to a single token
			string IncludeToken = Token.Format(ExpandedTokens);

			// Expand any macros in them and resolve it
			IncludePathType Type;
			if(IncludeToken.Length >= 2 && IncludeToken[0] == '\"' && IncludeToken[IncludeToken.Length - 1] == '\"')
			{
				Type = IncludePathType.Normal;
			}
			else if(IncludeToken.Length >= 2 && IncludeToken[0] == '<' && IncludeToken[IncludeToken.Length - 1] == '>')
			{
				Type = IncludePathType.System;
			}
			else
			{
				throw new PreprocessorException(Context, "Couldn't resolve include '{0}'", IncludeToken);
			}

			// Get the include path
			string IncludePath = IncludeToken.Substring(1, IncludeToken.Length - 2);

			// Resolve the included file
			FileItem? IncludedFile;
			if(!TryResolveIncludePath(Context, IncludePath, Type, out IncludedFile))
			{
				if (bIgnoreMissingIncludes)
				{
					Log.TraceWarningOnce("Couldn't resolve include '{0}' ({1})", IncludePath, Context.SourceFile.Location);
				}
				else
				{
					throw new PreprocessorException(Context, "Couldn't resolve include '{0}' ({1})", IncludePath, Context.SourceFile.Location);
				}
			}
			return IncludedFile;
		}

		/// <summary>
		/// Parse a source file fragment, using cached transforms if possible
		/// </summary>
		/// <param name="SourceFile">The source file being parsed</param>
		/// <param name="Fragment">Fragment to parse</param>
		/// <param name="Context">Current preprocessor context</param>
		void ParseFragment(SourceFile SourceFile, SourceFileFragment Fragment, PreprocessorFileContext Context)
		{
			// Check if there's a valid transform that matches the current state
			int TransformIdx = 0;
			for(;;)
			{
				PreprocessorTransform[] Transforms;
				lock(Fragment)
				{
					Transforms = Fragment.Transforms;
					if(TransformIdx == Transforms.Length)
					{
						// Attach a new transform to the current state
						PreprocessorTransform Transform = State.BeginCapture();
						for(; Context.MarkupIdx < Fragment.MarkupMax; Context.MarkupIdx++)
						{
							SourceFileMarkup Markup = SourceFile.Markup[Context.MarkupIdx];
							ParseMarkup(Markup.Type, Markup.Tokens!, Context);
						}
						Transform = State.EndCapture()!;

						// Add it to the fragment for future fragments
						PreprocessorTransform[] NewTransforms = new PreprocessorTransform[Fragment.Transforms.Length + 1];
						for(int Idx = 0; Idx < Transforms.Length; Idx++)
						{
							NewTransforms[Idx] = Transforms[Idx];
						}
						NewTransforms[Transforms.Length] = Transform;

						// Assign it to the fragment
						Fragment.Transforms = NewTransforms;
						return;
					}
				}

				for(; TransformIdx < Transforms.Length; TransformIdx++)
				{
					PreprocessorTransform Transform = Transforms[TransformIdx];
					if(State.TryToApply(Transform))
					{
						// Update the pragma once state
						if(Transform.bHasPragmaOnce)
						{
							PragmaOnceFiles.Add(SourceFile.File);
						}

						// Move to the end of the fragment
						Context.MarkupIdx = Fragment.MarkupMax;
						return;
					}
				}
			}
		}

		/// <summary>
		/// Validate and add a macro using the given parameter and token list
		/// </summary>
		/// <param name="Context">The current preprocessor context</param>
		/// <param name="Name">Name of the macro</param>
		/// <param name="Parameters">Parameter list for the macro</param>
		/// <param name="Tokens">List of tokens</param>
		void AddMacro(PreprocessorContext Context, Identifier Name, List<Identifier>? Parameters, List<Token> Tokens)
		{
			if(Tokens.Count == 0)
			{
				Tokens.Add(new Token(TokenType.Placemarker, TokenFlags.None));
			}
			else
			{
				if(Tokens[0].HasLeadingSpace)
				{
					Tokens[0] = Tokens[0].RemoveFlags(TokenFlags.HasLeadingSpace);
				}
				if (Tokens[0].Type == TokenType.HashHash || Tokens[Tokens.Count - 1].Type == TokenType.HashHash)
				{
					throw new PreprocessorException(Context, "Invalid use of concatenation at start or end of token sequence");
				}
				if (Parameters == null || Parameters.Count == 0 || Parameters[Parameters.Count - 1] != Identifiers.__VA_ARGS__)
				{
					if(Tokens.Any(x => x.Identifier == Identifiers.__VA_ARGS__))
					{
						throw new PreprocessorException(Context, "Invalid reference to {0}", Identifiers.__VA_ARGS__);
					}
				}
			}
			State.DefineMacro(new PreprocessorMacro(Name, Parameters, Tokens));
		}
		
		/// <summary>
		/// Set a predefined macro to a given value
		/// </summary>
		/// <param name="Name">Name of the macro</param>
		/// <param name="Type">Type of the macro token</param>
		/// <param name="Value">Value of the macro</param>
		/// <returns>The created macro</returns>
		void AddLiteralMacro(string Name, TokenType Type, string Value)
		{
			Token Token = new Token(Type, TokenFlags.None, Value);
			PreprocessorMacro Macro = new PreprocessorMacro(Identifier.FindOrAdd(Name), null, new List<Token> { Token });
			State.DefineMacro(Macro);
		}

		/// <summary>
		/// Parse a marked up directive from a file
		/// </summary>
		/// <param name="Type">The markup type</param>
		/// <param name="Tokens">Tokens for the directive</param>
		/// <param name="Context">The context that this markup is being parsed in</param>
		public void ParseMarkup(SourceFileMarkupType Type, List<Token> Tokens, PreprocessorContext Context)
		{
			switch(Type)
			{
				case SourceFileMarkupType.Include:
					throw new PreprocessorException(Context, "Include directives should be handled by the caller.");
				case SourceFileMarkupType.Define:
					ParseDefineDirective(Tokens, Context);
					break;
				case SourceFileMarkupType.Undef:
					ParseUndefDirective(Tokens, Context);
					break;
				case SourceFileMarkupType.If:
					ParseIfDirective(Tokens, Context);
					break;
				case SourceFileMarkupType.Ifdef:
					ParseIfdefDirective(Tokens, Context);
					break;
				case SourceFileMarkupType.Ifndef:
					ParseIfndefDirective(Tokens, Context);
					break;
				case SourceFileMarkupType.Elif:
					ParseElifDirective(Tokens, Context);
					break;
				case SourceFileMarkupType.Else:
					ParseElseDirective(Tokens, Context);
					break;
				case SourceFileMarkupType.Endif:
					ParseEndifDirective(Tokens, Context);
					break;
				case SourceFileMarkupType.Pragma:
					ParsePragmaDirective(Tokens, Context);
					break;
			}
		}

		/// <summary>
		/// Read a macro definition
		/// </summary>
		/// <param name="Tokens">List of tokens in the directive</param>
		/// <param name="Context">The context that this directive is being parsed in</param>
		public void ParseDefineDirective(List<Token> Tokens, PreprocessorContext Context)
		{
			if(State.IsCurrentBranchActive())
			{
				// Check there's a name token
				if(Tokens.Count < 1 || Tokens[0].Type != TokenType.Identifier || Tokens[0].Identifier == Identifiers.Defined)
				{
					throw new PreprocessorException(Context, "Invalid macro name");
				}

				// Read the macro name
				Identifier Name = Tokens[0].Identifier!;
				int TokenIdx = 1;

				// Read the macro parameter list, if there is one
				List<Identifier>? Parameters = null;
				if (TokenIdx < Tokens.Count && !Tokens[TokenIdx].HasLeadingSpace && Tokens[TokenIdx].Type == TokenType.LeftParen)
				{
					Parameters = new List<Identifier>();
					if(++TokenIdx == Tokens.Count)
					{
						throw new PreprocessorException(Context, "Unexpected end of macro parameter list");
					}
					if(Tokens[TokenIdx].Type != TokenType.RightParen)
					{
						for(;;TokenIdx++)
						{
							// Check there's enough tokens left for a parameter name, plus ',' or ')'
							if(TokenIdx + 2 > Tokens.Count)
							{
								throw new PreprocessorException(Context, "Unexpected end of macro parameter list");
							}

							// Check it's a valid name, and add it to the list
							Token NameToken = Tokens[TokenIdx++];
							if(NameToken.Type == TokenType.Ellipsis)
							{
								if(Tokens[TokenIdx].Type != TokenType.RightParen)
								{
									throw new PreprocessorException(Context, "Variadic macro arguments must be last in list");
								}
								else
								{
									NameToken = new Token(Identifiers.__VA_ARGS__, NameToken.Flags & TokenFlags.HasLeadingSpace);
								}
							}
							else
							{
								if (NameToken.Type != TokenType.Identifier || NameToken.Identifier == Identifiers.__VA_ARGS__)
								{
									throw new PreprocessorException(Context, "Invalid preprocessor token: {0}", NameToken);
								}
								if (Parameters.Contains(NameToken.Identifier!))
								{
									throw new PreprocessorException(Context, "'{0}' has already been used as an argument name", NameToken);
								}
							}
							Parameters.Add(NameToken.Identifier!);

							// Read the separator
							Token SeparatorToken = Tokens[TokenIdx];
							if (SeparatorToken.Type == TokenType.RightParen)
							{
								break;
							}
							if (SeparatorToken.Type != TokenType.Comma)
							{
								throw new PreprocessorException(Context, "Expected ',' or ')'");
							}
						}
					}
					TokenIdx++;
				}

				// Read the macro tokens
				AddMacro(Context, Name, Parameters, Tokens.Skip(TokenIdx).ToList());
			}
		}

		/// <summary>
		/// Parse an #undef directive
		/// </summary>
		/// <param name="Tokens">List of tokens in the directive</param>
		/// <param name="Context">The context that this directive is being parsed in</param>
		public void ParseUndefDirective(List<Token> Tokens, PreprocessorContext Context)
		{
			if(State.IsCurrentBranchActive())
			{
				// Check there's a name token
				if (Tokens.Count != 1)
				{
					throw new PreprocessorException(Context, "Expected a single token after #undef");
				}
				if (Tokens[0].Type != TokenType.Identifier)
				{
					throw new PreprocessorException(Context, "Invalid macro name '{0}'", Tokens[0]);
				}

				// Remove the macro from the list of definitions
				State.UndefMacro(Tokens[0].Identifier!);
			}
		}

		/// <summary>
		/// Parse an #if directive
		/// </summary>
		/// <param name="Tokens">List of tokens in the directive</param>
		/// <param name="Context">The context that this directive is being parsed in</param>
		public void ParseIfDirective(List<Token> Tokens, PreprocessorContext Context)
		{
			PreprocessorBranch Branch = PreprocessorBranch.HasIfDirective;
			if (State.IsCurrentBranchActive())
			{
				// Read a line into the buffer and expand the macros in it
				List<Token> ExpandedTokens = new List<Token>();
				ExpandMacros(Tokens, ExpandedTokens, true, Context);

				// Evaluate the condition
				long Result = PreprocessorExpression.Evaluate(Context, ExpandedTokens);
				if (Result != 0)
				{
					Branch |= PreprocessorBranch.Active | PreprocessorBranch.Taken;
				}
			}
			State.PushBranch(Branch);
		}

		/// <summary>
		/// Parse an #ifdef directive
		/// </summary>
		/// <param name="Tokens">List of tokens in the directive</param>
		/// <param name="Context">The context that this directive is being parsed in</param>
		public void ParseIfdefDirective(List<Token> Tokens, PreprocessorContext Context)
		{
			PreprocessorBranch Branch = PreprocessorBranch.HasIfdefDirective;
			if (State.IsCurrentBranchActive())
			{
				// Make sure there's only one token
				if(Tokens.Count != 1 || Tokens[0].Type != TokenType.Identifier)
				{
					throw new PreprocessorException(Context, "Missing or invalid macro name for #ifdef directive");
				}

				// Check if the macro is defined
				if(State.IsMacroDefined(Tokens[0].Identifier!))
				{
					Branch |= PreprocessorBranch.Active | PreprocessorBranch.Taken;
				}
			}
			State.PushBranch(Branch);
		}

		/// <summary>
		/// Parse an #ifndef directive
		/// </summary>
		/// <param name="Tokens">List of tokens for this directive</param>
		/// <param name="Context">The context that this directive is being parsed in</param>
		public void ParseIfndefDirective(List<Token> Tokens, PreprocessorContext Context)
		{
			PreprocessorBranch Branch = PreprocessorBranch.HasIfndefDirective;
			if (State.IsCurrentBranchActive())
			{
				// Make sure there's only one token
				if (Tokens.Count != 1 || Tokens[0].Type != TokenType.Identifier)
				{
					throw new PreprocessorException(Context, "Missing or invalid macro name for #ifndef directive");
				}

				// Check if the macro is defined
				if(!State.IsMacroDefined(Tokens[0].Identifier!))
				{
					Branch |= PreprocessorBranch.Active | PreprocessorBranch.Taken;
				}
			}
			State.PushBranch(Branch);
		}

		/// <summary>
		/// Parse an #elif directive
		/// </summary>
		/// <param name="Tokens">List of tokens for this directive</param>
		/// <param name="Context">The context that this directive is being parsed in</param>
		public void ParseElifDirective(List<Token> Tokens, PreprocessorContext Context)
		{
			// Check we're in a branch, and haven't already read an #else directive
			PreprocessorBranch Branch;
			if (!State.TryPopBranch(out Branch))
			{
				throw new PreprocessorException(Context, "#elif directive outside conditional block");
			}
			if (Branch.HasFlag(PreprocessorBranch.Complete))
			{
				throw new PreprocessorException(Context, "#elif directive cannot appear after #else");
			}

			// Pop the current branch state at this depth, so we can test against whether the parent state is enabled
			Branch = (Branch | PreprocessorBranch.HasElifDirective) & ~PreprocessorBranch.Active;
			if (State.IsCurrentBranchActive())
			{
				// Read a line into the buffer and expand the macros in it
				List<Token> ExpandedTokens = new List<Token>();
				ExpandMacros(Tokens, ExpandedTokens, true, Context);

				// Check we're at the end of a conditional block
				if (!Branch.HasFlag(PreprocessorBranch.Taken))
				{
					long Result = PreprocessorExpression.Evaluate(Context, ExpandedTokens);
					if (Result != 0)
					{
						Branch |= PreprocessorBranch.Active | PreprocessorBranch.Taken;
					}
				}
			}
			State.PushBranch(Branch);
		}

		/// <summary>
		/// Parse an #else directive
		/// </summary>
		/// <param name="Tokens">List of tokens in the directive</param>
		/// <param name="Context">The context that this directive is being parsed in</param>
		public void ParseElseDirective(List<Token> Tokens, PreprocessorContext Context)
		{
			// Make sure there's nothing else on the line
			if (Tokens.Count > 0)
			{
				throw new PreprocessorException(Context, "Garbage after #else directive");
			}

			// Check we're in a branch, and haven't already read an #else directive
			PreprocessorBranch Branch;
			if (!State.TryPopBranch(out Branch))
			{
				throw new PreprocessorException(Context, "#else directive without matching #if directive");
			}
			if ((Branch & PreprocessorBranch.Complete) != 0)
			{
				throw new PreprocessorException(Context, "Only one #else directive can appear in a conditional block");
			}

			// Check whether to take this branch, but only allow activating if the parent state is active.
			Branch &= ~PreprocessorBranch.Active;
			if(State.IsCurrentBranchActive() && !Branch.HasFlag(PreprocessorBranch.Taken))
			{
				Branch |= PreprocessorBranch.Active | PreprocessorBranch.Taken;
			}
			State.PushBranch(Branch | PreprocessorBranch.Complete);
		}

		/// <summary>
		/// Parse an #endif directive
		/// </summary>
		/// <param name="Tokens">List of tokens in the directive</param>
		/// <param name="Context">The context that this directive is being parsed in</param>
		public void ParseEndifDirective(List<Token> Tokens, PreprocessorContext Context)
		{
			// Pop the branch off the stack
			PreprocessorBranch Branch;
			if(!State.TryPopBranch(out Branch))
			{
				throw new PreprocessorException(Context, "#endif directive without matching #if/#ifdef/#ifndef directive");
			}
		}

		/// <summary>
		/// Parse a #pragma directive
		/// </summary>
		/// <param name="Tokens">List of tokens in the directive</param>
		/// <param name="Context">The context that this directive is being parsed in</param>
		public void ParsePragmaDirective(List<Token> Tokens, PreprocessorContext Context)
		{
			if(State.IsCurrentBranchActive())
			{
				if(Tokens.Count == 1 && Tokens[0].Identifier == Identifiers.Once)
				{
					SourceFile SourceFile = GetCurrentSourceFile(Context)!;
					PragmaOnceFiles.Add(SourceFile.File);
					State.MarkPragmaOnce();
				}
			}
		}

		/// <summary>
		/// Expand macros in the given sequence.
		/// </summary>
		/// <param name="InputTokens">Sequence of input tokens</param>
		/// <param name="OutputTokens">List to receive the expanded tokens</param>
		/// <param name="bIsConditional">Whether a conditional expression is being evaluated (and 'defined' expressions are valid)</param>
		/// <param name="Context">The context that this directive is being parsed in</param>
		public void ExpandMacros(IEnumerable<Token> InputTokens, List<Token> OutputTokens, bool bIsConditional, PreprocessorContext Context)
		{
			List<PreprocessorMacro> IgnoreMacros = new List<PreprocessorMacro>();
			ExpandMacrosRecursively(InputTokens, OutputTokens, bIsConditional, IgnoreMacros, Context);
		}

		/// <summary>
		/// Expand macros in the given sequence, ignoring previously expanded macro names from a list.
		/// </summary>
		/// <param name="InputTokens">Sequence of input tokens</param>
		/// <param name="OutputTokens">List to receive the expanded tokens</param>
		/// <param name="bIsConditional">Whether a conditional expression is being evaluated (and 'defined' expressions are valid)</param>
		/// <param name="IgnoreMacros">List of macros to ignore</param>
		/// <param name="Context">The context that this directive is being parsed in</param>
		void ExpandMacrosRecursively(IEnumerable<Token> InputTokens, List<Token> OutputTokens, bool bIsConditional, List<PreprocessorMacro> IgnoreMacros, PreprocessorContext Context)
		{
			IEnumerator<Token> InputEnumerator = InputTokens.GetEnumerator();
			if(InputEnumerator.MoveNext())
			{
				for(;;)
				{
					if(!ReadExpandedToken(InputEnumerator, OutputTokens, bIsConditional, IgnoreMacros, Context))
					{
						break;
					}
				}
			}
		}

		/// <summary>
		/// Merges an optional leading space flag into the given token (recycling the original token if possible).
		/// </summary>
		/// <param name="Token">The token to merge a leading space into</param>
		/// <param name="bHasLeadingSpace">The leading space flag</param>
		/// <returns>New token with the leading space flag set, or the existing token</returns>
		Token MergeLeadingSpace(Token Token, bool bHasLeadingSpace)
		{
			Token Result = Token;
			if(bHasLeadingSpace && !Result.HasLeadingSpace)
			{
				Result = Result.AddFlags(TokenFlags.HasLeadingSpace);
			}
			return Result;
		}

		/// <summary>
		/// Read a token from an enumerator and substitute it if it's a macro or 'defined' expression (reading more tokens as necessary to complete the expression).
		/// </summary>
		/// <param name="InputEnumerator">The enumerator of input tokens</param>
		/// <param name="OutputTokens">List to receive the expanded tokens</param>
		/// <param name="bIsConditional">Whether a conditional expression is being evaluated (and 'defined' expressions are valid)</param>
		/// <param name="IgnoreMacros">List of macros to ignore</param>
		/// <param name="Context">The context that this directive is being parsed in</param>
		/// <returns>Result from calling the enumerator's MoveNext() method</returns>
		bool ReadExpandedToken(IEnumerator<Token> InputEnumerator, List<Token> OutputTokens, bool bIsConditional, List<PreprocessorMacro> IgnoreMacros, PreprocessorContext Context)
		{
			// Capture the first token, then move to the next
			OutputTokens.Add(InputEnumerator.Current);
			bool bMoveNext = InputEnumerator.MoveNext();

			// If it's an identifier, try to resolve it as a macro
			if (OutputTokens[OutputTokens.Count - 1].Identifier == Identifiers.Defined && bIsConditional)
			{
				// Remove the 'defined' keyword
				OutputTokens.RemoveAt(OutputTokens.Count - 1);

				// Make sure there's another token
				if (!bMoveNext)
				{
					throw new PreprocessorException(Context, "Invalid syntax for 'defined' expression");
				}

				// Check for the form 'defined identifier'
				Token NameToken;
				if(InputEnumerator.Current.Type == TokenType.Identifier)
				{
					NameToken = InputEnumerator.Current;
				}
				else
				{
					// Otherwise assume the form 'defined ( identifier )'
					if(InputEnumerator.Current.Type != TokenType.LeftParen || !InputEnumerator.MoveNext() || InputEnumerator.Current.Type != TokenType.Identifier)
					{
						throw new PreprocessorException(Context, "Invalid syntax for 'defined' expression");
					}
					NameToken = InputEnumerator.Current;
					if(!InputEnumerator.MoveNext() || InputEnumerator.Current.Type != TokenType.RightParen)
					{
						throw new PreprocessorException(Context, "Invalid syntax for 'defined' expression");
					}
				}

				// Insert a token for whether it's defined or not
				OutputTokens.Add(new Token(TokenType.Number, TokenFlags.None, State.IsMacroDefined(NameToken.Identifier!)? OneLiteral : ZeroLiteral));
				bMoveNext = InputEnumerator.MoveNext();
			}
			else
			{
				// Repeatedly try to expand the last token into the list
				while(OutputTokens[OutputTokens.Count - 1].Type == TokenType.Identifier && !OutputTokens[OutputTokens.Count - 1].Flags.HasFlag(TokenFlags.DisableExpansion))
				{
					// Try to get a macro for the current token
					PreprocessorMacro? Macro;
					if (!State.TryGetMacro(OutputTokens[OutputTokens.Count - 1].Identifier!, out Macro) || IgnoreMacros.Contains(Macro))
					{
						break;
					}
					if(Macro.IsFunctionMacro && (!bMoveNext || InputEnumerator.Current.Type != TokenType.LeftParen))
					{
						break;
					}

					// Remove the macro name from the output list
					bool bHasLeadingSpace = OutputTokens[OutputTokens.Count - 1].HasLeadingSpace;
					OutputTokens.RemoveAt(OutputTokens.Count - 1);

					// Save the initial number of tokens in the list, so we can tell if it expanded
					int NumTokens = OutputTokens.Count;

					// If it's an object macro, expand it immediately into the output buffer
					if(Macro.IsObjectMacro)
					{
						// Expand the macro tokens into the output buffer
						ExpandObjectMacro(Macro, OutputTokens, bIsConditional, IgnoreMacros, Context);
					}
					else
					{
						// Read balanced token for argument list
						List<Token> ArgumentTokens = new List<Token>();
						bMoveNext = ReadBalancedToken(InputEnumerator, ArgumentTokens, Context);

						// Expand the macro tokens into the output buffer
						ExpandFunctionMacro(Macro, ArgumentTokens, OutputTokens, bIsConditional, IgnoreMacros, Context);
					}

					// If the macro expanded to nothing, quit
					if(OutputTokens.Count <= NumTokens)
					{
						break;
					}

					// Make sure the space is propagated to the expanded macro
					OutputTokens[NumTokens] = MergeLeadingSpace(OutputTokens[NumTokens], bHasLeadingSpace);

					// Mark any tokens matching the macro name as not to be expanded again. This can happen with recursive object macros, eg. #define DWORD ::DWORD
					for(int Idx = NumTokens; Idx < OutputTokens.Count; Idx++)
					{
						if(OutputTokens[Idx].Type == TokenType.Identifier && OutputTokens[Idx].Identifier == Macro.Name)
						{
							OutputTokens[Idx] = OutputTokens[Idx].AddFlags(TokenFlags.DisableExpansion);
						}
					}
				}
			}
            return bMoveNext;
		}

		/// <summary>
		/// Gets a string for the __FILE__ macro
		/// </summary>
		/// <param name="Context">Context to scan to find the current file</param>
		/// <returns>String representing the current context</returns>
		string GetCurrentFileMacroValue(PreprocessorContext Context)
		{
			SourceFile? SourceFile = GetCurrentSourceFile(Context);
			if(SourceFile == null)
			{
				return "<unknown>";
			}
			else
			{
				return SourceFile.Location.FullName;
			}
		}

		/// <summary>
		/// Gets a string for the current file
		/// </summary>
		/// <param name="Context">Context to scan to find the current file</param>
		/// <returns>Current source file being parsed</returns>
		SourceFile? GetCurrentSourceFile(PreprocessorContext Context)
		{
			SourceFile? SourceFile = null;
			for(PreprocessorContext? OuterContext = Context; OuterContext != null; OuterContext = OuterContext.Outer)
			{
				PreprocessorFileContext? OuterFileContext = OuterContext as PreprocessorFileContext;
				if(OuterFileContext != null)
				{
					SourceFile = OuterFileContext.SourceFile;
					break;
				}
			}
			return SourceFile;
		}

		/// <summary>
		/// Gets the current line number
		/// </summary>
		/// <param name="Context">Context to scan to find the current file</param>
		/// <returns>Line number in the first file encountered</returns>
		int GetCurrentLine(PreprocessorContext Context)
		{
			for(PreprocessorContext? OuterContext = Context; OuterContext != null; OuterContext = OuterContext.Outer)
			{
				PreprocessorFileContext? OuterFileContext = OuterContext as PreprocessorFileContext;
				if(OuterFileContext != null)
				{
					return OuterFileContext.SourceFile.Markup[OuterFileContext.MarkupIdx].LineNumber;
				}
			}
			return 0;
		}

		/// <summary>
		/// Expand an object macro
		/// </summary>
		/// <param name="Macro">The functional macro</param>
		/// <param name="OutputTokens">The list to receive the output tokens</param>
		/// <param name="bIsConditional">Whether the macro is being expanded in a conditional context, allowing use of the 'defined' keyword</param>
		/// <param name="IgnoreMacros">List of macros currently being expanded, which should be ignored for recursion</param>
		/// <param name="Context">The context that this directive is being parsed in</param>
		void ExpandObjectMacro(PreprocessorMacro Macro, List<Token> OutputTokens, bool bIsConditional, List<PreprocessorMacro> IgnoreMacros, PreprocessorContext Context)
		{
			// Special handling for the __LINE__ directive, since we need an updated line number for the current token
			if(Macro.Name == Identifiers.__FILE__)
			{
				Token Token = new Token(TokenType.String, TokenFlags.None, String.Format("\"{0}\"", GetCurrentFileMacroValue(Context).Replace("\\", "\\\\")));
				OutputTokens.Add(Token);
			}
			else if(Macro.Name == Identifiers.__LINE__)
			{
				Token Token = new Token(TokenType.Number, TokenFlags.None, GetCurrentLine(Context).ToString());
				OutputTokens.Add(Token);
			}
			else if(Macro.Name == Identifiers.__COUNTER__)
			{
				Token Token = new Token(TokenType.Number, TokenFlags.None, (Counter++).ToString());
				OutputTokens.Add(Token);
			}
			else
			{
				int OutputTokenCount = OutputTokens.Count;

				// Expand all the macros
				IgnoreMacros.Add(Macro);
				ExpandMacrosRecursively(Macro.Tokens, OutputTokens, bIsConditional, IgnoreMacros, Context);
				IgnoreMacros.RemoveAt(IgnoreMacros.Count - 1);

				// Concatenate any adjacent tokens
				for(int Idx = OutputTokenCount + 1; Idx < OutputTokens.Count - 1; Idx++)
				{
					if(OutputTokens[Idx].Type == TokenType.HashHash)
					{
						OutputTokens[Idx - 1] = Token.Concatenate(OutputTokens[Idx - 1], OutputTokens[Idx + 1], Context);
						OutputTokens.RemoveRange(Idx, 2);
						Idx--;
					}
				}
			}
		}

		/// <summary>
		/// Expand a function macro
		/// </summary>
		/// <param name="Macro">The functional macro</param>
		/// <param name="ArgumentListTokens">Identifiers for each argument token</param>
		/// <param name="OutputTokens">The list to receive the output tokens</param>
		/// <param name="bIsConditional">Whether the macro is being expanded in a conditional context, allowing use of the 'defined' keyword</param>
		/// <param name="IgnoreMacros">List of macros currently being expanded, which should be ignored for recursion</param>
		/// <param name="Context">The context that this macro is being expanded</param>
		void ExpandFunctionMacro(PreprocessorMacro Macro, List<Token> ArgumentListTokens, List<Token> OutputTokens, bool bIsConditional, List<PreprocessorMacro> IgnoreMacros, PreprocessorContext Context)
		{
			// Replace any newlines with spaces, and merge them with the following token
			for(int Idx = 0; Idx < ArgumentListTokens.Count; Idx++)
			{
				if(ArgumentListTokens[Idx].Type == TokenType.Newline)
				{
					if(Idx + 1 < ArgumentListTokens.Count)
					{
						ArgumentListTokens[Idx + 1] = MergeLeadingSpace(ArgumentListTokens[Idx + 1], true);
					}
					ArgumentListTokens.RemoveAt(Idx--);
				}
			}

			// Split the arguments out into separate lists
			List<List<Token>> Arguments = new List<List<Token>>();
			if(ArgumentListTokens.Count > 2)
			{
				for(int Idx = 1;;Idx++)
				{
					if (!Macro.HasVariableArgumentList || Arguments.Count < Macro.Parameters!.Count)
					{
						Arguments.Add(new List<Token>());
					}

					List<Token> Argument = Arguments[Arguments.Count - 1];

					int InitialIdx = Idx;
					while (Idx < ArgumentListTokens.Count - 1 && ArgumentListTokens[Idx].Type != TokenType.Comma)
					{
						if (!ReadBalancedToken(ArgumentListTokens, ref Idx, Argument))
						{
							throw new PreprocessorException(Context, "Invalid argument");
						}
					}

					if(Argument.Count > 0 && Arguments[Arguments.Count - 1][0].HasLeadingSpace)
					{
						Argument[0] = Argument[0].RemoveFlags(TokenFlags.HasLeadingSpace);
					}

					bool bHasLeadingSpace = false;
					for(int TokenIdx = 0; TokenIdx < Argument.Count; TokenIdx++)
					{
						if(Argument[TokenIdx].Text.Length == 0)
						{
							bHasLeadingSpace |= Argument[TokenIdx].HasLeadingSpace;
							Argument.RemoveAt(TokenIdx--);
						}
						else
						{
							Argument[TokenIdx] = MergeLeadingSpace(Argument[TokenIdx], bHasLeadingSpace);
							bHasLeadingSpace = false;
						}
					}

					if (Argument.Count == 0)
					{
						Argument.Add(new Token(TokenType.Placemarker, TokenFlags.None));
						Argument.Add(new Token(TokenType.Placemarker, bHasLeadingSpace? TokenFlags.HasLeadingSpace : TokenFlags.None));
					}

					if(Idx == ArgumentListTokens.Count - 1)
					{
						break;
					}

					if (ArgumentListTokens[Idx].Type != TokenType.Comma)
					{
						throw new PreprocessorException(Context, "Expected ',' between arguments");
					}

					if (Macro.HasVariableArgumentList && Arguments.Count == Macro.Parameters!.Count && Idx < ArgumentListTokens.Count - 1)
					{
						Arguments[Arguments.Count - 1].Add(ArgumentListTokens[Idx]);
					}
				}
			}

			// Add an empty variable argument if one was not specified
			if (Macro.HasVariableArgumentList && Arguments.Count == Macro.Parameters!.Count - 1)
			{
				Arguments.Add(new List<Token> { new Token(TokenType.Placemarker, TokenFlags.None) });
			}

			// Validate the argument list
			if (Arguments.Count != Macro.Parameters!.Count)
			{
				throw new PreprocessorException(Context, "Incorrect number of arguments to macro");
			}

			// Expand each one of the arguments
			List<List<Token>> ExpandedArguments = new List<List<Token>>();
			for (int Idx = 0; Idx < Arguments.Count; Idx++)
			{
				List<Token> NewArguments = new List<Token>();
				ExpandMacrosRecursively(Arguments[Idx], NewArguments, bIsConditional, IgnoreMacros, Context);
				ExpandedArguments.Add(NewArguments);
			}

			// Substitute all the argument tokens
			List<Token> ExpandedTokens = new List<Token>();
			for(int Idx = 0; Idx < Macro.Tokens.Count; Idx++)
			{
				Token Token = Macro.Tokens[Idx];
				if(Token.Type == TokenType.Hash && Idx + 1 < Macro.Tokens.Count)
				{
					// Stringizing operator
					int ParamIdx = Macro.FindParameterIndex(Macro.Tokens[++Idx].Identifier!);
					if (ParamIdx == -1)
					{
						throw new PreprocessorException(Context, "{0} is not an argument name", Macro.Tokens[Idx].Text);
					}
					ExpandedTokens.Add(new Token(TokenType.String, Token.Flags & TokenFlags.HasLeadingSpace, String.Format("\"{0}\"", Token.Format(Arguments[ParamIdx]).Replace("\\", "\\\\").Replace("\"", "\\\""))));
				}
				else if(Macro.HasVariableArgumentList && Idx + 2 < Macro.Tokens.Count && Token.Type == TokenType.Comma && Macro.Tokens[Idx + 1].Type == TokenType.HashHash && Macro.Tokens[Idx + 2].Identifier == Identifiers.__VA_ARGS__)
				{
					// Special MSVC/GCC extension: ', ## __VA_ARGS__' removes the comma if __VA_ARGS__ is empty. MSVC seems to format the result with a forced space.
					List<Token> ExpandedArgument = ExpandedArguments[ExpandedArguments.Count - 1];
					if(ExpandedArgument.Any(x => x.Text.Length > 0))
					{
						ExpandedTokens.Add(Token);
						AppendTokensWithWhitespace(ExpandedTokens, ExpandedArgument, false);
						Idx += 2;
					}
					else
					{
						ExpandedTokens.Add(new Token(TokenType.Placemarker, Token.Flags & TokenFlags.HasLeadingSpace));
						ExpandedTokens.Add(new Token(TokenType.Placemarker, TokenFlags.HasLeadingSpace));
						Idx += 2;
					}
				}
				else if (Token.Type == TokenType.Identifier)
				{
					// Expand a parameter
					int ParamIdx = Macro.FindParameterIndex(Token.Identifier!);
					if(ParamIdx == -1)
					{
						ExpandedTokens.Add(Token);
					}
					else if(Idx > 0 && Macro.Tokens[Idx - 1].Type == TokenType.HashHash)
					{
						AppendTokensWithWhitespace(ExpandedTokens, Arguments[ParamIdx], Token.HasLeadingSpace);
					}
					else if(Idx + 1 < Macro.Tokens.Count && Macro.Tokens[Idx + 1].Type == TokenType.HashHash)
					{
						AppendTokensWithWhitespace(ExpandedTokens, Arguments[ParamIdx], Token.HasLeadingSpace);
					}
					else
					{
						AppendTokensWithWhitespace(ExpandedTokens, ExpandedArguments[ParamIdx], Token.HasLeadingSpace);
					}
				}
				else
				{
					ExpandedTokens.Add(Token);
				}
			}

			// Concatenate adjacent tokens
			for (int Idx = 1; Idx < ExpandedTokens.Count - 1; Idx++)
			{
				if(ExpandedTokens[Idx].Type == TokenType.HashHash)
				{
					Token ConcatenatedToken = Token.Concatenate(ExpandedTokens[Idx - 1], ExpandedTokens[Idx + 1], Context);
					ExpandedTokens.RemoveRange(Idx, 2);
					ExpandedTokens[--Idx] = ConcatenatedToken;
				}
			}

			// Finally, return the expansion of this
			IgnoreMacros.Add(Macro);
			ExpandMacrosRecursively(ExpandedTokens, OutputTokens, bIsConditional, IgnoreMacros, Context);
			IgnoreMacros.RemoveAt(IgnoreMacros.Count - 1);
		}

		/// <summary>
		/// Appends a list of tokens to another list, setting the leading whitespace flag to the given value
		/// </summary>
		/// <param name="OutputTokens">List to receive the appended tokens</param>
		/// <param name="InputTokens">List of tokens to append</param>
		/// <param name="bHasLeadingSpace">Whether there is space before the first token</param>
		void AppendTokensWithWhitespace(List<Token> OutputTokens, List<Token> InputTokens, bool bHasLeadingSpace)
		{
			if(InputTokens.Count > 0)
			{
				OutputTokens.Add(MergeLeadingSpace(InputTokens[0], bHasLeadingSpace));
				OutputTokens.AddRange(InputTokens.Skip(1));
			}
		}

		/// <summary>
		/// Copies a single token from one list of tokens to another, or if it's an opening parenthesis, the entire subexpression until the closing parenthesis.
		/// </summary>
		/// <param name="InputTokens">The input token list</param>
		/// <param name="InputIdx">First token index in the input token list. Set to the last uncopied token index on return.</param>
		/// <param name="OutputTokens">List to recieve the output tokens</param>
		/// <returns>True if a balanced expression was read, or false if the end of the list was encountered before finding a matching token</returns>
		bool ReadBalancedToken(List<Token> InputTokens, ref int InputIdx, List<Token> OutputTokens)
		{
			// Copy a single token to the output list
			Token Token = InputTokens[InputIdx++];
			OutputTokens.Add(Token);

			// If it was the start of a subexpression, copy until the closing parenthesis
			if (Token.Type == TokenType.LeftParen)
			{
				// Copy the contents of the subexpression
				for (;;)
				{
					if (InputIdx == InputTokens.Count)
					{
						return false;
					}
					if (InputTokens[InputIdx].Type == TokenType.RightParen)
					{
						break;
					}
					if (!ReadBalancedToken(InputTokens, ref InputIdx, OutputTokens))
					{
						return false;
					}
				}

				// Copy the closing parenthesis
				Token = InputTokens[InputIdx++];
				OutputTokens.Add(Token);
			}
			return true;
		}

		/// <summary>
		/// Copies a single token from one list of tokens to another, or if it's an opening parenthesis, the entire subexpression until the closing parenthesis.
		/// </summary>
		/// <param name="InputEnumerator">The input token list</param>
		/// <param name="OutputTokens">List to recieve the output tokens</param>
		/// <param name="Context">The context that the parser is in</param>
		/// <returns>True if a balanced expression was read, or false if the end of the list was encountered before finding a matching token</returns>
		bool ReadBalancedToken(IEnumerator<Token> InputEnumerator, List<Token> OutputTokens, PreprocessorContext Context)
		{
			// Copy a single token to the output list
			Token Token = InputEnumerator.Current;
			bool bMoveNext = InputEnumerator.MoveNext();
			OutputTokens.Add(Token);

			// If it was the start of a subexpression, copy until the closing parenthesis
			if (Token.Type == TokenType.LeftParen)
			{
				// Copy the contents of the subexpression
				for (;;)
				{
					if (!bMoveNext)
					{
						throw new PreprocessorException(Context, "Unbalanced token sequence");
					}
					if (InputEnumerator.Current.Type == TokenType.RightParen)
					{
						OutputTokens.Add(InputEnumerator.Current);
						bMoveNext = InputEnumerator.MoveNext();
						break;
					}
					bMoveNext = ReadBalancedToken(InputEnumerator, OutputTokens, Context);
				}
			}
			return bMoveNext;
		}
	}
}
