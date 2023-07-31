# Copyright Epic Games, Inc. All Rights Reserved.

# /*=============================================================================
#	LLDB Data Formatters for Unreal Types
# =============================================================================*/

import lldb
import lldb.formatters.Logger

# Uncomment the line below to have the data formatters emit debug logging
# lldb.formatters.Logger._lldb_formatters_debug_level=1

# What documentation there is for parsing values in LLDB can be found here:
# https://lldb.llvm.org/python_reference/index.html
# https://lldb.llvm.org/python_reference/lldb.SBValue-class.html

# To install:
# 1) Open Terminal and run:
#        touch ~/.lldbinit
#        open ~/.lldbinit
# 2) Add the following text to .lldbini and save - modifying the path as appropriate:
#        settings set target.inline-breakpoint-strategy always
#        command script import "/Path/To/Epic/UE/Engine/Extras/LLDBDataFormatters/UEDataFormatters.py"
# 3) Restart Xcode

def UETCharSummaryProvider(valobj,dict):
    Data = valobj.GetValue()
    Val = valobj.GetSummary()
    Type = valobj.GetType().GetUnqualifiedType()
    if Type.IsPointerType():
        DataVal = valobj.GetValueAsUnsigned(0)
        if DataVal == 0:
            Val = 'NULL'
        else:
            Expr = '(wchar_t*)('+str(Data)+')'
            ValRef = valobj.CreateValueFromExpression('string', Expr)
            Val = str(ValRef.GetSummary())
    elif Type.IsReferenceType():
        Expr = '(wchar_t&)('+str(Data)+')'
        ValRef = valobj.CreateValueFromExpression('string', Expr)
        Val = str(ValRef.GetSummary())
    return Val

def UEFStringSummaryProvider(valobj,dict):
    Data = valobj.GetChildMemberWithName('Data')
    ArrayNumVal = Data.GetNumChildren()
    if ArrayNumVal < 0:
       return 'string=Invalid'
    elif ArrayNumVal == 0:
       return 'string=Empty'
    else:
        AllocatorInstance = Data.GetChildMemberWithName('AllocatorInstance')
        ActualData = AllocatorInstance.GetChildMemberWithName('Data')
        Expr = '(wchar_t*)('+str(ActualData.GetValue())+')'
        ValRef = valobj.CreateValueFromExpression('string', Expr)
        Val = str(ValRef.GetSummary())
        return 'string=' + Val

def UEFNameIndexToEntry(EntryId):
    Index = EntryId.GetChildMemberWithName('Value').GetValueAsUnsigned(0)
    # FNameDebugVisualizer::OffsetBits = 16
    # FNameDebugVisualizer::OffsetMask = (1 << OffsetBits) - 1 = 65535
    NameEntryExpr = '(FNameEntry*)(GNameBlocksDebug['+str(Index)+' >> 16]+((alignof(FNameEntry) * ('+str(Index)+' & 65535))))'
    NameEntry = EntryId.CreateValueFromExpression('NameEntry', NameEntryExpr)
    return NameEntry

def UEFNameEntrySummaryProvider(valobj,dict):
    Header = valobj.GetChildMemberWithName('Header')
    Len = min(Header.GetChildMemberWithName('Len').GetValueAsUnsigned(0), 1023)
    if Len == 0:
        UnderlyingId = valobj.GetValueForExpressionPath('.NumberedName.Id')
        Number = valobj.GetValueForExpressionPath('.NumberedName.Number').GetValueAsUnsigned(0)
        BaseNameEntry = UEFNameIndexToEntry(UnderlyingId)
        BaseName = UEFNameEntrySummaryProvider(BaseNameEntry, dict)
        return '%s_%s' % (BaseName, Number-1) # BaseName will already include name= from recursion 
    else:
        IsWide = Header.GetChildMemberWithName('bIsWide').GetValueAsUnsigned(0)
        if IsWide:
            DataPtr = valobj.GetValueForExpressionPath(".WideName").AddressOf().GetValueAsUnsigned(0)
            SizeOfTChar = valobj.CreateValueFromExpression('size', 'sizeof(TCHAR))').GetValueAsUnsigned(0)
            Encoding = "utf-16" if SizeOfTChar == 2 else "utf-32"
            NumBytes = Len * SizeOfTChar
            Data = valobj.process.ReadMemory(DataPtr,NumBytes,lldb.SBError())
            Name = Data.decode(Encoding).encode("utf-8")
        else:
            DataPtr = valobj.GetValueForExpressionPath(".AnsiName").AddressOf().GetValueAsUnsigned(0)
            Name = valobj.process.ReadMemory(DataPtr,Len,lldb.SBError())
    return 'name=%s' % Name

