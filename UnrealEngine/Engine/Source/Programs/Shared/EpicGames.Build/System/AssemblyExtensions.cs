// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Linq;
using System.Reflection;

namespace UnrealBuildBase
{
	public static class AssemblyExtensions
	{
		public static Type[] SafeGetLoadedTypes(this Assembly Dll)
		{
			Type[] AllTypes;
			try
			{
				AllTypes = Dll.GetTypes();
			}
			catch (ReflectionTypeLoadException e)
			{
				AllTypes = e.Types.Where(x => x != null).Cast<Type>().ToArray();
			}
			return AllTypes;
		}
	}
}
