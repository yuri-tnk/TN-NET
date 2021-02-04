<div id="wrap">

<div id="content">

## TN NET TCP/IP stack

## Description

<span class="blue1">**1\. Introduction**</span>

    TN NET TCP/IP stack was designed for embedded processors with RAM size 32 KBytes and more and FLASH size 128 KBytes and more (for instance, NXP (c) LPC23xx/LPC17XX, Atmel (c) AT91SAM7X, Luminary Micro (c) LM3S6xx/LM3S8xx/LM3S9xx, Freescale (c) MCF5223X and many other similar MCU).

    TN NET TCP/IP stack protocols implementation is based on the industry-standard BSD TCP/IP stack.
    A BSD sockets are used as the API (user interface).
   TN NET TCP/IP stack uses a TNKernel RTOS synchronization elements to provide a true multitasking reentrant stack.

<span class="blue1">**2\. TN NET TCP/IP stack executive model**</span>

    TN NET interface (an Ethernet interface is assumed here) provides the data sending/receiving procedure. An ARP protocol support is a part of the interface software. The interface contains a reception task and two TNKernel data queues - _Interface Rx Queue_ and _Interface Tx Queue_. An Ethernet driver after reception from a wire puts (inside the Rx interrupt handler) the packet into the _Interface Rx Queue_. The IP or ARP protocols, as data senders, puts a packet to send into _Interface Tx Queue_ directly. This event starts a transmitting, if transmitting is not yet active. If transmitting is already active (in progress), the _End of Transmit_ interrupt handler will restart transmitting for the next packet from the _Interface Tx Queue_.

   An interface reception task works with _Interface Rx Queue_. If the queue is not empty, the reception task invokes appropriate protocol input functions (IP or ARP). According to the type of the received packet, IP input function calls a related protocol input handler (UDP/TCP/ICMP etc) for the further processing. For instance, UDP protocol input function adds a received packet to the
user's socket input queue (it is a TNKernel data queue also) and user's socket task will be wake-up. If there are no open UDP sockets for the received packet destination port, UDP will respond to the sender with ICMP error message.

<span class="blue1">**3\. TN NET TCP/IP stack implementation details**</span>

  <span class="blue1">**3.1 Memory buffers**</span>

   TN NET TCP/IP stack uses 3 types of fixed-sized memory pools - with a 32, 128 and 1536 bytes elements memory size.
   A memory buffer in TN NET TCP/IP stack contains two items - descriptor and data buffer. A memory for each item is allocated separately.

   A descriptor size is 32 bytes, data buffer size can be 32, 128 or 1536 bytes. An allocation a separate memory block for the descriptor makes a memory buffers system very flexible (just a few examples):

             - for the zero-copy operation it is enough allocate a new descriptor only.

             - in the NXP LPC23xx CPU an Ethernet MAC has access only to the Ethernet RAM
               (16 kbyte). In this case, data buffers can be allocated in the Ethernet RAM
               and descriptors can be allocated in the regular RAM.

             - data buffer can be placed into the read-only memory.

   <span class="blue1">**3.2 Protocols**</span>

   An IP protocol in the TN NET TCP/IP stack supports reassembly for the input packets and fragmentation for the output packets.

   An ICMP implementation is very basic - reflect/ping response and ICMP error sending.

   The stack uses BSD protocol control blocks (PCB) to store internal data for UDP/TCP protocols.

   <span class="blue1">**3.3 Sockets**</span>

   In TN NET TCP/IP stack, an API (user interface) is a BSD sockets.
  The API functions, supported by this software, are a subset of the BSD socket API, supplied with UNIX compatible systems.
  The function names consist of BSD socket function names with a prefix 's_'

<div class="style4" align="center">

<table id="table8" width="77%" cellspacing="0" cellpadding="0" border="0">

<tbody>

<tr>

<td>_**  Supported socket functions**_</td>

</tr>

</tbody>

</table>

</div>

<table id="table" class="sortable" cellspacing="0" cellpadding="0" border="0">

<tbody>

<tr>

<td class="style4" width="235" valign="top" height="13" bgcolor="#F6F6F6" align="center">Function</td>

<td class="style4" width="306" valign="top" height="13" bgcolor="#F6F6F6" align="center">Supported protocols</td>

</tr>

<tr>

<td class="style4" width="235" valign="top" height="13" align="center">s_socket()</td>

<td class="style4" width="306" valign="top" height="13" align="center">TCP, UDP, RAW</td>

</tr>

<tr>

<td class="style4" width="235" valign="top" height="13" align="center">s_close()</td>

<td class="style4" width="306" valign="top" height="13" align="center">TCP, UDP, RAW</td>

</tr>

<tr>

<td class="style4" width="235" valign="top" height="13" align="center">s_bind()</td>

<td class="style4" width="306" valign="top" height="13" align="center">TCP, UDP, RAW</td>

</tr>

<tr>

<td class="style4" width="235" valign="top" height="13" align="center">s_connect()</td>

<td class="style4" width="306" valign="top" height="13" align="center">TCP, UDP, RAW (UDP, RAW - to set a peer address only)</td>

</tr>

<tr>