def UEFNameSummaryProvider(valobj,dict):
    EntryId = valobj.GetChildMemberWithName('DisplayIndex')
    if not EntryId.IsValid():
        EntryId = valobj.GetChildMemberWithName('ComparisonIndex')
    if not EntryId.IsValid():
        EntryId = valobj.GetChildMemberWithName('Index')
    NameEntry = UEFNameIndexToEntry(EntryId)
    Number = valobj.GetChildMemberWithName('Number')
    if not Number.IsValid():
        return UEFNameEntrySummaryProvider(NameEntry, dict)
    else:
        NameStr = UEFNameEntrySummaryProvider(NameEntry, dict)
        if Number.GetValueAsUnsigned(0) != 0:
            return "'%s'_%d" % (NameStr, Number-1)
        else:
            return "'%s'" % NameStr

def UEUObjectBaseSummaryProvider(valobj,dict):
    Name = valobj.GetChildMemberWithName('NamePrivate')
    return Name.GetSummary()

def UEFFieldClassSummaryProvider(valobj,dict):
    Name = valobj.GetChildMemberWithName('Name')
    return Name.GetSummary()

def UEFFieldSummaryProvider(valobj,dict):
    Name = valobj.GetChildMemberWithName('NamePrivate')
    return Name.GetSummary()

class UETWeakObjectPtrSynthProvider:

    def __init__(self, valobj, dict):
        logger = lldb.formatters.Logger.Logger()
        self.valobj = valobj

    def num_children(self):
        logger = lldb.formatters.Logger.Logger()
        return 1

    def get_child_index(self,name):
        logger = lldb.formatters.Logger.Logger()
        return 0

    def get_child_at_index(self,index):
        logger = lldb.formatters.Logger.Logger()
        logger >> "Retrieving child " + str(index)
        if self.ObjectSerialNumberVal >= 1:
            Expr = 'GObjectArrayForDebugVisualizers->Objects['+str(self.ObjectIndexVal/65536)+']['+str(self.ObjectIndexVal%65536)+'].SerialNumber == '+str(self.ObjectSerialNumberVal)
            Val = self.valobj.CreateValueFromExpression(str(self.ObjectIndexVal), Expr)
            Value = Val.GetValueAsUnsigned(0)
            if Value != 0:
                Expr = 'GObjectArrayForDebugVisualizers->Objects['+str(self.ObjectIndexVal/65536)+']['+str(self.ObjectIndexVal%65536)+'].Object'
                return self.valobj.CreateValueFromExpression('Object', Expr)
            else:
                Expr = '(void*)0xDEADBEEF'
                return self.valobj.CreateValueFromExpression('Object', Expr)

        Expr = 'nullptr'
        return self.valobj.CreateValueFromExpression('Object', Expr)

    def update(self):
        logger = lldb.formatters.Logger.Logger()
        try:
            self.ObjectSerialNumber = self.valobj.GetChildMemberWithName('ObjectSerialNumber')
            self.ObjectSerialNumberVal = self.ObjectSerialNumber.GetValueAsSigned(0)
            self.ObjectIndex = self.valobj.GetChildMemberWithName('ObjectIndex')
            self.ObjectIndexVal = self.ObjectIndex.GetValueAsSigned(0)
        except:
            pass

    def has_children(self):
            return True

def UEFWeakObjectPtrSummaryProvider(valobj,dict):
    ObjectSerialNumber = valobj.GetChildMemberWithName('ObjectSerialNumber')
    ObjectSerialNumberVal = ObjectSerialNumber.GetValueAsSigned(0)
    if ObjectSerialNumberVal < 1:
        return 'object=nullptr'
    ObjectIndex = valobj.GetChildMemberWithName('ObjectIndex')
    ObjectIndexVal = ObjectIndex.GetValueAsSigned(0)
    Expr = 'GObjectArrayForDebugVisualizers->Objects['+str(ObjectIndexVal/65536)+']['+str(ObjectIndexVal%65536)+'].SerialNumber == '+str(ObjectSerialNumberVal)
    Val = valobj.CreateValueFromExpression(str(ObjectIndexVal), Expr)
    ValRef = Val.GetValueAsUnsigned(0)
    if ValRef == 0:
        return 'object=STALE'
    else:
        Expr = 'GObjectArrayForDebugVisualizers->Objects['+str(ObjectIndexVal/65536)+']['+str(ObjectIndexVal%65536)+'].Object'
        Val = valobj.CreateValueFromExpression(str(ObjectIndexVal), Expr)
        return 'object=' + Val.GetValue()

