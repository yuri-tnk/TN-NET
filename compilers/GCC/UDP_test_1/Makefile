# =============================================================================
#
# Make file for TN NET examples
#
# Processor: LPC2368
#
# Example:   UDP_Test_1
#
# Compiler: GCC ARM v.4.3.2 (CodeSourcery Sourcery G++ Lite 2008q3-39 for ARM EABI, MS Windows)
#
# No dependencies support. Call 'make clean' and than 'make' to rebuild all
#
# =============================================================================

# =========  General =============

FORMAT = ihex
TST_N  = 1

# =========  Pathes ==============

INCLUDES = -I ./ -I ../../TNKernel


TOOLSROOT = c:/"Program Files"/CodeSourcery/"Sourcery G++ Lite"
TOOLS     = $(TOOLSROOT)/bin/arm-none-eabi-

PRJPATH  = F:/TN_NET/0.9/GCC/UDP_Test_1
PRJPATH_MSDOS  = F:\TN_NET\0.9\GCC\UDP_Test_1

DEBUG_MSDOS    = $(PRJPATH_MSDOS)\Debug
OBJ_CLR        = $(PRJPATH_MSDOS)\OBJ

DEBUG         = $(PRJPATH)/DEBUG
OBJ           = $(PRJPATH)/OBJ
TNKERNELPATH  = $(PRJPATH)/../../TNKernel
TESTFILESPATH = $(PRJPATH)/../../UDP_Test_1
TNNETFILESPATH = $(PRJPATH)/../../tn_net
TNNETCPUFILESPATH = $(PRJPATH)/../../tn_net/cpu_lpc23xx


#===========  Files  ==============

#--  Project target file name (without extension).

IMAGE = UDP_Test_$(TST_N)

#--  Linker script

LDSCRIPT = $(PRJPATH)/lpc2368_gcc.ld

#-- List of objects

OBJECTS = \
  $(OBJ)/tnkernel_startup_gcc.o\
  $(OBJ)/startup_hardware_init.o\
  $(OBJ)/tn_port_asm.o\
  $(OBJ)/tn.o\
  $(OBJ)/tn_dqueue.o\
  $(OBJ)/tn_event.o\
  $(OBJ)/tn_mem.o\
  $(OBJ)/tn_mutex.o\
  $(OBJ)/tn_port.o\
  $(OBJ)/tn_sem.o\
  $(OBJ)/tn_tasks.o\
  $(OBJ)/tn_utils.o\
  $(OBJ)/udp_test_1.o\
  $(OBJ)/daq_support.o\
  $(OBJ)/tn_sprintf.o\
  $(OBJ)/tn_user.o\
  $(OBJ)/utils.o\
  $(OBJ)/lpc23xx_int.o\
  $(OBJ)/lpc23xx_mac.o\
  $(OBJ)/lpc23xx_mac_drv.o\
  $(OBJ)/lpc23xx_net.o\
  $(OBJ)/lpc23xx_phy_KS8721.o\
  $(OBJ)/in4_cksum.o\
  $(OBJ)/tn_arp.o\
  $(OBJ)/tn_dhcp.o\
  $(OBJ)/tn_icmp.o\
  $(OBJ)/tn_in_pcb.o\
  $(OBJ)/tn_ip_input.o\
  $(OBJ)/tn_ip_output.o\
  $(OBJ)/tn_mbuf.o\
  $(OBJ)/tn_net_mem.o\
  $(OBJ)/tn_net_utils.o\
  $(OBJ)/tn_sockets.o\
  $(OBJ)/tn_sockets_udp.o\
  $(OBJ)/tn_tcp_input.o\
  $(OBJ)/tn_tcp_output.o\
  $(OBJ)/tn_tcp_sockets.o\
  $(OBJ)/tn_tcp_sockets2.o\
  $(OBJ)/tn_tcp_subr.o\
  $(OBJ)/tn_tcp_timer.o\
  $(OBJ)/tn_udp.o\

