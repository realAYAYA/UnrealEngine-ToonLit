// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading;
using System.Threading.Tasks;
using EpicGames.Core;

namespace UnrealBuildTool
{
	/// <summary>
	/// Possible types of source file markup
	/// </summary>
	enum SourceFileMarkupType
	{
		/// <summary>
		/// A span of tokens which are not preprocessor directives
		/// </summary>
		Text,

		/// <summary>
		/// An #include directive
		/// </summary>
		Include,

		/// <summary>
		/// A #define directive
		/// </summary>
		Define,

		/// <summary>
		/// An #undef directive
		/// </summary>
		Undef,

		/// <summary>
		/// An #if directive
		/// </summary>
		If,

		/// <summary>
		/// An #ifdef directive
		/// </summary>
		Ifdef,

		/// <summary>
		/// An #ifndef directive
		/// </summary>
		Ifndef,

		/// <summary>
		/// An #elif directive
		/// </summary>
		Elif,

		/// <summary>
		/// An #else directive
		/// </summary>
		Else,

		/// <summary>
		/// An #endif directive
		/// </summary>
		Endif,

		/// <summary>
		/// A #pragma directive
		/// </summary>
		Pragma,

		/// <summary>
		/// An #error directive
		/// </summary>
		Error,

		/// <summary>
		/// A #warning directive
		/// </summary>
		Warning,

		/// <summary>
		/// An empty '#' on a line of its own
		/// </summary>
		Empty,

		/// <summary>
		/// Some other directive
		/// </summary>
		OtherDirective
	}

	/// <summary>
	/// Base class for an annotated section of a source file
	/// </summary>
	[Serializable]
	class SourceFileMarkup
	{
		/// <summary>
		/// The directive corresponding to this markup
		/// </summary>
		public SourceFileMarkupType Type;

		/// <summary>
		/// The one-based line number of this markup
		/// </summary>
		public int LineNumber;

		/// <summary>
		/// The tokens parsed for this markup. Set for directives.
		/// </summary>
		public List<Token>? Tokens;

		/// <summary>
		/// Construct the annotation with the given range
		/// </summary>
		/// <param name="Type">The type of this directive</param>
		/// <param name="LineNumber">The line number of this markup</param>
		/// <param name="Tokens">List of tokens</param>
		public SourceFileMarkup(SourceFileMarkupType Type, int LineNumber, List<Token>? Tokens)
		{
			this.Type = Type;
			this.LineNumber = LineNumber;
			this.Tokens = Tokens;
		}

		/// <summary>
		/// Constructs a markup object using data read from an archive
		/// </summary>
		/// <param name="Reader">The reader to deserialize from</param>
		public SourceFileMarkup(BinaryArchiveReader Reader)
		{
			Type = (SourceFileMarkupType)Reader.ReadByte();
			LineNumber = Reader.ReadInt();
			Tokens = Reader.ReadList(() => Reader.ReadToken());
		}

		/// <summary>
		/// Serializes this object to a binary archive
		/// </summary>
		/// <param name="Writer">Writer to serialize to</param>
		public void Write(BinaryArchiveWriter Writer)
		{
			Writer.WriteByte((byte)Type);
			Writer.WriteInt(LineNumber);
			Writer.WriteList(Tokens, x => Writer.WriteToken(x));
		}

		/// <summary>
		/// Determines if this markup indicates a conditional preprocessor directive
		/// </summary>
		/// <returns>True if this object is a conditional preprocessor directive</returns>
		public bool IsConditionalPreprocessorDirective()
		{
			switch(Type)
			{
				case SourceFileMarkupType.If:
				case SourceFileMarkupType.Ifdef:
				case SourceFileMarkupType.Ifndef:
				case SourceFileMarkupType.Elif:
				case SourceFileMarkupType.Else:
				case SourceFileMarkupType.Endif:
					return true;
			}
			return false;
		}

		/// <summary>
		/// How this condition modifies the condition depth. Opening "if" statements have a value of +1, "endif" statements have a value of -1, and "else" statements have a value of 0.
		/// </summary>
		public int GetConditionDepthDelta()
		{
			if(Type == SourceFileMarkupType.If || Type == SourceFileMarkupType.Ifdef || Type == SourceFileMarkupType.Ifndef)
			{
				return +1;
			}
			else if(Type == SourceFileMarkupType.Endif)
			{
				return -1;
			}
			else
			{
				return 0;
			}
		}

