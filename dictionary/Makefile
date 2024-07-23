CXXFLAGS = -std=c++17 -fPIC -O2

all: libEveBuilder.so

MyDict.cxx: ../include/THitData.hpp LinkDef.h
	rootcling -f $@ -c -O2 -p $^
	
libEveBuilder.so: MyDict.cxx
	g++ -shared -o$@ $(CXXFLAGS) -I$(ROOTSYS)/include $^ `root-config --ldflags --libs`

clean:
	rm -f MyDict.cxx libEveBuilder.so *.pcm
