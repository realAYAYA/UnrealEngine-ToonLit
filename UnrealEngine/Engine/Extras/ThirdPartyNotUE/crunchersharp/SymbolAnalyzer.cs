using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Linq;
using System.Text.RegularExpressions;

namespace CruncherSharp
{
    public class LoadPDBTask
    {
        public string FileName { get; set; }
        public string Filter { get; set; }
        public bool SecondPDB { get; set; }
        public bool MatchCase { get; set; }
        public bool WholeExpression { get; set; }
        public bool UseRegularExpression { get; set; }
        public bool UseProgressBar { get; set; }
    }

	public class LoadCSVTask
	{
		public string ClassName { get; set; }
		public ulong Count { get; set; }
	}

    public abstract class SymbolAnalyzer
    {
        public Dictionary<string, SymbolInfo> Symbols { get; }
        public SortedSet<string> RootNamespaces { get; }
        public SortedSet<string> Namespaces { get; }
        public string LastError { get; private set; }
        public string FileName { get; set; }

		private List<uint> _MemPools;
		public List<uint> MemPools 
		{
			get => _MemPools;
			set
			{
				_MemPools = value;
				foreach (var symbol in Symbols.Values)
				{
					symbol.SetMemPools(_MemPools);
				}
			}
		}

		private bool _FunctionAnalysis = false;
		public bool FunctionAnalysis 
		{
			get => _FunctionAnalysis;
			set
			{
				_FunctionAnalysis = value;
				if (value)
				{
					foreach (var symbol in Symbols.Values)
					{
						symbol.CheckOverride();
						symbol.CheckMasking();
					}
				}
			}
		}

        public SymbolAnalyzer()
        {
            Symbols = new Dictionary<string, SymbolInfo>();
            RootNamespaces = new SortedSet<string>();
            Namespaces = new SortedSet<string>();
			_MemPools = new List<uint>();
			LastError = string.Empty;
        }

        public void Reset()
        {
            RootNamespaces.Clear();
            Namespaces.Clear();
            Symbols.Clear();
			LastError = string.Empty;
            FileName = null;
        }

		public virtual void OpenAsync(string filename) 
		{
			FileName = filename;
		}

        public bool LoadPdb(object sender, DoWorkEventArgs e)
        {
            try
            {
                var task = e.Argument as LoadPDBTask;
                FileName = task.FileName;
                if (!LoadPdb(sender, task))
                {
                    e.Cancel = true;
                    return false;
                }
                return true;
            }
            catch (System.Runtime.InteropServices.COMException exception)
            {
                LastError = exception.ToString();
                return false; 
            }
        }

		public abstract bool LoadPdb(object sender, LoadPDBTask task);

        protected void RunAnalysis()
        {
            foreach (var symbol in Symbols.Values)
            {
                symbol.ComputeTotalPadding(this);
                symbol.UpdateBaseClass(this);
            }

			if (!FunctionAnalysis)
			{
				return;
			}

            foreach (var symbol in Symbols.Values)
            {
                symbol.CheckOverride();
                symbol.CheckMasking();
            }
        }
        public bool HasSymbolInfo(string name)
        {
            return Symbols.ContainsKey(name);
        }

		public bool LoadCSV(object sender, DoWorkEventArgs e)
		{
			try
			{
				var task = e.Argument as List<LoadCSVTask>;
				if (!LoadCSV(sender, task))
				{
					e.Cancel = true;
					return false;
				}
				return true;
			}
			catch (System.Runtime.InteropServices.COMException exception)
			{
				LastError = exception.ToString();
				return false;
			}
		}

		public abstract bool LoadCSV(object sender, List<LoadCSVTask> tasks);

        public SymbolInfo FindSymbolInfo(string name, bool loadMissingSymbol = false)
        {
            if (!HasSymbolInfo(name) && name.Length > 0)
            {
                if (!loadMissingSymbol)
                    return null;
                var task = new LoadPDBTask
                {
                    FileName = FileName,
                    SecondPDB = false,
                    Filter = name,
                    MatchCase = true,
                    WholeExpression = true,
                    UseRegularExpression = false,
                    UseProgressBar = false
                };

                if (!LoadPdb(null, task))
                    return null;
            }

            Symbols.TryGetValue(name, out var symbolInfo);
            return symbolInfo;
        }
    }
}
