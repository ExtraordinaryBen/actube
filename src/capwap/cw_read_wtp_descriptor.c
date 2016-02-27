#include "cw.h"

#include "capwap.h"
#include "capwap_items.h"
#include "dbg.h"


int cw_read_wtp_descriptor(mbag_t mbag, struct conn *conn,
			   struct cw_action_in *a, uint8_t * data, int len)
{


	mbag_set_byte(mbag, CW_ITEM_WTP_MAX_RADIOS, cw_get_byte(data));
	mbag_set_byte(mbag, CW_ITEM_WTP_RADIOS_IN_USE, cw_get_byte(data + 1));


	/* Get number of encryption elements */
	int ncrypt = cw_get_byte(data + 2);
	if (ncrypt == 0) {
		if (conn->strict_capwap) {
			cw_dbg(DBG_ELEM_ERR,
			       "Bad WTP Descriptor, number of encryption elements is 0.");
			return 0;
		}
		cw_dbg(DBG_RFC,
		       "Non standard conform WTP Descriptor, number of encryptoin elements is 0.");
	}

	int pos = 3;
	int i;
	for (i = 0; i < ncrypt; i++) {
		// It's a dummy for now
		pos += 3;
	}

	return cw_read_wtp_descriptor_versions(mbag, data + pos, len - pos);

}
