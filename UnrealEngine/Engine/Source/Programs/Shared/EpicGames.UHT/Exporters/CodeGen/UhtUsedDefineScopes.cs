// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using EpicGames.UHT.Types;
using EpicGames.UHT.Utils;

namespace EpicGames.UHT.Exporters.CodeGen
{

	/// <summary>
	/// Helper class for collection where types exist in different define scope blocks
	/// </summary>
	/// <typeparam name="T"></typeparam>
	public class UhtUsedDefineScopes<T> where T : UhtType
	{
		private bool _first = true;
		private UhtDefineScope _soleScope = UhtDefineScope.Invalid;
		private UhtDefineScope _allScopes = UhtDefineScope.None;
		private uint _present = 0;

		/// <summary>
		/// Collection of instances
		/// </summary>
		public List<T> Instances { get; set; } = new();

		/// <summary>
		/// If true, there are no instances
		/// </summary>
		public bool IsEmpty => Instances.Count == 0;

		/// <summary>
		/// If true, at least one instance had no scope
		/// </summary>
		public bool HasNoneScope => HasScope(UhtDefineScope.None);

		/// <summary>
		/// If all types share the same scope, then the sole scope is that scope.  The value will be Invalid if the
		/// instances have different scopes.
		/// </summary>
		public UhtDefineScope SoleScope => _soleScope;

		/// <summary>
		/// Collection of all scopes found in the types.  If 
		/// </summary>
		public UhtDefineScope AllScopes => _allScopes;

		/// <summary>
		/// If any instance has no scope, then None is returned.  Otherwise AllScopes is returned.
		/// </summary>
		public UhtDefineScope NoneAwareScopes => HasNoneScope ? UhtDefineScope.None : AllScopes;

		/// <summary>
		/// Constructor with no initial instances
		/// </summary>
		public UhtUsedDefineScopes()
		{
		}

		/// <summary>
		/// Constructor with initial range of types
		/// </summary>
		/// <param name="instances">Instances to initially add</param>
		public UhtUsedDefineScopes(IEnumerable<T> instances)
		{
			AddRange(instances);
		}

		/// <summary>
		/// Add an instance to the collection
		/// </summary>
		/// <param name="instance">Instance to be added</param>
		public void Add(T instance)
		{
			UhtDefineScope defineScope = instance.DefineScope;
			if (_first)
			{
				_soleScope = defineScope;
				_first = false;
			}
			else if (_soleScope != UhtDefineScope.Invalid && _soleScope != defineScope)
			{
				_soleScope = UhtDefineScope.Invalid;
			}
			_allScopes |= defineScope;
			_present |= ScopeToMask(defineScope);
			Instances.Add(instance);
		}

		/// <summary>
		/// Add a range of instances to the collection
		/// </summary>
		/// <param name="instances">Collection of instances</param>
		public void AddRange(IEnumerable<T> instances)
		{
			foreach (T instance in instances)
			{
				Add(instance);
			}
		}

		/// <summary>
		/// Check to see if the given scope has instances
		/// </summary>
		/// <param name="defineScope">Scope to test</param>
		/// <returns>True if the scope has elements</returns>
		public bool HasScope(UhtDefineScope defineScope)
		{
			return (_present & ScopeToMask(defineScope)) != 0;
		}

		/// <summary>
		/// Update the list to by ordered by the define scope
		/// </summary>
		public void OrderByDefineScope()
		{
			Instances = Instances.OrderBy(x => x.DefineScope).ToList();
		}

		/// <summary>
		/// Enumerate all of the used defined scopes
		/// </summary>
		/// <returns>Enumeration</returns>
		public IEnumerable<UhtDefineScope> EnumerateDefinedScopes()
		{
			ulong present = _present;
			for (int index = 0; present != 0; ++index, present >>= 1)
			{
				if ((present & 1) != 0)
				{
					yield return (UhtDefineScope)index;
				}
			}
		}

		/// <summary>
		/// Enumerate all of the used defined scopes
		/// </summary>
		/// <returns>Enumeration</returns>
		public IEnumerable<UhtDefineScope> EnumerateDefinedScopesNoneAtEnd()
		{
			ulong present = _present >> 1;
			for (int index = 1; present != 0; ++index, present >>= 1)
			{
				if ((present & 1) != 0)
				{
					yield return (UhtDefineScope)index;
				}
			}

			// At this time, we alway return None
			yield return UhtDefineScope.None;
		}

