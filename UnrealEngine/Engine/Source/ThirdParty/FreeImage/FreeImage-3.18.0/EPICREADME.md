
2020/07/09

While porting UE to Apple silicon on Mac I discovered the source to FreeImage was never checked in.

I downloaded the matching version from SourceForge, fixed up some compilation errors under C99, and updated makefile.osx to remove 32-bit support and replace it with arm64. Mac now builds universal binaries as standard via make.

Those changes are mirrored here - https://github.com/andrewgrant/FreeImage-applesi