class UEChunkedArraySynthProvider:

    def __init__(self, valobj, dict):
        logger = lldb.formatters.Logger.Logger()
        self.valobj = valobj

    def num_children(self):
        logger = lldb.formatters.Logger.Logger()
        try:
            NumElementsVal = self.NumElements.GetValueAsSigned(0)
            return NumElementsVal;
        except:
            return 0;

    def get_child_index(self,name):
        logger = lldb.formatters.Logger.Logger()
        try:
            return int(name.lstrip('[').rstrip(']'))
        except:
            return None

    def get_child_at_index(self,index):
        logger = lldb.formatters.Logger.Logger()
        logger >> "Retrieving child " + str(index)
        Expr = '(unsigned)sizeof('+str(self.valobj.GetType().GetUnqualifiedType().GetName())+'::FChunk)/'+str(self.ElementTypeSize)
        self.ChunkBytes = self.valobj.CreateValueFromExpression('['+str(index)+']', Expr)
        self.ChunkBytesSize = self.ChunkBytes.GetValueAsUnsigned(0)
        assert self.ChunkBytesSize != 0

        Expr = '*(*((('+str(self.ElementType.GetName())+'**)'+str(self.AllocatorData.GetValue())+')+'+str(index / self.ChunkBytesSize)+')+'+str(index % self.ChunkBytesSize)+')'
        return self.valobj.CreateValueFromExpression('['+str(index)+']', Expr)

    def extract_type(self):
        logger = lldb.formatters.Logger.Logger()
        ArrayType = self.valobj.GetType().GetUnqualifiedType()
        if ArrayType.IsReferenceType():
            ArrayType = ArrayType.GetDereferencedType()
        elif ArrayType.IsPointerType():
            ArrayType = ArrayType.GetPointeeType()
        if ArrayType.GetNumberOfTemplateArguments() > 0:
            ElementType = ArrayType.GetTemplateArgumentType(0)
        else:
            ElementType = None
        return ElementType

    def update(self):
        logger = lldb.formatters.Logger.Logger()
        try:
            self.ElementType = self.extract_type()
            self.ElementTypeSize = self.ElementType.GetByteSize()
            assert self.ElementTypeSize != 0
            self.NumElements = self.valobj.GetChildMemberWithName('NumElements')
            self.Chunks = self.valobj.GetChildMemberWithName('Chunks')
            self.ArrayNum = self.Chunks.GetChildMemberWithName('ArrayNum')
            self.AllocatorInstance = self.Chunks.GetChildMemberWithName('AllocatorInstance')
            self.AllocatorData = self.AllocatorInstance.GetChildMemberWithName('Data')
        except:
            pass

    def has_children(self):
        return True

def UEChunkedArraySummaryProvider(valobj,dict):
    return 'size=' + str(valobj.GetNumChildren())

