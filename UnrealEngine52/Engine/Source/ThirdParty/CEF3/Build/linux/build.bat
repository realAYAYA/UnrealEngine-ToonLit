REM Linux specific GN defines are for cups(printing) and the keyring
set GN_DEFINES="is_component_build=false enable_precompiled_headers=false is_official_build=true use_sysroot=true use_allocator=none symbol_level=1 is_cfi=false use_thin_lto=false use_cups=false use_gnome_keyring=false enable_remoting=false"

docker build -f Dockerfile_base -t cef3 .
docker build -f Dockerfile_build -t cef3_build --build-arg CEF_BRANCH=4577 --build-arg GN_DEFINES=%GN_DEFINES% .
REM Delete any old CEF3 build images that may exist
docker rm cef3_build
docker run --name cef3_build -it cef3_build
docker cp cef3_build:/code/chromium/src/cef/binary_distrib/ .
echo "###"
echo "### The binary_distrib folder now contains the Linux CEF build."
