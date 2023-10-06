// Copyright Epic Games, Inc. All Rights Reserved.

using System.Collections.Generic;
using System.Linq;
using System.Text;
using Microsoft.VisualStudio.TestTools.UnitTesting;
using UnrealBuildTool;

namespace UnrealBuildToolTests
{
	[TestClass]
	public class PreprocessorStateTests
	{
		[TestMethod]
		public void Run()
		{
			PreprocessorState BaseState = new PreprocessorState();
			BaseState.DefineMacro(ParseMacro("FOO", null, "123"));
			BaseState.DefineMacro(ParseMacro("BAR", new List<string> { "Arg1", "Arg2" }, "Arg1 + Arg2"));
			Assert.AreEqual("m BAR(Arg1, Arg2)=Arg1 + Arg2\nm FOO=123\n", FormatState(BaseState));

			// Modify the state and revert it
			{
				PreprocessorState State = new PreprocessorState(BaseState);
				Assert.AreEqual(FormatState(BaseState), FormatState(State));

				PreprocessorTransform Transform1 = State.BeginCapture();
				State.UndefMacro(Identifier.FindOrAdd("FOO"));
				State.DefineMacro(ParseMacro("FOO2", null, "FOO()"));
				State.PushBranch(PreprocessorBranch.Active);
				Assert.AreEqual("b Active\nm BAR(Arg1, Arg2)=Arg1 + Arg2\nm FOO2=FOO()\n", FormatState(State));
				Assert.AreEqual("+b Active\n+m FOO undef\n+m FOO2=FOO()\n", FormatTransform(Transform1));

				PreprocessorState State2 = new PreprocessorState(BaseState);
				State2.TryToApply(Transform1);
				Assert.AreEqual(FormatState(State), FormatState(State2));

				PreprocessorTransform Transform2 = State.BeginCapture();
				State.PopBranch();
				State.UndefMacro(Identifier.FindOrAdd("FOO"));
				State.DefineMacro(ParseMacro("FOO", null, "123"));
				State.UndefMacro(Identifier.FindOrAdd("FOO2"));
				Assert.AreEqual(FormatState(BaseState), FormatState(State));
				Assert.AreEqual("-b Active\n+m FOO=123\n+m FOO2 undef\n", FormatTransform(Transform2));

				State2.TryToApply(Transform2);
				Assert.AreEqual(FormatState(BaseState), FormatState(State2));
			}

			// Check the tracking of branches
			{
				PreprocessorState State = new PreprocessorState(BaseState);
				PreprocessorTransform Transform1 = State.BeginCapture();
				State.IsCurrentBranchActive();
				Assert.AreEqual("=b Active\n", FormatTransform(Transform1));

				State.PushBranch(PreprocessorBranch.Active);
				PreprocessorTransform Transform2 = State.BeginCapture();
				State.IsCurrentBranchActive();
				State.PopBranch();
				State.EndCapture();
				Assert.AreEqual("-b Active\n", FormatTransform(Transform2));

				State.PushBranch(0);
				Assert.AreEqual("False", State.CanApply(Transform2).ToString());

				State.PopBranch();
				State.PushBranch(PreprocessorBranch.Active);
				Assert.AreEqual("True", State.CanApply(Transform2).ToString());
			}
		}

		static string FormatState(PreprocessorState State)
		{
			StringBuilder Result = new StringBuilder();
			foreach (PreprocessorBranch Branch in State.CurrentBranches)
			{
				Result.AppendFormat("b {0}\n", Branch);
			}
			foreach (PreprocessorMacro Macro in State.CurrentMacros.OrderBy(x => x.Name))
			{
				Result.AppendFormat("m {0}\n", Macro.ToString().TrimEnd());
			}
			return Result.ToString();
		}

		static string FormatTransform(PreprocessorTransform Transform)
		{
			StringBuilder Result = new StringBuilder();
			if (Transform.RequireTopmostActive.HasValue)
			{
				Result.AppendFormat("=b {0}\n", Transform.RequireTopmostActive.Value ? "Active" : "0");
			}
			foreach (PreprocessorBranch Branch in Transform.RequiredBranches)
			{
				Result.AppendFormat("-b {0}\n", Branch);
			}
			foreach (PreprocessorBranch Branch in Transform.NewBranches)
			{
				Result.AppendFormat("+b {0}\n", Branch);
			}
			foreach (KeyValuePair<Identifier, PreprocessorMacro> Macro in Transform.RequiredMacros)
			{
				if (Macro.Value == null)
				{
					Result.AppendFormat("=m {0} undef\n", Macro.Key);
				}
				else
				{
					Result.AppendFormat("=m {0}\n", Macro.Value.ToString().TrimEnd());
				}
			}
			foreach (KeyValuePair<Identifier, PreprocessorMacro> Macro in Transform.NewMacros)
			{
				if (Macro.Value == null)
				{
					Result.AppendFormat("+m {0} undef\n", Macro.Key);
				}
				else
				{
					Result.AppendFormat("+m {0}\n", Macro.Value.ToString().TrimEnd());
				}
			}
			return Result.ToString();
		}

		static PreprocessorMacro ParseMacro(string Name, List<string> Parameters, string Value)
		{
			List<Token> Tokens = new List<Token>();

			using TokenReader Reader = new TokenReader(Value);
			while (Reader.MoveNext())
			{
				Tokens.Add(Reader.Current);
			}

			if (Parameters == null)
			{
				return new PreprocessorMacro(Identifier.FindOrAdd(Name), null, Tokens);
			}
			else
			{
				return new PreprocessorMacro(Identifier.FindOrAdd(Name), Parameters.Select(x => Identifier.FindOrAdd(x)).ToList(), Tokens);
			}
		}
	}
}
