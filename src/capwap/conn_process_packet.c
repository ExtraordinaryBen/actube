/*
    This file is part of libcapwap.

    libcapwap is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    libcapwap is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with Foobar.  If not, see <http://www.gnu.org/licenses/>.

*/


#include <stdlib.h>
#include <string.h>

#include "capwap.h"
#include "cw_log.h"
#include "cw_util.h"

#include "conn.h"
#include "sock.h"

#include <stdio.h> //tube

static int cwrmsg_init_ctrlhdr(struct conn * conn, struct cwrmsg * cwrmsg, uint8_t * msg, int len)
{
	if (len<8){
		cw_dbg(DBG_CW_PKT_ERR,"Discarding packet from %s, len=%d (too short)",sock_addr2str(&conn->addr),len);
		return 0;
	}

	uint32_t val;

	/* first dword of header is the message type*/
	cwrmsg->type = ntohl(*((uint32_t*)(msg))); 

	/* second dword = seqnum, len and flags */
	val = ntohl(*((uint32_t*)(msg+4)));

	cwrmsg->seqnum = cw_get_dword_bits(val,0,8);

	cwrmsg->msgelems_len=cw_get_dword_bits(val,8,16)-3;

//	ch->flags=CW_GET_DWORD_BITS(val,24,8);
	cwrmsg->msgelems=msg+8;

	if (8+cwrmsg->msgelems_len != len){
		if (conn_is_strict_capwap(conn)){
			cw_dbg(DBG_CW_PKT_ERR,"Discarding packet from %s, msgelems len=%d, data len=%d, (strict capwap) ",
				sock_addr2str(&conn->addr),cwrmsg->msgelems_len,len-8);
			return 0;
		}
		if (8+cwrmsg->msgelems_len < len){
			cw_dbg(DBG_CW_RFC,"Packet from from %s has %d bytes extra data.",
				sock_addr2str(&conn->addr),len-8-cwrmsg->msgelems_len);
			cwrmsg->msgelems_len=len-8;
		}

		if (8+cwrmsg->msgelems_len > len){
			cw_dbg(DBG_CW_RFC,"Packet from from %s hass msgelems len of %d bytes but has only %d bytes of data, truncating.",
				sock_addr2str(&conn->addr),cwrmsg->msgelems_len,len-8);
		}
		return 1;
	}
	return 1;

}


static int process_message(struct conn * conn,uint8_t *rawmsg,int rawlen,int (*cb)(void*,uint8_t *,int),void *cbarg)
{
	uint8_t *msgptr = rawmsg+cw_get_hdr_msg_offset(rawmsg);


	uint32_t type = cw_get_msg_type(msgptr);

	if (!(type & 0x1)) {
		/* It's a response  message, no further examination required. */
		cb(cbarg,rawmsg,rawlen);
		return 0;
	}

	/* It's a request message, check if seqnum is right and if
	 * we have already sent a response message*/

	uint8_t seqnum = cw_get_msg_seqnum(msgptr);

	int s1=conn->last_seqnum_received;
	int s2=seqnum;
	int sd=s2-s1;

	if ((sd>0 && sd<128) || (sd<0 && sd<-128) || s1<0){
		/* seqnum is ok, normal message processing */
		conn->last_seqnum_received=seqnum;
		cb(cbarg,rawmsg,rawlen);
		return 0;
	}

	if (sd != 0)
	{
		cw_dbg(DBG_MSG_ERR,
			"Discarding message from %s, old seqnum, seqnum = %d, last seqnum=%d",
			sock_addr2str(&conn->addr),s2,s1);

		return 1;
	}

	/* the received request message was retransmitte by our peer,
	 * let's retransmit our response message if we have one*/

	cw_dbg(DBG_MSG_ERR,"Retransmitted request message from %s detected, seqnum=%d, type=%d",
		sock_addr2str(&conn->addr),s2,type);

	if (conn->resp_msg.type-1 != type ){
		cw_dbg(DBG_MSG_ERR,"No cached response for retransmission, request seqnum=%d,in cache=%d",s2,conn->resp_msg.type );
		return 0;
	}

	cw_dbg(DBG_MSG_ERR,"Retransmitting response message to %s, seqnum=%d",
		sock_addr2str(&conn->addr),s2);
	conn_send_cwmsg(conn,&conn->resp_msg);
	return 1;	
}


#ifdef WITH_CW_LOG_DEBUG

