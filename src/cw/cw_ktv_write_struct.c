#include "ktv.h"
#include "dbg.h"
#include "log.h"

int cw_ktv_write_struct(mavl_t ktv, const cw_KTVStruct_t * stru, const char *pkey, 
	uint8_t * dst)
{
	char key[CW_KTV_MAX_KEY_LEN];
	int pos, i;
	cw_KTV_t * result;
	
	pos=0; i=0;
	for(i=0; stru[i].type != NULL;i++){
	
		if (stru[i].position!=-1){
			pos=stru[i].position;
		}
		memset(dst+pos,0,stru[i].len);
		
		sprintf(key,"%s/%s",pkey,stru[i].key);
		result = cw_ktv_get(ktv,key,stru[i].type);
		
		if (result == NULL){
			cw_log(LOG_ERR,"Can't put %s, no value found, filling zero.",key);
			memset(dst+pos,0,stru[i].len);
		}
		else{
			result->type->put(result,dst+pos);
		}

		pos+=stru[i].len;

	}

	return pos;
}