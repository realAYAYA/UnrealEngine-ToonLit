#
# GDB Printers for the Unreal Engine 4
#
# How to install:
# If the file ~/.gdbinit doesn't exist
#        touch ~/.gdbinit
#        open ~/.gdbinit
#
# and add the following lines:
#   python
#   import sys
#   ...
#   sys.path.insert(0, '/Path/To/Epic/UE/Engine/Extras/GDBPrinters')      <--
#   ...
#   from UEPrinters import register_ue_printers                          <--
#   register_ue_printers (None)                                           <--
#   ...
#   end


import itertools
import random
import re
import sys

import gdb

# ------------------------------------------------------------------------------
# We make our own base of the iterator to prevent issues between Python 2/3.
#

if sys.version_info[0] == 3:
	Iterator = object
else:
	class Iterator(object):

		def next(self):
			return type(self).__next__(self)

def default_iterator(val):
	for field in val.type.fields():
		yield field.name, val[field.name]


# ------------------------------------------------------------------------------
#
#  Custom pretty printers.
#
#


# ------------------------------------------------------------------------------
# FBitReference
#
class FBitReferencePrinter:
	def __init__(self, val):
		self.Value = val

	def to_string(self):
		self.Mask = self.Value['Mask']
		self.Data = self.Value['Data']
		return '\'%d\'' % (self.Data & self.Mask)

# ------------------------------------------------------------------------------
# TBitArray
#
class TBitArrayPrinter:
	"Print TBitArray"

	class _iterator(Iterator):
		def __init__(self, val):
			self.Value = val
			self.Counter = -1

			try:
				self.NumBits = self.Value['NumBits']
				if self.NumBits.is_optimized_out:
					self.NumBits = 0
				else:
					self.AllocatorInstance = self.Value['AllocatorInstance']
					self.InlineData = self.AllocatorInstance['InlineData']
					self.SecondaryData = self.AllocatorInstance['SecondaryData']
					self.SecondaryDataData = self.AllocatorInstance['SecondaryData']['Data']
					if self.SecondaryData != None:
						self.SecondaryDataData = self.SecondaryData['Data']
			except:
				raise

		def __iter__(self):
			return self

		def __next__(self):
			if self.NumBits == 0:
				raise StopIteration

			self.Counter = self.Counter + 1

			if self.Counter >= self.NumBits:
				raise StopIteration

			if self.SecondaryDataData > 0:
				data = self.SecondaryDataData.cast(gdb.lookup_type("uint32").pointer())
			else:
				data = self.InlineData.cast(gdb.lookup_type("uint32").pointer())

			return ('[%d]' % self.Counter, (data[self.Counter/32] >> self.Counter) & 1)

	def __init__(self, val):
		self.Value = val
		self.NumBits = self.Value['NumBits']

	def to_string(self):
		if self.NumBits.is_optimized_out:
			pass
		if self.NumBits == 0:
			return 'empty'
		pass

	def children(self):
		return self._iterator(self.Value)

	def display_hint(self):
		return 'array'
# ------------------------------------------------------------------------------
# TIndirectArray
#


# ------------------------------------------------------------------------------
# TChunkedArray
#
class TChunkedArrayPrinter:
	"Print TChunkedArray"

	class _iterator(Iterator):
		def __init__(self, val, typename):
			self.Value = val
			self.Typename = typename
			self.Counter = -1
			self.ElementType = self.Value.type.template_argument(0)
			self.ElementTypeSize = self.ElementType.sizeof

			try:
				self.NumElements = self.Value['NumElements']
				if self.NumElements.is_optimized_out:
					self.NumElements = 0
				else:
					self.Chunks = self.Value['Chunks']
					self.Array = self.Chunks['Array']
					self.ArrayNum = self.Array['ArrayNum']
					self.AllocatorInstance = self.Array['AllocatorInstance']
					self.AllocatorData = self.AllocatorInstance['Data']
			except:
				raise

		def __iter__(self):
			return self

		def __next__(self):
			return self.next()

		def __next__(self):
			if self.NumElements == 0:
				raise StopIteration

			self.Counter = self.Counter + 1

			if self.Counter >= self.NumElements:
				raise StopIteration()

			Expr = '(unsigned)sizeof('+str(self.Typename)+'::FChunk)/'+str(self.ElementTypeSize)
			self.ChunkBytes = gdb.parse_and_eval(Expr)
			assert self.ChunkBytes != 0

			Expr = '*(*((('+str(self.ElementType.name)+'**)'+str(self.AllocatorData)+')+'+str(self.Counter / self.ChunkBytes)+')+'+str(self.Counter % self.ChunkBytes)+')'
			Val = gdb.parse_and_eval(Expr)
			return ('[%d]' % self.Counter, Val)

	def __init__(self, val):
		self.Value = val
		self.Typename = typename
		self.NumElements = self.Value['NumElements']

	def to_string(self):
		if self.NumElements.is_optimized_out:
			pass
		if self.NumElements == 0:
			return 'empty'
		pass

	def children(self):
		return self._iterator(self.Value, self.Typename)

	def display_hint(self):
		return 'array'

