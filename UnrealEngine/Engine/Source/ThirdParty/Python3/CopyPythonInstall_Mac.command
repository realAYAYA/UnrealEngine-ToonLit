#!/bin/sh

# --------------------------------------------------------------------------------
# Introduction
# --------------------------------------------------------------------------------
# To distribute Python with UE5 on Mac, it must built locally and the produced binaries install names must
# be updated to support relocation (so that the libraries are found whereever the user installs UE. We also
# need to build universal binaries (Intel x64 and ARM M1) and support various version of MacOS. That scripts
# is meant to help for that. Note that the binaries you downloaod from Python.org are not relocatable and the
# the binaries are signed, so we cannot fix that with install_name_tool because it breaks the signature. We
# We really need to build that stuff.
#
# --------------------------------------------------------------------------------
# Steps
# --------------------------------------------------------------------------------
#    - Download the desired Python version from https://www.python.org/ftp/python/
#    - Download xz (liblzma) from https://tukaani.org/xz/
#    - Unpack the downloaded packages somewhere. ex: $HOME/Build/
#    - Copy OpenSSL from Engine/Source/ThirdParty/OpenSSL into $HOME/Deploy
#    - Copy zlib from Engine/Source/ThirdParty/zlib into $HOME/Deploy
#    - Build lzma for x64 architecture.
#       cd $HOME/Build/xz-5.2.5
#		./configure --prefix=$HOME/Deploy/xz-5.2.5 --disable-xz --disable-lzma-links --disable-scripts --disable-doc CFLAGS="-isysroot `xcrun --sdk macosx --show-sdk-path` -mmacosx-version-min=10.15 -gdwarf-2 -arch x86_64" CPPFLAGS="-mmacosx-version-min=10.15 -gdwarf-2 -arch x86_64" LDFLAGS="-mmacosx-version-min=10.15 -arch x86_64"
#       make -j28
#       make install
#       mv $HOME/Deploy/xz-5.2.5/lib $HOME/Deploy/xz-5.2.5/lib_x64
#    - Build lzma for ARM architecture
#       make clean
#		./configure --prefix=$HOME/Deploy/xz-5.2.5 --host=aarch64-apple-darwin --disable-xz --disable-lzma-links --disable-scripts --disable-doc CFLAGS="-isysroot `xcrun --sdk macosx --show-sdk-path` -mmacosx-version-min=10.15 -gdwarf-2 -arch arm64" CPPFLAGS="-mmacosx-version-min=10.15 -gdwarf-2 -arch arm64" LDFLAGS="-mmacosx-version-min=10.15 -arch arm64"
#       make -j28
#       make install
#       mv $HOME/Deploy/xz-5.2.5/lib $HOME/Deploy/xz-5.2.5/lib_arm
#    - Smash lzma x64 and ARM libraries together
#        mkdir $HOME/Deploy/xz-5.2.5/lib
#        lipo -create $HOME/Deploy/xz-5.2.5/lib_x64/liblzma.a $HOME/Deploy/xz-5.2.5/lib_arm/liblzma.a -output $HOME/Deploy/xz-5.2.5/lib/liblzma.a
#    - Export the variables to make it easy (adjust the versions)
#        export SSL_HOME=$HOME/Deploy/OpenSSL/1.1.1k
#        export ZLIB_HOME=$HOME/Deploy/zlib/v1.2.8
#        export LZMA_HOME=$HOME/Deploy/xz-5.2.5
#    - Go to the source directory '$HOME/Build/python-3.9.7' and run the configure command (might need some fixes)
#        ./configure --prefix=$HOME/Deploy/Python3.9.7 --enable-shared --enable-universalsdk=`xcrun --sdk macosx --show-sdk-path` --with-universal-archs=universal2 --enable-optimizations --with-openssl="$SSL_HOME" CFLAGS="-isysroot `xcrun --sdk macosx --show-sdk-path` -mmacosx-version-min=10.15 -gdwarf-2 -I$ZLIB_HOME/include -I$LZMA_HOME/include" CPPFLAGS="-mmacosx-version-min=10.15 -gdwarf-2 -I$ZLIB_HOME/include -I$LZMA_HOME/include" LDFLAGS="$ZLIB_HOME/lib/libz.a $LZMA_HOME/lib/liblzma.a -mmacosx-version-min=10.15"
#        make -j28
#        make install
#    - Adjust the variables below according to your setup.
#    - Run this scripts. It copies the binaries and fix their install name.
#
# --------------------------------------------------------------------------------
# Install names
# --------------------------------------------------------------------------------
# On Mac, executables and dynamic libraries (.dylib, .so) contains install name ids to locate their dependencies. Because 
# we copy, move and redistribute Python, the stored ids, which are paths, should be relative, so the end-user can put UE5 anywhere on
# his system. Unfortunately, the build system set hardcoded path. I didn't figure out how to pass the correct options to ./configure
# and/or make install to make them relative. So it is done by this script before copying in UE5 tree.
#
# To view a binary dependencies, we can use 'otool' 
#
#    otool -L python3.9          -> View the dynamic library install name ids loaded by the executable. (ids are paths)
#    otool -l python3.9          -> View the LC_RPATH (rpath stored in the executable)
#    otool -L libpython3.9.dylib -> View this dynamic library install name id and the dynamic librairy install name ids loaded by this libraries.
#    otool -L hashlib.so         -> View this dynamic library install name id and the dynamic librairy install name ids loaded by this libraries.
#
# To support relocating python elsewhere, we need to update the install name ids (the path) with tokens that are going to
# be pattern matched at load time. For executable like UnrealEditor or python3.9, we need to tell the dynamic loader where
# to look for the libraries, relative to the executable. An executable can have serveral "rpath" values. For python3.9
# executable, we want to add this one:
#
#    install_name_tool -add_rpath @executable_path/../lib python3.9
#
# This gives one possible value to the token "@rpath" that we will used when searching the dependent librairies. Now
# we want to replace the libraries install name ids loaded by python3.9 executable to be relative too. This can be done as following:
#
#    install_name_tool -change /Users/devqa/.pyenv/versions/3.9.7/lib/libpython3.9.dylib @rpath/libpython3.9.dylib python3.9
#
# The dynamic loader matches the install name id stored in the executable and the install name id of the library, so we want to
# change the libpython3.9.dylib install name id to something relative too:
#
#    install_name_tool -id @rpath/libpython3.9.dylib Engine/Binaries/ThirdParty/Python3/Mac/lib/python3.9/libpython3.9.dylib
#
# The tokens '@rpath' and '@executable_path' are going to be replaced by the dynamic loader to locate files and then match
# the install name ids relative to the executable.
#
# The libpython3.9.dylib is loaded by UnrealEditor through a plugin. The UnrealEditor rpaths ensure we finds the plugin library
# first, then the plugin libraries refers to libpython3.9.dylib properly.
#
# The python core stuff is in libpython3.9.dylib, but several libraries are loaded 'on demand' when the intepreter encounter an import
# statement. Those dependencies are not visible by looking at libpython3.9.dylib. We need to be careful to ensure the module libraies
# are also correctly referred.
#
# --------------------------------------------------------------------------------
# Python tests suite (before running this script)
# --------------------------------------------------------------------------------
# Run Python unit tests (just after make install, before copying to UE tree). Those
# are the standard regression tests and they can be used to figure out if our build
# works outside the engine.
#
# WARNING: Few tests are sensible to the binary install names and from the root 
#          folder where the tests are invoked. From my understanding, this is just
#          some limitations in the test itself.
#
# NOTE 1: When the full test suite ends, you get a summary. You will get the list of
#         tests the passed, failed and the ones that were skipped. Review carefully
#         the skipped tests list. Those were skipped because we didn't build the
#         dependencies, because we didn't enable the resources (see below) or because
#         they are platform specific. (I tried to run them all and check)
#
# NOTE 2: Some tests are disabled by default. If you scan the list of tests that
#         were skipped, you can run some of them manually by enabling the 'resource'
#         it needs with the -uall option. Bewared that some tests might be disabled
#         for good reason, like the test_zipfile64 that use large amount of disk.
#
# Run the entire test suite from the binary install dir:
#    cd $HOME/Deploy/Python3.9.7/bin
#    ./python3.9 -m test
#
# To run a specific test:
#    ./python3.9 -m test test_venv
#
# To run a specific test with verbose:
#    ./python3.9 -m test -v test_venv
#
# To run a specific tests that was disabled becaue the resource wasn't enabled:
# See https://docs.python.org/3/library/test.html for more options.
#    ./python3.9 -m test -uall -v test_socketserver
#
# --------------------------------------------------------------------------------
# Testing (after running this script)
# --------------------------------------------------------------------------------
#
#   - Ensure the produced binaries are universal (Intel/Apple Sillicon ARM)
#     - Run: lipo -archs Engine/Binaries/ThirdParty/Python3/Mac/lib/libpython3.9.dylib
#   - Check for dependencies.
#     - $:> cd Engine/Binaries/ThridParty/Python3/Mac/lib/python3.9/lib-dynload
#     - $:> otool -L *.so'
#     - Check the libraries dependencies for: 
#       - No dependencies are in /opt/usr/local -> Those are MacPort/Homebrew that user will likely not have.
#   - Delete the $HOME/Deploy/Python3.9.7 -> This will ensure UE distribution doesn't have lib dependencies there.
#   - Run the binary we plan to distribute as:  Engine/Binaries/ThirdParty/Python3/Mac/bin/python3.9 --version
#   - Run the unit tests distributed with python:
#     - $:> cd Engine/Binaries/ThirdParty/Python3/Mac/lib/python3.9/test
#     - $:> ../../../bin/python3.9 -m unittest discover
#   - Launch UE5 EngineTest project
#     - Go to menu Tools -> Test Automation
#     - Search for python to discover all tests
#     - Run all the tests (it is a good idea to run the test before the update to see if we have regression)

