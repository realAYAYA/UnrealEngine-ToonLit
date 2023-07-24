This library exists to support UE's custom native visualizers in Visual Studio within modules loaded by Live Code (displaying FNames, etc..).

When generating a patch executable, we include this library and force the linker to include a reference to the global InitNatvisHelpers() function. 

This creates references to GNameBlocksDebug and GObjectArrayForDebugVisualizers. Object files in static libraries are only included if a symbol in them is referenced, so if those symbols already exist in the patch, those references will be used. Otherwise the definitions
in Global.cpp will be used.
