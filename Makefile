# Makefile
# Sample for RH_RF95 (client and server) on Raspberry Pi
# Caution: requires bcm2835 library to be already installed
# http://www.airspayce.com/mikem/bcm2835/

CC            = g++
CFLAGS        = -DRASPBERRY_PI -DBCM2835_NO_DELAY_COMPATIBILITY -D__BASEFILE__=\"$*\"
LIBS          = -lbcm2835 -lpaho-mqtt3c
RADIOHEADBASE = RadioHead
INCLUDE       = -I$(RADIOHEADBASE)

all: radiohead_gateway

RasPi.o: $(RADIOHEADBASE)/RHutil/RasPi.cpp
				$(CC) $(CFLAGS) -c $(RADIOHEADBASE)/RHutil/RasPi.cpp $(INCLUDE)

radiohead_gateway.o: radiohead_gateway.cpp
				$(CC) $(CFLAGS) -c $(INCLUDE) $<

RH_RF95.o: $(RADIOHEADBASE)/RH_RF95.cpp
				$(CC) $(CFLAGS) -c $(INCLUDE) $<

RHDatagram.o: $(RADIOHEADBASE)/RHDatagram.cpp
				$(CC) $(CFLAGS) -c $(INCLUDE) $<

RHReliableDatagram.o: $(RADIOHEADBASE)/RHReliableDatagram.cpp
				$(CC) $(CFLAGS) -c $(INCLUDE) $<

RHHardwareSPI.o: $(RADIOHEADBASE)/RHHardwareSPI.cpp
				$(CC) $(CFLAGS) -c $(INCLUDE) $<

RHSPIDriver.o: $(RADIOHEADBASE)/RHSPIDriver.cpp
				$(CC) $(CFLAGS) -c $(INCLUDE) $<

RHGenericDriver.o: $(RADIOHEADBASE)/RHGenericDriver.cpp
				$(CC) $(CFLAGS) -c $(INCLUDE) $<

RHGenericSPI.o: $(RADIOHEADBASE)/RHGenericSPI.cpp
				$(CC) $(CFLAGS) -c $(INCLUDE) $<

radiohead_gateway: radiohead_gateway.o RH_RF95.o RasPi.o RHDatagram.o RHReliableDatagram.o RHHardwareSPI.o RHGenericDriver.o RHGenericSPI.o RHSPIDriver.o
				$(CC) $^ $(LIBS) -o radiohead_gateway

clean:
				rm -rf *.o radiohead_gateway

install:
	sudo cp -f ./radiohead_gateway.service /lib/systemd/system
	sudo systemctl enable radiohead_gateway.service
	sudo systemctl daemon-reload
	sudo systemctl start radiohead_gateway
	sudo systemctl status radiohead_gateway

uninstall:
	sudo systemctl stop radiohead_gateway
	sudo systemctl disable radiohead_gateway.service
	sudo rm -f /lib/systemd/system/radiohead_gateway.service