static void cw_dbg_packet(struct conn * conn, uint8_t * packet, int len)
{
	if (!cw_dbg_is_level(DBG_CW_PKT_IN))
		return;


	/* print the header */
	char hdr[200];
	hdr_print(hdr,packet,len);


	if (!cw_dbg_is_level(DBG_CW_PKT_DMP)){
		cw_dbg(DBG_CW_PKT_IN,"Processing capwap packet from %s, len=%d\n%s",sock_addr2str(&conn->addr),len,hdr);
		return;

	}

	cw_dbg_dmp(DBG_CW_PKT_DMP,packet,len,"Processing packet from %s, len=%d\n%s\n\tDump:",
			sock_addr2str(&conn->addr),len,hdr
		);


}

#else
	#define cw_dbg_packet(...)
#endif


int conn_process_packet(struct conn * conn, uint8_t *packet, int len,int (*cb)(void*,uint8_t *,int),void *cbarg)
{

	if (len<8){
		/* packet too short */
		cw_dbg(DBG_CW_PKT_ERR,"Discarding packet from %s, packet too short, len=%d",sock_addr2str(&conn->addr),len);
		return 0;
	}

	int preamble = cw_get_hdr_preamble(packet);

	if ( (preamble & 0xf0) != CW_VERSION){
		/* wrong version */
		cw_dbg(DBG_CW_PKT_ERR,"Discarding packet from %s, wrong version, version=%d",sock_addr2str(&conn->addr),(preamble&0xf0)>>8);
		return 0;
	}

	if (preamble & 0xf ) {
		/* decode dtls */
		return 0;
	}

	/* log this packet */
	cw_dbg_packet(conn,packet,len);


	int offs = cw_get_hdr_msg_offset(packet); 
	

	int payloadlen = len - offs;
	if (payloadlen<0){
		cw_dbg(DBG_CW_PKT_ERR,"Discarding packet from %s, header length greater than len, hlen=%d",sock_addr2str(&conn->addr),offs);
		/* EINVAL */
		return 0;
	}

/*
	struct cwrmsg cwrmsg;
	cwrmsg.wbid=(val>>9) & 0x1f;
	cwrmsg.rid=(val>>14) & 0x1f;
*/

//printf ("Offs is %d RML is %d\n",offs,cw_get_hdr_rmac_len(packet));

	/* Check Radio MAC if preset */
	if (cw_get_hdr_flag_m(packet)){
		
		if (cw_get_hdr_rmac_len(packet)+8>offs){
			/* wrong rmac size */
			cw_dbg(DBG_CW_PKT_ERR,"Discarding packet, wrong R-MAC size, size=%d",*(packet+8));
			return 0;
		}
//		memcpy(cwrmsg.rmac, packet+8,8);
	}
//	else{
//		cwrmsg.rmac[0]=0;
//	}


	if (cw_get_hdr_flag_f(packet)){	/* fragmented */
		uint8_t * f;
		f = fragman_add(conn->fragman, packet,offs,payloadlen);
		if (f==NULL)
			return 0;

		cw_dbg_packet(conn,f+4,*(uint32_t*)f);

	
	//	extern int cw_process_msg(struct conn * conn,uint8_t*msg,int len);
	//	cw_process_msg(conn,f+4,*(uint32_t*)f);

//printf("Received a fragmented packetm should process it");
//exit(0);

/*
		if (!cwrmsg_init_ctrlhdr(conn,&cwrmsg,f+4,*(uint32_t*)f)){
			free(f);
			return;
		};
*/
		process_message(conn,f+4,*(uint32_t*)f,cb,cbarg); 

		free (f);
		return 1;
	}

//extern int cw_process_msg(struct conn * conn,uint8_t*msg,int len);
//cw_process_msg(conn,packet,len);


	//if (!cwrmsg_init_ctrlhdr(conn,&cwrmsg,packet+hlen,len-hlen) ){
	//	cw_dbg(DBG_CW_PKT_ERR,"Discarding packet from %s, len=%d (too short)",sock_addr2str(&conn->addr));
	//	return;
	//}

//msg_4*((val >> 19) & 0x1f);
	process_message(conn,packet,len,cb,cbarg); 
	return 1;
}


/**
 * Used as main message loop
 */ 
int cw_read_messages(struct conn *conn)
{
        uint8_t buf[2024];
        int len = 2024;

        int n = conn->read(conn, buf, len);
	if (n<0 ) 
		return n;
	
        if (n > 0)
                conn_process_packet(conn, buf, n, cw_process_msg, conn);

	
}


