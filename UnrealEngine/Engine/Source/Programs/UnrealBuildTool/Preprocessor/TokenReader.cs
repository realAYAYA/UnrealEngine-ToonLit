// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace UnrealBuildTool
{
	/// <summary>
	/// Tokenizer for C++ source files. Provides functions for navigating a source file skipping whitespace, comments, and so on when required.
	/// </summary>
	class TokenReader : IEnumerator<Token>
	{
		/// <summary>
		/// Flags used to classify the first character in a token
		/// </summary>
		enum FirstCharacterClass : byte
		{
			Unknown,
			Terminator,
			Whitespace,
			Newline,
			Identifier,
			Number,
			Character,
			String,
			Dot,
			QuestionMark,
			Colon,
			ExclamationMark,
			Pipe,
			Ampersand,
			Caret,
			Equals,
			LeftTriangleBracket,
			RightTriangleBracket,
			Plus,
			Minus,
			Star,
			Slash,
			PercentSign,
			Tilde,
			LeftParen,
			RightParen,
			Hash,
			Comma,
		}

		/// <summary>
		/// Flags used to classify subsequent characters in a token
		/// </summary>
		[Flags]
		enum CharacterFlags : byte
		{
			Identifier = 1,
			Digit = 2,
			NumberTail = 4,
			Whitespace = 8,
		}

		/// <summary>
		/// Map of utf-8 leading bytes to their class
		/// </summary>
		static FirstCharacterClass[] FirstCharacters;

		/// <summary>
		/// Array of flags for different leading utf-8 sequences
		/// </summary>
		static CharacterFlags[] Characters;

		/// <summary>
		/// The current buffer being read from. Encoded as UTF-8 with a null terminator.
		/// </summary>
		byte[] Data;

		/// <summary>
		/// Current offset within the buffer
		/// </summary>
		int Offset;

		/// <summary>
		/// The current token
		/// </summary>
		Token? CurrentToken;

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="Data">The raw byte stream to read from, encoded as UTF-8</param>
		public TokenReader(byte[] Data)
		{
			this.Data = Data;
			this.LineNumber = 0;
			this.LineNumberAfterToken = 1;

			// Make sure the input data has a null terminator
			if(Data.Length == 0 || Data[Data.Length - 1] != 0)
			{
				throw new ArgumentException("Data parameter must be null terminated.");
			}

			// If the data contains a UTF-8 BOM, skip over it
			if(Data[0] == 0xef && Data[1] == 0xbb && Data[2] == 0xbf)
			{
				Offset = 3;
			}
		}

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="Text">The text to read from</param>
		public TokenReader(string Text)
			: this(GetNullTerminatedByteArray(Text))
		{
		}

		/// <summary>
		/// Copy constructor
		/// </summary>
		/// <param name="Other">Token reader to copy from</param>
		public TokenReader(TokenReader Other)
		{
			this.Data = Other.Data;
			this.Offset = Other.Offset;
			this.LineNumber = Other.LineNumber;
			this.LineNumberAfterToken = Other.LineNumberAfterToken;
		}

		/// <summary>
		/// Gets a null terminated byte array from the given string
		/// </summary>
		/// <param name="Text">String to convert into bytes</param>
		/// <returns>Array of bytes</returns>
		public static byte[] GetNullTerminatedByteArray(string Text)
		{
			byte[] Bytes = new byte[Encoding.UTF8.GetByteCount(Text) + 1];
			Encoding.UTF8.GetBytes(Text, 0, Text.Length, Bytes, 0);
			return Bytes;
		}

		/// <summary>
		/// Tokenize an input string
		/// </summary>
		/// <param name="Text">Text to tokenize</param>
		/// <returns>List of tokens parsed from the text</returns>
		public static List<Token> Tokenize(string Text)
		{
			List<Token> Tokens = new List<Token>();

			TokenReader Reader = new TokenReader(Text);
			while(Reader.MoveNext())
			{
				Tokens.Add(Reader.Current);
			}

			return Tokens;
		}

		/// <summary>
		/// Current line number (one-based)
		/// </summary>
		public int LineNumber
		{
			get;
			private set;
		}

		/// <summary>
		/// Line number at the end of this token (one-based)
		/// </summary>
		public int LineNumberAfterToken
		{
			get;
			private set;
		}

		/// <summary>
		/// Returns the current token
		/// </summary>
		public Token Current
		{
			get { return CurrentToken!; }
		}

		/// <summary>
		/// Untyped implementation of Current for IEnumerator.
		/// </summary>
		object IEnumerator.Current
		{
			get { return Current; }
		}

		/// <summary>
		/// Override of IEnumerator.Dispose. Not required.
		/// </summary>
		void IDisposable.Dispose()
		{
		}

		/// <summary>
		/// Move to the next token
		/// </summary>
		/// <returns>True if the reader could move to the next token, false otherwise</returns>
		public bool MoveNext()
		{
			// Skip past the leading whitespace
			TokenFlags Flags = SkipWhitespace();

			// Update the line number to the one after the current token
			LineNumber = LineNumberAfterToken;

			// Capture the start of the token
			int StartOffset = Offset;

			// Initialize the flags for the next token
			byte FirstByte = Data[Offset++];
			switch(FirstCharacters[FirstByte])
			{
				case FirstCharacterClass.Unknown:
					if((FirstByte & 0x80) != 0)
					{
						while((Data[Offset] & 0x80) != 0)
						{
							Offset++;
						}
					}
					CurrentToken = CreateLiteral(TokenType.Unknown, Flags, StartOffset, Offset - StartOffset);
					return true;
				case FirstCharacterClass.Terminator:
					Offset = StartOffset;
					CurrentToken = new Token(TokenType.End, Flags);
					return false;
				case FirstCharacterClass.Newline:
					LineNumberAfterToken++;
					CurrentToken = new Token(TokenType.Newline, Flags);
					return true;
				case FirstCharacterClass.Identifier:
					// Identifier (or text literal with prefix)
					while((Characters[Data[Offset]] & CharacterFlags.Identifier) != 0)
					{
						Offset++;
					}

					// Check if it's a prefixed text literal
					if(Data[Offset] == '\'')
					{
						Offset++;
						SkipTextLiteral('\'');
						CurrentToken = CreateLiteral(TokenType.Character, Flags, StartOffset, Offset - StartOffset);
						return true;
					}
					else if(Data[Offset] == '\"')
					{
						Offset++;
						SkipTextLiteral('\"');
						CurrentToken = CreateLiteral(TokenType.String, Flags, StartOffset, Offset - StartOffset);
						return true;
					}
					else
					{
						string Name = Encoding.UTF8.GetString(Data, StartOffset, Offset - StartOffset);
						Identifier Identifier = Identifier.FindOrAdd(Name);
						CurrentToken = new Token(Identifier, Flags);
						return true;
					}
				case FirstCharacterClass.Number:
					// Numeric literal
					SkipNumericLiteral();
					CurrentToken = CreateLiteral(TokenType.Number, Flags, StartOffset, Offset - StartOffset);
					return true;
				case FirstCharacterClass.Character:
					// Character literal
					SkipTextLiteral('\'');
					CurrentToken = CreateLiteral(TokenType.Character, Flags, StartOffset, Offset - StartOffset);
					return true;
				case FirstCharacterClass.String:
					// String literal
					SkipTextLiteral('\"');
					CurrentToken = CreateLiteral(TokenType.String, Flags, StartOffset, Offset - StartOffset);
					return true;
				case FirstCharacterClass.Dot:
					// Numeric literal, ellipsis, or dot
					if((Characters[Data[Offset]] & CharacterFlags.Digit) != 0)
					{
						Offset++;
						SkipNumericLiteral();
						CurrentToken = CreateLiteral(TokenType.Number, Flags, StartOffset, Offset - StartOffset);
						return true;
					}
					else if(Data[Offset] == '.' && Data[Offset + 1] == '.')
					{
						Offset += 2;
						CurrentToken = new Token(TokenType.Ellipsis, Flags);
						return true;
					}
					else
					{
						CurrentToken = new Token(TokenType.Dot, Flags);
						return true;
					}
				case FirstCharacterClass.QuestionMark:
					CurrentToken = new Token(TokenType.QuestionMark, Flags);
					return true;
				case FirstCharacterClass.Colon:
					CurrentToken = new Token(TokenType.Colon, Flags);
					return true;
				case FirstCharacterClass.ExclamationMark:
					if(ReadCharacter('='))
					{
						CurrentToken = new Token(TokenType.CompareNotEqual, Flags);
					}
					else
					{
						CurrentToken = new Token(TokenType.LogicalNot, Flags);
					}
					return true;
				case FirstCharacterClass.Pipe:
					if(ReadCharacter('|'))
					{
						CurrentToken = new Token(TokenType.LogicalOr, Flags);
					}
					else
					{
						CurrentToken = new Token(TokenType.BitwiseOr, Flags);
					}
					return true;
				case FirstCharacterClass.Ampersand:
					if(ReadCharacter('&'))
					{
						CurrentToken = new Token(TokenType.LogicalAnd, Flags);
					}
					else
					{
						CurrentToken = new Token(TokenType.BitwiseAnd, Flags);
					}
					return true;
				case FirstCharacterClass.Caret:
					CurrentToken = new Token(TokenType.BitwiseXor, Flags);
					return true;
				case FirstCharacterClass.Equals:
					if(ReadCharacter('='))
					{
						CurrentToken = new Token(TokenType.CompareEqual, Flags);
					}
					else
					{
						CurrentToken = new Token(TokenType.Equals, Flags);
					}
					return true;
				case FirstCharacterClass.LeftTriangleBracket:
					if(ReadCharacter('<'))
					{
						CurrentToken = new Token(TokenType.LeftShift, Flags);
					}
					else if(ReadCharacter('='))
					{
						CurrentToken = new Token(TokenType.CompareLessOrEqual, Flags);
					}
					else
					{
						CurrentToken = new Token(TokenType.CompareLess, Flags);
					}
					return true;
				case FirstCharacterClass.RightTriangleBracket:
					if(ReadCharacter('>'))
					{
						CurrentToken = new Token(TokenType.RightShift, Flags);
					}
					else if(ReadCharacter('='))
					{
						CurrentToken = new Token(TokenType.CompareGreaterOrEqual, Flags);
					}
					else
					{
						CurrentToken = new Token(TokenType.CompareGreater, Flags);
					}
					return true;
				case FirstCharacterClass.Plus:
					CurrentToken = new Token(TokenType.Plus, Flags);
					return true;
				case FirstCharacterClass.Minus:
					CurrentToken = new Token(TokenType.Minus, Flags);
					return true;
				case FirstCharacterClass.Star:
					CurrentToken = new Token(TokenType.Multiply, Flags);
					return true;
				case FirstCharacterClass.Slash:
					CurrentToken = new Token(TokenType.Divide, Flags);
					return true;
				case FirstCharacterClass.PercentSign:
					CurrentToken = new Token(TokenType.Modulo, Flags);
					return true;
				case FirstCharacterClass.Tilde:
					CurrentToken = new Token(TokenType.BitwiseNot, Flags);
					return true;
				case FirstCharacterClass.LeftParen:
					CurrentToken = new Token(TokenType.LeftParen, Flags);
					return true;
				case FirstCharacterClass.RightParen:
					CurrentToken = new Token(TokenType.RightParen, Flags);
					return true;
				case FirstCharacterClass.Hash:
					if(ReadCharacter('#'))
					{
						CurrentToken = new Token(TokenType.HashHash, Flags);
					}
					else
					{
						CurrentToken = new Token(TokenType.Hash, Flags);
					}
					return true;
				case FirstCharacterClass.Comma:
					CurrentToken = new Token(TokenType.Comma, Flags);
					return true;
				default:
					throw new NotImplementedException();
			}
		}

		/// <summary>
		/// Move to the next token, parsing a single literal token string until the end of the line (in the form used by error directives)
		/// </summary>
		/// <returns>True if a token was parsed</returns>
		public bool MoveNextTokenString()
		{
			TokenFlags Flags = SkipWhitespace();

			int StartOffset = Offset;
			while(Data[Offset] != 0 && Data[Offset] != '\n')
			{
				Offset++;
			}

			CurrentToken = CreateLiteral(TokenType.StringOfTokens, TokenFlags.None, StartOffset, Offset - StartOffset);
			return true;
		}

		/// <summary>
		/// Move to the next token, allowing include directives in triangle brackets in the style permitted by system include directives
		/// </summary>
		/// <returns>True if a token was read</returns>
		public bool MoveNextIncludePath()
		{
			bool bResult = MoveNext();
			if(bResult && Current.Type == TokenType.CompareLess)
			{
				int StartOffset = Offset - 1;
				for(int EndOffset = Offset; Data[EndOffset] != 0 && Data[EndOffset] != '\n'; EndOffset++)
				{
					if(Data[EndOffset] == '>')
					{
						CurrentToken = CreateLiteral(TokenType.SystemInclude, TokenFlags.None, StartOffset, EndOffset + 1 - StartOffset);
						Offset = EndOffset + 1;
						break;
					}
				}
			}
			return bResult;
		}

		/// <summary>
		/// Creates a literal token
		/// </summary>
		/// <param name="Type">Type of token to create</param>
		/// <param name="Flags">Flags for the token</param>
		/// <param name="Offset">Offset of the literal within the source data stream</param>
		/// <param name="Length">Length of the literal</param>
		/// <returns>New token for the literal</returns>
		private Token CreateLiteral(TokenType Type, TokenFlags Flags, int Offset, int Length)
		{
			byte[] Literal = new byte[Length];
			for(int Idx = 0; Idx < Length; Idx++)
			{
				Literal[Idx] = Data[Offset + Idx];
			}
			return new Token(Type, Flags, Literal);
		}

		/// <summary>
		/// Scan ahead until we reach the next directive (a hash token after a newline). Assumes that the current line does not contain a directive.
		/// </summary>
		/// <returns>True if we were able to find another token</returns>
		public bool MoveToNextDirective()
		{
			// Scan lines until we reach a directive
			for(;;)
			{
				// Move to the next newline
				while(Data[Offset] != '\n')
				{
					if(Data[Offset] == 0)
					{
						return false;
					}
					else if(Data[Offset] == '\\' && Data[Offset + 1] == '\r' && Data[Offset + 2] == '\n')
					{
						LineNumberAfterToken++;
						Offset += 3;
					}
					else if(Data[Offset] == '\\' && Data[Offset + 1] == '\n')
					{
						LineNumberAfterToken++;
						Offset += 2;
					}
					else if (Data[Offset] == '/' && Data[Offset + 1] == '*')
					{
						Offset += 2;
						for(; Data[Offset] != 0; Offset++)
						{
							if(Data[Offset] == '\n')
							{
								LineNumberAfterToken++;
								continue;
							}
							if(Data[Offset] == '*' && Data[Offset + 1] == '/')
							{
								Offset += 2;
								break;
							}
						}
					}
					else
					{
						Offset++;
					}
				}

				// Move past the newline
				Offset++;
				LineNumberAfterToken++;

				// Skip any horizontal whitespace
				TokenFlags Flags = SkipWhitespace();

				// Check if this is a line marker
				if(Data[Offset] == '#' && Data[Offset + 1] != '#')
				{
					Offset++;
					LineNumber = LineNumberAfterToken;
					CurrentToken = new Token(TokenType.Hash, Flags);
					return true;
				}
			}
		}

		/// <summary>
		/// Definition of IEnumerator.Reset(). Not supported.
		/// </summary>
		void IEnumerator.Reset()
		{
			throw new NotSupportedException();
		}

		/// <summary>
		/// Attempts to read a given character from the stream
		/// </summary>
		/// <param name="Character">Character to read</param>
		/// <returns>True if the character was read (and the current position was updated)</returns>
		bool ReadCharacter(char Character)
		{
			if(Data[Offset] == Character)
			{
				Offset++;
				return true;
			}
			return false;
		}

		/// <summary>
		/// Advances the given position past any horizontal whitespace or comments
		/// </summary>
		/// <returns>Flags for the following token</returns>
		TokenFlags SkipWhitespace()
		{
			TokenFlags Flags = TokenFlags.None;
			for(;;)
			{
				// Quickly skip over trivial whitespace
				while((Characters[Data[Offset]] & CharacterFlags.Whitespace) != 0)
				{
					Offset++;
					Flags |= TokenFlags.HasLeadingSpace;
				}

				// Look at what's next
				char Character = (char)Data[Offset];
				if (Character == '\\')
				{
					byte NextCharacter = Data[Offset + 1];
					if(NextCharacter == '\r' && Data[Offset + 2] == '\n')
					{
						LineNumberAfterToken++;
						Offset += 3;
					}
					else if (NextCharacter == '\n')
					{
						LineNumberAfterToken++;
						Offset += 2;
					}
					else
					{
						break;
					}
				}
				else if (Character == '/')
				{
					byte NextCharacter = Data[Offset + 1];
					if (NextCharacter == '/')
					{
						Offset += 2;
						for(; Data[Offset] != 0; Offset++)
						{
							if(Data[Offset] == '\n')
							{
								break;
							}
						}
						Flags |= TokenFlags.HasLeadingSpace;
					}
					else if (NextCharacter == '*')
					{
						Offset += 2;
						for(; Data[Offset] != 0; Offset++)
						{
							if(Data[Offset] == '\n')
							{
								LineNumberAfterToken++;
								continue;
							}
							if(Data[Offset] == '*' && Data[Offset + 1] == '/')
							{
								Offset += 2;
								break;
							}
						}
						Flags |= TokenFlags.HasLeadingSpace;
					}
					else
					{
						break;
					}
				}
				else
				{
					if(Character == '\r')
					{
						Offset++;
					}
					else
					{
						break;
					}
				}
			}
			return Flags;
		}

		/// <summary>
		/// Skip past a text literal (a quoted character literal or string literal)
		/// </summary>
		/// <param name="LastCharacter">The terminating character to look for, ignoring escape sequences</param>
		void SkipTextLiteral(char LastCharacter)
		{
			for(;;)
			{
				char Character = (char)Data[Offset];
				if(Character == '\0')
				{
					throw new Exception("Unexpected end of file in text literal");
				}

				Offset++;

				if(Character == '\\' && Data[Offset] != 0)
				{
					Offset++;
				}
				else if(Character == LastCharacter)
				{
					break;
				}
			}
		}

		/// <summary>
		/// Skips over a numeric literal after the initial digit or dot/digit pair.
		/// </summary>
		void SkipNumericLiteral()
		{
			for (;;)
			{
				while((Characters[Data[Offset]] & CharacterFlags.NumberTail) != 0)
				{
					Offset++;
				}
				if((Data[Offset] == '+' || Data[Offset] == '-') && (Data[Offset - 1] == 'e' || Data[Offset - 1] == 'E'))
				{
					Offset++;
				}
				else
				{
					break;
				}
			}
		}

		/// <summary>
		/// Static constructor. Initializes the lookup tables used by the lexer.
		/// </summary>
		static TokenReader()
		{
			FirstCharacters = new FirstCharacterClass[256];

			// End of file
			FirstCharacters[0] = FirstCharacterClass.Terminator;

			// Horizontal whitespace
			FirstCharacters[' '] = FirstCharacterClass.Whitespace;
			FirstCharacters['\t'] = FirstCharacterClass.Whitespace;
			FirstCharacters['\v'] = FirstCharacterClass.Whitespace;
			FirstCharacters['\r'] = FirstCharacterClass.Whitespace;

			// Newline
			FirstCharacters['\n'] = FirstCharacterClass.Newline;

			// Identifiers
			FirstCharacters['_'] = FirstCharacterClass.Identifier;
			for(int Idx = 'a'; Idx <= 'z'; Idx++)
			{
				FirstCharacters[Idx] = FirstCharacterClass.Identifier;
			}
			for(int Idx = 'A'; Idx <= 'Z'; Idx++)
			{
				FirstCharacters[Idx] = FirstCharacterClass.Identifier;
			}

			// Numeric literals
			for(int Idx = '0'; Idx <= '9'; Idx++)
			{
				FirstCharacters[Idx] = FirstCharacterClass.Number;
			}

			// Character literals
			FirstCharacters['\''] = FirstCharacterClass.Character;

			// String literals
			FirstCharacters['\"'] = FirstCharacterClass.String;

			// Other symbols
			FirstCharacters['.'] = FirstCharacterClass.Dot;
			FirstCharacters['?'] = FirstCharacterClass.QuestionMark;
			FirstCharacters[':'] = FirstCharacterClass.Colon;
			FirstCharacters['!'] = FirstCharacterClass.ExclamationMark;
			FirstCharacters['|'] = FirstCharacterClass.Pipe;
			FirstCharacters['&'] = FirstCharacterClass.Ampersand;
			FirstCharacters['^'] = FirstCharacterClass.Caret;
			FirstCharacters['='] = FirstCharacterClass.Equals;
			FirstCharacters['<'] = FirstCharacterClass.LeftTriangleBracket;
			FirstCharacters['>'] = FirstCharacterClass.RightTriangleBracket;
			FirstCharacters['+'] = FirstCharacterClass.Plus;
			FirstCharacters['-'] = FirstCharacterClass.Minus;
			FirstCharacters['*'] = FirstCharacterClass.Star;
			FirstCharacters['/'] = FirstCharacterClass.Slash;
			FirstCharacters['%'] = FirstCharacterClass.PercentSign;
			FirstCharacters['~'] = FirstCharacterClass.Tilde;
			FirstCharacters['('] = FirstCharacterClass.LeftParen;
			FirstCharacters[')'] = FirstCharacterClass.RightParen;
			FirstCharacters['#'] = FirstCharacterClass.Hash;
			FirstCharacters[','] = FirstCharacterClass.Comma;

			// Flags for secondary characters
			Characters = new CharacterFlags[256];

			// Identifiers
			Characters['_'] |= CharacterFlags.Identifier;
			for(int Idx = 'a'; Idx <= 'z'; Idx++)
			{
				Characters[Idx] |= CharacterFlags.Identifier;
			}
			for(int Idx = 'A'; Idx <= 'Z'; Idx++)
			{
				Characters[Idx] |= CharacterFlags.Identifier;
			}
			for(int Idx = '0'; Idx <= '9'; Idx++)
			{
				Characters[Idx] |= CharacterFlags.Identifier;
			}

			// Numbers
			for(int Idx = '0'; Idx <= '9'; Idx++)
			{
				Characters[Idx] |= CharacterFlags.Digit;
			}

			// Preprocessing number tail
			Characters['.'] |= CharacterFlags.NumberTail;
			Characters['_'] |= CharacterFlags.NumberTail;
			for(int Idx = '0'; Idx <= '9'; Idx++)
			{
				Characters[Idx] |= CharacterFlags.NumberTail;
			}
			for(int Idx = 'a'; Idx <= 'z'; Idx++)
			{
				Characters[Idx] |= CharacterFlags.NumberTail;
			}
			for(int Idx = 'A'; Idx <= 'Z'; Idx++)
			{
				Characters[Idx] |= CharacterFlags.NumberTail;
			}

			// Whitespace
			Characters[' '] |= CharacterFlags.Whitespace;
			Characters['\t'] |= CharacterFlags.Whitespace;
			Characters['\v'] |= CharacterFlags.Whitespace;
		}
	}
}
