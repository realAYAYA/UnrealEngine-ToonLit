The build script should work normally.

However, using non-XCode utilities can produce wrong binaries, and you will get linker errors.
Please check what your computer will use for build - ar, nm, strip, etc. 

You can do it by command "which ar", "which strip", etc. 

In a normal state, it shouldn't do call strip function so that you will see "[CP] libvpx.a < libvpx_g.a".
If you see "[STRIP] libvpx.a < libvpx_g.a", then you have installed GNU strip utilities what will create a wrong library.
Check if it is a state.
You can turn it off by using "make -j USE_GNU_STRIP=false" command for make library, but I strongly suggest 
checking and setting the environment to use XCode command tools only (cc, gcc, ar, lipo).