		private static uint ScopeToMask(UhtDefineScope defineScope)
		{
			return (uint)1 << (int)defineScope;
		}
	}

	/// <summary>
	/// Defines which set of names will be used for each scope
	/// </summary>
	internal enum UhtDefineScopeNames
	{

		/// <summary>
		/// Names match the define
		/// </summary>
		Standard,

		/// <summary>
		/// Use WITH_EDITOR in place of WITH_EDITORONLY_DATA
		/// </summary>
		WithEditor,
	}

	/// <summary>
	/// Helper extensions when working with defined scope collections.
	/// 
	/// This class helps with producing consistent and correct code that works with the DefineScope element in UhtType.
	/// 
	/// In generated header files, it supports two flavors of macro definitions used to generate the body macro.
	/// 
	/// The multi macro style will generate one macro for each unique combination of DefineScope found.  Only the instances
	/// that match the DefineScope will be placed in that macro.  The macros will be generated in such a way that they 
	/// will always be present and each much be included in the generated body macro.  This style is used to generate
	/// declarations as needed.
	/// 
	/// The single macro style will generate a single macro, but that macro will appear multiple times depending on
	/// each combination of the DefineScope combinations required.  Each macro will have a complete set of instances
	/// filtered by the DefineScope.  This style is used to populate such things as enum definitions needed by the engine.
	/// 
	/// In generated cpp files, there is support for enumerating through all the instances and emitting the appropriate
	/// #if block to include instances based on their DefineScope.
	/// </summary>
	internal static class UhtUsedDefineScopesExtensions
	{

		/// <summary>
		/// Append a macro scoped macro
		/// </summary>
		/// <param name="builder">Destination builder</param>
		/// <param name="defineScopeNames">Which set of scope names will be used</param>
		/// <param name="defineScope">Specified scope</param>
		/// <param name="appendAction">Action to invoke to append an instance</param>
		/// <returns>String builder</returns>
		public static StringBuilder AppendScoped(this StringBuilder builder, UhtDefineScopeNames defineScopeNames, UhtDefineScope defineScope, Action<StringBuilder> appendAction)
		{
			using UhtMacroBlockEmitter blockEmitter = new(builder, defineScopeNames, defineScope);
			appendAction(builder);
			return builder;
		}

		/// <summary>
		/// Append a macro scoped macro
		/// </summary>
		/// <param name="builder">Destination builder</param>
		/// <param name="defineScopeNames">Which set of scope names will be used</param>
		/// <param name="defineScope">Specified scope</param>
		/// <param name="generator">Header code generator</param>
		/// <param name="outerType">Output type owning the instances</param>
		/// <param name="macroSuffix">Macro being created</param>
		/// <param name="includeSuffix">If true, include such things as _EOD onto the macro name</param>
		/// <param name="appendAction">Action to invoke to append an instance</param>
		/// <returns>String builder</returns>
		public static StringBuilder AppendScopedMacro(this StringBuilder builder, UhtDefineScopeNames defineScopeNames, UhtDefineScope defineScope,
			UhtHeaderCodeGenerator generator, UhtType outerType, string macroSuffix, bool includeSuffix, Action<StringBuilder> appendAction)
		{
			using (UhtMacroBlockEmitter blockEmitter = new(builder, defineScopeNames, defineScope))
			{
				using (UhtMacroCreator macro = new(builder, generator, outerType, macroSuffix, defineScope, includeSuffix))
				{
					appendAction(builder);
				}

				// We can skip writing the macros if there are no properties to declare, as the 'if' and 'else' would be the same
				if (defineScope != UhtDefineScope.None)
				{
					// Trim the extra newlines added after the macro generator
					if (builder.Length > 4 &&
						builder[^4] == '\r' &&
						builder[^3] == '\n' &&
						builder[^2] == '\r' &&
						builder[^1] == '\n')
					{
						builder.Length -= 4;
					}

					builder.AppendElsePreprocessor(defineScope, defineScopeNames);
					using UhtMacroCreator macro = new(builder, generator, outerType, macroSuffix, defineScope, includeSuffix); // Empty macro
				}
			}

			if (defineScope != UhtDefineScope.None)
			{
				builder.Append("\r\n\r\n");
			}
			return builder;
		}