# ------------------------------------------------------------------------------
# TSparseArray
#
class TSparseArrayPrinter:
	"Print TSparseArray"

	class _iterator(Iterator):
		def __init__(self, val, typename):

			self.Value = val
			self.Counter = -1
			self.Typename = typename
			self.ElementType = self.Value.type.template_argument(0)

			try:
				self.NumFreeIndices = self.Value['NumFreeIndices']
				self.Data = self.Value['Data']
				self.InternalElementType = self.Data.type.template_argument(0)
				self.ArrayNum = self.Data['ArrayNum']
				if self.ArrayNum.is_optimized_out:
					self.ArrayNum = 0
				else:
					self.AllocatorInstance = self.Data['AllocatorInstance']
					self.AllocatorData = self.AllocatorInstance['Data']
					self.AllocationFlags = self.Value['AllocationFlags']
					self.AllocationFlagsInstance = self.AllocationFlags['AllocatorInstance']
					self.InlineData = self.AllocationFlagsInstance['InlineData']
					self.SecondaryData = self.AllocationFlagsInstance['SecondaryData']
					self.SecondaryDataData = self.SecondaryData['Data']
			except:
				raise

		def __iter__(self):
			return self

		def __next__(self):
			return self.next()

		def __next__(self):
			if self.ArrayNum == 0:
				raise StopIteration

			self.Counter = self.Counter + 1

			if self.Counter >= self.ArrayNum:
				raise StopIteration
			else:
				Data = None
				if self.SecondaryDataData > 0:
					Data = (self.SecondaryDataData.address.cast(gdb.lookup_type("int").pointer())[self.Counter/32] >> self.Counter) & 1
				else:
					Data = (self.InlineData.address.cast(gdb.lookup_type("int").pointer())[self.Counter/32] >> self.Counter) & 1

				if Data != 0:
					ElementOrFreeListValue = (self.AllocatorData.cast(self.InternalElementType.pointer()) + self.Counter).dereference()
					Value = ElementOrFreeListValue['ElementData'].reinterpret_cast(self.ElementType.reference())
					return ('[%d]' % self.Counter, Value)
				else:
					return self.__next__()

	def __init__(self, val):
		self.Value = val
		self.Typename = typename
		self.ArrayNum = self.Value['Data']['ArrayNum']

	def to_string(self):
		if self.ArrayNum.is_optimized_out:
			pass
		if self.ArrayNum == 0:
			return 'empty'
		pass

	def children(self):
		return self._iterator(self.Value, self.Typename)

	def display_hint(self):
		return 'string'

