using System;
using System.Collections.Generic;
using System.Linq;
using System.Reflection;
using System.Runtime.CompilerServices;
using System.Runtime.InteropServices;
using System.Runtime.Remoting.Proxies;
using System.Text;
using System.Threading.Tasks;

namespace PDBReader
{
	public unsafe struct TypeBasicInfo
	{
		public string Name { get; private set; }
		public ulong Size { get; private set; }
		public uint InternalId { get; private set; }
		public uint PointedToTypeId { get; private set; }

		public bool IsPrimitive { get; private set; }
		public bool IsPointer { get; private set; }
		public bool IsUDT { get; private set; }

		internal TPIRecord* _fieldListRecord;

		private unsafe TypeBasicInfo(string name, ulong size, uint internalId)
		{
			Name = name;
			Size = size;
			InternalId = internalId;
			IsPrimitive = false;
			IsPointer = false; 
			IsUDT = false;
			PointedToTypeId = 0;
			_fieldListRecord = null;
		}

		internal static TypeBasicInfo MakePrimitive(string name, ulong size, uint internalId)
		{
			return new TypeBasicInfo(name, size, internalId)
			{
				IsPrimitive = true,
			};
		}

		internal static TypeBasicInfo MakePrimitivePointer(string name, ulong size, uint internalId, uint pointedToType)
		{
			return new TypeBasicInfo(name, size, internalId)
			{
				IsPointer = true,
				PointedToTypeId = pointedToType,
			};
		}

		internal static TypeBasicInfo MakeUDT(string name, ulong size, uint internalId, TPIRecord* fieldListRecord)
		{
			return new TypeBasicInfo(name, size, internalId)
			{
				IsUDT = true,
				_fieldListRecord = fieldListRecord,
			};
		}

		internal static TypeBasicInfo MakeEnum(string name, ulong size, uint internalId)
		{
			return new TypeBasicInfo($"enum {name}", size, internalId);
		}

		internal static TypeBasicInfo MakeArray(TypeBasicInfo elementType, ulong size, uint internalId)
		{
			if (elementType.Size > 0)
			{
				var count = size / elementType.Size;
				return new TypeBasicInfo($"{elementType.Name}[{count}]", size, internalId);
			}
			else
			{
				return new TypeBasicInfo($"{elementType.Name}[?]", size, internalId);
			}
		}
		
		internal static TypeBasicInfo MakePointer(TypeBasicInfo pointedToType, ulong size, uint internalId)
		{
			return new TypeBasicInfo($"{pointedToType.Name}*", size, internalId)
			{
				IsPointer = true,
				PointedToTypeId = pointedToType.InternalId,
			};
		}

		internal static TypeBasicInfo MakeModified(TypeBasicInfo underlyingType, ModifierFlags flags, uint internalId)
		{
			var name = new StringBuilder();
			if ((flags & ModifierFlags.Const) == ModifierFlags.Const)
			{
				name.Append("const ");
			}
			if ((flags & ModifierFlags.Volatile) == ModifierFlags.Volatile)
			{
				name.Append("volatile ");
			}
			name.Append(underlyingType.Name);
			return new TypeBasicInfo(name.ToString(), underlyingType.Size, internalId)
			{
				IsPointer = underlyingType.IsPointer,
				IsUDT = underlyingType.IsUDT,
				IsPrimitive = underlyingType.IsPrimitive,
				PointedToTypeId = underlyingType.PointedToTypeId,
				_fieldListRecord = underlyingType._fieldListRecord,
			};
		}
	
		internal static TypeBasicInfo MakeUnknown(string name, ulong size, uint internalId)
		{
			return new TypeBasicInfo(name, size, internalId);
		}
	}

	public unsafe class TypeFullInfo 
	{
		private TypeBasicInfo _basic;

		public string Name { get { return _basic.Name; } }
		public ulong Size { get { return _basic.Size; } }
		public ulong InternalId { get { return _basic.InternalId; } }
		public bool IsPrimitive { get { return _basic.IsPrimitive; } }
		public bool IsPointer { get { return _basic.IsPointer; } }
		public ulong PointedToTypeId { get { return _basic.PointedToTypeId; } }

		private TypeMember[] _members;

		private TypeFullInfo(TypeBasicInfo basic)
		{
			_basic = basic;
		}

		internal static TypeFullInfo MakePrimitive(TypeBasicInfo basic)
		{
			return new TypeFullInfo(basic) { _members = new TypeMember[0] };
		}

