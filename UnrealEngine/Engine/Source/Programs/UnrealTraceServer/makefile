# vim: expandtab foldlevel=1 ft=make

# Ensure "all" is the first encountered target so it is the default
all :

ifdef MAKEDIR: # gmake=false condition, nmake=two unused targets
!ifdef MAKEDIR # gmake=skipped,			nmake=true condition

!include build/nmake.makefile

!else
else

include build/gmake.makefile

endif 
!endif :