		/// <summary>
		/// Append multi macros for the given collection of scopes
		/// </summary>
		/// <param name="builder">Destination builder</param>
		/// <param name="instances">Defined scope instances</param>
		/// <param name="defineScopeNames">Which set of scope names will be used</param>
		/// <param name="generator">Header code generator</param>
		/// <param name="outerType">Output type owning the instances</param>
		/// <param name="macroSuffix">Macro being created</param>
		/// <param name="appendAction">Action to invoke to append an instance</param>
		/// <returns>String builder</returns>
		public static StringBuilder AppendMultiMacros<T>(this StringBuilder builder, UhtUsedDefineScopes<T> instances, UhtDefineScopeNames defineScopeNames,
			UhtHeaderCodeGenerator generator, UhtType outerType, string macroSuffix, Action<StringBuilder, IEnumerable<T>> appendAction) where T : UhtType
		{
			foreach (UhtDefineScope defineScope in instances.EnumerateDefinedScopes())
			{
				AppendScopedMacro(builder, defineScopeNames, defineScope, generator, outerType, macroSuffix, true,
					builder => appendAction(builder, instances.Instances.Where(x => x.DefineScope == defineScope)));
			}
			return builder;
		}

		/// <summary>
		/// Append the macro definitions requested
		/// </summary>
		/// <param name="builder">Destination builder</param>
		/// <param name="instances">Defined scope instances</param>
		/// <param name="generator">Header code generator</param>
		/// <param name="outerType">Output type owning the instances</param>
		/// <param name="macroSuffix">Macro being created</param>
		/// <returns>String builder</returns>
		public static StringBuilder AppendMultiMacroRefs<T>(this StringBuilder builder, UhtUsedDefineScopes<T> instances, 
			UhtHeaderCodeGenerator generator, UhtType outerType, string macroSuffix) where T : UhtType
		{
			foreach (UhtDefineScope defineScope in instances.EnumerateDefinedScopes())
			{
				builder.Append('\t').AppendMacroName(generator, outerType, macroSuffix, defineScope).Append(" \\\r\n");
			}
			return builder;
		}

		/// <summary>
		/// Append a single macro, where the macro -same- definition can exist inside of define scopes where
		/// the instances contained are all instances that would be satisfied by a scope.
		/// </summary>
		/// <param name="builder">Destination builder</param>
		/// <param name="instances">Defined scope instances</param>
		/// <param name="defineScopeNames">Which set of scope names will be used</param>
		/// <param name="generator">Header code generator</param>
		/// <param name="outerType">Output type owning the instances</param>
		/// <param name="macroSuffix">Macro being created</param>
		/// <param name="appendAction">Action to invoke to append an instance</param>
		/// <returns>String builder</returns>
		public static StringBuilder AppendSingleMacro<T>(this StringBuilder builder, UhtUsedDefineScopes<T> instances, UhtDefineScopeNames defineScopeNames, 
			UhtHeaderCodeGenerator generator, UhtType outerType, string macroSuffix, Action<StringBuilder, IEnumerable<T>> appendAction) where T : UhtType
		{
			if (instances.SoleScope == UhtDefineScope.None)
			{
				using UhtMacroCreator macro = new(builder, generator, outerType, macroSuffix, UhtDefineScope.None);
				appendAction(builder, instances.Instances);
			}
			else
			{
				bool first = true;
				foreach (UhtDefineScope defineScope in instances.EnumerateDefinedScopesNoneAtEnd())
				{
					if (first)
					{
						builder.AppendIfPreprocessor(defineScope, defineScopeNames);
						first = false;
					}
					else if (defineScope != UhtDefineScope.None)
					{
						builder.AppendElseIfPreprocessor(defineScope, defineScopeNames);
					}
					else
					{
						builder.Append("#else\r\n");
					}
					using (UhtMacroCreator macro = new(builder, generator, outerType, macroSuffix, UhtDefineScope.None))
					{
						appendAction(builder, instances.Instances.Where(x => (x.DefineScope & ~defineScope) == 0));
					}
					if (builder.Length > 4 &&
						builder[^4] == '\r' &&
						builder[^3] == '\n' &&
						builder[^2] == '\r' &&
						builder[^1] == '\n')
					{
						builder.Length -= 4;
					}
				}
				builder.Append("#endif\r\n\r\n\r\n");
			}
			return builder;
		}

