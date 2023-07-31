// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Linq;
using System.Reflection;
using System.Text;
using System.Threading.Tasks;
using AutomationTool;

namespace Gauntlet
{

	public interface IAutoParamNotifiable
	{
		void ParametersWereApplied(string[] Params);
	};


	/// <summary>
	/// An attribute that can be used to apply commandline options to fields or properties. 
	/// 
	/// Simply tag properties or fields with the CommandLineOption, a name, and a default value, then 
	/// call CommandLineOption.Apply(obj, args) where args is a list of -switches or -key=value pairs
	/// 
	/// The main constraint is that your object type must be convertable from a string
	/// 
	/// </summary>
	[AttributeUsage(AttributeTargets.Field | AttributeTargets.Property)]
	public class AutoParam : System.Attribute
	{
		/// <summary>
		/// Default value
		/// </summary>
		protected object Default;

		/// <summary>
		/// Names that can refer to this param
		/// </summary>
		public string[] OptionNames { get; protected set; }

		/// <summary>
		/// Constructor that takes nothing. Param option should be -MemberName or -MemberName=value.
		/// Members with no matching param will be left as-is.
		/// </summary>
		public AutoParam()
		{
			this.OptionNames = null;
			this.Default = null;
		}

		/// <summary>
		/// Constructor that takes an array of of potential argument names, e.g. {"build","builds"}
		/// Members with no matching param will be left as-is.
		/// </summary>
		/// <param name="Name"></param>
		/// <param name="Default"></param>
		protected AutoParam(params string[] OptionNames)
		{
			this.OptionNames = OptionNames;
			this.Default = null;
		}

		/// <summary>

		/// <summary>
		/// Constructor that takes a default argument to use if no param is specified. Param option should be -MemberName or -MemberName=value.
		/// Members with no matching param will be set to 'Default'
		/// </summary>
		/// <param name="Default"></param>
		public AutoParam(object Default)
		{
			this.OptionNames = null;
			this.Default = Default;
		}

		/// <summary>
		/// Constructor that takes an array of of potential argument names, e.g. {"build","builds"}
		/// Members with no matching param will be set to 'Default'
		/// </summary>
		/// <param name="Name"></param>
		/// <param name="Default"></param>
		protected AutoParam(object Default, params string[] OptionNames)
		{
			this.OptionNames = OptionNames;
			this.Default = Default;
		}

		

		/// <summary>
		/// Checks whether Args contains a -Param statement, if so returns true else
		/// returns Default
		/// </summary>
		/// <param name="Param"></param>
		/// <param name="Default"></param>
		/// <param name="Args"></param>
		/// <returns></returns>
		static protected bool SwitchExists(string Param, string[] Args)
		{
			foreach (string Arg in Args)
			{
				string StringArg = Arg;

				if (StringArg.StartsWith("-"))
				{
					StringArg = Arg.Substring(1);
				}

				if (StringArg.ToString().Equals(Param, StringComparison.InvariantCultureIgnoreCase))
				{
					return true;
				}
			}
			return false;
		}

		/// <summary>
		/// Checks Args for a -param=value statement and either returns value or the
		/// provided default
		/// </summary>
		/// <param name="Param"></param>
		/// <param name="Default"></param>
		/// <param name="Args"></param>
		/// <returns></returns>
		static protected object ParaseAndCoerceParam(string Param, Type ParamType, string[] Args)
		{
			if (!Param.EndsWith("="))
			{
				Param += "=";
			}
			foreach (string Arg in Args)
			{
				string StringArg = Arg;

				if (StringArg.StartsWith("-"))
				{
					StringArg = Arg.Substring(1);
				}

				if (StringArg.StartsWith(Param, StringComparison.InvariantCultureIgnoreCase))
				{
					string StringVal = StringArg.Substring(Param.Length);

					if (ParamType.IsEnum)
					{
						var AllValues = Enum.GetValues(ParamType).Cast<object>();

						var Enums = AllValues.Where(P => string.Equals(StringVal, P.ToString(), StringComparison.OrdinalIgnoreCase));

						if (Enums.Count() == 0)
						{
							Log.Error("Could not convert param {0} to enum of type {1}", StringVal, ParamType);
						}

						return Enums.First();
					}
					else
					{
						return Convert.ChangeType(StringVal, ParamType);
					}
				}
			}

			return null;
		}

		/// <summary>
		/// Returns true if this type is considered a simple primitive (there is IsClass in c# but no IsStruct :()
		/// </summary>
		/// <param name="type"></param>
		/// <returns></returns>
		static bool IsSimple(Type type)
		{
			return type.IsPrimitive
			  || type.IsEnum
			  || type.Equals(typeof(string))
			  || type.Equals(typeof(decimal));
		}

