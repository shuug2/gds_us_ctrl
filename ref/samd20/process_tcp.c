#include <stdio.h>
#include "process_tcp.h"
#include "socket.h"
#include "wizchip_conf.h"

int32_t control_tcps(uint8_t sn, uint8_t * status, uint16_t port)
{
   int32_t ret;
   uint16_t size = 0, sentsize=0;

#ifdef _TCPSRV_DEBUG_
   uint8_t destip[4];
   uint16_t destport;
#endif

   switch(getSn_SR(sn))
   {
      case SOCK_ESTABLISHED :
         if(getSn_IR(sn) & Sn_IR_CON)
         {
#ifdef _TCPSRV_DEBUG_
			   getSn_DIPR(sn, destip);
			   destport = getSn_DPORT(sn);

			   printf("%d:Connected - %d.%d.%d.%d : %d\r\n",sn, destip[0], destip[1], destip[2], destip[3], destport);
#endif
            setSn_IR(sn,Sn_IR_CON);
            *status = SERVER_CONNECT;
         }
         else 
            *status = SERVER_ESTABLISHED;
         break;
      case SOCK_CLOSE_WAIT :
#ifdef _TCPSRV_DEBUG_
         //printf("%d:CloseWait\r\n",sn);
#endif
         if((ret = disconnect(sn)) != SOCK_OK)
         {
            *status = SERVER_CLOSE_WAIT;
            return ret;
         } 
#ifdef _TCPSRV_DEBUG_
         printf("%d:Socket Closed\r\n", sn);
#endif
         *status = SERVER_CLOSED;
         break;
      case SOCK_INIT :
#ifdef _TCPSRV_DEBUG_
    	   printf("%d:Listen, TCP server, port [%d]\r\n", sn, port);
#endif
         if( (ret = listen(sn)) != SOCK_OK)
         {
            *status = SERVER_INIT;
            return ret;
         }
         *status = SERVER_LISTEN;
         break;
      case SOCK_CLOSED:
#ifdef _TCPSRV_DEBUG_
         printf("%d:TCP server loopback start\r\n",sn);
#endif
         if((ret = socket(sn, Sn_MR_TCP, port, 0x00)) != sn)
         {
            *status = SERVER_ESTABLISHED;
            return ret;
         }
#ifdef _TCPSRV_DEBUG_
         printf("%d:Socket opened\r\n",sn);
#endif
         *status = SERVER_CLOSED;
         break;
      default:
         break;
   }
   return 1;
}

int32_t send_tcps(uint8_t sn, uint8_t* buf, uint16_t size)
{
   int32_t ret;
   uint16_t sentsize=0;

#ifdef _TCPSRV_DEBUG_
   uint8_t destip[4];
   uint16_t destport;
#endif
   printf("send data to %d - (%d): %s\r\n", sn, size, buf);

   switch(getSn_SR(sn))
   {
      case SOCK_ESTABLISHED :
         while(size != sentsize)
         {
            ret = send(sn, buf+sentsize, size-sentsize);
            if(ret < 0)
            {
               return ret;
            }
            sentsize += ret; // Don't care SOCKERR_BUSY, because it is zero.
         }
         break;
      default:
         break;
   }
   return 0;
}

int32_t recv_tcps(uint8_t sn, uint8_t* buf)
{
   int32_t ret;
   uint16_t size = 0, sentsize=0;

#ifdef _TCPSRV_DEBUG_
   uint8_t destip[4];
   uint16_t destport;
#endif

   switch(getSn_SR(sn))
   {
      case SOCK_ESTABLISHED :
         if((size = getSn_RX_RSR(sn)) > 0) // Don't need to check SOCKERR_BUSY because it doesn't not occur.
         {
			   if(size > DATA_BUF_SIZE) size = DATA_BUF_SIZE;
			   ret = recv(sn, buf, size);

			   return ret;      // check SOCKERR_BUSY & SOCKERR_XXX. For showing the occurrence of SOCKERR_BUSY.
         }
         break;
      default:
         break;
   }
   return 0;
}
