# EpicGames.Perforce.Native

OBJDIR=obj/linux-x64/Release
OUTDIR=../../../../Binaries/DotNET/EpicGames.Perforce.Native/linux-x64/Release
 
CXXFLAGS=-fPIC -std=c++14 -O1 -I../../../ThirdParty/Perforce/p4api-2018.1/Include/Linux/p4
 
LIBPATH=../../../ThirdParty/Perforce/p4api-2018.1/Lib/Linux
LIBS=$(LIBPATH)/libclient.a $(LIBPATH)/librpc.a $(LIBPATH)/libsupp.a -lssl -lcrypto -lpthread -ldl -lstdc++ -L../../../ThirdParty/OpenSSL/1.1.1c/lib/Unix/x86_64-unknown-linux-gnu
 
all: clean build

build: $(OBJDIR)/Client.o
	mkdir -p $(OUTDIR)/
	$(CC) $(LDFLAGS) -shared -o $(OUTDIR)/EpicGames.Perforce.Native.so $^ $(LIBS)
 
clean:
	rm -rf $(OBJDIR)/ 
	rm -rf $(OUTDIR)/

$(OBJDIR):
	mkdir -p $(OBJDIR)
 
$(OBJDIR)/%.o: %.cpp | $(OBJDIR)
	$(CC) $(CXXFLAGS) -c -o $@ $<
 