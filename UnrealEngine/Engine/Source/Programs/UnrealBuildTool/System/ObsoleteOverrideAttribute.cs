// Copyright Epic Games, Inc. All Rights Reserved.

using System;

namespace UnrealBuildTool
{
	/// <summary>
	/// Attribute used to denote that a method should no longer be overriden. Used by RulesCompiler.
	/// </summary>
	[AttributeUsage(AttributeTargets.Method)]
	public class ObsoleteOverrideAttribute : Attribute
	{
		/// <summary>
		/// Message to display to the user if the method is overridden.
		/// </summary>
		public string Message;

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="Message">Message to display to the user if the method is overridden</param>
		public ObsoleteOverrideAttribute(string Message)
		{
			this.Message = Message;
		}
	}
}
