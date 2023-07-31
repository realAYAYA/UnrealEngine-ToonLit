using Dia2Lib;
using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;

namespace CruncherSharp
{
    public class SymbolInfo
    {
        public string Name { get; private set; }
        public string TypeName { get; set; }
        public ulong Size { get; set; }
        public ulong NewSize { get; set; }
        public ulong EndPadding { get; set; }
        public ulong Padding => (ulong)((long)EndPadding + Members.Sum(info => (long)info.PaddingBefore)); // This is the local (intrinsic) padding
        public ulong PaddingZonesCount => (ulong)((EndPadding > 0 ? 1 : 0) + Members.Sum(info => info.PaddingBefore > 0 ? 1 : 0));
        public ulong? TotalPadding { get; set; } // Includes padding from base classes and members
        public ulong NumInstances { get; set; }
        public ulong TotalCount { get; set; }
        public bool IsAbstract { get; set; }
        public bool IsTemplate { get; set; }
		public bool IsImportedFromCSV { get; set; }
		public List<SymbolMemberInfo> Members { get; set; }
        public List<SymbolFunctionInfo> Functions { get; set; }
        public List<SymbolInfo> DerivedClasses { get; set; }

        private const string PaddingMarker = "****Padding";

        public SymbolInfo(string name, string typeName, ulong size)
        {
            Name = name;
            TypeName = typeName;
            Size = size;
            EndPadding = 0;
            TotalPadding = null;
            Members = new List<SymbolMemberInfo>();
            Functions = new List<SymbolFunctionInfo>();
            IsAbstract = false;
			IsImportedFromCSV = false;

			if (Name.Contains("<") && Name.Contains(">"))
                IsTemplate = true;
        }

        private void AddMember(SymbolMemberInfo member)
        {
            Members.Add(member);
        }

        private void AddFunction(SymbolFunctionInfo function)
        {
            Functions.Add(function);
        }

        private bool ComputeOffsetCollision(int index)
        {
            return index > 0 && Members[index].Offset == Members[index - 1].Offset;
        }

        public bool HasVtable
        {
            get
            {
                foreach (var member in Members)
                {
                    if (member.Category == SymbolMemberInfo.MemberCategory.VTable)
                    {
                        return true;
                    }
                }
                return false;
            }
        }

        public bool HasBaseClass
        {
            get
            {
                foreach (var member in Members)
                {
                    if (member.Category == SymbolMemberInfo.MemberCategory.Base)
                    {
                        return true;
                    }
                }
                return false;
            }
        }

		// https://randomascii.wordpress.com/2013/12/01/vc-2013-class-layout-change-and-wasted-space/
		public bool HasMSVCExtraPadding
        {
            get
            {
                if (HasBaseClass)
                    return false;
                if (!HasVtable)
                    return false;
                if (Members.Count < 2)
                    return false;
                return Members[1].Size == 8 && Members[1].Offset == 16;
            }
        }

        public bool HasMSVCEmptyBaseClass
        {
            get
            {
                var n = 1;
                while (n + 2 < Members.Count)
                {
                    if (Members[n - 1].IsBase && Members[n].IsBase && Members[n].Size == 1 && Members[n].Offset > Members[n - 1].Offset && Members[n + 1].Offset > Members[n].Offset)
                        return true;
                    n++;
                }
                return false;

            }
        }

        private ulong ComputePadding(int index)
        {
            if (index < 1 || index > Members.Count)
            {
                return 0;
            }
            if (index < Members.Count && Members[index].AlignWithPrevious)
            {
                return 0;
            }
            int previousIndex = index - 1;
            ulong biggestSize = Members[previousIndex].Size;
            while (Members[previousIndex].AlignWithPrevious && previousIndex > 0)
            {
                previousIndex--;
                if (biggestSize < Members[previousIndex].Size)
                    biggestSize = Members[previousIndex].Size;
            }

            ulong currentOffset = index > Members.Count - 1 ? Size : Members[index].Offset;
            ulong previousEnd = Members[previousIndex].Offset + biggestSize;
            return currentOffset > previousEnd ? currentOffset - previousEnd : 0;
        }

        private ulong ComputeBitPadding(int index)
        {
            if (index > Members.Count)
            {
                return 0;
            }
            if (!Members[index].BitField)
            {
                return 0;
            }
            if (index + 1 < Members.Count)
            {
                if (Members[index + 1].BitField && Members[index + 1].Offset == Members[index].Offset)
                    return 0;
            }
            return (8 * Members[index].Size) - (Members[index].BitPosition + Members[index].BitSize);
        }

        private ulong ComputeEndPadding()
        {
            return ComputePadding(Members.Count);
        }

        public ulong ComputeTotalPadding(SymbolAnalyzer symbolAnalyzer)
        {
            if (TotalPadding.HasValue)
            {
                return TotalPadding.Value;
            }
            TotalPadding = (ulong)((long)Padding + Members.Sum(info =>
            {
                if (info.AlignWithPrevious)
                    return 0;
                if (info.Category == SymbolMemberInfo.MemberCategory.Member)
                    return 0;
                if (info.TypeName == Name)
                    return 0; // avoid infinite loops
                var referencedInfo = symbolAnalyzer.FindSymbolInfo(info.TypeName);
                if (referencedInfo == null)
                {
                    return 0;
                }
                return (long)referencedInfo.ComputeTotalPadding(symbolAnalyzer);
            }));
            return TotalPadding.Value;
        }

        public void UpdateBaseClass(SymbolAnalyzer symbolAnalyzer)
        {
            foreach (var member in Members)
            {
                if (member.Category == SymbolMemberInfo.MemberCategory.Base)
                {
                    var referencedInfo = symbolAnalyzer.FindSymbolInfo(member.Name);
                    if (referencedInfo != null)
                    {
                        if (referencedInfo.DerivedClasses == null)
                            referencedInfo.DerivedClasses = new List<SymbolInfo>();
                        referencedInfo.DerivedClasses.Add(this);
                    }
                }
            }
        }