		/// <summary>
		/// Invoke the append action if any types are present.  If all types are from the same define scope, then
		/// it will be wrapped with and #if block.
		/// </summary>
		/// <param name="builder">Destination builder</param>
		/// <param name="instances">Defined scope instances</param>
		/// <param name="defineScopeNames">Names to use when outputting the scope</param>
		/// <param name="appendAction">Action to invoke</param>
		/// <returns>String builder</returns>
		public static StringBuilder AppendIfInstances<T>(this StringBuilder builder, UhtUsedDefineScopes<T> instances, UhtDefineScopeNames defineScopeNames, Action<StringBuilder> appendAction) where T : UhtType
		{
			if (!instances.IsEmpty)
			{
				using UhtMacroBlockEmitter blockEmitter = new(builder, defineScopeNames, instances.NoneAwareScopes);
				appendAction(builder);
			}
			return builder;
		}

		/// <summary>
		/// Append each instance to the builder
		/// </summary>
		/// <param name="builder">Destination builder</param>
		/// <param name="instances">Defined scope instances</param>
		/// <param name="defineScopeNames">Names to use when outputting the scope</param>
		/// <param name="appendAction">Action to invoke</param>
		/// <returns>String builder</returns>
		public static StringBuilder AppendInstances<T>(this StringBuilder builder, UhtUsedDefineScopes<T> instances, UhtDefineScopeNames defineScopeNames, Action<StringBuilder, T> appendAction) where T : UhtType
		{
			return builder.AppendInstances(instances, defineScopeNames, null, appendAction, null);
		}

		/// <summary>
		/// Append each instance to the builder
		/// </summary>
		/// <param name="builder">Destination builder</param>
		/// <param name="instances">Defined scope instances</param>
		/// <param name="defineScopeNames">Names to use when outputting the scope</param>
		/// <param name="preambleAction">Action to invoke prior to first instance</param>
		/// <param name="appendAction">Action to invoke</param>
		/// <param name="postambleAction">Action to invoke following all instances</param>
		/// <returns>String builder</returns>
		public static StringBuilder AppendInstances<T>(this StringBuilder builder, UhtUsedDefineScopes<T> instances, UhtDefineScopeNames defineScopeNames,
			Action<StringBuilder>? preambleAction, Action<StringBuilder, T> appendAction, Action<StringBuilder>? postambleAction) where T : UhtType
		{
			if (!instances.IsEmpty)
			{
				using UhtMacroBlockEmitter blockEmitter = new(builder, defineScopeNames, instances.SoleScope);
				preambleAction?.Invoke(builder);
				foreach (T instance in instances.Instances)
				{
					blockEmitter.Set(instance.DefineScope);
					appendAction(builder, instance);
				}

				// Make sure the postable run with the proper initial scope
				blockEmitter.Set(instances.SoleScope);
				postambleAction?.Invoke(builder);
			}
			return builder;
		}

