// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Text;

namespace UnrealBuildBase
{
	public partial class CommandLine
	{
		/// <summary>
		/// Converts a list of arguments to a string where each argument is separated with a space character.
		/// </summary>
		/// <param name="Arguments">Arguments</param>
		/// <returns>Single string containing all arguments separated with a space.</returns>
		public static string FormatCommandLine(IEnumerable<string> Arguments)
		{
			StringBuilder Result = new StringBuilder();
			foreach(string Argument in Arguments)
			{
				if(Result.Length > 0)
				{
					Result.Append(" ");
				}
				Result.Append(FormatArgumentForCommandLine(Argument));
			}
			return Result.ToString();
		}

		/// <summary>
		/// Format a single argument for passing on the command line, inserting quotes as necessary.
		/// </summary>
		/// <param name="Argument">The argument to quote</param>
		/// <returns>The argument, with quotes if necessary</returns>
		public static string FormatArgumentForCommandLine(string Argument)
		{
			// Check if the argument contains a space. If not, we can just pass it directly.
			int SpaceIdx = Argument.IndexOf(' ');
			if(SpaceIdx == -1)
			{
				return Argument;
			}

			// If the argument has quotes in it, the nested quotes must be escaped (unless already escaped)
			if (Argument.Contains('"') && !Argument.Contains("\\\""))
			{
				Argument = Argument.Replace("\"", "\\\"");
			}

			// If it does have a space, and it's formatted as an option (ie. -Something=), try to insert quotes after the equals character
			int EqualsIdx = Argument.IndexOf('=');
			if(Argument.StartsWith("-") && EqualsIdx != -1 && EqualsIdx < SpaceIdx)
			{
				return String.Format("{0}=\"{1}\"", Argument.Substring(0, EqualsIdx), Argument.Substring(EqualsIdx + 1));
			}
			else
			{
				return String.Format("\"{0}\"", Argument);
			}
		}
	}
}
