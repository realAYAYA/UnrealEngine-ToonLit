# vim: expandtab foldlevel=1 ft=make

# Ensure "all" is the first encountered target so it is the default
all :

ifdef MAKEDIR: # gmake=false condition, nmake=two unused targets
!ifdef MAKEDIR # gmake=skipped,			nmake=true condition

!include nmake.makefile

!else
else

include gmake.makefile

endif 
!endif :
