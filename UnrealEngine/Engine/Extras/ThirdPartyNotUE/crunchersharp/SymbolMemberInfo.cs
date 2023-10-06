using Dia2Lib;
using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;

namespace CruncherSharp
{
    public class SymbolMemberInfo
    {
        public enum MemberCategory
        {
            VTable = 0,
            Base = 1,
            Member = 2,
            UDT = 3,
            Pointer = 4
        }

        public MemberCategory Category { get; set; }
        public string Name { get; set; }
        public string DisplayName
        {
            get
            {
                switch (Category)
                {
                    case MemberCategory.VTable:
                        return "vtable";
                    case MemberCategory.Base:
                        return $"Base: {Name}";
                    default:
                        return Name;
                }
            }
        }
        public string TypeName { get; set; }
        public ulong Size { get; set; }
        public ulong BitSize { get; set; }
        public ulong Offset { get; set; }
        public uint BitPosition { get; set; }
        public ulong PaddingBefore { get; set; }
        public ulong BitPaddingAfter { get; set; }

        public bool AlignWithPrevious { get; set; }
        public bool BitField { get; set; }

        public bool Volatile { get; set; }
        public bool Expanded { get; set; }

        public SymbolMemberInfo(MemberCategory category, string name, string typeName, ulong size, ulong bitSize, ulong offset, uint bitPosition)
        {
            Category = category;
            Name = name;
            TypeName = typeName;
            Size = size;
            BitSize = bitSize;
            Offset = offset;
            BitPosition = bitPosition;
            AlignWithPrevious = false;
            PaddingBefore = 0;
            BitPaddingAfter = 0;
            BitField = false;
            Volatile = false;
            Expanded = false;
        }

        public bool IsBase => Category == MemberCategory.Base;

        public bool IsExapandable => (Category == MemberCategory.Base || Category == MemberCategory.UDT);

        public static int CompareOffsets(SymbolMemberInfo a, SymbolMemberInfo b)
        {
            if (a.Offset != b.Offset)
            {
                return a.Offset < b.Offset ? -1 : 1;
            }
            if (a.IsBase != b.IsBase)
            {
                return a.IsBase ? -1 : 1;
            }
            if (a.BitPosition != b.BitPosition)
            {
                return a.BitPosition < b.BitPosition ? -1 : 1;
            }
            if (a.Size != b.Size)
            {
                return a.Size > b.Size ? -1 : 1;
            }
            return 0;
        }

        public static string GetBaseType(IDiaSymbol typeSymbol)
        {
            //cf. https://msdn.microsoft.com/en-us/library/4szdtzc3.aspx
            switch (typeSymbol.baseType)
            {
                case 0:
                    return string.Empty;
                case 1:
                    return "void";
                case 2:
                    return "char";
                case 3:
                    return "wchar";
                case 6:
                {
                    switch (typeSymbol.length)
                    {
                        case 1:
                            return "int8";
                        case 2:
                            return "int16";
                        case 4:
                            return "int32";
                        case 8:
                            return "int64";
                        default:
                            return "int";
                    }
                }
                case 7:
                    switch (typeSymbol.length)
                    {
                        case 1:
                            return "uint8";
                        case 2:
                            return "uint16";
                        case 4:
                            return "uint32";
                        case 8:
                            return "uint64";
                        default:
                            return "uint";
                    }
                case 8:
                    return "float";
                case 9:
                    return "BCS";
                case 10:
                    return "bool";
                case 13:
                    return "int32";
                case 14:
                    return "uint32";
                case 29:
                    return "bit";
                default:
                    return $"Unhandled: {typeSymbol.baseType}";
            }
        }
    }
}