# --------------------------------------------------------------------------------
# Plugins
# --------------------------------------------------------------------------------
#  - Some UE5 plugins likely needs to be rebuild when upgrading a minor version (3.7 to 3.9 for example)
#    - USDImporter is one such plugin.
#    - ML Deformer relies on PyTorch and may need some care.
#
# --------------------------------------------------------------------------------
# Tips:
# --------------------------------------------------------------------------------
#   - To see all possible configure options:  run ./configure --help
#   - Read Mac/README.rst
# --------------------------------------------------------------------------------

python_src_dest_dir="`dirname \"$0\"`/Mac"
python_bin_dest_dir="`dirname \"$0\"`/../../../Binaries/ThirdParty/Python3/Mac"
python_bin_lib_dest_dir="$python_bin_dest_dir"/lib
python_src_dir="$HOME/Deploy/Python3.9.7"
python_exe_name=python3.9
python_lib_name=libpython3.9.dylib


if [ -d "$python_src_dir" ]
then
	#
	# Fixing up install names.
	#

	echo "Adding rpath to $python_exe_name."
	install_name_tool -add_rpath @executable_path/../lib "$python_src_dir"/bin/$python_exe_name

	echo "Fixing $python_exe_name dependencies: $python_src_dir/lib/$python_lib_name -> @rpath/$python_lib_name"
	install_name_tool -change "$python_src_dir"/lib/$python_lib_name @rpath/$python_lib_name "$python_src_dir"/bin/$python_exe_name

	echo "Fixing $python_lib_name install name id"
	install_name_tool -id @rpath/libpython3.9.dylib "$python_src_dir"/lib/libpython3.9.dylib

	#
	# Deleting exiting destination folders from UE tree.
	#
	if [ -d "$python_src_dest_dir" ]
	then
		echo "Removing Existing Target Directory: $python_src_dest_dir"
		rm -rf "$python_src_dest_dir"
	fi

	if [ -d "$python_bin_dest_dir" ]
	then
		echo "Removing Existing Target Directory: $python_bin_dest_dir"
		rm -rf "$python_bin_dest_dir"
	fi

	#
	# Copying local python intallation into UE tree.
	#
	echo "Copying Python: $python_src_dir"

	mkdir -p "$python_src_dest_dir"/include
	mkdir -p "$python_bin_dest_dir"/bin
	mkdir -p "$python_bin_lib_dest_dir"

	cp -R "$python_src_dir"/include/python3.9/* "$python_src_dest_dir"/include
	cp -R "$python_src_dir"/bin/* "$python_bin_dest_dir"/bin
	cp -R "$python_src_dir"/lib/* "$python_bin_lib_dest_dir"

	cp -R "$python_src_dir"/lib/libpython3.9.dylib "$python_bin_dest_dir"
	chmod 755 "$python_bin_dest_dir"/libpython3.9.dylib

	#
	# Copy TPS file back.
	#
#	if [ -f "$python_src_dest_dir"/../../../../Restricted/NoRedist/Source/ThirdParty/Python3/TPS/PythonMacBin.tps ]
#	then
#		cp -R "$python_src_dest_dir"/../../../../Restricted/NoRedist/Source/ThirdParty/Python3/TPS/PythonMacBin.tps "$python_bin_dest_dir"/
#	fi

	#
	# Remove all symlinks (not a cross-platform thing).
	#
	echo "Processing Python symlinks: $python_dest_dir"
	function remove_symlinks()
	{
		for file in $1/*
		do
			if [ -L "$file" ]
			then
				resolved_file="$1/`readlink \"$file\"`"
				trimmed_file=".${file:$2}"
				trimmed_resolved_file=".${resolved_file:$2}"
				if [ -f "$resolved_file" ]
				then
					# for debugging
					#echo "  Removing symlink: $file -> $resolved_file"
					echo "  Removing symlink: $trimmed_file -> $trimmed_resolved_file"
				rm -f "$file"
					cp -R "$resolved_file" "$file"
				else
					echo "WARNING NOT FOUND: $resolved_file:"
				fi
			fi

			if [ -d "$file" ]
			then
				remove_symlinks "$file" $2
			fi
		done
	}
	remove_symlinks "$python_bin_lib_dest_dir" ${#python_bin_lib_dest_dir}

	function process_symlinks()
	{
		for file in $1/*
		do
			if [ -L "$file" ]
			then
				resolved_file="$1/`readlink \"$file\"`"
				trimmed_file=".${file:$2}"
				trimmed_resolved_file=".${resolved_file:$2}"
				if [ -f "$resolved_file" ]
				then
					# for debugging
					#echo "  Processing symlink: $file -> $resolved_file"
					echo "  Processing symlink: $trimmed_file -> $trimmed_resolved_file"
					rm -f "$file"
					cp "$resolved_file" "$file"
				else
					echo "WARNING NOT FOUND: $resolved_file:"
				fi
			fi

			if [ -d "$file" ]
			then
				process_symlinks "$file" $2
			fi
		done
	}
	process_symlinks "$python_bin_dest_dir" ${#python_bin_dest_dir}

	#
	# Remove any temporary files that were moved
	#
	function remove_obj_files()
	{
		for file in $1/*
		do
			if [ "${file}" != "${file%.pyc}" ] || [ "${file}" != "${file%.pyo}" ]
			then
				trimmed_file=".${file:$2}"
				#echo "  Removing: $trimmed_file"
				rm -f "$file"
			fi

			if [ -d "$file" ]
			then
				remove_obj_files "$file" $2
			fi
		done
	}
	remove_obj_files "$python_bin_lib_dest_dir" ${#python_bin_lib_dest_dir}

	function copy_openssl_libs()
	{
		# this was needed when using latest python3.9, their latest hashlib
		# uses some of libssl and libcrypto functions (instead of their own anymore)
		# TODO: see if this can be statically linked next time

		# might need a peek at lib*.x.y.z.dylib for the actual hard coded path
		openssl_lib_dir=/usr/local/opt/openssl/lib

		# saving these instructions here on how to do this for future reference
		cp "$openssl_lib_dir"/libssl.1.0.0.dylib "$python_bin_dest_dir"
		cp "$openssl_lib_dir"/libcrypto.1.0.0.dylib "$python_bin_dest_dir"

		install_name_tool -id "@rpath/libssl.1.0.0.dylib" "$python_bin_dest_dir"/libssl.1.0.0.dylib
		install_name_tool -change "$openssl_lib_dir/libcrypto.1.0.0.dylib" "@executable_path/../libcrypto.1.0.0.dylib" "$python_bin_dest_dir"/libssl.1.0.0.dylib

		install_name_tool -id "@rpath/libcrypto.1.0.0.dylib" "$python_bin_dest_dir"/libcrypto.1.0.0.dylib
		
		# finally:
		install_name_tool -change "$openssl_lib_dir/libssl.1.0.0.dylib" "@executable_path/../libssl.1.0.0.dylib" "$python_bin_dest_dir"/lib/python3.9/lib-dynload/_hashlib.so
		install_name_tool -change "$openssl_lib_dir/libcrypto.1.0.0.dylib" "@executable_path/../libcrypto.1.0.0.dylib" "$python_bin_dest_dir"/lib/python3.9/lib-dynload/_hashlib.so
	}
	# disabling this - again, for future reference
	#copy_openssl_libs

else
	echo "Python Source Directory Missing: $python_src_dir"
fi

if [ ! -f "$python_src_dest_dir"/../../../../Restricted/NoRedist/Source/ThirdParty/Python/TPS/PythonMacBin.tps ]
then
	echo "."
	echo "WARNING: restore (i.e. revert) deleted $python_bin_dest_dir/PythonMacBin.tps before checking in"
	echo "."
fi