class UESparseArraySynthProvider:

    def __init__(self, valobj, dict):
        logger = lldb.formatters.Logger.Logger()
        self.valobj = valobj

    def num_children(self):
        logger = lldb.formatters.Logger.Logger()
        try:
            NumBitsVal = self.NumFreeIndices.GetValueAsSigned(0)
            ArrayNumVal = self.Data.GetChildMemberWithName('ArrayNum').GetValueAsSigned(0)
            return ArrayNumVal - NumBitsVal;
        except:
            return 0;

    def get_child_index(self,name):
        logger = lldb.formatters.Logger.Logger()
        try:
            return int(name.lstrip('[').rstrip(']'))
        except:
            return None

    def get_child_at_index(self,index):
        logger = lldb.formatters.Logger.Logger()
        logger >> "Retrieving child " + str(index)
        if index < 0:
            return None;

        if index >= self.num_children():
            return None;

        Val = None
        if self.SecondaryDataDataVal > 0:
            Expr = '(bool)((((int*)'+str(self.SecondaryDataData.GetAddress())+')['+str(index)+'/32] >> '+str(index)+') & 1)'
            Val = self.SecondaryDataData.CreateValueFromExpression('['+str(index)+']', Expr)
        else:
            Expr = '(bool)((((int*)'+str(self.InlineData.GetAddress())+')['+str(index)+'/32] >> '+str(index)+') & 1)'
            Val = self.InlineData.CreateValueFromExpression('['+str(index)+']', Expr)

        if Val.GetValueAsSigned(0) != 0:
            offset = index * self.ElementTypeSize
            return self.AllocatorData.CreateChildAtOffset('['+str(index)+']',offset,self.ElementType)
        else:
            return None

    def extract_type(self):
        logger = lldb.formatters.Logger.Logger()
        ArrayType = self.valobj.GetType().GetUnqualifiedType()
        if ArrayType.IsReferenceType():
            ArrayType = ArrayType.GetDereferencedType()
        elif ArrayType.IsPointerType():
            ArrayType = ArrayType.GetPointeeType()
        if ArrayType.GetNumberOfTemplateArguments() > 0:
            ElementType = ArrayType.GetTemplateArgumentType(0)
        else:
            ElementType = None
        return ElementType

    def update(self):
        logger = lldb.formatters.Logger.Logger()
        try:
            self.ElementType = self.extract_type()
            self.ElementTypeSize = self.ElementType.GetByteSize()
            assert self.ElementTypeSize != 0
            self.NumFreeIndices = self.valobj.GetChildMemberWithName('NumFreeIndices')
            self.Data = self.valobj.GetChildMemberWithName('Data')
            self.AllocatorInstance = self.Data.GetChildMemberWithName('AllocatorInstance')
            self.AllocatorData = self.AllocatorInstance.GetChildMemberWithName('Data')
            self.AllocationFlags = self.valobj.GetChildMemberWithName('AllocationFlags')
            self.InlineData = self.AllocationFlags.GetChildMemberWithName('InlineData')
            self.SecondaryData = self.AllocationFlags.GetChildMemberWithName('SecondaryData')
            self.SecondaryDataData = self.SecondaryData.GetChildMemberWithName('Data')
            self.SecondaryDataDataVal = self.SecondaryDataData.GetValueAsSigned(0)
        except:
            pass

    def has_children(self):
        return True

def UESparseArraySummaryProvider(valobj,dict):
    return 'size=' + str(valobj.GetNumChildren())

class UEBitArraySynthProvider:

    def __init__(self, valobj, dict):
        logger = lldb.formatters.Logger.Logger()
        self.valobj = valobj

    def num_children(self):
        logger = lldb.formatters.Logger.Logger()
        try:
            NumBitsVal = self.NumBits.GetValueAsSigned(0)
            return NumBitsVal;
        except:
            return 0;

    def get_child_index(self,name):
        logger = lldb.formatters.Logger.Logger()
        try:
            return int(name.lstrip('[').rstrip(']'))
        except:
            return None

    def get_child_at_index(self,index):
        logger = lldb.formatters.Logger.Logger()
        logger >> "Retrieving child " + str(index)
        if index < 0:
            return None;

        if index >= self.num_children():
            return None;

        if self.SecondaryDataDataVal > 0:
            Expr = '(bool)((((int*)'+str(self.SecondaryDataData.GetAddress())+')['+str(index)+'/32] >> '+str(index)+') & 1)'
            return self.SecondaryDataData.CreateValueFromExpression('['+str(index)+']', Expr)
        else:
            Expr = '(bool)((((int*)'+str(self.InlineData.GetAddress())+')['+str(index)+'/32] >> '+str(index)+') & 1)'
            return self.InlineData.CreateValueFromExpression('['+str(index)+']', Expr)

        return None

    def update(self):
        logger = lldb.formatters.Logger.Logger()
        try:
            self.SecondaryDataData = None
            self.SecondaryDataDataVal = 0
            self.NumBits = self.valobj.GetChildMemberWithName('NumBits')
            self.MaxBits = self.valobj.GetChildMemberWithName('MaxBits')
            self.InlineData = self.valobj.GetChildMemberWithName('InlineData')
            self.SecondaryData = self.valobj.GetChildMemberWithName('SecondaryData')
            if self.SecondaryData != None:
                self.SecondaryDataData = self.SecondaryData.GetChildMemberWithName('Data')
                self.SecondaryDataDataVal = self.SecondaryDataData.GetValueAsUnsigned(0)
        except:
            pass

    def has_children(self):
        return True

def UEBitArraySummaryProvider(valobj,dict):
    return 'size=' + str(valobj.GetNumChildren())

