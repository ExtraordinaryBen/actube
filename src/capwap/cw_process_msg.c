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

/**
 * @file 
 * @brief Processing of incomming messaages.
 */

#include <stdint.h>
#include <stdio.h>

#include "conn.h"
#include "capwap.h"
#include "capwap_items.h"

#include "sock.h"
#include "action.h"
#include "cw_log.h"
#include "dbg.h"



int conn_send_msg(struct conn *conn, uint8_t * rawmsg);




/**
 * Init response message header
 */
void cw_init_response(struct conn *conn, uint8_t * req)
{
	uint8_t *buffer = conn->resp_buffer;
	int shbytes = cw_get_hdr_msg_offset(req);
	int dhbytes;
	memcpy(buffer, req, shbytes);
	cw_set_hdr_hlen(buffer, 2);
	cw_set_hdr_flags(buffer, CW_FLAG_HDR_M, 1);
	dhbytes = cw_get_hdr_msg_offset(buffer);

	uint8_t *msgptr = req + shbytes;
	uint8_t *dmsgptr = buffer + dhbytes;

	cw_set_msg_type(dmsgptr, cw_get_msg_type(msgptr) + 1);
	cw_set_msg_seqnum(dmsgptr, cw_get_msg_seqnum(msgptr));
	cw_set_msg_flags(dmsgptr, 0);
}

void cw_init_request(struct conn *conn, int msg_id)
{
	uint8_t *buffer = conn->req_buffer;

	cw_put_dword(buffer + 0, 0);
	cw_put_dword(buffer + 4, 0);
	cw_set_hdr_preamble(buffer, 0);
	cw_set_hdr_hlen(buffer, 2);
	cw_set_hdr_wbid(buffer, 1);
	cw_set_hdr_rid(buffer, 0);
	uint8_t *msgptr = cw_get_hdr_msg_offset(buffer) + buffer;
	cw_set_msg_type(msgptr, msg_id);
	cw_set_msg_flags(msgptr, 0);
	cw_set_msg_elems_len(msgptr, 0);


}

/**
 * send a response 
 */
int cw_send_response(struct conn *conn, uint8_t * rawmsg, int len)
{
	cw_init_response(conn, rawmsg);
	if (cw_put_msg(conn, conn->resp_buffer) == -1)
		return 0;
	conn_send_msg(conn, conn->resp_buffer);
	return 1;
}




/**
 * Special case error message, which is sent when an unexpected messages 
 * was received or somethin else happened.
 * @param conn conection
 * @param rawmsg the received request message, which the response belongs to
 * @pqram result_code result code to send
 * @return 1
 */
int cw_send_error_response(struct conn *conn, uint8_t * rawmsg, uint32_t result_code)
{
	cw_init_response(conn, rawmsg);

	uint8_t *out = conn->resp_buffer;

	uint8_t *dst = cw_get_hdr_msg_elems_ptr(out);
	int l = cw_put_elem_result_code(dst, result_code);

	cw_set_msg_elems_len(out + cw_get_hdr_msg_offset(out), l);

	conn_send_msg(conn, conn->resp_buffer);

	return 1;
}


