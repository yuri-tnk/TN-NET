#ifndef _TN_DHCP_H_
#define _TN_DHCP_H_



#define  DHCP_BOOTP_REQUEST       1
#define  DHCP_HTYPE_ETHERNET      1
#define  DHCP_HLEN_ETHERNET       6

#define  DHCP_ST_REBOOTING        1
#define  DHCP_ST_INIT             2
#define  DHCP_ST_SELECTING        3
#define  DHCP_ST_REQUESTING       4
#define  DHCP_ST_BOUND            5
#define  DHCP_ST_RENEWING         6
#define  DHCP_ST_REBINDING        7
#define  DHCP_ST_INITREBOOT       8

// DHCP options and values (ref. RFC1533/RFC2132).
// Many are also BOOTP options.

enum DHCP_Options
{
   DHCP_OPT_PAD                      =  0,
   DHCP_OPT_SUBNET_MASK              =  1,
   DHCP_OPT_TIME_OFFSET              =  2,
   DHCP_OPT_ROUTERS_ON_SNET          =  3,
   DHCP_OPT_TIME_SRV                 =  4,
   DHCP_OPT_NAME_SRV                 =  5,
   DHCP_OPT_DNS_SRV                  =  6,
   DHCP_OPT_LOG_SRV                  =  7,
   DHCP_OPT_COOKIE_SRV               =  8,
   DHCP_OPT_LPR_SRV                  =  9,
   DHCP_OPT_IMPRESS_SRV              = 10,
   DHCP_OPT_RES_LOCATION_SRV         = 11,
   DHCP_OPT_HOST_NAME                = 12,
   DHCP_OPT_BOOT_FSIZE               = 13,
   DHCP_OPT_MERIT_DUMPFILE           = 14,
   DHCP_OPT_DOMAIN_NAME              = 15,
   DHCP_OPT_SWAP_SRV                 = 16,
   DHCP_OPT_ROOT_PATH                = 17,
   DHCP_OPT_EXTENTIONS_PATH          = 18,
   DHCP_OPT_IP_FORWARDING            = 19,
   DHCP_OPT_NON_LOCAL_SRC_ROUTE      = 20,
   DHCP_OPT_POLICY_FILTER            = 21,
   DHCP_OPT_MAX_DGRAM_REASM_SIZE     = 22,
   DHCP_OPT_IP_DEFAULT_TTL           = 23,
   DHCP_OPT_PATH_MTU_AGING_TIMEOUT   = 24,
   DHCP_OPT_PATH_MTU_PLATEAU_TABLE   = 25,
   DHCP_OPT_IF_MTU                   = 26,
   DHCP_OPT_ALL_SUBNETS_LOCAL        = 27,
   DHCP_OPT_BROADCAST_ADDR           = 28,
   DHCP_OPT_PERFORM_MASK_DISCOVERY   = 29,
   DHCP_OPT_MASK_SUPPLIER            = 30,
   DHCP_OPT_PERFORM_ROUTER_DISCOVERY = 31,
   DHCP_OPT_ROUTER_SOLICITATION_ADDR = 32,
   DHCP_OPT_STATIC_ROUTE             = 33,
   DHCP_OPT_TRAILER_ENCAPSULATION    = 34,
   DHCP_OPT_ARP_CACHE_TIMEOUT        = 35,
   DHCP_OPT_ETHERNET_ENCAPSULATION   = 36,
   DHCP_OPT_TCP_DEFAULT_TTL          = 37,
   DHCP_OPT_TCP_KEEPALIVE_INTERVAL   = 38,
   DHCP_OPT_TCP_KEEPALIVE_GARBAGE    = 39,
   DHCP_OPT_NIS_DOMAIN_NAME          = 40,
   DHCP_OPT_NIS_SRVS                 = 41,
   DHCP_OPT_NTP_SRVS                 = 42,
   DHCP_OPT_VENDOR_SPECIFIC_INFO     = 43,
   DHCP_OPT_NBIOS_NAME_SRV           = 44,
   DHCP_OPT_NBIOS_DGRAM_DIST_SRV     = 45,
   DHCP_OPT_NBIOS_NODE_TYPE          = 46,
   DHCP_OPT_NBIOS_SCOPE              = 47,
   DHCP_OPT_XFONT_SRV                = 48,
   DHCP_OPT_XDISPLAY_MANAGER         = 49,
   DHCP_OPT_REQUESTED_IP_ADDR        = 50,
   DHCP_OPT_IP_ADDR_LEASE_TIME       = 51,
   DHCP_OPT_OVERLOAD                 = 52,
   DHCP_OPT_MSG_TYPE                 = 53,
   DHCP_OPT_SRV_IDENTIFIER           = 54,
   DHCP_OPT_PARAM_REQUEST            = 55,
   DHCP_OPT_MSG                      = 56,
   DHCP_OPT_MAX_MSG_SIZE             = 57,
   DHCP_OPT_T1_VALUE                 = 58,
   DHCP_OPT_T2_VALUE                 = 59,
   DHCP_OPT_CLASS_ID                 = 60,
   DHCP_OPT_CLIENT_ID                = 61,
   DHCP_OPT_NIS_DOMAIN               = 64,
   DHCP_OPT_NIS_SRV_OPT              = 65,
   DHCP_OPT_TFTP_SERVER              = 66,
   DHCP_OPT_BOOT_FILENAME            = 67,
   DHCP_OPT_MOBIP_HOME_AGENTS        = 68,
   DHCP_OPT_SMTP_SRVS                = 69,
   DHCP_OPT_POP3_SRVS                = 70,
   DHCP_OPT_NNTP_SRVS                = 71,
   DHCP_OPT_WWW_SRVS                 = 72,
   DHCP_OPT_FINGER_SRVS              = 73,
   DHCP_OPT_IRC_SRVS                 = 74,
   DHCP_OPT_STREET_TALK_SRVS         = 75,
   DHCP_OPT_STDA_SRVS                = 76,
   DHCP_OPT_USER_CLASS               = 77,
   DHCP_OPT_GRUB_MENU                = 150,
   DHCP_OPT_END                      = 255
};

