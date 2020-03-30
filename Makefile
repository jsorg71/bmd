
YAMIPATH=/opt/yami
BMSDKINCPATH=/home/jay/bbsdk10.9.9/Linux/include

OBJS=bmd.o bmd_declink.o DeckLinkAPIDispatch.o
# bmd_peer.o bmd_log.o bmd_utils.o

CFLAGS=-O2 -g -Wall -Wextra -I$(YAMIPATH)/include

CXXFLAGS=-O2 -g -Wall -Wextra -I$(BMSDKINCPATH)

LDFLAGS=-L$(YAMIPATH)/lib -Wl,-rpath=$(YAMIPATH)/lib

LIBS=-lyami_inf -lm -ldl -lpthread

bmd: $(OBJS)
	$(CXX) -o bmd $(OBJS) $(LDFLAGS) $(LIBS)

clean:
	rm -f $(OBJS) bmd

bmd_declink.o: bmd_declink.cpp
	$(CXX) $(CXXFLAGS) -c bmd_declink.cpp

DeckLinkAPIDispatch.o: $(BMSDKINCPATH)/DeckLinkAPIDispatch.cpp
	$(CXX) $(CXXFLAGS) -c $(BMSDKINCPATH)/DeckLinkAPIDispatch.cpp