		internal static TypeFullInfo MakeStructure(TypeBasicInfo basic, DataMemberInfo[] dataMembers)
		{
			var members = dataMembers.Select(x =>
			{
				return new TypeMember
				{
					Name = Marshal.PtrToStringAnsi(x.Name) ?? "",
					Offset = x.Offset,
					IsVtable = x.IsVtable,
					IsBitfield = x.IsBitfield,
					IsBaseClass = x.IsBaseClass,
					BitPosition = x.BitPosition,
					BitSize = x.BitSize,
					TypeIndex = x.TypeIndex,

				};
			}).ToArray();
			return new TypeFullInfo(basic) { _members = members };
		}

		public IEnumerable<TypeMember> Members
		{
			get
			{
				return _members;
			}
		}
	}

	public struct TypeMember
	{
		public string Name { get; internal set; }
		public ulong Size { get; internal set; }
		public ulong Offset { get; internal set; }

		public bool IsBaseClass { get; internal set; }
		public bool IsVtable { get; internal set; }

		public bool IsBitfield { get; internal set; }
		public uint BitPosition { get; internal set; }
		public uint BitSize { get; internal set; }

		public uint TypeIndex { get; internal set; }
	}

	public unsafe class PDB : IDisposable
	{
		PDBHandle* _handle = null;

		struct TPIStream
		{
			public TPIRecord** Records;
			public ulong RecordCount;
			public uint TypeIndexBegin, TypeIndexEnd;

		}
		TPIStream _tpiStream;

		Task _lookupsTask;
		Dictionary<string, uint> _byName = new Dictionary<string, uint>(StringComparer.InvariantCulture);
		Dictionary<string, uint> _byNameInsensitive = new Dictionary<string, uint>(StringComparer.InvariantCultureIgnoreCase);
		Dictionary<uint, uint> _byFwdDeclIndex = new Dictionary<uint, uint>();

		internal PDB(string filename)
		{
			_handle = PDBReader.LoadPDB(filename);
			if (_handle == null)
			{
				throw new Exception("Failed to load PDB");
			}

			PDBReader.GetTypeRecordsInfo(
				_handle,
				out TPIRecord** Records,
				out ulong RecordCount,
				out uint TypeIndexBegin,
				out uint TypeIndexEnd);
			_tpiStream = new TPIStream
			{
				Records = Records,
				RecordCount = RecordCount,
				TypeIndexBegin = TypeIndexBegin,
				TypeIndexEnd = TypeIndexEnd
			};

			_lookupsTask = Task.Run(PrepareLookups);
		}

		private void PrepareLookups()
		{ 
			for (uint i = _tpiStream.TypeIndexBegin; i < _tpiStream.TypeIndexEnd; ++i)
			{
				TPIRecord* record = _tpiStream.Records[i - _tpiStream.TypeIndexBegin];
				var kind = (*record).Kind;
				if (kind != TypeRecordKind.Structure && kind != TypeRecordKind.Class && kind != TypeRecordKind.Union)
				{
					continue;
				}

				var flags = (*record).Class.Flags; // Class and union have the same offset for this field 
				if ((flags & ClassFlags.FwdRef) == ClassFlags.FwdRef
					|| (flags & ClassFlags.HasUniqueName) != ClassFlags.HasUniqueName)
				{
					continue;
				}

				var basicInfo = GetBasicTypeInfoInternal(i);
				// TODO: Investigate if dupes are ODR violations or something else
				if (!_byName.ContainsKey(basicInfo.Name))
				{
					_byName.Add(basicInfo.Name, i);
				}
				if(!_byNameInsensitive.ContainsKey(basicInfo.Name))
				{
					_byNameInsensitive.Add(basicInfo.Name, i);
				}
			}

			for (uint i = _tpiStream.TypeIndexBegin; i < _tpiStream.TypeIndexEnd; ++i)
			{
				TPIRecord* record = _tpiStream.Records[i - _tpiStream.TypeIndexBegin];
				var kind = (*record).Kind;
				
				if (kind == TypeRecordKind.Structure || kind == TypeRecordKind.Class)
				{
					var flags = (*record).Class.Flags;
					if ((flags & ClassFlags.FwdRef) == ClassFlags.FwdRef)
					{
						var basicInfo = GetBasicTypeInfoInternal(i);
						if (_byName.TryGetValue(basicInfo.Name, out var originalIndex))
						{
							_byFwdDeclIndex.Add(i, originalIndex);
						}
					}
				}
				else if (kind == TypeRecordKind.Union)
				{
					var flags = (*record).Union.Flags;
					if ((flags & ClassFlags.FwdRef) == ClassFlags.FwdRef)
					{
						var basicInfo = GetBasicTypeInfoInternal(i);
						if (_byName.TryGetValue(basicInfo.Name, out var originalIndex))
						{
							_byFwdDeclIndex.Add(i, originalIndex);
						}
					}
				}
			}
		}

		public void Dispose()
		{
			if (_handle != null)
			{
				PDBReader.ClosePDB(_handle);
				_handle = null;
			}
		}

