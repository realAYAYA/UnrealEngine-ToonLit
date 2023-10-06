using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using System.ComponentModel;
using System.Text.RegularExpressions;

using Dia2Lib;

namespace CruncherSharp
{
	public class SymbolAnalyzerDIA : SymbolAnalyzer
	{
		public override bool LoadPdb(object sender, LoadPDBTask task)
		{
			IDiaDataSource source = new DiaSourceClass();
			source.loadDataFromPdb(FileName);
			source.openSession(out IDiaSession session);
			if (!LoadSymbols(session, sender, task))
				return false;
			RunAnalysis();
			return true;
		}

		public bool LoadSymbols(IDiaSession session, object sender, LoadPDBTask task)
		{
			var worker = sender as BackgroundWorker;
			worker?.ReportProgress(0, "Finding symbols");

			IDiaEnumSymbols allSymbols;

			if (task.Filter.Length > 0)
			{
				uint compareFlags = 0;
				if (!task.WholeExpression || task.UseRegularExpression)
					compareFlags |= 0x8;
				if (!task.MatchCase)
					compareFlags |= 0x2;
				else
					compareFlags |= 0x2;

				if (task.UseRegularExpression)
					session.findChildren(session.globalScope, SymTagEnum.SymTagUDT, @task.Filter, compareFlags, out allSymbols);
				else if (task.WholeExpression)
					session.findChildren(session.globalScope, SymTagEnum.SymTagUDT, task.Filter, compareFlags, out allSymbols);
				else
				{
					string filter = '*' + task.Filter + '*';
					session.findChildren(session.globalScope, SymTagEnum.SymTagUDT, @filter, compareFlags, out allSymbols);
				}
			}
			else
			{
				session.findChildren(session.globalScope, SymTagEnum.SymTagUDT, null, 0, out allSymbols);
			}

			if (allSymbols == null)
				return false;

			worker?.ReportProgress(0, "Counting symbols");

			var allSymbolsCount = (worker != null && task.UseProgressBar) ? allSymbols.count : 0;
			var i = 0;

			worker?.ReportProgress(0, "Adding symbols");

			foreach (IDiaSymbol sym in allSymbols)
			{
				if (worker != null && worker.CancellationPending)
				{
					return false;
				}

				if (task.SecondPDB)
				{
					SymbolInfo info = FindSymbolInfo(sym.name);
					if (info != null)
					{
						info.NewSize = sym.length;
					}
				}
				else
				{
					if (sym.length > 0 && !HasSymbolInfo(sym.name))
					{
						var symbolInfo = new SymbolInfo(sym.name, sym.GetType().Name, sym.length, MemPools);
						ProcessChildren(symbolInfo, sym);
						Symbols.Add(symbolInfo.Name, symbolInfo);

						if (symbolInfo.Name.Contains("::") && !symbolInfo.Name.Contains("<"))
						{
							RootNamespaces.Add(symbolInfo.Name.Substring(0, symbolInfo.Name.IndexOf("::")));
							Namespaces.Add(symbolInfo.Name.Substring(0, symbolInfo.Name.LastIndexOf("::")));
						}
					}
				}
				var percentProgress = (int)Math.Round((double)(100 * i++) / allSymbolsCount);
				percentProgress = Math.Max(Math.Min(percentProgress, 99), 1);
				worker?.ReportProgress(percentProgress, String.Format("Adding symbol {0} on {1}", i, allSymbolsCount));
			}


			worker?.ReportProgress(100, String.Format("{0} symbols added", allSymbolsCount));

			return true;
		}

