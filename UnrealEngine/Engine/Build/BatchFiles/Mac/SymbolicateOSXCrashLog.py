#!/usr/bin/python
# Copyright Epic Games, Inc. All Rights Reserved.
#
# This script will symbolicate a macOS crash report. It currently only works with x86_64 binaries because the regexes are dumb.
# It depends on atos so you must have xcode installed. It will use the currently set xcode to find atos.
# It is sort of stupid right now and does not go searching for symbols, it will only symbolicate based on the symbol file you give it.
# This can be a dSYM or mach-o binary. If you point it to a .app bundle it will use the binary at Contents/MacOS/<bundle_name>
#
# Future:
# Do UUID checking
# Find symbol files based on parsed libraries

import re
import sys
from subprocess import check_output
import os.path

HelpMessage = \
"""	Symbolicates x86_64 crash reports on macOS.
	Usage:
	symbolicate.py CrashFile SymbolFile [-v]
	CrashFile		- Text file from Apple's Crash Reporter
	SymbolsFile		- Executable or dSYM for the binary that crashed
	-v				- Print Debug/log info"""


# Note: HFS (and APFS) allow any unicode character except NUL as a valid path so these are weird.
ProcessRegex = "(?:^Path:)+\s+(.+)"
IdentifierRegex = "(?:^Identifier:)+\s+(.+)"
ThreadInfoHeaderRegex = "Thread (\d+)\s*(Crashed)*:+ *([ \S]*)"
BacktraceLineRegex = "(\d+)\s*(.+)\s\s+(0x[abcdefx\d]+)"
LoadAddressHeaderRegex = "Binary Images:"
LibraryRegex = "(0x[abcdefx\d]+)\s+-\s+(0x[abcdefx\d]+)\s*\+*(.+)\s*x86_64\s+\((.+)\)\s<([\da-f]+)>\s+(.+)"

def FindPathToAtos():
	Path = check_output(['xcrun','which','atos'])
	return Path.rstrip()

class StackFrame(object):
	"""Info about a single stack frame"""
	def __init__(self, InFrameNumber, InLibraryName, InAddress):
		self.FrameNumber = InFrameNumber.strip().rstrip()
		self.LibraryName = InLibraryName.strip().rstrip()
		self.Address = InAddress.strip().rstrip()
	def __str__(self):
		return str(self.FrameNumber) + ' ' + self.LibraryName + ' ' + self.Address

class ThreadInfo(object):
	"""Contains Info about Threads/Backtraces"""
	def __init__(self):
		self.Crashed = False
		self.ThreadNumber = -1
		self.ThreadName = ""
		self.StackTrace = []
	def __str__(self):
		l = []
		l.append('Thread ')
		l.append(str(self.ThreadNumber))
		l.append(' ')
		l.append(self.ThreadName)
		if self.Crashed:
			l.append(' (This thread crashed)')
		l.append('\n')
		for f in self.StackTrace:
			l.append('\t')
			l.append(str(f))
			l.append('\n')
		return ''.join(l)

class LibraryInfo(object):
	def __init__(self, InStartAddress, InEndAddress, InLibraryName, InVersion, InGUID, InPath):
		self.StartAddress = InStartAddress.strip().rstrip()
		self.EndAddress = InEndAddress.strip().rstrip()
		self.Name = InLibraryName.strip().rstrip()
		self.Version = InVersion.strip().rstrip()
		self.GUID = InGUID.strip().rstrip()
		self.Path = InPath.strip().rstrip()
	def __str__(self):
		return self.StartAddress + ' - ' + self.EndAddress + ' ' + self.Name + ' (' + self.Version + ') ' + ' <' + self.GUID + '> ' + self.Path