# ------------------------------------------------------------------------------
# TSet
#
class TSetPrinter:
	"Print TSet"

	class _iterator(Iterator):
		def __init__(self, val, typename):
			self.Value = val
			self.Counter = -1
			self.typename = typename
			self.ElementType = self.Value.type.template_argument(0)

			try:
				self.Elements = self.Value["Elements"]
				self.ElementsData = self.Elements["Data"]
				self.ElementsArrayNum = self.ElementsData['ArrayNum']
				self.NumFreeIndices = self.Elements['NumFreeIndices']

				self.AllocatorInstance = self.ElementsData['AllocatorInstance']
				self.AllocatorInstanceData = self.AllocatorInstance['Data']

				self.AllocationFlags = self.Elements['AllocationFlags']
				self.AllocationFlagsInstance = self.AllocationFlags['AllocatorInstance']
				self.InlineData = self.AllocationFlagsInstance['InlineData']
				self.SecondaryData = self.AllocationFlagsInstance['SecondaryData']
				self.SecondaryDataData = self.SecondaryData['Data']
			except:
				self.ElementsArrayNum = 0

			Expr = '(size_t)sizeof(FSetElementId) + sizeof(int32)'
			TSetElement = gdb.parse_and_eval(Expr)
			self.ElementTypeSize = self.ElementType.sizeof + TSetElement

		def __iter__(self):
			return self

		def __next__(self):
			self.Counter = self.Counter + 1

			if self.Counter >= self.ElementsArrayNum:
				raise StopIteration()
			else:
				Data = None
				if self.SecondaryDataData > 0:
					Data = (self.SecondaryDataData.address.cast(gdb.lookup_type("int").pointer())[self.Counter/32] >> self.Counter) & 1
				else:
					Data = (self.InlineData.address.cast(gdb.lookup_type("int").pointer())[self.Counter/32] >> self.Counter) & 1

				Value = None
				if Data != 0:
					offset = self.Counter * self.ElementTypeSize
					Value = (self.AllocatorInstanceData + offset).cast(self.ElementType.pointer())
				else:
					Value = None

			return ('[%s]' % self.Counter, Value.dereference())

	def __init__(self, val):
		self.Value = val
		self.typename = typename
		self.ArrayNum = self.Value["Elements"]["Data"]['ArrayNum']

	def to_string(self):
		if self.ArrayNum.is_optimized_out:
			pass
		if self.ArrayNum == 0:
			return 'empty'
		pass

	def children(self):
		return self._iterator(self.Value, self.typename)

	def display_hint(self):
		return 'array'

# ------------------------------------------------------------------------------
# TSetElementPrinter
#

class TSetElementPrinter:
	"Print TSetElement"

	def __init__(self, val):
		self.Value = val

	def to_string(self):
		if self.Value.is_optimized_out:
			return '<optimized out>'

		return self.Value["Value"]

	def display_hint(self):
		return "string"

# ------------------------------------------------------------------------------
# TMap
#
class TMapPrinter:
	"Print TMap"

	class _iterator(Iterator):
		def __init__(self, val):
			self.Value = val
			self.Counter = -1
			try:
				self.Pairs = self.Value['Pairs']
				if self.Pairs.is_optimized_out:
					self.ArrayNum = 0
				else:
					self.Elements = self.Pairs['Elements']
					self.ElementsData = self.Elements['Data']
					self.ArrayNum = self.ElementsData['ArrayNum']
			except:
				raise

		def __iter__(self):
			return self

		def __next__(self):
			if self.ArrayNum == 0:
				raise StopIteration

			self.Counter = self.Counter + 1

			if self.Counter > 0:
				raise StopIteration

			return ('Pairs', self.Pairs)

	def __init__(self, val):
		self.Value = val
		self.ArrayNum = self.Value['Pairs']['Elements']['Data']['ArrayNum']

	def children(self):
		return self._iterator(self.Value)

	def to_string(self):
		if self.ArrayNum.is_optimized_out:
			pass
		if self.ArrayNum == 0:
			return 'empty'
		pass

	def display_hint(self):
		return 'map'

# ------------------------------------------------------------------------------
# TWeakObjectPtr
#

class TWeakObjectPtrPrinter:
	"Print TWeakObjectPtr"

	class _iterator(Iterator):
		def __init__(self, val):
			self.Value = val
			self.Counter = 0
			self.Object = None

			self.ObjectSerialNumber = int(self.Value['ObjectSerialNumber'])
			if self.ObjectSerialNumber >= 1:
				ObjectIndexValue = int(self.Value['ObjectIndex'])
				ObjectItemExpr = 'GCoreObjectArrayForDebugVisualizers->Objects['+str(ObjectIndexValue)+'/FChunkedFixedUObjectArray::NumElementsPerChunk]['+str(ObjectIndexValue)+ '% FChunkedFixedUObjectArray::NumElementsPerChunk]'
				ObjectItem = gdb.parse_and_eval(ObjectItemExpr);
				IsValidObject = int(ObjectItem['SerialNumber']) == self.ObjectSerialNumber
				if IsValidObject == True:
					ObjectType = self.Value.type.template_argument(0)
					self.Object = ObjectItem['Object'].dereference().cast(ObjectType.reference())

		def __iter__(self):
			return self

		def __next__(self):
			if self.Counter > 0:
				raise StopIteration

			self.Counter = self.Counter + 1

			if self.Object != None:
				return ('Object', self.Object)
			elif self.ObjectSerialNumber > 0:
				return ('Object', 'STALE')
			else:
				return ('Object', 'nullptr')


	def __init__(self, val):
		self.Value = val

	def children(self):
		return self._iterator(self.Value)

	def to_string(self):
		ObjectType = self.Value.type.template_argument(0)
		return 'TWeakObjectPtr<%s>' % ObjectType.name;