		public ulong NumTypeRecords { get { return _tpiStream.RecordCount; } }
		public ulong NumElements { get { return _tpiStream.TypeIndexEnd - _tpiStream.TypeIndexBegin; } }

		private bool IsNonFwdDeclaredStruct(uint index)
		{
			unsafe
			{
				TPIRecord* record = _tpiStream.Records[index - _tpiStream.TypeIndexBegin];
				if ((*record).Kind != TypeRecordKind.Structure && (*record).Kind != TypeRecordKind.Class)
				{
					return false;
				}

				if (((*record).Class.Flags & ClassFlags.FwdRef) == ClassFlags.FwdRef)
				{
					return false;
				}
				return true;
			}
		}

		public IEnumerable<TypeBasicInfo> TypeBasicInfos
		{
			get
			{
				return Enumerable.Range(0, (int)(_tpiStream.TypeIndexEnd - _tpiStream.TypeIndexBegin))
					.Where(i => IsNonFwdDeclaredStruct((uint)i + _tpiStream.TypeIndexBegin))
					.Select(i => GetBasicTypeInfo((uint)i + _tpiStream.TypeIndexBegin));
			}
		}

		public TypeBasicInfo? FindType(string name, bool matchCase)
		{
			_lookupsTask.Wait();
			if (matchCase)
			{
				if (_byName.TryGetValue(name, out uint index))
				{
					return GetBasicTypeInfo(index);
				}
			}
			else
			{
				if (_byNameInsensitive.TryGetValue(name, out uint index))
				{
					return GetBasicTypeInfo(index);
				}
			}
			return null;
		}

		public TypeBasicInfo GetBasicTypeInfo(uint index)
		{
			_lookupsTask.Wait();
			if (_byFwdDeclIndex.TryGetValue(index, out var redirectIndex))
			{
				return GetBasicTypeInfoInternal(redirectIndex);
			}
			return GetBasicTypeInfoInternal(index);
		}

		private TypeBasicInfo GetBasicTypeInfoInternal(uint index)
		{
			unsafe
			{
				PDBReader.GetBasicTypeInfo(_handle, index, out RecordBasicInfo basicInfo);
				if (index < _tpiStream.TypeIndexBegin)
				{
					return TypeBasicInfo.MakePrimitive(Marshal.PtrToStringAnsi(basicInfo.Name), basicInfo.Size, index);
				}

				TypeBasicInfo underlying;
				switch (basicInfo.Kind)
				{
					case TypeRecordKind.Class:
					case TypeRecordKind.Structure:
					case TypeRecordKind.Union:
						return TypeBasicInfo.MakeUDT(
							Marshal.PtrToStringAnsi(basicInfo.Name) ?? "",
							basicInfo.Size,
							index,
							basicInfo.RelatedTypeIndex > 0 ? _tpiStream.Records[basicInfo.RelatedTypeIndex - _tpiStream.TypeIndexBegin] : null
							);
					case TypeRecordKind.Pointer:
						underlying = GetBasicTypeInfo(basicInfo.RelatedTypeIndex);
						return TypeBasicInfo.MakePointer(underlying, basicInfo.Size, index);
					case TypeRecordKind.Modifier:
						underlying = GetBasicTypeInfo(basicInfo.RelatedTypeIndex);
						return TypeBasicInfo.MakeModified(underlying, basicInfo.Modifiers, index);
					case TypeRecordKind.Enum:
						underlying = GetBasicTypeInfo(basicInfo.RelatedTypeIndex);
						return TypeBasicInfo.MakeEnum(
							Marshal.PtrToStringAnsi(basicInfo.Name) ?? "",
							underlying.Size,
							index
							);
					case TypeRecordKind.Array:
						var elementType = GetBasicTypeInfo(basicInfo.RelatedTypeIndex);
						return TypeBasicInfo.MakeArray(elementType, basicInfo.Size, index);
					default:
						return TypeBasicInfo.MakeUnknown(
							Marshal.PtrToStringAnsi(basicInfo.Name) ?? "",
							basicInfo.Size,
							index
							);
				}
			}
		}

		public TypeFullInfo GetFullTypeInfo(TypeBasicInfo info)
		{
			if (info.IsPrimitive)
			{
				return TypeFullInfo.MakePrimitive(info);
			}
			unsafe
			{
				TPIRecord* fieldListRecord = info._fieldListRecord;
				PDBReader.CountNumFields(_handle, fieldListRecord, out StructFieldCounts counts);
				var memberInfos = new DataMemberInfo[counts.NumDataMembers];
				PDBReader.ReadDataMembers(_handle, fieldListRecord, memberInfos);
				return TypeFullInfo.MakeStructure(info, memberInfos);
			}
		}
	}
}
