@echo off
REM Requirements:
REM - python3
REM - jsonschema for python ("pip install jsonschema")


REM Script does not create the debug folder and fails if it's not there
mkdir debug


REM "-r" for the Vulkan registry file to use (vk.xml).  Generate the header using the registry of the SDK installed with the engine.
REM "-i" for the input folder.  It will generate the header using all the .JSON files in this folder.
REM "-d" to also generate a 'debug' version of the header.  The debug version also includes log printing functions which could be useful.
REM "--output-library-inc" and "--output-library-src" for where to generate the output files.
REM "--output-doc" to generate a nice looking .md table of our requirements that easy to browse and search.  TODO: Should convert this to HTML to be easier to browse...
python ..\share\vulkan\registry\gen_profiles_solution.py -r ..\share\vulkan\registry\vk.xml -i .\ -d --output-library-inc .\ --output-library-src .\ --output-doc .\unreal_profile_doc.md


REM This will add our namepsace in front of Vulkan functions that are directly called and place the output in the \Include folder
python .\add_namespace.py
