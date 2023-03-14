// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections;
using System.Collections.Generic;
using System.Collections.Immutable;
using System.Diagnostics;
using System.IO;
using System.Linq;
using System.Reflection;
using System.Text;
using System.Text.RegularExpressions;
using System.Xml;
using EpicGames.BuildGraph.Expressions;
using EpicGames.Core;
using EpicGames.Serialization;
using Microsoft.Extensions.Logging;

namespace EpicGames.BuildGraph
{
	/// <summary>
	/// Exception thrown by the runtime
	/// </summary>
	public sealed class BgBytecodeException : Exception
	{
		/// <summary>
		/// Source file that the error was thrown from
		/// </summary>
		public string SourceFile { get; }

		/// <summary>
		/// Line number that threw the exception
		/// </summary>
		public int SourceLine { get; }

		/// <summary>
		/// Message to display
		/// </summary>
		public string Diagnostic { get; }

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="sourceFile"></param>
		/// <param name="sourceLine"></param>
		/// <param name="diagnostic"></param>
		public BgBytecodeException(string sourceFile, int sourceLine, string diagnostic)
		{
			SourceFile = sourceFile;
			SourceLine = sourceLine;
			Diagnostic = diagnostic;
		}

		/// <inheritdoc/>
		public override string ToString() => $"{SourceFile}({SourceLine}): {Diagnostic}";
	}

	/// <summary>
	/// Interprets compiled buildgraph bytecode
	/// </summary>
	public class BgInterpreter
	{
		class Frame
		{
			public int Offset { get; set; }
			public IReadOnlyList<object> Arguments { get; }
			public Dictionary<int, object> Objects { get; set; }

			public Frame(Frame other)
			{
				Offset = other.Offset;
				Arguments = other.Arguments;
				Objects = other.Objects;
			}

			public Frame(int offset, IReadOnlyList<object> arguments)
			{
				Offset = offset;
				Arguments = arguments;
				Objects = new Dictionary<int, object>();
			}
		}

		readonly byte[] _data;
		readonly BgThunkDef[] _thunks;
		readonly string[] _names;
		readonly IReadOnlyDictionary<string, string> _options;
		readonly BgBytecodeVersion _version;
		readonly int[] _fragments;
		readonly List<BgOptionDef> _optionDefs = new List<BgOptionDef>();

		/// <summary>
		/// The option definitions that were parsed during execution
		/// </summary>
		public IReadOnlyList<BgOptionDef> OptionDefs => _optionDefs;

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="data"></param>
		/// <param name="thunks">Thunks to native code</param>
		/// <param name="options">Options for evaluating the graph</param>
		public BgInterpreter(byte[] data, BgThunkDef[] thunks, IReadOnlyDictionary<string, string> options)
		{
			_data = data;
			_thunks = thunks;
			_options = options;

			MemoryReader reader = new MemoryReader(data);

			_version = (BgBytecodeVersion)reader.ReadUnsignedVarInt();
			_names = reader.ReadVariableLengthArray(() => reader.ReadString());
			int[] fragmentLengths = reader.ReadVariableLengthArray(() => (int)reader.ReadUnsignedVarInt());

			int offset = data.Length - reader.Memory.Length;

			_fragments = new int[fragmentLengths.Length];
			for (int idx = 0; idx < _fragments.Length; idx++)
			{
				_fragments[idx] = offset;
				offset += fragmentLengths[idx];
			}
		}

		/// <summary>
		/// Evaluates the graph
		/// </summary>
		public object Evaluate()
		{
			// Take the given parameters, evaluate the graph that was produced (creating a map of all named entites)
			// Filter the map to the required target
			// Evaluate it fully into a BgGraph
			return Evaluate(new Frame(_fragments[0], Array.Empty<object>()));
		}

