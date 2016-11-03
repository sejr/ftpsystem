# Makefile
# Builds (and destroys) the required executables for the FTP system.
# Last modified: 3 November 2016

# Compiler specification; we're using the gnu compiler collection (gcc)

GCC = gcc

# Source files for the main subsystems, each given their own directory

SRVSRC = src/server/ftps.c
CLISRC = src/client/ftpc.c
TMRSRC = src/timer/timer.c
TCPSRC = src/tcpd/tcpd.c

# Source files for the helper systems, grouped in 'include' directory

ROCKET = include/rocket.c
BUFFER = include/buffer.c
AUXLST = include/list.c

# Output executable destinations

SRVBLD = build/server
CLIBLD = build/client
TMRBLD = build/timer
TCPBLD = build/tcpd

all:
	mkdir build
	$(GCC) -o $(SRVBLD) $(SRVSRC) $(ROCKET)
	$(GCC) -o $(CLIBLD) $(CLISRC) $(ROCKET)
	$(GCC) -o $(TCPBLD) $(TCPSRC) $(BUFFER) $(AUXLST) -lrt
	$(GCC) -o $(TMRBLD) $(TMRSRC) -lrt

clean:
	rm -rf build