		/// <summary>
		/// Generate a string describing this annotation
		/// </summary>
		/// <returns>String representation for debugging</returns>
		public override string ToString()
		{
			StringBuilder Result = new StringBuilder();
			Result.AppendFormat("[{0}] ", LineNumber);

			if(Type == SourceFileMarkupType.Text)
			{
				Result.Append("...");
			}
			else
			{
				Result.Append("#");
				if(Type != SourceFileMarkupType.OtherDirective)
				{
					Result.Append(Type.ToString().ToLowerInvariant());
				}
				if(Tokens != null && Tokens.Count > 0)
				{
					Result.Append(' ');
					Token.Format(Tokens, Result);
				}
			}
			return Result.ToString();
		}

		/// <summary>
		/// Create markup for the given file
		/// </summary>
		/// <param name="Reader">Reader for tokens in the file</param>
		/// <returns>Array of markup objects which split up the given text buffer</returns>
		public static SourceFileMarkup[] Parse(TokenReader Reader)
		{
			List<SourceFileMarkup> Markup = new List<SourceFileMarkup>();
			if(Reader.MoveNext())
			{
				bool bMoveNext = true;
				while(bMoveNext)
				{
					int StartLineNumber = Reader.LineNumber;
					if (Reader.Current.Type == TokenType.Hash)
					{
						// Create the appropriate markup object for the directive
						SourceFileMarkupType Type = SourceFileMarkupType.OtherDirective;
						if(Reader.MoveNext())
						{
							if(Reader.Current.Type == TokenType.Identifier)
							{
								Identifier Directive = Reader.Current.Identifier!;
								if(Directive == Identifiers.Include)
								{
									Type = SourceFileMarkupType.Include;
								}
								else if(Directive == Identifiers.Define)
								{
									Type = SourceFileMarkupType.Define;
								}
								else if(Directive == Identifiers.Undef)
								{
									Type = SourceFileMarkupType.Undef;
								}
								else if(Directive == Identifiers.If)
								{
									Type = SourceFileMarkupType.If;
								}
								else if(Directive == Identifiers.Ifdef)
								{
									Type = SourceFileMarkupType.Ifdef;
								}
								else if(Directive == Identifiers.Ifndef)
								{
									Type = SourceFileMarkupType.Ifndef;
								}
								else if(Directive == Identifiers.Elif)
								{
									Type = SourceFileMarkupType.Elif;
								}
								else if(Directive == Identifiers.Else)
								{
									Type = SourceFileMarkupType.Else;
								}
								else if(Directive == Identifiers.Endif)
								{
									Type = SourceFileMarkupType.Endif;
								}
								else if(Directive == Identifiers.Pragma)
								{
									Type = SourceFileMarkupType.Pragma;
								}
								else if(Directive == Identifiers.Error)
								{
									Type = SourceFileMarkupType.Error;
								}
								else if(Directive == Identifiers.Warning)
								{
									Type = SourceFileMarkupType.Warning;
								}
							}
							else if(Reader.Current.Type == TokenType.Newline)
							{
								Type = SourceFileMarkupType.Empty;
							}
						}

						// Create the token list
						List<Token> Tokens = new List<Token>();
						if(Type == SourceFileMarkupType.OtherDirective)
						{
							Tokens.Add(Reader.Current);
						}

						// Read the first token
						if(Type == SourceFileMarkupType.Empty)
						{
							bMoveNext = true;
						}
						else if(Type == SourceFileMarkupType.Include)
						{
							bMoveNext = Reader.MoveNextIncludePath();
						}
						else if(Type == SourceFileMarkupType.Error || Type == SourceFileMarkupType.Warning)
						{
							bMoveNext = Reader.MoveNextTokenString();
						}
						else
						{
							bMoveNext = Reader.MoveNext();
						}

						// Read the rest of the tokens
						while(bMoveNext && Reader.Current.Type != TokenType.Newline)
						{
							Tokens.Add(Reader.Current);
							bMoveNext = Reader.MoveNext();
						}

						// Create the markup
						Markup.Add(new SourceFileMarkup(Type, StartLineNumber, Tokens));

						// Move to the next token
						bMoveNext = Reader.MoveNext();
					}
					else if(Reader.Current.Type != TokenType.Newline)
					{
						// Create the new fragment
						Markup.Add(new SourceFileMarkup(SourceFileMarkupType.Text, StartLineNumber, null));

						// Move to the next directive
						bMoveNext = Reader.MoveToNextDirective();
					}
					else
					{
						// Skip the empty line
						bMoveNext = Reader.MoveNext();
					}
				}
			}
			return Markup.ToArray();
		}
	}
}