# ------------------------------------------------------------------------------
# FString
#
class FStringPrinter:
	"Print FString"

	def __init__(self, val):
		self.Value = val

	def to_string(self):
		if self.Value.is_optimized_out:
			return '<optimized out>'

		ArrayNum = self.Value['Data']['ArrayNum']
		if ArrayNum == 0:
			return 'empty'
		elif ArrayNum < 0:
			return "nullptr"
		else:
			ActualData = self.Value['Data']['AllocatorInstance']['Data']
			data = ActualData.cast(gdb.lookup_type("TCHAR").pointer())
			return '%s' % (data.string())

	def display_hint (self):
		return 'string'

# ------------------------------------------------------------------------------
# FName shared 
#
def lookup_fname_entry(id):
	expr = '((FNameEntry&)GNameBlocksDebug[%d >> FNameDebugVisualizer::OffsetBits][FNameDebugVisualizer::EntryStride * (%d & FNameDebugVisualizer::OffsetMask)])' % (id, id)
	return gdb.parse_and_eval(expr)

def get_fname_entry_string(entry):
	header = entry['Header']
	len = int(header['Len'].cast(gdb.lookup_type('uint16')))
	is_wide = header['bIsWide'].cast(gdb.lookup_type('bool'))
	if is_wide:
		wide_string = entry['WideName'].cast(gdb.lookup_type('WIDECHAR').pointer())
		return str(wide_string.string('','',len))
	else:
		ansi_string = entry['AnsiName'].cast(gdb.lookup_type('ANSICHAR').pointer())
		return str(ansi_string.string('','',len))

def get_fname_string(entry, number):
	if number == 0:
		return "'%s'" % get_fname_entry_string(entry)
	else:
		return "'%s'_%u" % (get_fname_entry_string(entry), number - 1)

# ------------------------------------------------------------------------------
# FNameEntry
#

class FNameEntryPrinter:
	"Print FNameEntry"

	def __init__(self, val):
		self.Value = val

	def to_string(self):
		header = self.Value['Header']
		len = int(header['Len'].cast(gdb.lookup_type('uint16')))
		if len == 0:
			base_entry  = lookup_fname_entry(int(self.Value['NumberedName']['Id']['Value']))
			number = self.Value['NumberedName']['Number']
			return get_fname_string(base_entry, number)
		else:
			return get_fname_string(self.Value, 0)

# ------------------------------------------------------------------------------
# FNameEntryId
#

class FNameEntryIdPrinter:
	"Print FNameEntryId"

	def __init__(self, val):
		self.Value = val

	def to_string(self):
		if self.Value.is_optimized_out:
			return '<optimized out>'
		id = int(self.Value['Value'])
		unused_mask = gdb.parse_and_eval('FNameDebugVisualizer::UnusedMask')
		if (id & unused_mask) != 0:
			return 'invalid'
		return lookup_fname_entry(id)


# ------------------------------------------------------------------------------
# FName
#
class FNamePrinter:
	"Print FName"

	def __init__(self, id, number):
		self.Id = id
		self.Number = number

	def to_string(self):
		return get_fname_string(lookup_fname_entry(self.Id), self.Number)


def make_fname_printer(val):
	if val.is_optimized_out:
		return '<optimized out>'
	id = int(val['ComparisonIndex']['Value'])
	unused_mask = gdb.parse_and_eval('FNameDebugVisualizer::UnusedMask')
	if (id & unused_mask) != 0:
		return 'invalid'
	
	# We need to pick the right printer based on whether FName has a Number member or not
	if gdb.types.has_field(val.type, "Number"):
		return FNamePrinter(id, int(val['Number']))
	else:
		# look up the id and use the FNameEntry printer
		return FNameEntryPrinter(lookup_fname_entry(id))
	

# ------------------------------------------------------------------------------
# FMinimalName
#
def make_fminimalname_printer(val):
	if val.is_optimized_out:
		return '<optimized out>'
	id = int(val['Index']['Value'])
	unused_mask = gdb.parse_and_eval('FNameDebugVisualizer::UnusedMask')
	if (id & unused_mask) != 0:
		return 'invalid'
	
	# We need to pick the right printer based on whether FName has a Number member or not
	if gdb.types.has_field(val.type, "Number"):
		return FNamePrinter(id, int(val['Number']))
	else:
		# look up the id and use the FNameEntry printer
		return FNameEntryIdPrinter(val['Index'])