		/// <summary>
		/// Append the given array list and count as arguments to a structure constructor
		/// </summary>
		/// <param name="builder">Destination builder</param>
		/// <param name="instances">Collected instances</param>
		/// <param name="defineScopeNames">Names to use when outputting the scope</param>
		/// <param name="staticsName">Name of the statics section</param>
		/// <param name="arrayName">The name of the arrray</param>
		/// <param name="tabs">Number of tabs to start the line</param>
		/// <param name="endl">Text to end the line</param>
		/// <returns>Destination builder</returns>
		public static StringBuilder AppendArrayPtrAndCountLine<T>(this StringBuilder builder, UhtUsedDefineScopes<T> instances, UhtDefineScopeNames defineScopeNames, string? staticsName, string arrayName, int tabs, string endl) where T : UhtType
		{
			UhtDefineScope noneAwareScopes = instances.NoneAwareScopes;

			// We have no instances in the collection, the list will always be empty
			if (instances.IsEmpty)
			{
				builder
					.AppendTabs(tabs)
					.Append("nullptr, 0")
					.Append(endl);
			}

			// If we have any instances that have no scope, then we can always just reference the array
			else if (noneAwareScopes == UhtDefineScope.None)
			{
				builder
					.AppendTabs(tabs)
					.AppendCppName(staticsName, arrayName)
					.Append(", UE_ARRAY_COUNT(")
					.AppendCppName(staticsName, arrayName)
					.Append(')')
					.Append(endl);
			}

			// If the only scope we have is WITH_EDITORONLY_DATA, then we can use the existing macros
			else if (noneAwareScopes == UhtDefineScope.EditorOnlyData)
			{
				switch (defineScopeNames)
				{
					case UhtDefineScopeNames.Standard:
						builder
							.AppendTabs(tabs)
							.Append("IF_WITH_EDITORONLY_DATA(")
							.AppendCppName(staticsName, arrayName)
							.Append(", nullptr), IF_WITH_EDITORONLY_DATA(UE_ARRAY_COUNT(")
							.AppendCppName(staticsName, arrayName)
							.Append("), 0)")
							.Append(endl);
						break;
					case UhtDefineScopeNames.WithEditor:
						builder
							.AppendTabs(tabs)
							.Append("IF_WITH_EDITOR(")
							.AppendCppName(staticsName, arrayName)
							.Append(", nullptr), IF_WITH_EDITOR(UE_ARRAY_COUNT(")
							.AppendCppName(staticsName, arrayName)
							.Append("), 0)")
							.Append(endl);
						break;
					default:
						throw new UhtIceException("Unexpected define scope names value");
				}
			}

			// Otherwise we have many different scopes but no none scope, must generate #if block
			else
			{
				builder.AppendIfPreprocessor(instances.AllScopes, defineScopeNames);
				builder
					.AppendTabs(tabs)
					.AppendCppName(staticsName, arrayName)
					.Append(", UE_ARRAY_COUNT(")
					.AppendCppName(staticsName, arrayName)
					.Append(')')
					.Append(endl);
				builder.AppendElsePreprocessor(instances.AllScopes, defineScopeNames);
				builder
					.AppendTabs(tabs)
					.Append("nullptr, 0")
					.Append(endl);
				builder.AppendEndIfPreprocessor(instances.AllScopes, defineScopeNames);
			}
			return builder;
		}

		/// <summary>
		/// Append the given array list as arguments to a structure constructor
		/// </summary>
		/// <param name="builder">Destination builder</param>
		/// <param name="instances">Collected instances</param>
		/// <param name="defineScopeNames">Names to use when outputting the scope</param>
		/// <param name="staticsName">Name of the statics section</param>
		/// <param name="arrayName">The name of the arrray</param>
		/// <param name="tabs">Number of tabs to start the line</param>
		/// <param name="endl">Text to end the line</param>
		/// <returns>Destination builder</returns>
		public static StringBuilder AppendArrayPtrLine<T>(this StringBuilder builder, UhtUsedDefineScopes<T> instances, UhtDefineScopeNames defineScopeNames, string? staticsName, string arrayName, int tabs, string endl) where T : UhtType
		{
			UhtDefineScope noneAwareScopes = instances.NoneAwareScopes;

			// We have no instances in the collection, the list will always be empty
			if (instances.IsEmpty)
			{
				builder
					.AppendTabs(tabs)
					.Append("nullptr")
					.Append(endl);
			}

			// If we have any instances that have no scope, then we can always just reference the array
			else if (noneAwareScopes == UhtDefineScope.None)
			{
				builder
					.AppendTabs(tabs)
					.AppendCppName(staticsName, arrayName)
					.Append(endl);
			}

			// If the only scope we have is WITH_EDITORONLY_DATA, then we can use the existing macros
			else if (noneAwareScopes == UhtDefineScope.EditorOnlyData)
			{
				switch (defineScopeNames)
				{
					case UhtDefineScopeNames.Standard:
						builder
							.AppendTabs(tabs)
							.Append("IF_WITH_EDITORONLY_DATA(")
							.AppendCppName(staticsName, arrayName)
							.Append(", nullptr)")
							.Append(endl);
						break;
					case UhtDefineScopeNames.WithEditor:
						builder
							.AppendTabs(tabs)
							.Append("IF_WITH_EDITOR(")
							.AppendCppName(staticsName, arrayName)
							.Append(", nullptr)")
							.Append(endl);
						break;
					default:
						throw new UhtIceException("Unexpected define scope names value");
				}
			}

			// Otherwise we have many different scopes but no none scope, must generate #if block
			else
			{
				builder.AppendIfPreprocessor(instances.AllScopes, defineScopeNames);
				builder
					.AppendTabs(tabs)
					.AppendCppName(staticsName, arrayName)
					.Append(endl);
				builder.AppendElsePreprocessor(instances.AllScopes, defineScopeNames);
				builder
					.AppendTabs(tabs)
					.Append("nullptr")
					.Append(endl);
				builder.AppendEndIfPreprocessor(instances.AllScopes, defineScopeNames);
			}
			return builder;
		}

