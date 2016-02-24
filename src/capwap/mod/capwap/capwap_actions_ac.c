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


#include "capwap/capwap.h"
#include "capwap/action.h"
#include "capwap/capwap_items.h"
#include "capwap_actions.h"
#include "capwap/strheap.h"
#include "capwap/radio.h"



cw_action_in_t _capwap_actions_ac_in[] = {


	/* ------------------------------------------------------------------------------- */
	/* Message Discovery Request */
	{
		.capwap_state = CW_STATE_DISCOVERY,
		.msg_id = CW_MSG_DISCOVERY_REQUEST,
		.end = cw_in_check_disc_req
	}
	,

	/* Element Discovery Type */
	{
		.capwap_state = CW_STATE_DISCOVERY, 
		.msg_id = CW_MSG_DISCOVERY_REQUEST, 
		.elem_id = CW_ELEM_DISCOVERY_TYPE,
	 	.start = cw_in_generic2, 
		.item_id = "discovery_type", 
		.mand = 1, 
		.min_len = 1, 
		.max_len = 1
	}
	,

	/* Element WTP Name */
	{
		.capwap_state = CW_STATE_DISCOVERY, 
		.msg_id = CW_MSG_DISCOVERY_REQUEST, 
		.elem_id = CW_ELEM_WTP_NAME,
	 	.start = cw_in_generic2, 
		.item_id = "wtp_name", 
		.mand = 1, 
		.min_len = 1, 
		.max_len = 1024
	}
	,

	/* End of list */
	{0, 0}
};


#include "capwap/item.h"

static struct cw_itemdef _capwap_itemdefs[] = {

	{"wtp_name",CW_ITEM_NONE,MBAG_STR},
	{"wtp_mac_type",CW_ITEM_NONE,MBAG_BYTE},
	{"discovery_type",CW_ITEM_NONE,MBAG_BYTE},
	{CW_ITEM_NONE}

};


int capwap_register_actions_ac(struct cw_actiondef *def)
{
	def->in = cw_actionlist_in_create();
	def->out = cw_actionlist_out_create();
	def->strmsg = cw_strheap_create();
	def->strelem = cw_strheap_create();
	def->wbids = intavltree_create();
	def->items = cw_itemdefheap_create();
	def->radioitems = cw_itemdefheap_create();

	int rc;
	rc = cw_actionlist_in_register_actions(def->in, _capwap_actions_ac_in);
//	rc += cw_actionlist_out_register_actions(def->out, capwap_actions_ac_out);

	rc += cw_strheap_register_strings(def->strmsg, capwap_strings_msg);
	rc += cw_strheap_register_strings(def->strelem, capwap_strings_elem);

	rc += cw_itemdefheap_register(def->items, _capwap_itemdefs);
	rc += cw_itemdefheap_register(def->radioitems, capwap_radioitemdefs);

	intavltree_add(def->wbids, 0);



	return rc;
}