enum  DHCP_MsgTypes
{
   DHCP_DISCOVER = 1,
   DHCP_OFFER,
   DHCP_REQUEST,
   DHCP_DECLINE,
   DHCP_ACK,
   DHCP_NAK,
   DHCP_RELEASE,
   DHCP_INFORM
};

#define DHCP_SERVER_PORT     67
#define DHCP_CLIENT_PORT     68


//--RFC 2131

#define DHCP_OPTIONS_SIZE   312

#if defined (__ICCARM__)   // IAR ARM
#pragma pack(push, 1)
#endif

struct dhcp
{
   __pkt_field unsigned char  dh_op;         // packet op code / message type.
   __pkt_field unsigned char  dh_htype;      // hardware address type, 1 = 10 mb ethernet
   __pkt_field unsigned char  dh_hlen;       // hardware address len, eg '6' for ethernet
   __pkt_field unsigned char  dh_hops;       // client sets to zero, optionally used by
                                 // gateways in cross-gateway booting.
   __pkt_field unsigned int   dh_xid;        // transaction ID, a random number
   __pkt_field unsigned short dh_secs;       // filled in by client, seconds elapsed
                                 // since client started trying to boot.
   __pkt_field unsigned short dh_flags;      // Flag (0x8000 for broadcast, else 0)
   __pkt_field unsigned int   dh_ciaddr;     // client IP address filled in by client if known
   __pkt_field unsigned int   dh_yiaddr;     // 'your' (client) IP address
                                 // filled by server if client doesn't know
   __pkt_field unsigned int   dh_siaddr;     // server IP address returned in bootreply
   __pkt_field unsigned int   dh_giaddr;     // gateway IP address,
                                 // used in optional cross-gateway booting.
   __pkt_field unsigned char  dh_chaddr[16]; // client hardware address, filled by client
   __pkt_field unsigned char  dh_sname[64];  // optional server host name, null terminated

   __pkt_field unsigned char  dh_file[128];  // boot file name, null terminated string
                                 // 'generic' name or null in bootrequest,
                                 // fully qualified directory-path
                                 // name in bootreply.
   __pkt_field unsigned char  dh_opt[DHCP_OPTIONS_SIZE];   // DHCP options area (minimum 308 unsigned chars)
}__pkt_struct_ex;

#if defined (__ICCARM__)   // IAR ARM
#pragma pack(pop)
#endif

// This was 284, but D-Link 614+ router ACKs are too small

#define DHCP_MIN_SIZE           280

#define DHCP_MAX_DNS              2
#define DHCP_MAX_INIT_RETRIES     4
#define DHCP_INIT_PERIOD       4000

#define BOOTP_REPLY               2

#define DHCP_INFINITE_LEASE    0xFFFFFFFFu

struct _DHCPINFO
{
   struct dhcp * dhcp_dh;  //-- For rx/tx DHCP data
   int state;
   int s;
   unsigned int rx_timeout;

   unsigned int dhcp_server_addr;
   unsigned int lease_time;
   unsigned int renewal_time;
   unsigned int rebind_time;
   int  dns_count;
   unsigned int dns_addr[DHCP_MAX_DNS];

   unsigned int t1_timeout;
   unsigned int t2_timeout;

   //--- Dynamic data - uses at DHCP lease obtaining states

   int  init_num_retries;

   unsigned int d_xid;

   unsigned int d_curr_ip_addr;
   unsigned int d_offer_ip_addr;
   unsigned int d_dhcp_server_addr;
   unsigned int d_subnet_mask;
   unsigned int d_router_addr;
   unsigned int d_broadcast_addr;
   int  d_dns_count;
   unsigned int d_dns_addr[DHCP_MAX_DNS];
   unsigned int d_lease_time;  //-- as suggest_lease also
   unsigned int d_renewal_time;
   unsigned int d_rebind_time;

   //---
};
typedef struct _DHCPINFO DHCPINFO;

//--- To pass parameter into dhcp task function
typedef struct _DHCPPAR
{
   TN_NET * tnet;
   TN_NETIF * ni;
}DHCPPAR;

  //-- DHCP task

#define DHCP_TASK_PRIORITY      (TNNET_NETTIMERTASK_PRIORITY)
#define DHCP_TASK_STACK_SIZE    192
  //--- tn_dhcp.c

int dhcp_init(TN_NET * tnet,
              TN_NETIF * ni,
              DHCPPAR * fparam,
              struct dhcp * dhcp_dh);
void dhcp_timer_func(TN_NETINFO * tneti, int cnt);
int dhcp_ready(TN_NETIF * ni);


#endif