		/// <summary>
		/// Append the given array count as arguments to a structure constructor
		/// </summary>
		/// <param name="builder">Destination builder</param>
		/// <param name="instances">Collected instances</param>
		/// <param name="defineScopeNames">Names to use when outputting the scope</param>
		/// <param name="staticsName">Name of the statics section</param>
		/// <param name="arrayName">The name of the arrray</param>
		/// <param name="tabs">Number of tabs to start the line</param>
		/// <param name="endl">Text to end the line</param>
		/// <returns>Destination builder</returns>
		public static StringBuilder AppendArrayCountLine<T>(this StringBuilder builder, UhtUsedDefineScopes<T> instances, UhtDefineScopeNames defineScopeNames, string? staticsName, string arrayName, int tabs, string endl) where T : UhtType
		{
			UhtDefineScope noneAwareScopes = instances.NoneAwareScopes;

			// We have no instances in the collection, the list will always be empty
			if (instances.IsEmpty)
			{
				builder
					.AppendTabs(tabs)
					.Append('0')
					.Append(endl);
			}

			// If we have any instances that have no scope, then we can always just reference the array
			else if (noneAwareScopes == UhtDefineScope.None)
			{
				builder
					.AppendTabs(tabs)
					.Append("UE_ARRAY_COUNT(")
					.AppendCppName(staticsName, arrayName)
					.Append(')')
					.Append(endl);
			}

			// If the only scope we have is WITH_EDITORONLY_DATA, then we can use the existing macros
			else if (noneAwareScopes == UhtDefineScope.EditorOnlyData)
			{
				switch (defineScopeNames)
				{
					case UhtDefineScopeNames.Standard:
						builder
							.AppendTabs(tabs)
							.Append("IF_WITH_EDITORONLY_DATA(UE_ARRAY_COUNT(")
							.AppendCppName(staticsName, arrayName)
							.Append("), 0)")
							.Append(endl);
						break;
					case UhtDefineScopeNames.WithEditor:
						builder
							.AppendTabs(tabs)
							.Append("IF_WITH_EDITOR(UE_ARRAY_COUNT(")
							.AppendCppName(staticsName, arrayName)
							.Append("), 0)")
							.Append(endl);
						break;
					default:
						throw new UhtIceException("Unexpected define scope names value");
				}
			}

			// Otherwise we have many different scopes but no none scope, must generate #if block
			else
			{
				builder.AppendIfPreprocessor(instances.AllScopes, defineScopeNames);
				builder
					.AppendTabs(tabs)
					.Append("UE_ARRAY_COUNT(")
					.AppendCppName(staticsName, arrayName)
					.Append(')')
					.Append(endl);
				builder.AppendElsePreprocessor(instances.AllScopes, defineScopeNames);
				builder
					.AppendTabs(tabs)
					.Append('0')
					.Append(endl);
				builder.AppendEndIfPreprocessor(instances.AllScopes, defineScopeNames);
			}
			return builder;
		}

		/// <summary>
		/// Start an #if block with the given scope
		/// </summary>
		/// <param name="builder">String builder</param>
		/// <param name="defineScope">Scope</param>
		/// <param name="defineScopeNames">Which set of scope names will be used</param>
		/// <returns>String builder</returns>
		public static StringBuilder AppendIfPreprocessor(this StringBuilder builder, UhtDefineScope defineScope, UhtDefineScopeNames defineScopeNames = UhtDefineScopeNames.Standard)
		{
			if (defineScope != UhtDefineScope.None && defineScope != UhtDefineScope.Invalid)
			{
				builder.Append("#if ").AppendScopeExpression(defineScope, defineScopeNames).Append("\r\n");
			}
			return builder;
		}

