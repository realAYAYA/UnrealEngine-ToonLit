#!/usr/bin/env python3
import argparse, itertools, json, os, shutil, sys
from os.path import dirname, exists, isdir, join


class EngineCopier:
	'''
	Copies data between Unreal Engine installations
	'''
	
	def __init__(self, sourceEngine, destEngine):
		'''
		Creates a new copier for the specified source Engine and destination Engine trees
		'''
		self._sourceEngine = sourceEngine
		self._destEngine = destEngine
	
	def resolveSource(self, path):
		'''
		Resolves the specified path with respect to the source Engine tree
		'''
		return path.replace('$(EngineDir)', self._sourceEngine)
	
	def resolveDest(self, path):
		'''
		Resolves the specified path with respect to the destination Engine tree
		'''
		return path.replace('$(EngineDir)', self._destEngine)
	
	def copy(self, path):
		'''
		Copies the specified file or directory from the source Engine tree to the destination Engine tree
		'''
		
		# Resolve the specified path with respect to our source and destination Engine trees
		source = self.resolveSource(path)
		dest = self.resolveDest(path)
		
		# Create the parent directory of the destination if it doesn't already exist
		parent = dirname(dest)
		os.makedirs(parent, exist_ok=True)
		
		# Log the copy operation
		print('{} -> {}'.format(source, dest), file=sys.stderr, flush=True)
		
		# Copy the file or directory
		if isdir(source):
			shutil.copytree(source, dest)
		else:
			shutil.copy2(source, dest)


class Utility:
	'''
	Provides utility functionality
	'''
	
	@staticmethod
	def parseJson(path):
		'''
		Parses the specified JSON file
		'''
		with open(path, 'rb') as f:
			return json.load(f)


# Parse our command-line arguments
parser = argparse.ArgumentParser()
parser.add_argument('EngineDir')
parser.add_argument('OutputDir')
parser.add_argument('Target')
parser.add_argument('Platform')
args = parser.parse_args()

# Create an EngineCopier to copy files from the source Unreal Engine to our output directory
copier = EngineCopier(args.EngineDir, args.OutputDir)

# Parse the JSON target file for the specified target
target = Utility.parseJson(join(args.EngineDir, 'Binaries', args.Platform, '{}.target'.format(args.Target)))

# Gather each of the build products and runtime dependencies for the target, excluding debug symbols
dependencies = itertools.chain.from_iterable([target['BuildProducts'] + target['RuntimeDependencies']])
dependencies = [d for d in dependencies if d['Type'] != 'SymbolFile']
for dependency in dependencies:
	if '$(EngineDir)/Content/Slate' not in dependency['Path']:
		copier.copy(dependency['Path'])

# If the target is a program then copy any configuration files
# (This is particularly important for UnrealMultiUserServer since it uses `DefaultEngine.ini` to enable the plugins it needs)
configDir = '$(EngineDir)/Programs/{}/Config'.format(args.Target)
if target['TargetType'] == 'Program' and exists(copier.resolveSource(configDir)):
	copier.copy(configDir)

# Copy the data files needed by ICU, which are required by multiple targets
copier.copy('$(EngineDir)/Content/Internationalization')