		object Evaluate(Frame frame)
		{
			BgOpcode opcode = ReadOpcode(frame);
			switch (opcode)
			{
				#region Bool opcodes

				case BgOpcode.BoolFalse:
					return false;
				case BgOpcode.BoolTrue:
					return true;
				case BgOpcode.BoolNot:
					return !(bool)Evaluate(frame);
				case BgOpcode.BoolAnd:
					{
						bool lhs = (bool)Evaluate(frame);
						bool rhs = (bool)Evaluate(frame);
						return lhs & rhs;
					}
				case BgOpcode.BoolOr:
					{
						bool lhs = (bool)Evaluate(frame);
						bool rhs = (bool)Evaluate(frame);
						return lhs | rhs;
					}
				case BgOpcode.BoolXor:
					{
						bool lhs = (bool)Evaluate(frame);
						bool rhs = (bool)Evaluate(frame);
						return lhs ^ rhs;
					}
				case BgOpcode.BoolEq:
					{
						bool lhs = (bool)Evaluate(frame);
						bool rhs = (bool)Evaluate(frame);
						return lhs == rhs;
					}
				case BgOpcode.BoolOption:
					{
						BgObjectDef obj = (BgObjectDef)Evaluate(frame);

						BgBoolOptionDef option = obj.Deserialize<BgBoolOptionDef>();
						_optionDefs.Add(option);

						bool value = option.DefaultValue;
						if (_options.TryGetValue(option.Name, out string? str))
						{
							value = Boolean.Parse(str);
						}

						return value;
					}

				#endregion
				#region Integer opcodes

				case BgOpcode.IntLiteral:
					return ReadSignedInteger(frame);
				case BgOpcode.IntEq:
					{
						int lhs = (int)Evaluate(frame);
						int rhs = (int)Evaluate(frame);
						return lhs == rhs;
					}
				case BgOpcode.IntLt:
					{
						int lhs = (int)Evaluate(frame);
						int rhs = (int)Evaluate(frame);
						return lhs < rhs;
					}
				case BgOpcode.IntGt:
					{
						int lhs = (int)Evaluate(frame);
						int rhs = (int)Evaluate(frame);
						return lhs > rhs;
					}
				case BgOpcode.IntAdd:
					{
						int lhs = (int)Evaluate(frame);
						int rhs = (int)Evaluate(frame);
						return lhs + rhs;
					}
				case BgOpcode.IntMultiply:
					{
						int lhs = (int)Evaluate(frame);
						int rhs = (int)Evaluate(frame);
						return lhs * rhs;
					}
				case BgOpcode.IntDivide:
					{
						int lhs = (int)Evaluate(frame);
						int rhs = (int)Evaluate(frame);
						return lhs / rhs;
					}
				case BgOpcode.IntModulo:
					{
						int lhs = (int)Evaluate(frame);
						int rhs = (int)Evaluate(frame);
						return lhs % rhs;
					}
				case BgOpcode.IntNegate:
					return -(int)Evaluate(frame);
				case BgOpcode.IntOption:
					{
						BgObjectDef obj = (BgObjectDef)Evaluate(frame);

						BgIntOptionDef option = obj.Deserialize<BgIntOptionDef>();
						_optionDefs.Add(option);

						int value = option.DefaultValue;
						if (_options.TryGetValue(option.Name, out string? str))
						{
							value = Int32.Parse(str);
						}

						return value;
					}

				#endregion
				#region String opcodes

				case BgOpcode.StrEmpty:
					return String.Empty;
				case BgOpcode.StrLiteral:
					return ReadString(frame);
				case BgOpcode.StrCompare:
					{
						string lhs = (string)Evaluate(frame);
						string rhs = (string)Evaluate(frame);
						StringComparison comparison = (StringComparison)ReadUnsignedInteger(frame);
						return String.Compare(lhs, rhs, comparison);
					}
				case BgOpcode.StrConcat:
					{
						string lhs = (string)Evaluate(frame);
						string rhs = (string)Evaluate(frame);
						return lhs + rhs;
					}
				case BgOpcode.StrFormat:
					{
						string format = (string)Evaluate(frame);

						int count = (int)ReadUnsignedInteger(frame);
						object[] arguments = new object[count];

						for (int idx = 0; idx < count; idx++)
						{
							object argument = Evaluate(frame);
							arguments[idx] = argument;
						}

						return String.Format(format, arguments);
					}
				case BgOpcode.StrSplit:
					{
						string source = (string)Evaluate(frame);
						string separator = (string)Evaluate(frame);
						return source.Split(separator, StringSplitOptions.RemoveEmptyEntries);
					}
				case BgOpcode.StrJoin:
					{
						string lhs = (string)Evaluate(frame);
						IEnumerable<object> rhs = (IEnumerable<object>)Evaluate(frame);
						return String.Join(lhs, rhs);
					}
				case BgOpcode.StrMatch:
					{
						string input = (string)Evaluate(frame);
						string pattern = (string)Evaluate(frame);
						return Regex.IsMatch(input, pattern);
					}
				case BgOpcode.StrReplace:
					{
						string input = (string)Evaluate(frame);
						string pattern = (string)Evaluate(frame);
						string replacement = (string)Evaluate(frame);
						return Regex.Replace(input, pattern, replacement);
					}
				case BgOpcode.StrOption:
					{
						BgObjectDef obj = (BgObjectDef)Evaluate(frame);

						BgStringOptionDef option = obj.Deserialize<BgStringOptionDef>();
						_optionDefs.Add(option);

						string value = option.DefaultValue;
						if (_options.TryGetValue(option.Name, out string? result))
						{
							value = result;
						}

						return value;
					}

				#endregion
				#region Enum opcodes

				case BgOpcode.EnumConstant:
					return ReadSignedInteger(frame);
				case BgOpcode.EnumParse:
					{
						string name = (string)Evaluate(frame);
						List<string> names = EvaluateList<string>(frame);
						List<int> values = EvaluateList<int>(frame);

						for (int idx = 0; idx < names.Count; idx++)
						{
							if (String.Equals(names[idx], name, StringComparison.OrdinalIgnoreCase))
							{
								return values[idx];
							}
						}

						throw new InvalidDataException($"Unable to parse enum '{name}'");
					}
				case BgOpcode.EnumToString:
					{
						int value = (int)Evaluate(frame);
						List<string> names = EvaluateList<string>(frame);
						List<int> values = EvaluateList<int>(frame);

						for (int idx = 0; idx < names.Count; idx++)
						{
							if (value == values[idx])
							{
								return names[idx];
							}
						}

						return $"{value}";
					}

				#endregion
				#region List opcodes

				case BgOpcode.ListEmpty:
					return Enumerable.Empty<object>();
				case BgOpcode.ListPush:
					{
						IEnumerable<object> list = (IEnumerable<object>)Evaluate(frame);
						object item = Evaluate(frame);

						return list.Concat(new[] { item });
					}
				case BgOpcode.ListPushLazy:
					{
						IEnumerable<object> list = (IEnumerable<object>)Evaluate(frame);
						int fragment = ReadFragment(frame);

						IEnumerable<object> item = LazyEvaluateItem(new Frame(frame), fragment);
						return list.Concat(item);
					}
				case BgOpcode.ListCount:
					{
						IEnumerable<object> list = (IEnumerable<object>)Evaluate(frame);
						return list.Count();
					}
				case BgOpcode.ListElement:
					{
						IEnumerable<object> list = (IEnumerable<object>)Evaluate(frame);
						int index = (int)Evaluate(frame);
						return list.ElementAt(index);
					}
				case BgOpcode.ListConcat:
					{
						IEnumerable<object> lhs = (IEnumerable<object>)Evaluate(frame);
						IEnumerable<object> rhs = (IEnumerable<object>)Evaluate(frame);
						return Enumerable.Concat(lhs, rhs);
					}
				case BgOpcode.ListUnion:
					{
						IEnumerable<object> lhs = (IEnumerable<object>)Evaluate(frame);
						IEnumerable<object> rhs = (IEnumerable<object>)Evaluate(frame);
						return lhs.Union(rhs);
					}
				case BgOpcode.ListExcept:
					{
						IEnumerable<object> lhs = (IEnumerable<object>)Evaluate(frame);
						IEnumerable<object> rhs = (IEnumerable<object>)Evaluate(frame);
						return lhs.Except(rhs);
					}
				case BgOpcode.ListSelect:
					{
						IEnumerable<object> source = (IEnumerable<object>)Evaluate(frame);
						int fragment = ReadFragment(frame);

						return source.Select(x => Call(fragment, new object[] { x }));
					}
				case BgOpcode.ListWhere:
					{
						IEnumerable<object> source = (IEnumerable<object>)Evaluate(frame);
						int fragment = ReadFragment(frame);

						return source.Where(x => (bool)Call(fragment, new object[] { x }));
					}
				case BgOpcode.ListDistinct:
					{
						IEnumerable<object> source = (IEnumerable<object>)Evaluate(frame);
						return source.Distinct();
					}
				case BgOpcode.ListContains:
					{
						IEnumerable<object> source = (IEnumerable<object>)Evaluate(frame);
						object item = Evaluate(frame);
						return source.Contains(item);
					}
				case BgOpcode.ListLazy:
					{
						int fragment = ReadFragment(frame);
						return LazyEvaluateList(new Frame(frame), fragment);
					}
				case BgOpcode.ListOption:
					{
						BgObjectDef obj = (BgObjectDef)Evaluate(frame);

						BgListOptionDef option = obj.Deserialize<BgListOptionDef>();
						_optionDefs.Add(option);

						string? value = option.DefaultValue ?? String.Empty;
						if (_options.TryGetValue(option.Name, out string? result))
						{
							value = result;
						}

						return value!.Split(new[] { '+', ';' }, StringSplitOptions.RemoveEmptyEntries).Select(x => (object)x).ToList();
					}

				#endregion
				#region Object opcodes

				case BgOpcode.ObjEmpty:
					return new BgObjectDef();
				case BgOpcode.ObjGet:
					{
						BgObjectDef obj = (BgObjectDef)Evaluate(frame);
						string name = ReadName(frame);
						object defaultValue = Evaluate(frame);

						object? value;
						if (!obj.Properties.TryGetValue(name, out value))
						{
							value = defaultValue;
						}

						return value!;
					}
				case BgOpcode.ObjSet:
					{
						BgObjectDef obj = (BgObjectDef)Evaluate(frame);
						string name = ReadName(frame);
						object value = Evaluate(frame);
						return obj.Set(name, value);
					}

				#endregion
				#region Function opcodes

				case BgOpcode.Call:
					{
						int count = (int)ReadUnsignedInteger(frame);

						object[] arguments = new object[count];
						for (int idx = 0; idx < count; idx++)
						{
							arguments[idx] = Evaluate(frame);
						}

						int function = (int)ReadUnsignedInteger(frame);
						return Evaluate(new Frame(_fragments[function], arguments));
					}
				case BgOpcode.Argument:
					{
						int index = (int)ReadUnsignedInteger(frame);
						return frame.Arguments[index];
					}
				case BgOpcode.Jump:
					{
						int fragment = (int)ReadUnsignedInteger(frame);
						return Jump(frame, fragment);
					}

				#endregion
				#region Generic opcodes

				case BgOpcode.Choose:
					{
						bool condition = (bool)Evaluate(frame);

						int fragmentIfTrue = (int)ReadUnsignedInteger(frame);
						int fragmentIfFalse = (int)ReadUnsignedInteger(frame);

						return Jump(frame, condition ? fragmentIfTrue : fragmentIfFalse);
					}
				case BgOpcode.Throw:
					{
						string sourceFile = ReadString(frame);
						int sourceLine = (int)ReadUnsignedInteger(frame);
						string message = (string)Evaluate(frame);
						throw new BgBytecodeException(sourceFile, sourceLine, message);
					}
				case BgOpcode.Null:
					return null!;
				case BgOpcode.Thunk:
					{
						BgThunkDef method = _thunks[(int)ReadUnsignedInteger(frame)];

						int count = (int)ReadUnsignedInteger(frame);
						object?[] arguments = new object?[count];

						ParameterInfo[] parameters = method.Method.GetParameters();
						for (int idx = 0; idx < count; idx++)
						{
							object value = Evaluate(frame);
							if (typeof(BgExpr).IsAssignableFrom(parameters[idx].ParameterType))
							{
								arguments[idx] = BgType.Constant(parameters[idx].ParameterType, value);
							}
							else
							{
								arguments[idx] = method.Arguments[idx];
							}
						}

						return new BgThunkDef(method.Method, arguments);
					}

				#endregion

				default:
					throw new InvalidDataException($"Invalid opcode: {opcode}");
			}
		}