int cw_process_msg(struct conn *conn, uint8_t * rawmsg, int len)
{
	struct cw_action_in as, *af, *afm;

	uint8_t *msg_ptr = rawmsg + cw_get_hdr_msg_offset(rawmsg);

	int elems_len = cw_get_msg_elems_len(msg_ptr);
/*
	if (8+elems_len != len){
		cw_dbg(DBG_MSG_ERR,"Discarding message from %s, msgelems len=%d, data len=%d, (strict capwap) ",
			sock_addr2str(&conn->addr),elems_len,len-8);

		if (conn_is_strict_capwap(conn)){
			cw_dbg(DBG_MSG_ERR,"Discarding message from %s, msgelems len=%d, data len=%d, (strict capwap) ",
				sock_addr2str(&conn->addr),elems_len,len-8);
			return 0;
		}
		if (8+elems_len < len){
			cw_dbg(DBG_CW_RFC,"Packet from from %s has %d bytes extra data.",
				sock_addr2str(&conn->addr),len-8-elems_len);
			elems_len=len-8;
		}

		if (8+elems_len > len){
			cw_dbg(DBG_CW_RFC,"Packet from from %s hass msgelems len of %d bytes but has only %d bytes of data, truncating.",
				sock_addr2str(&conn->addr),elems_len,len-8);
		}
		return 1;
	}

*/


	/* prepare struct for search operation */
	as.capwap_state = conn->capwap_state;
	as.msg_id = cw_get_msg_id(msg_ptr);
	as.vendor_id = 0;
	as.elem_id = 0;
	as.proto = 0;


	/* Search for state/message combination */
	afm = cw_actionlist_in_get(conn->actions->in, &as);

	if (!afm) {
		/* Throw away unexpected response messages */
		if (!(as.msg_id & 1)) {
			cw_dbg(DBG_MSG_ERR,
			       "Message type %d (%s) unexpected/illigal in %s State, discarding.",
			       as.msg_id, cw_strmsg(as.msg_id),
			       cw_strstate(conn->capwap_state));
			return 0;
		}

		/* Request message not found in current state, check if we know 
		   anything else about this message type */
		const char *str = cw_strheap_get(conn->actions->strmsg, as.msg_id);
		int result_code = 0;
		if (str) {
			/* Message found, but it was in wrong state */
			cw_dbg(DBG_MSG_ERR,
			       "Message type %d (%s) not allowed in %s State.", as.msg_id,
			       cw_strmsg(as.msg_id), cw_strstate(as.capwap_state));
			result_code = CW_RESULT_MSG_INVALID_IN_CURRENT_STATE;
		} else {
			/* Message is unknown */
			cw_dbg(DBG_MSG_ERR, "Message type %d (%s) unknown.",
			       as.msg_id, cw_strmsg(as.msg_id),
			       cw_strstate(as.capwap_state));
			result_code = CW_RESULT_MSG_UNRECOGNIZED;

		}
		cw_send_error_response(conn, rawmsg, result_code);
		return 0;
	}


	/* Execute start processor for message */
	if (afm->start) {
		afm->start(conn, afm, rawmsg, len);
	}

	uint8_t *elems_ptr = cw_get_msg_elems_ptr(msg_ptr);
	uint8_t *elem;

	/* avltree to bag the found mandatory elements */
	conn->mand = intavltree_create();

	/* iterate through message elements */
	cw_foreach_elem(elem, elems_ptr, elems_len) {

		as.elem_id = cw_get_elem_id(elem);
		int elem_len = cw_get_elem_len(elem);

		cw_dbg_elem(conn, as.msg_id, as.elem_id, cw_get_elem_data(elem),
			    elem_len);


		af = cw_actionlist_in_get(conn->actions->in, &as);

		if (!af) {
			cw_dbg(DBG_ELEM_ERR, "Element %d not allowed in msg %d (%s)",
			       as.elem_id, as.msg_id, cw_strmsg(as.msg_id));
			continue;
		}

		if (af->mand) {
			/* add found mandatory message element 
			   to mand list */
			intavltree_add(conn->mand, af->item_id);
		}

		if (af->start) {
			af->start(conn, af, cw_get_elem_data(elem), elem_len);
		}

	}

	int result_code = 0;
	if (afm->end) {
		result_code = afm->end(conn, afm, rawmsg, len);
	}

	/* if we've got a request message, we have to send a response message */
	if (as.msg_id & 1) {
		if (result_code > 0) {
			/* the end method gave us an result code, so
			   send an error message */
			cw_send_error_response(conn, rawmsg, result_code);
		} else {
			/* regular response message */
			cw_send_response(conn, rawmsg, len);
		}
	}

	intavltree_destroy(conn->mand);

	return 0;

}