        public void CheckOverride()
        {
            foreach (var function in Functions)
            {
                if (function.Virtual && 
					function.IsOverloaded == false && 
					function.Category == SymbolFunctionInfo.FunctionCategory.Function && 
					DerivedClasses != null)
                {
                    foreach(var derivedClass in DerivedClasses)
                    {
                        if (derivedClass.IsOverloadingFunction(function))
                        {
                            function.IsOverloaded = true;
                            break;
                        }
                    }
                }
            }
        }

        public void CheckMasking()
        {
            foreach (var function in Functions)
            {
                if (function.Virtual == false && function.Category == SymbolFunctionInfo.FunctionCategory.Function && DerivedClasses != null)
                {
                    foreach (var derivedClass in DerivedClasses)
                    {
                        derivedClass.CheckMasking(function);
                    }
                }
            }
        }

        private void CheckMasking(SymbolFunctionInfo func)
        {
            foreach (var function in Functions)
            {
                if (function.Virtual == false && function.Name == func.Name)
                {
                    function.IsMasking = true;

                    if (DerivedClasses != null)
                    {
                        foreach (var derivedClass in DerivedClasses)
                        {
                            derivedClass.CheckMasking(func);
                        }
                    }
                }
            }
        }

        private bool IsOverloadingFunction(SymbolFunctionInfo func)
        {
            foreach (var function in Functions)
            {
                if (function.Name == func.Name)
                    return true;
            }
            if (DerivedClasses != null)
            {
                foreach (var derivedClass in DerivedClasses)
                {
                    if (derivedClass.IsOverloadingFunction(func))
                        return true;
                }
            }
            return false;
        }

        public void UpdateTotalCount(SymbolAnalyzer symbolAnalyzer, ulong count)
        {
            foreach (var member in Members)
            {
                if (member.Category == SymbolMemberInfo.MemberCategory.UDT || member.Category == SymbolMemberInfo.MemberCategory.Base)
                {
                    var referencedInfo = symbolAnalyzer.FindSymbolInfo(member.TypeName);
                    if (referencedInfo != null)
                    {
                        referencedInfo.TotalCount += count;
                        referencedInfo.UpdateTotalCount(symbolAnalyzer,count);
                    }
                }
            }
        }

        public void ProcessChildren(IDiaSymbol symbol)
        {
            symbol.findChildren(SymTagEnum.SymTagNull, null, 0, out var children);
            if (children == null)
                return;
            foreach (IDiaSymbol child in children)
            {
                if (child.symTag == (uint)SymTagEnum.SymTagFunction)
                {
                    var functionInfo = ProcessFunction(child);
                    if (functionInfo != null)
                    {
                        AddFunction(functionInfo);
                        if (functionInfo.IsPure)
                            IsAbstract = true;
                    }
                }
                else
                {
                    var memberInfo = ProcessMember(child);
                    if (memberInfo != null)
                    {
                        AddMember(memberInfo);
                    }
                }
            }
            // Sort members by offset, recompute padding.
            // Sorting is usually not needed (for data fields), but sometimes base class order is wrong.
            Members.Sort(SymbolMemberInfo.CompareOffsets);
            for (int i = 0; i < Members.Count; ++i)
            {
                var member = Members[i];
                member.AlignWithPrevious = ComputeOffsetCollision(i);
                member.PaddingBefore = ComputePadding(i);
                member.BitPaddingAfter = ComputeBitPadding(i);
            }
            EndPadding = ComputeEndPadding();
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
                category = SymbolMemberInfo.MemberCategory.Base;
            else if ((SymTagEnum)typeSymbol.symTag == SymTagEnum.SymTagUDT)
                category = SymbolMemberInfo.MemberCategory.UDT;
            else if ((SymTagEnum)typeSymbol.symTag == SymTagEnum.SymTagPointerType)
                category = SymbolMemberInfo.MemberCategory.Pointer;

            var info = new SymbolMemberInfo(category, symbolName, typeName, typeSymbol.length, symbol.length, (ulong)symbol.offset, symbol.bitPosition);

            if (typeSymbol.volatileType == 1)
                info.Volatile = true;
            if (symbol.locationType == 6)
                info.BitField = true;

            return info;
        }

        public SymbolFunctionInfo ProcessFunction(IDiaSymbol symbol)
        {
            if (symbol.compilerGenerated > 0)
                return null;

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
                info.WasInlineRemoved = true;

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
                        returnType += "(" + string.Join(",",parameters) + ")";
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

        public override string ToString()
        {
            var sw = new StringWriter();

            sw.WriteLine($"Symbol: {Name}");
            sw.WriteLine($"TypeName: {TypeName}");
            sw.WriteLine($"Size: {Size}");
            sw.WriteLine($"Padding: {Padding}");
            sw.WriteLine($"Total padding: {TotalPadding}");
            sw.WriteLine("Members:");
            sw.WriteLine("-------");

            foreach (var member in Members)
            {
                if (member.PaddingBefore > 0)
                {
                    var paddingOffset = member.Offset - member.PaddingBefore;
                    sw.WriteLine($"{PaddingMarker,-40} {paddingOffset,5} {member.PaddingBefore,5}");
                }
                sw.WriteLine($"{member.DisplayName,-40} {member.Offset,5} {member.Size,5}");
            }

            if (EndPadding > 0)
            {
                var endPaddingOffset = Size - EndPadding;
                sw.WriteLine($"{PaddingMarker,-40} {endPaddingOffset,5} {EndPadding,5}");
            }

            return sw.ToString();
        }
    }
}
