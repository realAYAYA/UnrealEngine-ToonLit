using System;
using System.Collections.Generic;
using System.Linq;
using System.ComponentModel;
using System.Text.RegularExpressions;
using System.Net.Mime;
using System.Collections.Concurrent;
using System.Threading.Tasks;

#if RAWPDB
using PDBReader;
#endif

namespace CruncherSharp
{
#if RAWPDB
	internal class SymbolAnalyzerRawPDB : SymbolAnalyzer
	{
		PDB pdbFile = null;

		public override void OpenAsync(string filename) 
		{
			pdbFile = new PDB(filename);
		}

		public override bool LoadPdb(object sender, LoadPDBTask task)
		{
			if (task.SecondPDB || pdbFile == null)
			{
				pdbFile = new PDB(task.FileName);
			}

			if (!LoadSymbols(sender, task))
			{
				return false;
			}
			RunAnalysis();
			return true;
		}

		private bool LoadSymbols(object sender, LoadPDBTask task)
		{
			var worker = sender as BackgroundWorker;
			worker?.ReportProgress(0, "Finding symbols");

			IEnumerable<PDBReader.TypeBasicInfo> allSymbols;
			if (task.Filter.Length > 0)
			{
				if (task.UseRegularExpression)
				{
					Regex r = new Regex(task.WholeExpression ? $"^{task.Filter}$" : task.Filter, RegexOptions.Compiled);
					allSymbols = pdbFile.TypeBasicInfos.Where(x => r.IsMatch(x.Name));
				}
				else if (task.WholeExpression)
				{
					var sym = pdbFile.FindType(task.Filter, task.MatchCase);
					if (sym.HasValue)
					{
						allSymbols = new PDBReader.TypeBasicInfo[] { sym.Value };
					}
					else
					{
						allSymbols = new PDBReader.TypeBasicInfo[0];
					}
				}
				else
				{
					var Comparison = task.MatchCase ? StringComparison.InvariantCulture : StringComparison.InvariantCultureIgnoreCase;
					allSymbols = pdbFile.TypeBasicInfos.Where(x => x.Name.IndexOf(task.Filter, Comparison) != -1);
				}
			}
			else
			{
				allSymbols = pdbFile.TypeBasicInfos;
			}

			if (allSymbols == null)
			{
				return false;
			}

			worker?.ReportProgress(0, "Adding symbols");
			var allSymbolsCount = pdbFile.NumElements;
			uint addedSymbolsCount = 0;
			if (task.SecondPDB)
			{
				allSymbols.AsParallel().ForAll(symBasicInfo =>
				{
					SymbolInfo info = FindSymbolInfo(symBasicInfo.Name);
					if (info != null)
					{
						info.NewSize = symBasicInfo.Size;
						++addedSymbolsCount;
					}
				});
			}
			else
			{
				ConcurrentDictionary<string, SymbolInfo> results = new ConcurrentDictionary<string, SymbolInfo>();
				allSymbols.AsParallel().ForAll(symBasicInfo => 
				{
					if (symBasicInfo.Size == 0 || HasSymbolInfo(symBasicInfo.Name))
					{
						return;
					}

					if (task.Filter.Length > 0)
					{
						LoadSymbolsRecursive(symBasicInfo, results);
					}
					else
					{
						// Loading all symbols so don't recurse
						var symbolInfo = new SymbolInfo(symBasicInfo.Name, symBasicInfo.GetType().Name, symBasicInfo.Size, MemPools);
						if (results.TryAdd(symbolInfo.Name, symbolInfo))
						{
							var symFullInfo = pdbFile.GetFullTypeInfo(symBasicInfo);
							ProcessDataMembers(symbolInfo, pdbFile, symFullInfo);
							ProcessFunctions(symbolInfo, pdbFile, symFullInfo);
							symbolInfo.SortAndCalculate();
						}
					}
				});
				int progress = 0;
				foreach (KeyValuePair<string, SymbolInfo> pair in results)
				{
					if (!HasSymbolInfo(pair.Key))
					{
						Symbols.Add(pair.Key, pair.Value);
						++addedSymbolsCount;

						var percentProgress = (int)Math.Round((double)(100 * addedSymbolsCount) / allSymbolsCount);
						if (percentProgress > progress)
						{
							progress = Math.Max(Math.Min(percentProgress, 99), 1);
							worker?.ReportProgress(progress, String.Format("Adding symbol {0} on {1}", addedSymbolsCount, allSymbolsCount));
						}


						if (pair.Key.Contains("::") && !pair.Key.Contains("<"))
						{
							RootNamespaces.Add(pair.Key.Substring(0, pair.Key.IndexOf("::")));
							Namespaces.Add(pair.Key.Substring(0, pair.Key.LastIndexOf("::")));
						}
					}
				}
			}

			worker?.ReportProgress(100, String.Format("{0} symbols added", addedSymbolsCount));

			if (task.Filter.Length == 0)
			{
				// Everything was loaded, close PDB
				pdbFile.Dispose();
				pdbFile = null;
			}

			return true;
		}

		private void LoadSymbolsRecursive(TypeBasicInfo newType, ConcurrentDictionary<string, SymbolInfo> results)
		{
			if (Symbols.ContainsKey(newType.Name) || results.ContainsKey(newType.Name))
			{
				return;
			}
			if (newType.Size == 0)
			{
				return;
			}

			var symbolInfo = new SymbolInfo(newType.Name, newType.Name, newType.Size, MemPools);
			if (results.TryAdd(symbolInfo.Name, symbolInfo))
			{
				var symFullInfo = pdbFile.GetFullTypeInfo(newType);
				ProcessDataMembers(symbolInfo, pdbFile, symFullInfo);
				ProcessFunctions(symbolInfo, pdbFile, symFullInfo);
				symbolInfo.SortAndCalculate();
				foreach (var member in symbolInfo.Members)
				{
					if (member.Category == SymbolMemberInfo.MemberCategory.UDT || member.Category == SymbolMemberInfo.MemberCategory.Base)
					{
						var info = pdbFile.FindType(member.TypeName, true);
						if (info.HasValue)
						{
							LoadSymbolsRecursive(info.Value, results);
						}
					}
				}
			}
		}