class Parser(object):
	""" Parsing Logic """
	STATE_PROCESS = 0
	STATE_DEFAULT = 1
	STATE_PARSE_BACKTRACE = 2
	STATE_PARSE_LOAD_ADDRESS = 3

	def __init__(self):
		self.State = Parser.STATE_PROCESS
		self.CrashFileText = ""
		self.ProcessName = ""
		self.Identifier = ""
		self.pid = -1
		self.Threads = []
		self.CurrentThread = None
		self.Libraries = []
		self.LibNameToLibraryInfo = dict()
	
	def DoParseProcess(self, Line):
		Match = re.search(ProcessRegex, Line)
		if Match != None:
			ProcPath = Match.group(1)
			ProcPath = os.path.abspath(ProcPath)
			ProcPath = os.path.normpath(ProcPath)
			self.ProcessName = os.path.basename(ProcPath)
		
		else:
			Match = re.search(IdentifierRegex, Line)
			if Match != None:
				self.Identifier = Match.group(1)
				self.State = Parser.STATE_DEFAULT
		return True

	def ParseLineForThreadInfo(self, Line):
		Match = re.search(ThreadInfoHeaderRegex, Line)
		if Match != None:
			Info = ThreadInfo()
			Info.ThreadNumber = int(Match.group(1))
			Info.ThreadName = Match.group(3)
			if Match.group(2) != None:
				Info.Crashed = True
			return Info
		return None

	def ParseLineForLoadAddressHeader(self, Line):
		return Line.find(LoadAddressHeaderRegex)
	
	def ParseBacktraceLine(self, Line):
		Match = re.search(BacktraceLineRegex, Line)
		if Match != None:
			frame = StackFrame(Match.group(1), Match.group(2), Match.group(3))
			return frame
		return None
	
	def ParseLibrary(self, Line):
		Match = re.search(LibraryRegex, Line)
		lib = None
		if Match != None:
			lib = LibraryInfo(Match.group(1),Match.group(2),Match.group(3),Match.group(4),Match.group(5),Match.group(6))
			
			if lib.Name not in self.LibNameToLibraryInfo:
				self.LibNameToLibraryInfo[lib.Name] = lib
		elif Verbose:
			print "\tDid not match library for line %s" % Line
		return lib
	
	def DoDefaultState(self, Line):
		# In the default state we need to search for stacktraces
		# and library load addresses
		self.CurrentThread = self.ParseLineForThreadInfo(Line)
		if self.CurrentThread != None:
			# Line had thread info so we'll change state to look for lines of the stacktrace
			self.Threads.append(self.CurrentThread)
			self.State = Parser.STATE_PARSE_BACKTRACE
		else:
			# Otherwise we'll look for the load addresses
			Result = self.ParseLineForLoadAddressHeader(Line)
			if Result != -1:
				self.State = Parser.STATE_PARSE_LOAD_ADDRESS
		return True

	def DoParseBacktrace(self, Line):
		Result = P.ParseBacktraceLine(Line)
		if Result != None:
			P.CurrentThread.StackTrace.append(Result)
			return True
		else:
			self.State = Parser.STATE_DEFAULT
			return False

	def DoParseLoadAddress(self, Line):
		Lib = self.ParseLibrary(Line)
		if Lib != None:
			self.Libraries.append(Lib)
			return True
		else:
			self.State = Parser.STATE_DEFAULT
			return False

	def ParseLine(self, Line):
		if self.State == Parser.STATE_PROCESS:
			return self.DoParseProcess(Line)
		elif self.State == Parser.STATE_DEFAULT:
			return self.DoDefaultState(Line)
		elif self.State == Parser.STATE_PARSE_BACKTRACE:
			return self.DoParseBacktrace(Line)
		elif self.State == Parser.STATE_PARSE_LOAD_ADDRESS:
			return self.DoParseLoadAddress(Line)
		return True

	def ParseFile(self, FilePath):
		with open(FilePath) as CrashFile:
			self.CrashFileText = CrashFile.readlines()
		for Line in self.CrashFileText:
			while(self.ParseLine(Line) == False):
				pass

# Main entrypoint
if len(sys.argv) < 2:
	print HelpMessage
	sys.exit(0)

if not os.path.exists(sys.argv[1]):
	print "Crashfile doesn't exist at %s" % (sys.argv[1])
	sys.exit(1)

