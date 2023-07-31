# Plugin - Manual installation

Plugin files should be installed under "%USERPROFILE%\AppData\Roaming\SketchUp\SketchUp 2020\SketchUp\Plugins\"

Plugin files are built into:
C:\ue\main\Engine\Binaries\Win64\SketchUpRuby\2020\Plugin

Install by copying contents of the Plugin folder directly into Plugins folder of SketchUp. I.e. omit Plugin folder itself, copy only it's contents.

Resulting directory layout:

"%USERPROFILE%\AppData\Roaming\SketchUp\SketchUp 2020\SketchUp\Plugins\"
	UnrealDatasmithSketchUp2020.rb
	UnrealDatasmithSketchUp2020/
    	DatasmithSketchUpRuby2020.so
		plugin_main.rb


# Plugin usage

Plugin should load automatically and present two buttons - one is to export current datasmith scene into a file(currently temp directory) another updates DirectLink