OBJECTS_CLR = \
  $(OBJ_CLR)\tnkernel_startup_gcc.o\
  $(OBJ_CLR)\startup_hardware_init.o\
  $(OBJ_CLR)\tn_port_asm.o\
  $(OBJ_CLR)\tn.o\
  $(OBJ_CLR)\tn_dqueue.o\
  $(OBJ_CLR)\tn_event.o\
  $(OBJ_CLR)\tn_mem.o\
  $(OBJ_CLR)\tn_mutex.o\
  $(OBJ_CLR)\tn_port.o\
  $(OBJ_CLR)\tn_sem.o\
  $(OBJ_CLR)\tn_tasks.o\
  $(OBJ_CLR)\tn_utils.o\
  $(OBJ_CLR)\udp_test_1.o\
  $(OBJ_CLR)\daq_support.o\
  $(OBJ_CLR)\tn_sprintf.o\
  $(OBJ_CLR)\tn_user.o\
  $(OBJ_CLR)\utils.o\
  $(OBJ_CLR)\lpc23xx_int.o\
  $(OBJ_CLR)\lpc23xx_mac.o\
  $(OBJ_CLR)\lpc23xx_mac_drv.o\
  $(OBJ_CLR)\lpc23xx_net.o\
  $(OBJ_CLR)\lpc23xx_phy_KS8721.o\
  $(OBJ_CLR)\in4_cksum.o\
  $(OBJ_CLR)\tn_arp.o\
  $(OBJ_CLR)\tn_dhcp.o\
  $(OBJ_CLR)\tn_icmp.o\
  $(OBJ_CLR)\tn_in_pcb.o\
  $(OBJ_CLR)\tn_ip_input.o\
  $(OBJ_CLR)\tn_ip_output.o\
  $(OBJ_CLR)\tn_mbuf.o\
  $(OBJ_CLR)\tn_net_mem.o\
  $(OBJ_CLR)\tn_net_utils.o\
  $(OBJ_CLR)\tn_sockets.o\
  $(OBJ_CLR)\tn_sockets_udp.o\
  $(OBJ_CLR)\tn_tcp_input.o\
  $(OBJ_CLR)\tn_tcp_output.o\
  $(OBJ_CLR)\tn_tcp_sockets.o\
  $(OBJ_CLR)\tn_tcp_sockets2.o\
  $(OBJ_CLR)\tn_tcp_subr.o\
  $(OBJ_CLR)\tn_tcp_timer.o\
  $(OBJ_CLR)\tn_udp.o\

# ---------------------------------------------------------------------------
#  Tools, tools options(flags)
# ---------------------------------------------------------------------------

CC      = $(TOOLS)gcc.exe
AS      = $(TOOLS)as.exe
CPP     = $(TOOLS)g++.exe
LD      = $(TOOLS)ld.exe
OBJCOPY = $(TOOLS)objcopy.exe
OBJDUMP = $(TOOLS)objdump.exe
SIZE    = $(TOOLS)size.exe
NM      = $(TOOLS)nm.exe
REMOVE  = DEL
COPY    = copy


OPTFLAGS = -mcpu=arm7tdmi
DEBUG =

#-- Assembler flags

ADEFS =

#ASMOPTS  = $(OPTFLAGS) $(INCLUDES) -D_GNU_ASSEMBLER_ \
#           -mlittle-endian -nostdinc -x assembler-with-cpp \
#           $(ADEFS)  -Wa,-gstabs


ASMOPTS  =  --traditional-format -march=armv4t -EL

# -Wa,-adhlns=$(<:.S=.lst),-g$(DEBUG)

#-- Compiler  flags

CDEF =

   # -Wall -Wcast-align -Wcast-qual -Wimplicit \
   # -Wmissing-prototypes -Wmissing-declarations

CCOPTS  = $(OPTFLAGS) $(INCLUDES) $(CDEF) -c \
    -Wall -Wnested-externs -Wpointer-arith -Wswitch \
    -Wredundant-decls -Wreturn-type -Wshadow -Wstrict-prototypes -Wunused \
    -O1  -std=c99  -march=armv4t -mlittle-endian -fno-builtin