		List<T> EvaluateList<T>(Frame frame)
		{
			return ((IEnumerable<object>)Evaluate(frame)).Select(x => (T)x).ToList();
		}

		IEnumerable<object> LazyEvaluateItem(Frame frame, int fragment)
		{
			yield return Jump(frame, fragment);
		}

		IEnumerable<object> LazyEvaluateList(Frame frame, int fragment)
		{
			IEnumerable<object> result = (IEnumerable<object>)Jump(frame, fragment);
			foreach (object item in result)
			{
				yield return item;
			}
		}

		object Jump(Frame frame, int fragment)
		{
			object? result;
			if (!frame.Objects.TryGetValue(fragment, out result))
			{
				int prevOffset = frame.Offset;

				frame.Offset = _fragments[fragment];

				result = Evaluate(frame);
				frame.Objects.Add(fragment, result);

				frame.Offset = prevOffset;
			}
			return result;
		}

		object Call(int fragment, object[] arguments)
		{
			int offset = _fragments[fragment];
			return Evaluate(new Frame(offset, arguments));
		}

		int ReadFragment(Frame frame)
		{
			return (int)ReadUnsignedInteger(frame);
		}

		/// <summary>
		/// Reads an opcode from the input stream
		/// </summary>
		/// <returns>The opcode that was read</returns>
		BgOpcode ReadOpcode(Frame frame)
		{
			BgOpcode opcode = (BgOpcode)_data[frame.Offset++];
			return opcode;
		}

