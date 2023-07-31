using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace CruncherSharp
{
    public class SymbolFunctionInfo
    {
        public enum FunctionCategory
        {
            Function,
            StaticFunction,
            Constructor,
            Destructor
        }

        public string Name { get; set; }
        public string DisplayName
        {
            get
            {
                switch (Category)
                {
                    case FunctionCategory.StaticFunction:
                        return $"static {Name}";
                    default:
                        return Name + (IsConst ? " const" : "");
                }
            }
        }

        public bool UnusedVirtual
        {
            get
            {
                return Category == FunctionCategory.Function && Virtual && !IsOverloaded && !IsOverride;
            }
        }

        public FunctionCategory Category { get; set; }

        public bool Virtual { get; set; }
        public bool IsPure { get; set; }
        public bool IsOverride { get; set; }
        public bool IsOverloaded { get; set; }
        public bool IsMasking { get; set; }
        public bool IsConst { get; set; }
        public bool WasInlineRemoved { get; set; }

    }
}
