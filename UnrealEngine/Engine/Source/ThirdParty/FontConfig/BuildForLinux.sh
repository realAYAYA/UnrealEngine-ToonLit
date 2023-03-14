set -eu

cd fontconfig-2.13.94

UE_THIRD_PARTY_DIR=`cd "../.."; pwd`
UE_LIB_DIR=`cd "lib/Unix/x86_64-unknown-linux-gpu"; pwd`

# Add --disable-silent-rules tag to see more configure details
LIBS="$UE_THIRD_PARTY_DIR/FreeType2/FreeType2-2.10.0/lib/Unix/x86_64-unknown-linux-gnu/libfreetype_fPIC.a $UE_THIRD_PARTY_DIR/libPNG/libPNG-1.5.2/lib/Unix/x86_64-unknown-linux-gnu/libpng.a $UE_THIRD_PARTY_DIR/libxml2/libxml2-2.9.10/lib/x86_64-unknown-linux-gnu/libxml2.a $UE_THIRD_PARTY_DIR/zlib/v1.2.8/lib/Unix/x86_64-unknown-linux-gnu/libz_fPIC.a" \
LDFLAGS="-ldl -lm" \
CC="$UE_SDKS_ROOT"/HostLinux/Linux_x64/v19_clang-11.0.1-centos7/x86_64-unknown-linux-gnu/bin/clang \
CFLAGS=-fPIC \
CPPFLAGS="-I$UE_THIRD_PARTY_DIR/FreeType2/FreeType2-2.10.0/include -I$UE_THIRD_PARTY_DIR/libxml2/libxml2-2.9.10/include" \
./configure --prefix=`pwd` --enable-libxml2 --enable-static --disable-shared --sysconfdir=/etc --localstatedir=/var

# Add VERBOSE=1 tags to see more make details
make -j64 2>&1 && 
make install

cd src/.libs
ar x libfontconfig.a
ar x libxml2.a
ar r libfontconfig.a *.o libfreetype_fPIC.a libpng.a libz_fPIC.a
cp libfontconfig.a "$UE_LIB_DIR"/libfontconfig.a