		public void LoadSymbolsRecursive(IDiaSession session, string name, bool SetImportedFromCSV)
		{
			if (Symbols.ContainsKey(name))
			{
				return;
			}

			IDiaEnumSymbols allSymbols;
			session.findChildren(session.globalScope, SymTagEnum.SymTagUDT, name, 0x2, out allSymbols);
			if (allSymbols == null)
			{
				return;
			}

			foreach (IDiaSymbol sym in allSymbols)
			{
				if (sym.length > 0)
				{
					if (Symbols.ContainsKey(sym.name))
					{
						if (SetImportedFromCSV)
						{
							Symbols[sym.name].IsImportedFromCSV = true;
						}
						continue;
					}
					var symbolInfo = new SymbolInfo(sym.name, sym.GetType().Name, sym.length, MemPools);
					if (SetImportedFromCSV)
					{
						symbolInfo.IsImportedFromCSV = true;
					}
					ProcessChildren(symbolInfo, sym);
					Symbols.Add(symbolInfo.Name, symbolInfo);

					if (symbolInfo.Name.Contains("::") && !symbolInfo.Name.Contains("<"))
					{
						RootNamespaces.Add(symbolInfo.Name.Substring(0, symbolInfo.Name.IndexOf("::")));
						Namespaces.Add(symbolInfo.Name.Substring(0, symbolInfo.Name.LastIndexOf("::")));
					}

					foreach (var member in symbolInfo.Members)
					{
						if (member.Category == SymbolMemberInfo.MemberCategory.UDT || member.Category == SymbolMemberInfo.MemberCategory.Base)
						{
							LoadSymbolsRecursive(session, member.TypeName, false);
						}
					}
				}
			}
		}
		public override bool LoadCSV(object sender, List<LoadCSVTask> tasks)
		{
			IDiaDataSource source = new DiaSourceClass();
			source.loadDataFromPdb(FileName);
			source.openSession(out IDiaSession session);

			var worker = sender as BackgroundWorker;

			var allSymbolsCount = (worker != null) ? tasks.Count : 0;
			var addedSymbolsCount = 0;

			worker?.ReportProgress(0, "Adding symbols");
			int progress = 0;


			foreach (LoadCSVTask task in tasks)
			{
				if (worker != null && worker.CancellationPending)
				{
					return false;
				}

				IDiaEnumSymbols allSymbols;
				session.findChildren(session.globalScope, SymTagEnum.SymTagUDT, task.ClassName, 0x2, out allSymbols);
				if (allSymbols == null)
				{
					continue;
				}

				foreach (IDiaSymbol sym in allSymbols)
				{
					if (sym.length > 0)
					{
						LoadSymbolsRecursive(session, sym.name, true);
						Symbols.TryGetValue(sym.name, out var symbolInfo);
						if (symbolInfo != null)
						{
							symbolInfo.TotalCount = symbolInfo.NumInstances = task.Count;
						}
					}
				}

				var percentProgress = (int)Math.Round((double)(100 * addedSymbolsCount++) / allSymbolsCount);
				if (percentProgress > progress)
				{
					progress = Math.Max(Math.Min(percentProgress, 99), 1);
					worker?.ReportProgress(progress, String.Format("Adding symbol {0} on {1}", addedSymbolsCount, allSymbolsCount));
				}
			}

			worker?.ReportProgress(100, String.Format("{0} symbols added", allSymbolsCount));

			RunAnalysis();
			return true;
		}
		public void ProcessChildren(SymbolInfo outSymbol, IDiaSymbol symbol)
		{
			symbol.findChildren(SymTagEnum.SymTagNull, null, 0, out var children);
			if (children == null)
			{
				return;
			}
			foreach (IDiaSymbol child in children)
			{
				if (child.symTag == (uint)SymTagEnum.SymTagFunction)
				{
					var functionInfo = ProcessFunction(child);
					if (functionInfo != null)
					{
						outSymbol.AddFunction(functionInfo);
					}
				}
				else
				{
					var memberInfo = ProcessMember(child);
					if (memberInfo != null)
					{
						outSymbol.AddMember(memberInfo);
					}
				}
			}
			outSymbol.SortAndCalculate();
		}