		/// <summary>
		/// Reads a name from the input stream
		/// </summary>
		string ReadName(Frame frame)
		{
			int index = (int)ReadUnsignedInteger(frame);
			return _names[index];
		}

		/// <summary>
		/// Reads a string from the input stream
		/// </summary>
		string ReadString(Frame frame)
		{
			ReadOnlySpan<byte> buffer = _data[frame.Offset..];

			int length = (int)VarInt.ReadUnsigned(buffer, out int bytesRead);
			frame.Offset += bytesRead;

			string text = Encoding.UTF8.GetString(buffer.Slice(bytesRead, length));
			frame.Offset += length;

			return text;
		}

		/// <summary>
		/// Writes a signed integer value to the output
		/// </summary>
		/// <returns>the value that was read</returns>
		int ReadSignedInteger(Frame frame)
		{
			ulong encoded = ReadUnsignedInteger(frame);
			return DecodeSignedInteger(encoded);
		}

		/// <summary>
		/// Read an unsigned integer value from the input
		/// </summary>
		/// <returns>The value that was read</returns>
		ulong ReadUnsignedInteger(Frame frame)
		{
			ulong value = VarInt.ReadUnsigned(_data[frame.Offset..], out int bytesRead);
			frame.Offset += bytesRead;
			return value;
		}