		public override bool LoadCSV(object sender, List<LoadCSVTask> tasks)
		{
			// if pdbFile is null it means that it is fully loaded, just update count
			if (pdbFile != null)
			{
				var worker = sender as BackgroundWorker;

				var allSymbolsCount = (worker != null) ? tasks.Count : 0;
				worker?.ReportProgress(0, "Adding symbols");

				ConcurrentDictionary<string, SymbolInfo> bag = new ConcurrentDictionary<string, SymbolInfo>();
				tasks.AsParallel().ForAll(task =>
				{
					if (worker != null && worker.CancellationPending)
					{
						return;
					}

					var symbol = pdbFile.FindType(task.ClassName, true);
					if (symbol.HasValue)
					{
						LoadSymbolsRecursive(symbol.Value, bag);
					}
				});

				ulong addedSymbolsCount = 0;
				foreach (KeyValuePair<string, SymbolInfo> pair in bag)
				{
					if (!HasSymbolInfo(pair.Key))
					{
						Symbols.Add(pair.Key, pair.Value);
						++addedSymbolsCount;
						if (pair.Key.Contains("::") && !pair.Key.Contains("<"))
						{
							RootNamespaces.Add(pair.Key.Substring(0, pair.Key.IndexOf("::")));
							Namespaces.Add(pair.Key.Substring(0, pair.Key.LastIndexOf("::")));
						}
					}
				}
				worker?.ReportProgress(100, String.Format("{0} symbols added", addedSymbolsCount));
			}

			foreach (LoadCSVTask task in tasks)
			{
				if (Symbols.TryGetValue(task.ClassName, out var symbolInfo))
				{
					symbolInfo.IsImportedFromCSV = true;
					symbolInfo.TotalCount = symbolInfo.NumInstances = task.Count;
				}
			}

			RunAnalysis();
			return true;
		}

		private void ProcessDataMembers(SymbolInfo outSymbolInfo, PDB pdbFile, PDBReader.TypeFullInfo typeFullInfo)
		{
			foreach (var dataMember in typeFullInfo.Members)
			{
				var typeSymbol = pdbFile.GetBasicTypeInfo(dataMember.TypeIndex);

				var category = SymbolMemberInfo.MemberCategory.Member;
				var memberName = dataMember.Name;
				var typeName = typeSymbol.Name;

				if (dataMember.IsVtable)
				{
					category = SymbolMemberInfo.MemberCategory.VTable;
					memberName = string.Empty;
					typeName = string.Empty;
				}
				else if (dataMember.IsBaseClass)
				{
					category = SymbolMemberInfo.MemberCategory.Base;
				}
				// TODO: Fix this to fix expanding struct fields?
				else if (typeSymbol.IsUDT)
				{
					category = SymbolMemberInfo.MemberCategory.UDT;
				}
				else if (typeSymbol.IsPointer)
				{
					category = SymbolMemberInfo.MemberCategory.Pointer;
				}

				var info = new SymbolMemberInfo(
					category,
					memberName,
					typeName,
					typeSymbol.Size, 
					dataMember.BitSize, 
					(ulong)dataMember.Offset,
					dataMember.BitPosition 
				);
				info.BitField = dataMember.IsBitfield;

				outSymbolInfo.Members.Add(info);
			}
		}


		public void ProcessFunctions(SymbolInfo outSymbolInfo, PDB pdbFile, PDBReader.TypeFullInfo typeFullInfo)
		{
			// TODO:
			//foreach (var functionInfo in typeFullInfo.Functions)
			//{
				//if (symbol.compilerGenerated > 0)
				//	return null;

				//var info = new SymbolFunctionInfo();
				//if (symbol.isStatic > 0)
				//{
				//	info.Category = SymbolFunctionInfo.FunctionCategory.StaticFunction;
				//}
				//else if (symbol.classParent.name.EndsWith(symbol.name))
				//{
				//	info.Category = SymbolFunctionInfo.FunctionCategory.Constructor;
				//}
				//else if (symbol.name.StartsWith("~"))
				//{
				//	info.Category = SymbolFunctionInfo.FunctionCategory.Destructor;
				//}
				//else
				//{
				//	info.Category = SymbolFunctionInfo.FunctionCategory.Function;
				//}

				//info.Virtual = (symbol.@virtual == 1);
				//info.IsOverride = info.Virtual && (symbol.intro == 0);
				//info.IsPure = info.Virtual && (symbol.pure != 0);
				//info.IsConst = symbol.constType != 0;
				//if (symbol.wasInlined == 0 && symbol.inlSpec != 0)
				//	info.WasInlineRemoved = true;

				//info.Name = GetType(symbol.type.type) + " " + symbol.name;

				//symbol.type.findChildren(SymTagEnum.SymTagFunctionArgType, null, 0, out var syms);
				//if (syms.count == 0)
				//{
				//	info.Name += "(void)";
				//}
				//else
				//{
				//	var parameters = new List<string>();
				//	foreach (IDiaSymbol argSym in syms)
				//	{
				//		parameters.Add(GetType(argSym.type));
				//	}
				//	info.Name += "(" + string.Join(",", parameters) + ")";
				//}
				//outSymbol.AddFunction(functionInfo);
				//if (functionInfo.IsPure)
				//{
				//	outSymbol.IsAbstract = true;
				//}
			//}
		}
	}
#endif
}