# ------------------------------------------------------------------------------
# TTuple
#
class TTuplePrinter:
	"Print TTuple"

	def __init__(self, val):
		self.Value = val

		try:
			self.TKey = self.Value["Key"];
			self.TValue = self.Value["Value"];
		except:
			pass

	def to_string(self):
		return '(%s, %s)' % (self.TKey, self.TValue)


# ------------------------------------------------------------------------------
# TArray
#
class TArrayPrinter:
	"Print TArray"

	class _iterator(Iterator):
		def __init__(self, val):
			self.Value = val
			self.Counter = -1
			self.TType = self.Value.type.template_argument(0)

			try:
				self.ArrayNum = self.Value['ArrayNum']
				if self.ArrayNum.is_optimized_out:
					self.ArrayNum = 0

				if self.ArrayNum > 0:
					self.AllocatorInstance = self.Value['AllocatorInstance']
					self.AllocatorInstanceData = self.AllocatorInstance['Data']
					try:
						self.InlineData = self.AllocatorInstance['InlineData']
						self.SecondaryData = self.AllocatorInstance['SecondaryData']
						if self.SecondaryData != None:
							self.SecondaryDataData = self.SecondaryData['Data']
						else:
							self.SecondaryDataData = None
					except:
						pass
			except:
				raise

		def __iter__(self):
			return self

		def __next__(self):
			if self.ArrayNum == 0:
				raise StopIteration

			self.Counter = self.Counter + 1

			if self.Counter >= self.ArrayNum:
				raise StopIteration

			try:
				if self.AllocatorInstanceData != None:
					data = self.AllocatorInstanceData.cast(self.TType.pointer())
				elif self.SecondaryDataDataVal > 0:
					data = self.SecondaryDataData.cast(self.TType.pointer())
				else:
					data = self.InlineData.cast(self.TType.pointer())
			except:
				return ('[%d]' % self.Counter, "optmized")

			return ('[%d]' % self.Counter, data[self.Counter])

	def __init__(self, val):
		self.Value = val;
		self.ArrayNum = self.Value['ArrayNum']

	def to_string(self):
		if self.ArrayNum.is_optimized_out:
			pass
		if self.ArrayNum == 0:
			return 'empty'
		pass

	def children(self):
		return self._iterator(self.Value)

	def display_hint(self):
		return 'array'

#
# Register our lookup function. If no objfile is passed use all globally.
def register_ue_printers(objfile):
	if objfile == None:
		objfile = gdb.current_objfile()
	gdb.printing.register_pretty_printer(objfile, build_ue_pretty_printer(), True)
	print("Registered pretty printers for UE classes")

def build_ue_pretty_printer():
	# add a random numeric suffix to the printer name so we can reload printers during the same session for iteration
	pp = gdb.printing.RegexpCollectionPrettyPrinter("UnrealEngine")
	pp.add_printer("FString", '^FString$', FStringPrinter)
	pp.add_printer("FNameEntry", '^FNameEntry$', FNameEntryPrinter)
	pp.add_printer("FNameEntryId", '^FNameEntryId$', FNameEntryIdPrinter)
	pp.add_printer("FName", '^FName$', make_fname_printer)
	pp.add_printer("FMinimalName", '^FMinimalName$', make_fminimalname_printer)
	pp.add_printer("TArray", '^TArray<.+,.+>$', TArrayPrinter)
	pp.add_printer("TBitArray", '^TBitArray<.+>$', TBitArrayPrinter)
	pp.add_printer("TChunkedArray", '^TChunkedArray<.+>$', TChunkedArrayPrinter)
	pp.add_printer("TSparseArray", '^TSparseArray<.+>$', TSparseArrayPrinter)
	pp.add_printer("TSetElement", '^TSetElement<.+>$', TSetElementPrinter)
#	pp.add_printer("TSet", '^TSet<.+>$', TSetPrinter)
	pp.add_printer("FBitReference", '^FBitReference$', FBitReferencePrinter)
#	pp.add_printer("TMap", '^TMap<.+,.+,.+>$', TMapPrinter)
	pp.add_printer("TPair", '^TPair<.+,.+>$', TTuplePrinter)
	pp.add_printer("TTuple", '^TTuple<.+,.+>$', TTuplePrinter)
	pp.add_printer("TWeakObjectPtr", '^TWeakObjectPtr<.+>$', TWeakObjectPtrPrinter)
	return pp

register_ue_printers(None)