		/// <summary>
		/// Decode a signed integer using the lower bit for the sign flag, allowing us to encode it more efficiently as a <see cref="VarInt"/>
		/// </summary>
		/// <param name="value">Value to be decoded</param>
		/// <returns>The decoded value</returns>
		static int DecodeSignedInteger(ulong value)
		{
			if ((value & 1) != 0)
			{
				return -(int)(value >> 1);
			}
			else
			{
				return (int)(value >> 1);
			}
		}

		/// <summary>
		/// Disassemble the current script to a logger
		/// </summary>
		/// <param name="logger"></param>
		public void Disassemble(ILogger logger)
		{
			logger.LogInformation("Version: {Version}", _version);
			for (int idx = 0; idx < _fragments.Length; idx++)
			{
				logger.LogInformation("");
				logger.LogInformation("Fragment {Idx}:", idx);
				Disassemble(new Frame(_fragments[idx], Array.Empty<object>()), logger);
			}
		}

		void Disassemble(Frame frame, ILogger logger)
		{
			BgOpcode opcode = Trace(frame, null, ReadOpcode, logger);
			switch (opcode)
			{
				#region Bool opcodes

				case BgOpcode.BoolFalse:
				case BgOpcode.BoolTrue:
					break;
				case BgOpcode.BoolNot:
					Disassemble(frame, logger);
					break;
				case BgOpcode.BoolAnd:
				case BgOpcode.BoolOr:
				case BgOpcode.BoolXor:
				case BgOpcode.BoolEq:
					Disassemble(frame, logger);
					Disassemble(frame, logger);
					break;
				case BgOpcode.BoolOption:
					Disassemble(frame, logger);
					break;

				#endregion
				#region Integer opcodes

				case BgOpcode.IntLiteral:
					Trace(frame, null, ReadSignedInteger, logger);
					break;
				case BgOpcode.IntEq:
				case BgOpcode.IntLt:
				case BgOpcode.IntGt:
				case BgOpcode.IntAdd:
				case BgOpcode.IntMultiply:
				case BgOpcode.IntDivide:
				case BgOpcode.IntModulo:
					Disassemble(frame, logger);
					Disassemble(frame, logger);
					break;
				case BgOpcode.IntNegate:
					Disassemble(frame, logger);
					break;
				case BgOpcode.IntOption:
					Disassemble(frame, logger);
					break;

				#endregion
				#region String opcodes

				case BgOpcode.StrEmpty:
					break;
				case BgOpcode.StrLiteral:
					Trace(frame, ReadString, x => $"\"{x}\"", logger);
					break;
				case BgOpcode.StrCompare:
					Disassemble(frame, logger);
					Disassemble(frame, logger);
					Trace(frame, "type", ReadUnsignedInteger, logger);
					break;
				case BgOpcode.StrConcat:
				case BgOpcode.StrSplit:
				case BgOpcode.StrJoin:
					Disassemble(frame, logger);
					Disassemble(frame, logger);
					break;
				case BgOpcode.StrFormat:
					{
						Disassemble(frame, logger);

						int count = (int)Trace(frame, "count", ReadUnsignedInteger, logger);
						for (int idx = 0; idx < count; idx++)
						{
							Disassemble(frame, logger);
						}

						break;
					}
				case BgOpcode.StrMatch:
					Disassemble(frame, logger);
					Disassemble(frame, logger);
					break;
				case BgOpcode.StrReplace:
					Disassemble(frame, logger);
					Disassemble(frame, logger);
					Disassemble(frame, logger);
					break;
				case BgOpcode.StrOption:
					Disassemble(frame, logger);
					break;

				#endregion
				#region Enum opcodes

				case BgOpcode.EnumConstant:
					Trace(frame, "value", ReadSignedInteger, logger);
					break;
				case BgOpcode.EnumParse:
				case BgOpcode.EnumToString:
					Disassemble(frame, logger);
					Disassemble(frame, logger);
					Disassemble(frame, logger);
					break;

				#endregion
				#region List opcodes

				case BgOpcode.ListEmpty:
					break;
				case BgOpcode.ListPush:
					Disassemble(frame, logger);
					Disassemble(frame, logger);
					break;
				case BgOpcode.ListPushLazy:
				case BgOpcode.ListSelect:
				case BgOpcode.ListWhere:
					Disassemble(frame, logger);
					TraceFragment(frame, logger);
					break;
				case BgOpcode.ListCount:
				case BgOpcode.ListDistinct:
					Disassemble(frame, logger);
					break;
				case BgOpcode.ListElement:
				case BgOpcode.ListConcat:
				case BgOpcode.ListUnion:
				case BgOpcode.ListExcept:
				case BgOpcode.ListContains:
					Disassemble(frame, logger);
					Disassemble(frame, logger);
					break;
				case BgOpcode.ListLazy:
					TraceFragment(frame, logger);
					break;
				case BgOpcode.ListOption:
					Disassemble(frame, logger);
					break;

				#endregion

				#region Object opcodes

				case BgOpcode.ObjEmpty:
					break;
				case BgOpcode.ObjGet:
				case BgOpcode.ObjSet:
					Disassemble(frame, logger);
					Trace(frame, ReadName, x => $"name: {x}", logger);
					Disassemble(frame, logger);
					break;

				#endregion

				#region Function opcodes

				case BgOpcode.Call:
					{
						int count = (int)Trace(frame, "count", ReadUnsignedInteger, logger);
						for (int idx = 0; idx < count; idx++)
						{
							Disassemble(frame, logger);
						}

						TraceFragment(frame, logger);
						break;
					}
				case BgOpcode.Argument:
					Trace(frame, "arg", ReadUnsignedInteger, logger);
					break;
				case BgOpcode.Jump:
					TraceFragment(frame, logger);
					break;

				#endregion
				#region Generic opcodes

				case BgOpcode.Choose:
					Disassemble(frame, logger);
					TraceFragment(frame, logger);
					TraceFragment(frame, logger);
					break;
				case BgOpcode.Throw:
					Trace(frame, "file", ReadString, logger);
					Trace(frame, "line", ReadUnsignedInteger, logger);
					Trace(frame, "message", ReadString, logger);
					break;
				case BgOpcode.Null:
					break;
				case BgOpcode.Thunk:
					{
						Trace(frame, "method", ReadUnsignedInteger, logger);

						int count = (int)Trace(frame, "args", ReadUnsignedInteger, logger);
						for (int idx = 0; idx < count; idx++)
						{
							Disassemble(frame, logger);
						}

						break;
					}

				#endregion

				default:
					throw new InvalidDataException($"Invalid opcode: {opcode}");
			}
		}

		int TraceFragment(Frame frame, ILogger logger)
		{
			return Trace(frame, "-> Fragment", ReadFragment, logger);
		}

		T Trace<T>(Frame frame, string? type, Func<Frame, T> readValue, ILogger logger)
		{
			int offset = frame.Offset;
			T value = readValue(frame);
			int length = frame.Offset - offset;

			string valueAndType = (type == null) ? $"{value}" : $"{type} {value}";
			Trace<T>(offset, length, valueAndType, logger);
			return value;
		}

		T Trace<T>(Frame frame, Func<Frame, T> readValue, Func<T, object> formatValue, ILogger logger)
		{
			int offset = frame.Offset;
			T value = readValue(frame);
			int length = frame.Offset - offset;

			Trace<T>(offset, length, formatValue(value), logger);
			return value;
		}

		void Trace<T>(int offset, int length, object value, ILogger logger)
		{
			string bytes = String.Join(" ", _data.AsSpan(offset, length).ToArray().Select(x => $"{x:x2}"));
			logger.LogInformation("{Offset,6}: {Value,-20} {Bytes}", offset, value, bytes);
		}
	}
}
