
YAMIPATH=/opt/yami
#BMSDKINCPATH=/home/jay/bbsdk10.9.9/Linux/include
BMSDKINCPATH=/home/jay/bbsdk11.5.1/Linux/include

OBJS=bmd.o bmd_declink.o DeckLinkAPIDispatch.o bmd_utils.o bmd_log.o bmd_peer.o

CFLAGS=-O2 -g -Wall -Wextra -I$(YAMIPATH)/include

CXXFLAGS=-O2 -g -Wall -Wextra -I$(BMSDKINCPATH)

LDFLAGS=-L$(YAMIPATH)/lib -Wl,-rpath=$(YAMIPATH)/lib

LIBS=-lyami_inf -lm -ldl -lpthread

bmd: $(OBJS)
	$(CXX) -o bmd $(OBJS) $(LDFLAGS) $(LIBS)

clean:
	rm -f $(OBJS) bmd

DeckLinkAPIDispatch.o: $(BMSDKINCPATH)/DeckLinkAPIDispatch.cpp
	$(CXX) $(CXXFLAGS) -c $(BMSDKINCPATH)/DeckLinkAPIDispatch.cpp