class UEArraySynthProvider:

    def __init__(self, valobj, dict):
        logger = lldb.formatters.Logger.Logger()
        self.valobj = valobj

    def num_children(self):
        logger = lldb.formatters.Logger.Logger()
        try:
            ArrayNumVal = self.ArrayNum.GetValueAsSigned(0)
            return ArrayNumVal + self.NumChildren;
        except:
            return 0;

    def get_child_index(self,name):
        logger = lldb.formatters.Logger.Logger()
        try:
            return self.NumChildren + int(name.lstrip('[').rstrip(']'))
        except:
            return self.valobj.GetIndexOfChildWithName(name)

    def get_child_at_index(self,index):
        logger = lldb.formatters.Logger.Logger()
        logger >> "Retrieving child " + str(index)
        if index < 0:
            return None;

        if index < self.NumChildren:
            logger >> "Existing child " + str(index)
            return self.valobj.GetChildAtIndex(index)
        else:
            index -= self.NumChildren

        if index >= self.num_children():
            logger >> "Index is greater than num children " + str(self.num_children())
            return None;
        try:
            offset = index * self.ElementTypeSize
            if self.Data != None and self.Data.IsValid():
                logger >> "Retrieving Data child of type " + str(self.ElementType) + " at offset " + str(offset)
                return self.Data.CreateChildAtOffset('['+str(index)+']',offset,self.ElementType)
            elif self.SecondaryDataDataVal > 0:
                logger >> "Retrieving SecondaryData child of type " + str(self.ElementType) + " at offset " + str(offset)
                return self.SecondaryDataData.CreateChildAtOffset('['+str(index)+']',offset,self.ElementType)
            else:
                logger >> "Retrieving InlineData child of type " + str(self.ElementType) + " at offset " + str(offset)
                return self.InlineData.CreateChildAtOffset('['+str(index)+']',offset,self.ElementType)
        except:
            logger >> "Exception retrieving child of type " + str(self.ElementType) + " at index " + str(index)
            return None

    def extract_type(self):
        logger = lldb.formatters.Logger.Logger()
        ArrayType = self.valobj.GetType().GetUnqualifiedType()
        if ArrayType.IsReferenceType():
            ArrayType = ArrayType.GetDereferencedType()
        elif ArrayType.IsPointerType():
            ArrayType = ArrayType.GetPointeeType()

        if ArrayType.GetNumberOfTemplateArguments() > 0:
            ElementType = ArrayType.GetTemplateArgumentType(0)
        else:
            ElementType = None
        return ElementType

    def update(self):
        logger = lldb.formatters.Logger.Logger()
        try:
            self.NumChildren = self.valobj.GetNumChildren()
            self.ArrayNum = self.valobj.GetChildMemberWithName('ArrayNum')
            self.ArrayMax = self.valobj.GetChildMemberWithName('ArrayMax')
            self.AllocatorInstance = self.valobj.GetChildMemberWithName('AllocatorInstance')
            if self.AllocatorInstance.GetType().IsReferenceType():
                self.AllocatorInstance = self.AllocatorInstance.Dereference()
            self.Data = None
            self.Data = self.AllocatorInstance.GetChildMemberWithName('Data')
            self.InlineData = self.AllocatorInstance.GetChildMemberWithName('InlineData')
            self.SecondaryData = self.AllocatorInstance.GetChildMemberWithName('SecondaryData')
            if self.SecondaryData != None:
                self.SecondaryDataData = self.SecondaryData.GetChildMemberWithName('Data')
                self.SecondaryDataDataVal = self.SecondaryDataData.GetValueAsUnsigned(0)
            else:
                self.SecondaryDataData = None
                self.SecondaryDataDataVal = 0
        except:
            logger >> "UEArraySynthProvider::update failed accessing members"
            pass
        try:
            self.ElementType = self.extract_type()
            self.ElementTypeSize = self.ElementType.GetByteSize()
            assert self.ElementTypeSize != 0
        except:
            logger >> "UEArraySynthProvider::update failed accessing element type"
            pass

    def has_children(self):
        return True

def UEArraySummaryProvider(valobj,dict):
    return 'size=' + str(valobj.GetChildMemberWithName('ArrayNum').GetValueAsSigned(0))

