// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using AutomationTool;
using System.Threading;
using System.Text.RegularExpressions;
using System.Threading.Tasks;

namespace Gauntlet
{
	public class Params
	{
		public string[] AllArguments { get; protected set; }

		public Params(IEnumerable<string>InArgs)
		{
			// remove any leading -
			AllArguments = InArgs.Select(S => S.StartsWith("-") ? S.Substring(1) : S).ToArray();
		}

		/// <summary>
		/// Parses the argument list for a parameter and returns whether it is defined or not.
		/// </summary>
		/// <param name="ArgList">Argument list.</param>
		/// <param name="Param">Param to check for.</param>
		/// <returns>True if param was found, false otherwise.</returns>
		public bool ParseParam(string Param)
		{
			foreach (string Arg in AllArguments)
			{
				string StringArg = Arg;

				if (StringArg.ToString().Equals(Param, StringComparison.InvariantCultureIgnoreCase))
				{
					return true;
				}
			}
			return false;
		}

		/// <summary>
		/// Parses multiple values from a param. The multiple values can either be specified as repeated
		/// arguments, e.g. -foo=one -foo=two, or when 'CommaSeparated' is true as a comma-separated 
		/// list, e.g. -foo=one,two, -foo="one, two".
		/// </summary>
		/// <param name="Param"></param>
		/// <param name="CommaSeparated"></param>
		/// <returns></returns>
		public List<string> ParseValues(string Param, bool CommaSeparated=false)
		{
			List<string> FoundValues = new List<string>();

			if (!Param.EndsWith("="))
			{
				Param += "=";
			}
			foreach (string Arg in AllArguments)
			{
				string StringArg = Arg;

				if (StringArg.StartsWith(Param, StringComparison.InvariantCultureIgnoreCase))
				{
					string Value = StringArg.Substring(Param.Length);

					if (CommaSeparated == false)
					{
						FoundValues.Add(Value);
					}
					else
					{
						// split on comma and trim
						FoundValues.AddRange(Value.Split(new char[] { ',' }, StringSplitOptions.RemoveEmptyEntries).Select(S => S.Trim()));

						/*
						 * 
						string[] CSVs = Value.Split(new[] { ',' });
						for (int i = 0; i < CSVs.Length; i++)
						{
							string Item = CSVs[i];

							bool Bracketsmatch = false;
							do
							{
								int BracketCount = Item.Count(c => c == '(') - Item.Count(c => c == ')');

								Bracketsmatch = BracketCount == 0;

								if (!Bracketsmatch)
								{
									i++;

									if (i == CSVs.Length)
									{
										char MissingBracket = BracketCount > 0 ? '(' : ')';
										throw new AutomationException("Missing {0} in param {1}", MissingBracket, Value);
									}
									Item += "," + CSVs[++i];
								}

							} while (!Bracketsmatch);

							FoundValues.Add(Item);
						}*/
					}
				}
			}

			return FoundValues;
		}

		/// <summary>
		/// Parses the argument list for a string parameter and reads its value. 
		/// Ex. ParseParamValue(Args, "map=")
		/// </summary>
		/// <param name="ArgList">Argument list.</param>
		/// <param name="Param">Param to read its value.</param>
		/// <returns>Returns the value or Default if the parameter was not found.</returns>
		public string ParseValue(string Param, string Default = null)
		{
			var Values = ParseValues(Param);

			return Values.Count > 0 ? Values.First() : Default;
		}

		/// <summary>
		/// Parses the argument list for an int parameter and reads its value. 
		/// Ex. ParseParamValue(Args, "timeout=")
		/// </summary>
		/// <param name="ArgList">Argument list.</param>
		/// <param name="Param">Param to read its value.</param>
		/// <returns>Returns the value or Default if the parameter was not found.</returns>
		public int ParseValue(string Param, int Default = 0)
		{
			string Value = ParseValue(Param, Convert.ToString(Default));

			if (Value == null)
			{
				return Default;
			}

			return Convert.ToInt32(Value);
		}

		/// <summary>
		/// Parses the argument list for a float parameter and reads its value. 
		/// Ex. ParseParamValue(Args, "timeout=")
		/// </summary>
		/// <param name="ArgList">Argument list.</param>
		/// <param name="Param">Param to read its value.</param>
		/// <returns>Returns the value or Default if the parameter was not found.</returns>
		public float ParseValue(string Param, float Default = 0)
		{
			string Value = ParseValue(Param, Convert.ToString(Default));

			if (Value == null)
			{
				return Default;
			}

			return Convert.ToSingle(Value);
		}
	}

	public class ArgumentWithParams : Params
	{
		public string Argument { get; protected set; }

		public ArgumentWithParams(string InArgument, IEnumerable<string> InArgs)
			: base(InArgs)
		{
			Argument = InArgument;
		}

		// Turns a string like Test1,Test2(foo,bar=3) into a list of tuples where item1
		// is the name of the string and item2 a Params object
		public static List<ArgumentWithParams> CreateFromString(string Input)
		{
			List<ArgumentWithParams> TestsWithParams = new List<ArgumentWithParams>();

			// turn Name(p1,etc) into a collection of Name|(p1,etc) groups
			MatchCollection Matches = Regex.Matches(Input, @"([^,]+?)(?:\((.+?)\))?(?:,|\z)");

			foreach (Match M in Matches)
			{
				string Name = M.Groups[1].ToString().Trim();

				string Params = M.Groups[2].ToString();

				// to avoid an insane regex parse the params manually so we can deal with comma's in quotes
				List<string> TestArgs = new List<string>();
				string CurrentArg = "";
				// global state while parsing
				bool InQuote = false;

				for (int i = 0; i < Params.Length; i++)
				{
					char C = Params[i];

					bool LastChar = i == Params.Length - 1;

					bool IsQuote = C == '\'';

					// only treat comma's and brackets as special outside of quotes
					bool IsComma = C == ',' && !InQuote;

					// if starting or ending quotes, just flip our state and continue
					if (IsQuote)
					{
						InQuote = !InQuote;
						continue;
					}

					// if this is a comma, wrap up the current arg
					if (IsComma)
					{
						TestArgs.Add(CurrentArg.Trim());
						CurrentArg = "";
					}
					else
					{
						CurrentArg += C;
					}
				}

				// add last arg
				if (CurrentArg.Length > 0)
				{
					TestArgs.Add(CurrentArg.Trim());
				}

				TestsWithParams.Add(new ArgumentWithParams(Name, TestArgs.ToArray()));
			}

			return TestsWithParams;
		}
	}

}