#-- Linker    flags

LDFLAGS =  -nostartfiles -Wl,-Map=$(OBJ)/$(IMAGE).map,--cref  -lc \
           $(NEWLIBLPC) -lm -lc -lgcc -T$(LDSCRIPT)

#-- Combine all necessary flags and optional flags.

ALL_CFLAGS =  -I. $(CFLAGS)
ALL_ASFLAGS = -I. $(ASFLAGS)

# ---------------------------------------------------------------------------
#  Build
# ---------------------------------------------------------------------------

all: $(PRJPATH)/$(IMAGE).elf


#--- Compiling/Assembling ---

$(OBJ)/tnkernel_startup_gcc.o: $(PRJPATH)/tnkernel_startup_gcc.s
	$(AS) $(ASMOPTS) $(PRJPATH)/tnkernel_startup_gcc.s -o $(OBJ)/tnkernel_startup_gcc.o
$(OBJ)/startup_hardware_init.o: $(PRJPATH)/startup_hardware_init.s
	$(AS) $(ASMOPTS) $(PRJPATH)/startup_hardware_init.s -o $(OBJ)/startup_hardware_init.o
$(OBJ)/tn_port_asm.o: $(TNKERNELPATH)/tn_port_asm.s
	$(AS) $(ASMOPTS) $(TNKERNELPATH)/tn_port_asm.s -o $(OBJ)/tn_port_asm.o

$(OBJ)/tn.o: $(TNKERNELPATH)/tn.c
	$(CC) $(CCOPTS) $(TNKERNELPATH)/tn.c -o $(OBJ)/tn.o
$(OBJ)/tn_dqueue.o: $(TNKERNELPATH)/tn_dqueue.c
	$(CC) $(CCOPTS) $(TNKERNELPATH)/tn_dqueue.c -o $(OBJ)/tn_dqueue.o
$(OBJ)/tn_event.o: $(TNKERNELPATH)/tn_event.c
	$(CC) $(CCOPTS) $(TNKERNELPATH)/tn_event.c -o $(OBJ)/tn_event.o
$(OBJ)/tn_mem.o: $(TNKERNELPATH)/tn_mem.c
	$(CC) $(CCOPTS) $(TNKERNELPATH)/tn_mem.c -o $(OBJ)/tn_mem.o
$(OBJ)/tn_mutex.o: $(TNKERNELPATH)/tn_mutex.c
	$(CC) $(CCOPTS) $(TNKERNELPATH)/tn_mutex.c -o $(OBJ)/tn_mutex.o
$(OBJ)/tn_port.o: $(TNKERNELPATH)/tn_port.c
	$(CC) $(CCOPTS) $(TNKERNELPATH)/tn_port.c -o $(OBJ)/tn_port.o
$(OBJ)/tn_sem.o: $(TNKERNELPATH)/tn_sem.c
	$(CC) $(CCOPTS) $(TNKERNELPATH)/tn_sem.c -o $(OBJ)/tn_sem.o
$(OBJ)/tn_tasks.o: $(TNKERNELPATH)/tn_tasks.c
	$(CC) $(CCOPTS) $(TNKERNELPATH)/tn_tasks.c -o $(OBJ)/tn_tasks.o
$(OBJ)/tn_utils.o: $(TNKERNELPATH)/tn_utils.c
	$(CC) $(CCOPTS) $(TNKERNELPATH)/tn_utils.c -o $(OBJ)/tn_utils.o

$(OBJ)/udp_test_1.o: $(TESTFILESPATH)/udp_test_1.c
	$(CC) $(CCOPTS) $(TESTFILESPATH)/udp_test_1.c -o $(OBJ)/udp_test_1.o
$(OBJ)/daq_support.o: $(TESTFILESPATH)/daq_support.c
	$(CC) $(CCOPTS) $(TESTFILESPATH)/daq_support.c -o $(OBJ)/daq_support.o
