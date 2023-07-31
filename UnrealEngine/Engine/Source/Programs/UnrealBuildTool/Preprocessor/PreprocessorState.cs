// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Diagnostics.CodeAnalysis;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace UnrealBuildTool
{
	/// <summary>
	/// Flags indicating the state of each nested conditional block in a stack.
	/// </summary>
	[Flags]
	enum PreprocessorBranch
	{
		/// <summary>
		/// The current conditional branch is active.
		/// </summary>
		Active = 0x01,

		/// <summary>
		/// A branch within the current conditional block has been taken. Any #else/#elif blocks will not be taken.
		/// </summary>
		Taken = 0x02,

		/// <summary>
		/// The #else directive for this conditional block has been encountered. An #endif directive is expected next.
		/// </summary>
		Complete = 0x04,

		/// <summary>
		/// The first condition in this branch was an #if directive (as opposed to an #ifdef or #ifndef directive)
		/// </summary>
		HasIfDirective = 0x10,

		/// <summary>
		/// The first condition in this branch was an #if directive (as opposed to an #ifdef or #ifndef directive)
		/// </summary>
		HasIfdefDirective = 0x20,

		/// <summary>
		/// The first condition in this branch was an #ifndef directive (as opposed to an #ifdef or #if directive)
		/// </summary>
		HasIfndefDirective = 0x40,

		/// <summary>
		/// The branch has an #elif directive
		/// </summary>
		HasElifDirective = 0x80,
	}

	/// <summary>
	/// Marshals access to the preprocessor state, ensuring that any changes are tracked within the active transform
	/// </summary>
	class PreprocessorState
	{
		/// <summary>
		/// Stack of conditional branch states
		/// </summary>
		List<PreprocessorBranch> Branches = new List<PreprocessorBranch>();

		/// <summary>
		/// Mapping of name to macro definition
		/// </summary>
		Dictionary<Identifier, PreprocessorMacro> NameToMacro = new Dictionary<Identifier, PreprocessorMacro>();

		/// <summary>
		/// The current transform. Any queries or state modifications will be recorded in this.
		/// </summary>
		PreprocessorTransform? Transform;

		/// <summary>
		/// Enumerates the current branches. Do not use this for decision logic; any dependencies will not be tracked.
		/// </summary>
		public IEnumerable<PreprocessorBranch> CurrentBranches
		{
			get { return Branches; }
		}

		/// <summary>
		/// Enumerates the current macros. Do not use this for decision logic; any dependencies will not be tracked.
		/// </summary>
		public IEnumerable<PreprocessorMacro> CurrentMacros
		{
			get { return NameToMacro.Values; }
		}

		/// <summary>
		/// Initialize an empty preprocessor state
		/// </summary>
		public PreprocessorState()
		{
		}

		/// <summary>
		/// Duplicates another preprocessor state. Throws an exception if a transform is currently being built in the other state.
		/// </summary>
		/// <param name="Other">The preprocessor state to copy</param>
		public PreprocessorState(PreprocessorState Other)
		{
			if(Other.Transform != null)
			{
				throw new Exception("Unable to copy another preprocessor state while a transform is being built.");
			}

			Branches.AddRange(Other.Branches);

			foreach(KeyValuePair<Identifier, PreprocessorMacro> Pair in Other.NameToMacro)
			{
				NameToMacro[Pair.Key] = Pair.Value;
			}
		}

		/// <summary>
		/// Create a new transform object, and assign it to be current
		/// </summary>
		/// <returns>The new transform object</returns>
		public PreprocessorTransform BeginCapture()
		{
			Transform = new PreprocessorTransform();
			return Transform;
		}

		/// <summary>
		/// Detach the current transform
		/// </summary>
		/// <returns>The transform object</returns>
		public PreprocessorTransform? EndCapture()
		{
			PreprocessorTransform? PrevTransform = Transform;
			Transform = null;
			return PrevTransform;
		}

		/// <summary>
		/// Sets a flag indicating that a #pragma once directive was encountered
		/// </summary>
		public void MarkPragmaOnce()
		{
			if(Transform != null)
			{
				Transform.bHasPragmaOnce = true;
			}
		}

		/// <summary>
		/// Set a macro to a specific definition
		/// </summary>
		/// <param name="Macro">Macro definition</param>
		public void DefineMacro(PreprocessorMacro Macro)
		{
			NameToMacro[Macro.Name] = Macro;

			if(Transform != null)
			{
				Transform.NewMacros[Macro.Name] = Macro;
			}
		}

		/// <summary>
		/// Checks if a macro with the given name is defined
		/// </summary>
		/// <param name="Name">The macro name</param>
		/// <returns>True if the macro is defined</returns>
		public bool IsMacroDefined(Identifier Name)
		{
			// Could account for the fact that we don't need the full definition later...
			PreprocessorMacro? Macro;
			return TryGetMacro(Name, out Macro);
		}

		/// <summary>
		/// Removes a macro definition
		/// </summary>
		/// <param name="Name">Name of the macro</param>
		public void UndefMacro(Identifier Name)
		{
			NameToMacro.Remove(Name);

			if(Transform != null)
			{
				Transform.NewMacros[Name] = null;
			}
		}

		/// <summary>
		/// Attemps to get the definition for a macro
		/// </summary>
		/// <param name="Name">Name of the macro</param>
		/// <param name="Macro">Receives the macro definition, or null if it's not defined</param>
		/// <returns>True if the macro is defined, otherwise false</returns>
		public bool TryGetMacro(Identifier Name, [NotNullWhen(true)] out PreprocessorMacro? Macro)
		{
			bool bResult = NameToMacro.TryGetValue(Name, out Macro);

			if(Transform != null && !Transform.NewMacros.ContainsKey(Name))
			{
				Transform.RequiredMacros[Name] = Macro;
			}

			return Macro != null;
		}

		/// <summary>
		/// Pushes the preprocessor branch onto the stack
		/// </summary>
		/// <param name="Branch">The branch state</param>
		public void PushBranch(PreprocessorBranch Branch)
		{
			Branches.Add(Branch);

			if(Transform != null)
			{
				Transform.NewBranches.Add(Branch);
			}
		}

		/// <summary>
		/// Pops a preprocessor branch from the stack
		/// </summary>
		/// <returns>The popped branch state</returns>
		public PreprocessorBranch PopBranch()
		{
			PreprocessorBranch Branch;
			if(!TryPopBranch(out Branch))
			{
				throw new InvalidOperationException("Branch stack is empty");
			}
			return Branch;
		}

		/// <summary>
		/// Attempts to pops a preprocessor branch from the stack
		/// </summary>
		/// <param name="Branch">On success, receives the preprocessor branch state</param>
		/// <returns>True if a branch was active, else false</returns>
		public bool TryPopBranch(out PreprocessorBranch Branch)
		{
			if(Branches.Count == 0)
			{
				Branch = 0;
				return false;
			}
			else
			{
				Branch = Branches[Branches.Count - 1];
				Branches.RemoveAt(Branches.Count - 1);

				if(Transform != null)
				{
					if(Transform.NewBranches.Count > 0)
					{
						Transform.NewBranches.RemoveAt(Transform.NewBranches.Count - 1);
					}
					else
					{
						Transform.RequiredBranches.Add(Branch);
						Transform.bRequireTopmostActive = null;
					}
				}

				return true;
			}
		}

		/// <summary>
		/// Determines if the branch that the preprocessor is in is active
		/// </summary>
		/// <returns>True if the branch is active, false otherwise</returns>
		public bool IsCurrentBranchActive()
		{
			bool bActive = (Branches.Count == 0 || Branches[Branches.Count - 1].HasFlag(PreprocessorBranch.Active));
			if(Transform != null && Transform.NewBranches.Count == 0)
			{
				Transform.bRequireTopmostActive = bActive;
			}
			return bActive;
		}

		/// <summary>
		/// Determines if the given transform can apply to the current preprocessor state
		/// </summary>
		/// <param name="Transform">The transform to test</param>
		/// <returns>True if the transform can be applied to the current state</returns>
		public bool CanApply(PreprocessorTransform Transform)
		{
			// Check all the required branches match
			for(int Idx = 0; Idx < Transform.RequiredBranches.Count; Idx++)
			{
				if(Branches[Branches.Count - Idx - 1] != Transform.RequiredBranches[Idx])
				{
					return false;
				}
			}

			// Check the topmost branch is active
			if(Transform.bRequireTopmostActive.HasValue)
			{
				bool bTopmostActive = (Branches.Count == Transform.RequiredBranches.Count || Branches[Branches.Count - Transform.RequiredBranches.Count - 1].HasFlag(PreprocessorBranch.Active));
				if(Transform.bRequireTopmostActive.Value != bTopmostActive)
				{
					return false;
				}
			}

			// Check all the required macros match
			foreach(KeyValuePair<Identifier, PreprocessorMacro?> RequiredPair in Transform.RequiredMacros)
			{
				PreprocessorMacro? Macro;
				if(NameToMacro.TryGetValue(RequiredPair.Key, out Macro))
				{
					if(RequiredPair.Value == null || !Macro.IsEquivalentTo(RequiredPair.Value))
					{
						return false;
					}
				} 
				else
				{
					if(RequiredPair.Value != null)
					{
						return false;
					}
				}
			}

			return true;
		}

		/// <summary>
		/// Apply the given transform to the current preprocessor state
		/// </summary>
		/// <param name="Transform">The transform to apply</param>
		/// <returns>True if the transform was applied to the current state</returns>
		public bool TryToApply(PreprocessorTransform Transform)
		{
			if(!CanApply(Transform))
			{
				return false;
			}

			// Update the branch state
			Branches.RemoveRange(Branches.Count - Transform.RequiredBranches.Count, Transform.RequiredBranches.Count);
			Branches.AddRange(Transform.NewBranches);

			// Update the macro definitions
			foreach(KeyValuePair<Identifier, PreprocessorMacro?> NewMacro in Transform.NewMacros)
			{
				if(NewMacro.Value == null)
				{
					NameToMacro.Remove(NewMacro.Key);
				}
				else
				{
					NameToMacro[NewMacro.Key] = NewMacro.Value;
				}
			}

			return true;
		}
	}
}