if not os.path.exists(sys.argv[2]):
	print "Symbol file doesn't exist at %s" % (sys.argv[2])
	sys.exit(2)

if len(sys.argv) == 4:
	if sys.argv[3] == '-v':
		Verbose = True
else:
	Verbose = False

# This represents the libraries we have symbols for.
# Extending this script means putting paths in here.
# TODO: Verify entries with dwarfdump -u. That will give us UUIDs which we can cross reference with the parsed Libraries from the crash report.
LibToSymbolPath = dict()

# Currently we only support a symbol file passed on the command line
PathToSymbolFile = os.path.abspath(os.path.normpath(sys.argv[2]))
SymbolFileBaseName = os.path.basename(PathToSymbolFile)
(SymPath, SymExt) = os.path.splitext(SymbolFileBaseName)

# If you give a dSYM we will use it directly.
# We have to strip the name down to the base name to make the mapping of Library Name -> Symbol File Path
if SymExt == '.dSYM':
	SymbolFileBaseName = SymbolFileBaseName.replace('.dSYM', '')
	(SymPath, SymExt) = os.path.splitext(SymbolFileBaseName)
	if SymExt == '.app':
		SymbolFileBaseName = SymbolFileBaseName.replace('.app', '')
	LibToSymbolPath[SymbolFileBaseName] = PathToSymbolFile

# If it's a .app we have to drill down into the bundle and use that path.
elif SymExt == '.app':
	PathToSymbolFile = os.path.join(PathToSymbolFile, 'Contents', 'MacOS', SymPath)
	LibToSymbolPath[SymPath] = PathToSymbolFile

P = Parser()
P.ParseFile(sys.argv[1])

if Verbose:
	print "Parsed:"
	print P.ProcessName
	print P.Identifier

	for T in P.Threads:
		print T

	for Lib in P.Libraries:
		print Lib

	print "Found: " + str(len(P.Libraries))

AddressToSymbol = dict()
LibNameToAddressSet = dict()

# Build a mapping of Libraries to addresses
for T in P.Threads:
	for BackTrace in T.StackTrace:
		Address = BackTrace.Address
		if BackTrace.LibraryName not in LibNameToAddressSet:
			LibNameToAddressSet[BackTrace.LibraryName] = set()
		LibNameToAddressSet[BackTrace.LibraryName].add(Address)

if Verbose:
	for key in P.LibNameToLibraryInfo:
		print "Key: %s Lib: %s" % (key, P.LibNameToLibraryInfo[key])

# Now find atos
_atosPath = FindPathToAtos()
if Verbose:
	print "Found atos at: %s" % _atosPath

for LibName in LibNameToAddressSet:
	if Verbose:
		print "%s" % LibName
	if LibName in LibToSymbolPath:
		PathToSymbols = LibToSymbolPath[LibName]
		AddressesToWrite = list(LibNameToAddressSet[LibName])

		AddressTempFile = open("/tmp/addrlist.txt", "w")
		for Addr in AddressesToWrite:
			AddressTempFile.write('%s\n' % Addr)
		AddressTempFile.close()
		
		LibLoadAddress = P.LibNameToLibraryInfo[LibName].StartAddress

		args = [_atosPath, '-o', PathToSymbols, '-l', LibLoadAddress, '-f', '/tmp/addrlist.txt']
		if Verbose:
			print "Run ATOS %s" % ' '.join(args)
		atosOutput = check_output(args)

		if Verbose:
			print "ATOS returned:"
			print atosOutput

		lines = atosOutput.split('\n')

		for i in xrange(0, len(AddressesToWrite)):
			AddressToSymbol[AddressesToWrite[i]] = lines[i]
	elif Verbose:
		print "Skipping Image %s (does not match %s or %s)" % (LibName, P.Identifier, P.ProcessName)

for Line in P.CrashFileText:
	sf = P.ParseBacktraceLine(Line)
	if sf != None and sf.Address in AddressToSymbol:
			print "%s - %s - %s - %s" % (sf.FrameNumber, sf.LibraryName, sf.Address, AddressToSymbol[sf.Address])
	else:
		print Line.rstrip()