$(OBJ)/tn_sprintf.o: $(TESTFILESPATH)/tn_sprintf.c
	$(CC) $(CCOPTS) $(TESTFILESPATH)/tn_sprintf.c -o $(OBJ)/tn_sprintf.o
$(OBJ)/tn_user.o: $(TESTFILESPATH)/tn_user.c
	$(CC) $(CCOPTS) $(TESTFILESPATH)/tn_user.c -o $(OBJ)/tn_user.o
$(OBJ)/utils.o: $(TESTFILESPATH)/utils.c
	$(CC) $(CCOPTS) $(TESTFILESPATH)/utils.c -o $(OBJ)/utils.o

$(OBJ)/lpc23xx_int.o: $(TNNETCPUFILESPATH)/lpc23xx_int.c
	$(CC) $(CCOPTS) $(TNNETCPUFILESPATH)/lpc23xx_int.c -o $(OBJ)/lpc23xx_int.o
$(OBJ)/lpc23xx_mac.o: $(TNNETCPUFILESPATH)/lpc23xx_mac.c
	$(CC) $(CCOPTS) $(TNNETCPUFILESPATH)/lpc23xx_mac.c -o $(OBJ)/lpc23xx_mac.o
$(OBJ)/lpc23xx_mac_drv.o: $(TNNETCPUFILESPATH)/lpc23xx_mac_drv.c
	$(CC) $(CCOPTS) $(TNNETCPUFILESPATH)/lpc23xx_mac_drv.c -o $(OBJ)/lpc23xx_mac_drv.o
$(OBJ)/lpc23xx_net.o: $(TNNETCPUFILESPATH)/lpc23xx_net.c
	$(CC) $(CCOPTS) $(TNNETCPUFILESPATH)/lpc23xx_net.c -o $(OBJ)/lpc23xx_net.o
$(OBJ)/lpc23xx_phy_KS8721.o: $(TNNETCPUFILESPATH)/lpc23xx_phy_KS8721.c
	$(CC) $(CCOPTS) $(TNNETCPUFILESPATH)/lpc23xx_phy_KS8721.c -o $(OBJ)/lpc23xx_phy_KS8721.o


$(OBJ)/in4_cksum.o: $(TNNETFILESPATH)/in4_cksum.c
	$(CC) $(CCOPTS) $(TNNETFILESPATH)/in4_cksum.c -o $(OBJ)/in4_cksum.o
$(OBJ)/tn_arp.o: $(TNNETFILESPATH)/tn_arp.c
	$(CC) $(CCOPTS) $(TNNETFILESPATH)/tn_arp.c -o $(OBJ)/tn_arp.o
$(OBJ)/tn_dhcp.o: $(TNNETFILESPATH)/tn_dhcp.c
	$(CC) $(CCOPTS) $(TNNETFILESPATH)/tn_dhcp.c -o $(OBJ)/tn_dhcp.o
$(OBJ)/tn_icmp.o: $(TNNETFILESPATH)/tn_icmp.c
	$(CC) $(CCOPTS) $(TNNETFILESPATH)/tn_icmp.c -o $(OBJ)/tn_icmp.o
$(OBJ)/tn_in_pcb.o: $(TNNETFILESPATH)/tn_in_pcb.c
	$(CC) $(CCOPTS) $(TNNETFILESPATH)/tn_in_pcb.c -o $(OBJ)/tn_in_pcb.o
$(OBJ)/tn_ip_input.o: $(TNNETFILESPATH)/tn_ip_input.c
	$(CC) $(CCOPTS) $(TNNETFILESPATH)/tn_ip_input.c -o $(OBJ)/tn_ip_input.o
$(OBJ)/tn_ip_output.o: $(TNNETFILESPATH)/tn_ip_output.c
	$(CC) $(CCOPTS) $(TNNETFILESPATH)/tn_ip_output.c -o $(OBJ)/tn_ip_output.o
$(OBJ)/tn_mbuf.o: $(TNNETFILESPATH)/tn_mbuf.c
	$(CC) $(CCOPTS) $(TNNETFILESPATH)/tn_mbuf.c -o $(OBJ)/tn_mbuf.o