<td class="style4" width="235" valign="top" height="13" align="center">s_accept()</td>

<td class="style4" width="306" valign="top" height="13" align="center">TCP</td>

</tr>

<tr>

<td class="style4" width="235" valign="top" height="13" align="center">s_listen()</td>

<td class="style4" width="306" valign="top" height="13" align="center">TCP</td>

</tr>

<tr>

<td class="style4" width="235" valign="top" height="13" align="center">s_recv()</td>

<td class="style4" width="306" valign="top" height="13" align="center">TCP</td>

</tr>

<tr>

<td class="style4" width="235" valign="top" height="13" align="center">s_recvfrom()</td>

<td class="style4" width="306" valign="top" height="13" align="center">UDP, RAW</td>

</tr>

<tr>

<td class="style4" width="235" valign="top" height="13" align="center">s_send()</td>

<td class="style4" width="306" valign="top" height="13" align="center">TCP</td>

</tr>

<tr>

<td class="style4" width="235" valign="top" height="13" align="center">s_sendto()</td>

<td class="style4" width="306" valign="top" height="13" align="center">UDP, RAW</td>

</tr>

<tr>

<td class="style4" width="235" valign="top" height="13" align="center">s_ioctl()</td>

<td class="style4" width="306" valign="top" height="13" align="center">TCP, UDP</td>

</tr>

<tr>

<td class="style4" width="235" valign="top" height="13" align="center">s_shutdown()</td>

<td class="style4" width="306" valign="top" height="13" align="center">TCP</td>

</tr>

<tr>

<td class="style4" width="235" valign="top" height="13" align="center">s_getpeername()</td>

<td class="style4" width="306" valign="top" height="13" align="center">TCP, UDP, RAW</td>

</tr>

<tr>

<td class="style4" width="235" valign="top" height="13" align="center">s_getsockopt()</td>

<td class="style4" width="306" valign="top" height="13" align="center">TCP, UDP, RAW</td>

</tr>

<tr>

<td class="style4" width="235" valign="top" height="13" align="center">s_setsockopt()</td>

<td class="style4" width="306" valign="top" height="13" align="center">TCP, UDP, RAW</td>

</tr>

</tbody>

</table>

<span class="blue1">**4\. TN NET TCP/IP stack usage**</span>

   These two file are used to set the stack configuration:

                   _tn_net_cfg.h_   - sets CPU endian, buffers number etc.

                   _lpc23xx_net.c_ - sets MAC address, IP addresses and mask

   The examples source code can be used as reference - how to initialize TN NET TCP/IP stack, create sockets, send/receive data etc.

    <span class="blue1">**4.1 UDP Examples**</span>

     "UDP_Test_1" - This is a UDP data acquisition example. The device sends a packet to the host, host sends back an ACK response.
      This example works together with a PC-based "_UDP_daq_" This is a TDTP protocol demo (it uses UDP as a transport layer protocol).

     "UDP_Test_2" - the example works together with a PC-based "_UDP_server_" application.
There are 2 network related user tasks in the project - TASK_PROCESSING and TASK_DAQ. Each task has an own UDP socket to transfer data. Both tasks have an equal priority and perform a UDP data exchange simultaneously.
   A TASK_PROCESSING task sends a on-the-fly generated file (~4Mbytes) to the "_UDP_server_" application, when a "_Receive to file_" radio button is selected in the "_UDP_server_" dialog window.
   When "_Send file_" radio button is selected in the "_UDP_server_" dialog window, a file from the PC (user should choose the file) is transferred to the device.
   Both transfers are performed with TDTP protocol.

  A TASK_DAQ task simulates a data acquisition process by the "_UDP_server_" application request. After a request from the "_UDP_server_" application, the task sends a data to the "_UDP_server_".
   The transfer is performed with TDTP protocol.

    <span class="blue1">**4.2 TCP Examples**</span>

  An examples "_TCP_test_1_", "_TCP_test_2_" and "_TCP_test_3_" show TN NET TCP protocol client operating - reception, transmission and both reception and transmission. The examples work together with a PC applications _TCP_test_1_", "_TCP_test_2_" and "_TCP_test_3_".
  An examples "_TCP_test_4_", "_TCP_test_5_" and "_TCP_test_6_" show TN NET TCP protocol server operating - reception, transmission and both reception and transmission. The examples work together with a PC applications _TCP_test_4_", "_TCP_test_5_" and "_TCP_test_6_".
  An examples "_TCP_test_7_" shows TN NET TCP server's sockets opening/closing.
  An examples "_HTTP_test_1_" shows TN NET simple embedded Web server. The Web site files for the server should be prepared by the "_htm_to_c.exe_" utility ( [http-pc-1-0-src.zip](http://tnkernel.com/downloads/http-pc-1-0-src.zip) ).
 The Web server supports forms and dynamic data updating.

</div>

<div class="tabbertab  tabbertabhide" title="">

## License

<table id="table_1" class="sortable" cellspacing="0" cellpadding="0" border="0">

<tbody>


<tr>

<td></td>
<td></td>

</tr>

</tbody>

</table>

</div>

</div>

</div>

<div id="footer">

© 2005, 2017 **Yuri Tiomkin**

</div>

</div>