class UESetSynthProvider:

    def __init__(self, valobj, dict):
        logger = lldb.formatters.Logger.Logger()
        self.valobj = valobj

    def num_children(self):
        logger = lldb.formatters.Logger.Logger()
        try:
            self.ArrayNumVal = self.ArrayNum.GetValueAsUnsigned(0)
            self.NumFreeIndicesVal = self.NumFreeIndices.GetValueAsUnsigned(0)
            return self.ArrayNumVal - self.NumFreeIndicesVal;
        except:
            return 0;

    def get_child_index(self,name):
        logger = lldb.formatters.Logger.Logger()
        try:
            return int(name.lstrip('[').rstrip(']'))
        except:
            return 0

    def get_child_at_index(self,index):
        logger = lldb.formatters.Logger.Logger()
        logger >> "Retrieving child " + str(index)
        if index < 0:
            return None;

        if index >= self.num_children():
            return None;
        try:
            offset = index * self.ElementTypeSize
            HasObject = 0
            if self.SecondaryDataDataVal > 0:
                Expr = '(bool)((((int*)'+str(self.SecondaryDataDataVal)+')['+str(index)+'/32] >> '+str(index)+') & 1)'
                HasObject = 1 ##self.AllocationFlagsSecondaryDataData.CreateValueFromExpression('['+str(index)+']', Expr).GetValueAsUnsigned(0)
            else:
                Expr = '(bool)((((int*)'+str(self.AllocationFlagsInlineDataAddr)+')['+str(index)+'/32] >> '+str(index)+') & 1)'
                HasObject = 1 ##self.AllocationFlagsInlineData.CreateValueFromExpression('['+str(index)+']', Expr).GetValueAsUnsigned(0)

            if HasObject == 1:
                return self.AllocatorInstanceData.CreateChildAtOffset('['+str(index)+']',offset,self.ElementType)
            else:
                return self.valobj.CreateValueFromExpression('['+str(index)+']', '(void*)0xDEADBEEF')
        except:
            return None

    def extract_type(self):
        logger = lldb.formatters.Logger.Logger()
        ArrayType = self.valobj.GetType().GetUnqualifiedType()
        if ArrayType.IsReferenceType():
            ArrayType = ArrayType.GetDereferencedType()
        elif ArrayType.IsPointerType():
            ArrayType = ArrayType.GetPointeeType()
        if ArrayType.GetNumberOfTemplateArguments() > 0:
            ElementType = ArrayType.GetTemplateArgumentType(0)
        else:
            ElementType = None
        return ElementType

    def update(self):
        logger = lldb.formatters.Logger.Logger()
        try:
            self.Elements = self.valobj.GetChildMemberWithName('Elements')
            self.ElementsData = self.Elements.GetChildMemberWithName('Data')
            self.ArrayNum = self.ElementsData.GetChildMemberWithName('ArrayNum')
            self.NumFreeIndices = self.Elements.GetChildMemberWithName('NumFreeIndices')
            self.AllocationFlags = self.Elements.GetChildMemberWithName('AllocationFlags')
            self.AllocationFlagsAllocatorInstance = self.AllocationFlags.GetChildMemberWithName('AllocatorInstance')
            self.AllocatorInstance = self.ElementsData.GetChildMemberWithName('AllocatorInstance')
            self.AllocatorInstanceData = self.AllocatorInstance.GetChildMemberWithName('Data')
            self.AllocationFlagsInlineData = self.AllocationFlagsAllocatorInstance.GetChildMemberWithName('InlineData')
            self.AllocationFlagsInlineDataAddr = self.AllocationFlagsInlineData.AddressOf().GetValueAsUnsigned(0)
            self.AllocationFlagsSecondaryData = self.AllocationFlagsAllocatorInstance.GetChildMemberWithName('SecondaryData')
            self.AllocationFlagsSecondaryDataData = self.AllocationFlagsSecondaryData.GetChildMemberWithName('Data')
            self.SecondaryDataDataVal = self.AllocationFlagsSecondaryDataData.GetValueAsUnsigned(0)
            self.ElementType = self.extract_type()
            # Can't cast to TSetElement - the template instantiation won't allow it
            Expr = '(size_t)sizeof(FSetElementId) + sizeof(int32)'
            TSetElement = self.valobj.CreateValueFromExpression('TSetElement', Expr)
            # This may fail due to C++ struct padding - will have to check
            self.ElementTypeSize = self.ElementType.GetByteSize() + TSetElement.GetValueAsUnsigned(0)
            assert self.ElementTypeSize != 0
        except:
            pass

    def has_children(self):
        return True

def UESetSummaryProvider(valobj,dict):
    return 'size=' + str(valobj.GetNumChildren())