		/// <summary>
		/// Start an #else block with the given scope
		/// </summary>
		/// <param name="builder">String builder</param>
		/// <param name="defineScope">Scope</param>
		/// <param name="defineScopeNames">Which set of scope names will be used</param>
		/// <returns>String builder</returns>
		public static StringBuilder AppendElsePreprocessor(this StringBuilder builder, UhtDefineScope defineScope, UhtDefineScopeNames defineScopeNames = UhtDefineScopeNames.Standard)
		{
			if (defineScope != UhtDefineScope.None && defineScope != UhtDefineScope.Invalid)
			{
				builder.Append("#else // ").AppendScopeExpression(defineScope, defineScopeNames).Append("\r\n");
			}
			return builder;
		}

		/// <summary>
		/// Start an #elif block with the given scope
		/// </summary>
		/// <param name="builder">String builder</param>
		/// <param name="defineScope">Scope</param>
		/// <param name="defineScopeNames">Which set of scope names will be used</param>
		/// <returns>String builder</returns>
		public static StringBuilder AppendElseIfPreprocessor(this StringBuilder builder, UhtDefineScope defineScope, UhtDefineScopeNames defineScopeNames = UhtDefineScopeNames.Standard)
		{
			if (defineScope != UhtDefineScope.None && defineScope != UhtDefineScope.Invalid)
			{
				builder.Append("#elif ").AppendScopeExpression(defineScope, defineScopeNames).Append("\r\n");
			}
			return builder;
		}

		/// <summary>
		/// Start an #endif block with the given scope
		/// </summary>
		/// <param name="builder">String builder</param>
		/// <param name="defineScope">Scope</param>
		/// <param name="defineScopeNames">Which set of scope names will be used</param>
		/// <returns>String builder</returns>
		public static StringBuilder AppendEndIfPreprocessor(this StringBuilder builder, UhtDefineScope defineScope, UhtDefineScopeNames defineScopeNames = UhtDefineScopeNames.Standard)
		{
			if (defineScope != UhtDefineScope.None && defineScope != UhtDefineScope.Invalid)
			{
				builder.Append("#endif // ").AppendScopeExpression(defineScope, defineScopeNames).Append("\r\n");
			}
			return builder;
		}

		/// <summary>
		/// Append scope expression (i.e. WITH_X || WITH_Y || ...)
		/// </summary>
		/// <param name="builder">String builder</param>
		/// <param name="defineScope">Scope</param>
		/// <param name="defineScopeNames">Which set of scope names will be used</param>
		/// <returns>String builder</returns>
		public static StringBuilder AppendScopeExpression(this StringBuilder builder, UhtDefineScope defineScope, UhtDefineScopeNames defineScopeNames)
		{
			if (defineScope != UhtDefineScope.None && defineScope != UhtDefineScope.Invalid)
			{
				bool needOr = false;
				switch (defineScopeNames)
				{
					case UhtDefineScopeNames.Standard:
						builder
							.AppendName(defineScope, UhtDefineScope.EditorOnlyData, "WITH_EDITORONLY_DATA", ref needOr)
							.AppendName(defineScope, UhtDefineScope.VerseVM, "WITH_VERSE_VM", ref needOr)
							;
						break;
					case UhtDefineScopeNames.WithEditor:
						builder
							.AppendName(defineScope, UhtDefineScope.EditorOnlyData, "WITH_EDITOR", ref needOr)
							.AppendName(defineScope, UhtDefineScope.VerseVM, "WITH_VERSE_VM", ref needOr)
							;
						break;
					default:
						throw new UhtIceException("Unexpected define scope names value");
				}
			}
			return builder;
		}

		private static StringBuilder AppendCppName(this StringBuilder builder, string? staticsName, string arrayName)
		{
			if (!String.IsNullOrEmpty(staticsName))
			{
				builder.Append(staticsName).Append("::");
			}
			return builder.Append(arrayName);
		}

		private static StringBuilder AppendName(this StringBuilder builder, UhtDefineScope defineScope, UhtDefineScope testScope, string name, ref bool needOr)
		{
			if (defineScope.HasAnyFlags(testScope))
			{
				if (needOr)
				{
					builder.Append("|| ");
				}
				builder.Append(name);
				needOr = true;
			}
			return builder;
		}
	}
}
