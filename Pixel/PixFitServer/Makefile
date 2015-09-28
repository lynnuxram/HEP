PACKAGE = PixFitServer

SRC = PixFitFitter_lmfit.cxx PixFitManager.cxx PixFitNet.cxx PixFitNetConfiguration.cxx PixFitResult.cxx PixFitPublisher.cxx PixFitWorker.cxx PixFitScanConfig.cxx PixFitAssembler.cxx RawHisto.cxx PixFitThread.cxx PixFitInstanceConfig.cxx

include ../PixLib.mk

Q = @
#------------------------------------------------------------------------------------------

.SUFFIXES: .cxx .o
.cxx.o:
	@echo "CPP $<"
	$(Q) $(CPP) $(CFLAGS) -Wall -I$(ROD_DAQ)/IblDaq/common -I$(PIXELDAQ_ROOT)/packages/lmfit-5.1/lib $(shell root-config --cflags) -c $<

.o :
	@echo "CPP $<"
	$(Q) $(CPP) -g $< $(LFLAGS)  -o $@ 

OBJ = $(addsuffix .o, $(basename $(SRC)))

lib$(PACKAGE).so: $(OBJ)
	@echo "Building $@"
	$(Q) $(CPP) -shared $(OBJ) -o lib$(PACKAGE).so

all: lib$(PACKAGE).so

depend:
	makedepend -I$(CMTCONFIG) -I$(ROD_DAQ)/IblDaq/common -I$(PIXELDAQ_ROOT)/packages/lmfit-5.1/lib -I.. -Y $(SRC)

clean:
	rm -f *.o *.a *.so *.cc *.hh 


inst: lib$(PACKAGE).so 
	mkdir -p ../installed/$(CMTCONFIG)/lib
	install -m775 lib$(PACKAGE).so ../installed/$(CMTCONFIG)/lib

# Warning,   no   -m32 !!!!!
# DO NOT DELETE

#PixFitServerInterface.o: ../PixFitServer/PixFitServerInterface.h