class UEMapSynthProvider:

    def __init__(self, valobj, dict):
        logger = lldb.formatters.Logger.Logger()
        self.valobj = valobj

    def num_children(self):
        logger = lldb.formatters.Logger.Logger()
        try:
            self.ArrayNumVal = self.ArrayNum.GetValueAsUnsigned(0)
            self.NumFreeIndicesVal = self.NumFreeIndices.GetValueAsUnsigned(0)
            return self.ArrayNumVal - self.NumFreeIndicesVal;
        except:
            return 0;

    def get_child_index(self,name):
        logger = lldb.formatters.Logger.Logger()
        try:
            return int(name.lstrip('[').rstrip(']'))
        except:
            return 0

    def get_child_at_index(self,index):
        logger = lldb.formatters.Logger.Logger()
        logger >> "Retrieving child " + str(index)
        if index < 0:
            return None;

        if index >= self.num_children():
            return None;
        try:
            offset = index * self.ElementTypeSize
            HasObject = 0
            if self.SecondaryDataDataVal != 0:
                Expr = '(bool)((((unsigned int*)'+str(self.SecondaryDataDataVal)+')['+str(index)+'/32] >> '+str(index)+') & 1)'
                HasObject = 1 ##self.AllocationFlagsSecondaryDataData.CreateValueFromExpression('['+str(index)+']', Expr).GetValueAsUnsigned(0)
            else:
                Expr = '(bool)((((unsigned int*)'+str(self.AllocationFlagsInlineDataAddr)+')['+str(index)+'/32] >> '+str(index)+') & 1)'
                HasObject = 1 ##self.AllocationFlagsInlineData.CreateValueFromExpression('['+str(index)+']', Expr).GetValueAsUnsigned(0)

            if HasObject == 1:
                return self.AllocatorInstanceData.CreateChildAtOffset('['+str(index)+']',offset,self.ElementType)
            else:
                return self.valobj.CreateValueFromExpression('['+str(index)+']', '(void*)0xDEADBEEF')
        except:
            return None

    def extract_type(self):
        logger = lldb.formatters.Logger.Logger()
        ArrayType = self.Pairs.GetType().GetUnqualifiedType()
        if ArrayType.IsReferenceType():
            ArrayType = ArrayType.GetDereferencedType()
        elif ArrayType.IsPointerType():
            ArrayType = ArrayType.GetPointeeType()
        if ArrayType.GetNumberOfTemplateArguments() > 0:
            ElementType = ArrayType.GetTemplateArgumentType(0)
        else:
            ElementType = None
        return ElementType

    def update(self):
        logger = lldb.formatters.Logger.Logger()
        try:
            self.Pairs = self.valobj.GetChildMemberWithName('Pairs')
            self.Elements = self.Pairs.GetChildMemberWithName('Elements')
            self.ElementsData = self.Elements.GetChildMemberWithName('Data')
            self.ArrayNum = self.ElementsData.GetChildMemberWithName('ArrayNum')
            self.NumFreeIndices = self.Elements.GetChildMemberWithName('NumFreeIndices')
            self.AllocationFlags = self.Elements.GetChildMemberWithName('AllocationFlags')
            self.AllocationFlagsAllocatorInstance = self.AllocationFlags.GetChildMemberWithName('AllocatorInstance')
            self.AllocatorInstance = self.ElementsData.GetChildMemberWithName('AllocatorInstance')
            self.AllocatorInstanceData = self.AllocatorInstance.GetChildMemberWithName('Data')
            self.AllocationFlagsInlineData = self.AllocationFlagsAllocatorInstance.GetChildMemberWithName('InlineData')
            self.AllocationFlagsInlineDataAddr = self.AllocationFlagsInlineData.AddressOf().GetValueAsUnsigned(0)
            self.AllocationFlagsSecondaryData = self.AllocationFlagsAllocatorInstance.GetChildMemberWithName('SecondaryData')
            self.AllocationFlagsSecondaryDataData = self.AllocationFlagsSecondaryData.GetChildMemberWithName('Data')
            self.SecondaryDataDataVal = self.AllocationFlagsSecondaryDataData.GetValueAsUnsigned(0)
            self.ElementType = self.extract_type()
            # Can't cast to TSetElement - the template instantiation won't allow it
            Expr = '(size_t)sizeof(FSetElementId) + sizeof(int32)'
            TSetElement = self.valobj.CreateValueFromExpression('TSetElement', Expr)
            # This may fail due to C++ struct padding - will have to check
            self.ElementTypeSize = self.ElementType.GetByteSize() + TSetElement.GetValueAsUnsigned(0)
            assert self.ElementTypeSize != 0
        except:
            pass

    def has_children(self):
        return True

def UEMapSummaryProvider(valobj,dict):
    return 'size=' + str(valobj.GetNumChildren())