$(OBJ)/tn_net_mem.o: $(TNNETFILESPATH)/tn_net_mem.c
	$(CC) $(CCOPTS) $(TNNETFILESPATH)/tn_net_mem.c -o $(OBJ)/tn_net_mem.o
$(OBJ)/tn_net_utils.o: $(TNNETFILESPATH)/tn_net_utils.c
	$(CC) $(CCOPTS) $(TNNETFILESPATH)/tn_net_utils.c -o $(OBJ)/tn_net_utils.o
$(OBJ)/tn_sockets.o: $(TNNETFILESPATH)/tn_sockets.c
	$(CC) $(CCOPTS) $(TNNETFILESPATH)/tn_sockets.c -o $(OBJ)/tn_sockets.o
$(OBJ)/tn_sockets_udp.o: $(TNNETFILESPATH)/tn_sockets_udp.c
	$(CC) $(CCOPTS) $(TNNETFILESPATH)/tn_sockets_udp.c -o $(OBJ)/tn_sockets_udp.o
$(OBJ)/tn_tcp_input.o: $(TNNETFILESPATH)/tn_tcp_input.c
	$(CC) $(CCOPTS) $(TNNETFILESPATH)/tn_tcp_input.c -o $(OBJ)/tn_tcp_input.o
$(OBJ)/tn_tcp_output.o: $(TNNETFILESPATH)/tn_tcp_output.c
	$(CC) $(CCOPTS) $(TNNETFILESPATH)/tn_tcp_output.c -o $(OBJ)/tn_tcp_output.o
$(OBJ)/tn_tcp_sockets.o: $(TNNETFILESPATH)/tn_tcp_sockets.c
	$(CC) $(CCOPTS) $(TNNETFILESPATH)/tn_tcp_sockets.c -o $(OBJ)/tn_tcp_sockets.o
$(OBJ)/tn_tcp_sockets2.o: $(TNNETFILESPATH)/tn_tcp_sockets2.c
	$(CC) $(CCOPTS) $(TNNETFILESPATH)/tn_tcp_sockets2.c -o $(OBJ)/tn_tcp_sockets2.o
$(OBJ)/tn_tcp_subr.o: $(TNNETFILESPATH)/tn_tcp_subr.c
	$(CC) $(CCOPTS) $(TNNETFILESPATH)/tn_tcp_subr.c -o $(OBJ)/tn_tcp_subr.o
$(OBJ)/tn_tcp_timer.o: $(TNNETFILESPATH)/tn_tcp_timer.c
	$(CC) $(CCOPTS) $(TNNETFILESPATH)/tn_tcp_timer.c -o $(OBJ)/tn_tcp_timer.o
$(OBJ)/tn_udp.o: $(TNNETFILESPATH)/tn_udp.c
	$(CC) $(CCOPTS) $(TNNETFILESPATH)/tn_udp.c -o $(OBJ)/tn_udp.o

#-- Linking ---

$(PRJPATH)/$(IMAGE).elf: $(OBJECTS)

	$(REMOVE)  $(OBJ_CLR)\$(IMAGE).elf
	$(REMOVE)  $(OBJ_CLR)\$(IMAGE).hex
#	$(REMOVE)  $(OBJ_CLR)\$(IMAGE).lst
	$(REMOVE)  $(OBJ_CLR)\$(IMAGE).map

	$(CC) $(ALL_CFLAGS) $(OBJECTS) --output $(OBJ)/$(notdir $@) $(LDFLAGS)

	$(OBJCOPY) -O$(FORMAT)    $(OBJ)/$(IMAGE).elf  $(OBJ)/$(IMAGE).hex

#--- Display segment's size

	$(SIZE) -A $(OBJ)/$(IMAGE).elf


# ===========================================================================

.PHONY:   clean

clean:

	$(REMOVE) $(OBJECTS_CLR)

# ===========================================================================
# ===========================================================================
# ===========================================================================