		public SymbolMemberInfo ProcessMember(IDiaSymbol symbol)
		{
			if (symbol.symTag == (uint)SymTagEnum.SymTagVTable)
			{
				return new SymbolMemberInfo(SymbolMemberInfo.MemberCategory.VTable, string.Empty, string.Empty, 8, 0, (ulong)symbol.offset, symbol.bitPosition);
			}

			if (symbol.isStatic != 0 || (symbol.symTag != (uint)SymTagEnum.SymTagData && symbol.symTag != (uint)SymTagEnum.SymTagBaseClass))
			{
				return null;
			}

			// LocIsThisRel || LocIsNull || LocIsBitField
			if (symbol.locationType != 4 && symbol.locationType != 0 && symbol.locationType != 6)
			{
				return null;
			}

			var typeSymbol = symbol.type;

			var typeName = GetType(typeSymbol);

			var symbolName = symbol.name;
			var category = SymbolMemberInfo.MemberCategory.Member;

			if ((SymTagEnum)symbol.symTag == SymTagEnum.SymTagBaseClass)
			{
				category = SymbolMemberInfo.MemberCategory.Base;
			}
			else if ((SymTagEnum)typeSymbol.symTag == SymTagEnum.SymTagUDT)
			{
				category = SymbolMemberInfo.MemberCategory.UDT;
			}
			else if ((SymTagEnum)typeSymbol.symTag == SymTagEnum.SymTagPointerType)
			{
				category = SymbolMemberInfo.MemberCategory.Pointer;
			}

			var info = new SymbolMemberInfo(category, symbolName, typeName, typeSymbol.length, symbol.length, (ulong)symbol.offset, symbol.bitPosition);

			if (typeSymbol.volatileType == 1)
			{
				info.Volatile = true;
			}
			if (symbol.locationType == 6)
			{
				info.BitField = true;
			}

			return info;
		}

		public SymbolFunctionInfo ProcessFunction(IDiaSymbol symbol)
		{
			if (symbol.compilerGenerated > 0)
			{
				return null;
			}

			var info = new SymbolFunctionInfo();
			if (symbol.isStatic > 0)
			{
				info.Category = SymbolFunctionInfo.FunctionCategory.StaticFunction;
			}
			else if (symbol.classParent.name.EndsWith(symbol.name))
			{
				info.Category = SymbolFunctionInfo.FunctionCategory.Constructor;
			}
			else if (symbol.name.StartsWith("~"))
			{
				info.Category = SymbolFunctionInfo.FunctionCategory.Destructor;
			}
			else
			{
				info.Category = SymbolFunctionInfo.FunctionCategory.Function;
			}

			info.Virtual = (symbol.@virtual == 1);
			info.IsOverride = info.Virtual && (symbol.intro == 0);
			info.IsPure = info.Virtual && (symbol.pure != 0);
			info.IsConst = symbol.constType != 0;
			if (symbol.wasInlined == 0 && symbol.inlSpec != 0)
			{
				info.WasInlineRemoved = true;
			}

			info.Name = GetType(symbol.type.type) + " " + symbol.name;

			symbol.type.findChildren(SymTagEnum.SymTagFunctionArgType, null, 0, out var syms);
			if (syms.count == 0)
			{
				info.Name += "(void)";
			}
			else
			{
				var parameters = new List<string>();
				foreach (IDiaSymbol argSym in syms)
				{
					parameters.Add(GetType(argSym.type));
				}
				info.Name += "(" + string.Join(",", parameters) + ")";
			}
			return info;
		}

		private string GetType(IDiaSymbol typeSymbol)
		{
			switch ((SymTagEnum)typeSymbol.symTag)
			{
				case SymTagEnum.SymTagFunctionType:
					var returnType = GetType(typeSymbol.type);

					typeSymbol.findChildren(SymTagEnum.SymTagFunctionArgType, null, 0, out var syms);
					if (syms.count == 0)
					{
						returnType += "(void)";
					}
					else
					{
						var parameters = new List<string>();
						foreach (IDiaSymbol argSym in syms)
						{
							parameters.Add(GetType(argSym.type));
						}
						returnType += "(" + string.Join(",", parameters) + ")";
					}
					return returnType;
				case SymTagEnum.SymTagPointerType:
					return typeSymbol.reference != 0 ? $"{GetType(typeSymbol.type)}&" : $"{GetType(typeSymbol.type)}*";
				case SymTagEnum.SymTagBaseType:
					if (typeSymbol.constType != 0)
						return "const " + SymbolMemberInfo.GetBaseType(typeSymbol);
					return SymbolMemberInfo.GetBaseType(typeSymbol);
				case SymTagEnum.SymTagArrayType:
					// get array dimension:
					var dimension = typeSymbol.count.ToString();
					return $"{GetType(typeSymbol.type)}[{dimension}]";
				case SymTagEnum.SymTagUDT:
					return typeSymbol.name;
				case SymTagEnum.SymTagEnum:
					return $"enum {typeSymbol.name}";
				default:
					return string.Empty;
			}
		}
	}


}