		/// <summary>
		/// Call to process all CommandLineOption attributes on an objects members and set them based on the
		/// provided argument list
		/// </summary>
		/// <param name="Obj"></param>
		/// <param name="Args"></param>
		protected static void ApplyParamsAndDefaultsInternal(object Obj, string[] Args, bool ApplyDefaults)
		{
			// get all field and property members
			var Fields = Obj.GetType().GetFields(BindingFlags.Public | BindingFlags.NonPublic | BindingFlags.Instance);
			var Properties = Obj.GetType().GetProperties(BindingFlags.Public | BindingFlags.NonPublic | BindingFlags.Instance);

			var AllMembers = Fields.Cast<MemberInfo>().Concat(Properties);

			foreach (var Member in AllMembers)
			{
				Type MemberType = null;

				// Get the type of the member (note - this is not Member.Type!)
				if (Member is PropertyInfo)
				{
					MemberType = ((PropertyInfo)Member).PropertyType;
				}
				else if (Member is FieldInfo)
				{
					MemberType = ((FieldInfo)Member).FieldType;
				}

				// Go through all attributes
				foreach (object Attrib in Member.GetCustomAttributes(true))
				{
					if (Attrib is AutoParam)
					{
						// If this is a struct then we want to recurse
						if (IsSimple(MemberType) == false)
						{
							object Value = null;

							// Get the reference value
							if (Member is PropertyInfo)
							{
								Value = ((PropertyInfo)Member).GetValue(Obj);
							}
							else if (Member is FieldInfo)
							{
								Value = ((FieldInfo)Member).GetValue(Obj);
							}

							// if null create a new one (e.g. a new instance of a struct);
							if (Value == null)
							{
								try
								{
									Value = Activator.CreateInstance(MemberType);
								}
								catch
								{
									throw new AutomationException("Add a default constructor to the class {0}!", MemberType);
								}

								// Set the new object as the refernce
								if (Member is PropertyInfo)
								{
									((PropertyInfo)Member).SetValue(Obj, Value);
								}
								else if (Member is FieldInfo)
								{
									((FieldInfo)Member).SetValue(Obj, Value);
								}
							}

							// Recurse into this struct
							ApplyParamsAndDefaultsInternal(Value, Args, ApplyDefaults);
						}
						else
						{
							AutoParam Opt = Attrib as AutoParam;

							// if the attribute had names provided use them, else use the name of the variable
							string[] ParamNames = (Opt.OptionNames != null && Opt.OptionNames.Length > 0) ? Opt.OptionNames : new string[] { Member.Name };

							// save the default
							object DefaultValue = Opt.Default;
							object NewValue = null;

							if (DefaultValue != null && DefaultValue.GetType() != MemberType)
							{
								Log.Warning("AutoParam default value for member {0} is type {1}, not {2}", Member.Name, DefaultValue.GetType(), MemberType);
							}

							// Go through all params used to refer to this member
							foreach (string Name in ParamNames)
							{
								// if default is a bool then just check if the switch exists
								if (MemberType == typeof(bool))
								{
									if (SwitchExists(Name, Args))
									{
										NewValue = true;
									}
								}
								else
								{
									// for all other types try to parse out the value
									NewValue = ParaseAndCoerceParam(Name, MemberType, Args);
								}

								// stop as soon as we find something
								if (NewValue != null)
								{
									break;
								}
							}

							// If no value was found, use the default
							if (NewValue != null || (ApplyDefaults && DefaultValue != null))
							{
								if (NewValue == null)
								{
									NewValue = DefaultValue;
								}

								if (MemberType.IsEnum)
								{
									if (NewValue.GetType() != MemberType)
									{
										Log.Warning("Default for member {0} is an enum of an incorrect type!", Member.Name);
									}
								}
								else
								{
									// Force a conversion - need to do this even for default values incase someone specified a double for a float
									NewValue = Convert.ChangeType(NewValue, MemberType);
								}

								// Set the value on the member
								if (Member is PropertyInfo)
								{
									((PropertyInfo)Member).SetValue(Obj, NewValue);
								}
								else if (Member is FieldInfo)
								{
									((FieldInfo)Member).SetValue(Obj, NewValue);
								}
							}
						}
					}
				}
			}

			IAutoParamNotifiable ParamNotifable = Obj as IAutoParamNotifiable;

			if (ParamNotifable != null)
			{
				ParamNotifable.ParametersWereApplied(Args);
			}
		}

		public static void ApplyDefaults(object Obj)
		{
			ApplyParamsAndDefaultsInternal(Obj, new string[0], true);
		}

		public static void ApplyParams(object Obj, string[] Args)
		{
			ApplyParamsAndDefaultsInternal(Obj, Args, false);
		}

		public static void ApplyParamsAndDefaults(object Obj, string[] Args)
		{
			ApplyParamsAndDefaultsInternal(Obj, Args, true);
		}

		/*public static string[] GetParams(object Obj)
		{
			List<string> CopiedParams = new List<string>();

			var Fields = Obj.GetType().GetFields(BindingFlags.Public | BindingFlags.Instance);

			var Properties = Obj.GetType().GetProperties(BindingFlags.Public | BindingFlags.Instance);

			var AllMembers = Fields.Cast<MemberInfo>().Concat(Properties);

			foreach (var Member in AllMembers)
			{
				foreach (object Attrib in Member.GetCustomAttributes(true))
				{
					if (Attrib is AutoParam)
					{
						AutoParam Opt = Attrib as AutoParam;

						string ParamName = string.IsNullOrEmpty(Opt.Name) ? Member.Name : Opt.Name;

						object ParamValue = null;

						if (Member is PropertyInfo)
						{
							ParamValue = ((PropertyInfo)Member).GetValue(Obj);
						}
						else if (Member is FieldInfo)
						{
							ParamValue = ((FieldInfo)Member).GetValue(Obj);
						}

						if (Opt.Default.GetType() == typeof(bool))
						{
							if ((bool)ParamValue)
							{
								CopiedParams.Add(ParamName);
							}							
						}
						else
						{
							CopiedParams.Add(string.Format("{0}={1}", ParamName, ParamValue));
						}
					}
				}
			}

			return CopiedParams.ToArray();
		}*/
	}

	[AttributeUsage(AttributeTargets.Field | AttributeTargets.Property)]
	public class AutoParamWithNames : AutoParam
	{
		public AutoParamWithNames(object Default, params string[] OptionNames)  :
			base(Default, OptionNames)
		{
		}

		public AutoParamWithNames(params string[] OptionNames) :
			base(OptionNames)
		{
		}
	}
}
