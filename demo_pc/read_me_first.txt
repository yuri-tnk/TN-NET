
TCP connections
---------------------------------

  Under some conditions, Windows keeps a TCP connection, even application
that created the connection already terminated.

  To remove this "ghost" connection, a free utility 'CurrPorts'
(http://www.nirsoft.net) is a very convenient tool.


For the Windows(c) Vista(c) users:
---------------------------------

  If you got some unstable data exchange and/or a PING utility with -t option
sometimes reports "General failure":

   - update a network card driver (especially a Realtek RTL8168/8111 PCI-E)
       to the latest version
   - check a Windows Firewall settings
   - check an additional network security software(antiviruses etc.) settings

Examples
----------------

 An examples 'HTTP_Test_1', 'TCP_Test_7', 'UDP_Test_1', 'UDP_Test_2' were
updated for the DHCP using.

  An examples 'TCP_Test_1' .. 'TCP_Test_6' from the file 'tn-net-0-8-3-src.zip'
should be prepared for the DHCP using. An example 'TCP_Test_7' (see above) can
be used as the reference for this purpose.