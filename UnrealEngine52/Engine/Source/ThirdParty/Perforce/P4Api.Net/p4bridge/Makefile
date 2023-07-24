# p4api native bridge for Linux

# dotnet build /p:Configuration=Debug /p:Platform="x64" ./p4api.net.core/p4api.net.core.csproj
# dotnet build /p:Configuration=Debug /p:Platform="x64" ./examples/sln-bld-cmd/sln-bld-cmd.csproj
# make clean p4bridge && cp ./p4bridge.so ../examples/sln-bld-cmd/bin/x64/Debug/netcoreapp3.1/

OBJDIR=Build/Obj

# don't think we need $(OBJDIR)/ClientManager.o
P4API_OBJS= $(OBJDIR)/P4BridgeEnviro.o $(OBJDIR)/P4Connection.o $(OBJDIR)/P4BridgeClient.o $(OBJDIR)/P4BridgeServer.o $(OBJDIR)/p4base.o $(OBJDIR)/p4map-api.o $(OBJDIR)/p4bridge-api.o $(OBJDIR)/utils.o $(OBJDIR)/Lock.o $(OBJDIR)/DoublyLinkedList.o

TEST_OBJS=$(OBJDIR)/Test.o

CXXFLAGS=-fPIC -std=c++14 -O1 -I../p4api/include/p4

# LDFLAGS=-Wl,--unresolved-symbols=ignore-in-object-files

# fix me, absolute path
LIBPATH=../p4api/lib

LIBS=$(LIBPATH)/libclient.a $(LIBPATH)/librpc.a $(LIBPATH)/libsupp.a $(LIBPATH)/libp4api.a -lssl -lcrypto -lpthread -ldl -lstdc++ 

$(OBJDIR):
	mkdir -p $(OBJDIR)

$(OBJDIR)/%.o: %.cpp | $(OBJDIR)
	$(CC) $(CXXFLAGS) -c -o $@ $<

# note by default we strip debug info for size
p4bridge: $(P4API_OBJS)
	$(CC) $(LDFLAGS) -shared -o p4bridge.so $^ $(LIBS)

clean:
	rm -rf $(OBJDIR)/ 

all: p4bridge