def __lldb_init_module(debugger,dict):
    debugger.HandleCommand('type summary add -F UEDataFormatters.UETCharSummaryProvider -e TCHAR -w UEDataFormatters')
    debugger.HandleCommand('type summary add -F UEDataFormatters.UEFStringSummaryProvider -e -x "FString$" -w UEDataFormatters')
    debugger.HandleCommand('type summary add -F UEDataFormatters.UEFNameEntrySummaryProvider -e -x "FNameEntry$" -w UEDataFormatters')
    debugger.HandleCommand('type summary add -F UEDataFormatters.UEFNameSummaryProvider -e -x "FName$" -w UEDataFormatters')
    debugger.HandleCommand('type summary add -F UEDataFormatters.UEFNameSummaryProvider -e -x "FMinimalName$" -w UEDataFormatters')
    debugger.HandleCommand('type summary add -F UEDataFormatters.UEUObjectBaseSummaryProvider -e UObject -w UEDataFormatters')
    debugger.HandleCommand('type summary add -F UEDataFormatters.UEUObjectBaseSummaryProvider -e UObjectBase -w UEDataFormatters')
    debugger.HandleCommand('type summary add -F UEDataFormatters.UEUObjectBaseSummaryProvider -e UObjectBaseUtility -w UEDataFormatters')
    debugger.HandleCommand('type summary add -F UEDataFormatters.UEFFieldClassSummaryProvider -e FFieldClass -w UEDataFormatters')
    debugger.HandleCommand('type summary add -F UEDataFormatters.UEFFieldSummaryProvider -e FField -w UEDataFormatters')
    debugger.HandleCommand('type summary add -F UEDataFormatters.UEFWeakObjectPtrSummaryProvider -e FWeakObjectPtr -w UEDataFormatters')
    debugger.HandleCommand('type synthetic add -l UEDataFormatters.UETWeakObjectPtrSynthProvider -x "TWeakObjectPtr<.+>$" -w UEDataFormatters')
    debugger.HandleCommand('type synthetic add -l UEDataFormatters.UETWeakObjectPtrSynthProvider -x "TAutoWeakObjectPtr<.+>$" -w UEDataFormatters')
    debugger.HandleCommand('type synthetic add -l UEDataFormatters.UEArraySynthProvider -x "TArray<.+,.+>$" -w UEDataFormatters')
    debugger.HandleCommand('type summary add -F UEDataFormatters.UEArraySummaryProvider -e -x "TArray<.+>$" -w UEDataFormatters')
    debugger.HandleCommand('type synthetic add -l UEDataFormatters.UEBitArraySynthProvider -x "TBitArray<.+>$" -w UEDataFormatters')
    debugger.HandleCommand('type summary add -F UEDataFormatters.UEBitArraySummaryProvider -e -x "TBitArray<.+>$" -w UEDataFormatters')
    debugger.HandleCommand('type synthetic add -l UEDataFormatters.UESparseArraySynthProvider -x "TSparseArray<.+>$" -w UEDataFormatters')
    debugger.HandleCommand('type summary add -F UEDataFormatters.UESparseArraySummaryProvider -e -x "TSparseArray<.+>$" -w UEDataFormatters')
    debugger.HandleCommand('type synthetic add -l UEDataFormatters.UEChunkedArraySynthProvider -x "TChunkedArray<.+>$" -w UEDataFormatters')
    debugger.HandleCommand('type summary add -F UEDataFormatters.UEChunkedArraySummaryProvider -e -x "TChunkedArray<.+>$" -w UEDataFormatters')
    debugger.HandleCommand('type synthetic add -l UEDataFormatters.UESetSynthProvider -x "TSet<.+>$" -w UEDataFormatters')
    debugger.HandleCommand('type summary add -F UEDataFormatters.UESetSummaryProvider -e -x "TSet<.+>$" -w UEDataFormatters')
    debugger.HandleCommand('type synthetic add -l UEDataFormatters.UEMapSynthProvider -x "TMap<.+>$" -w UEDataFormatters')
    debugger.HandleCommand('type summary add -F UEDataFormatters.UEMapSummaryProvider -e -x "TMap<.+>$" -w UEDataFormatters')
    debugger.HandleCommand('type synthetic add -l UEDataFormatters.UEMapSynthProvider -x "TMapBase<.+>$" -w UEDataFormatters')
    debugger.HandleCommand('type summary add -F UEDataFormatters.UEMapSummaryProvider -e -x "TMapBase<.+>$" -w UEDataFormatters')
    debugger.HandleCommand("type category enable UEDataFormatters")

