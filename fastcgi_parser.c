#include "fastcgi_parser.h"

enum STATUS_t {
	// 1. RECORD
	STATUS_HEAD_VERSION,
	STATUS_HEAD_TYPE,
	STATUS_HEAD_REQUEST_ID_1,
	STATUS_HEAD_REQUEST_ID_2,
	STATUS_HEAD_CONTENT_LEN_1,
	STATUS_HEAD_CONTENT_LEN_2,
	STATUS_HEAD_PADDING_LEN,
	STATUS_HEAD_RESERVED,
	// 2. BEGIN_REQUEST
	STATUS_BODY_1_ROLE_1,
	STATUS_BODY_1_ROLE_2,
	STATUS_BODY_1_FLAGS,
	STATUS_BODY_1_RESERVED_1,
	STATUS_BODY_1_RESERVED_2,
	STATUS_BODY_1_RESERVED_3,
	STATUS_BODY_1_RESERVED_4,
	STATUS_BODY_1_RESERVED_5,
	// 3. PARAMS
	STATUS_BODY_2_KEY_LEN_1,
	STATUS_BODY_2_KEY_LEN_2,
	STATUS_BODY_2_KEY_LEN_3,
	STATUS_BODY_2_KEY_LEN_4,
	STATUS_BODY_2_KEY_DATA,
	STATUS_BODY_2_VAL_LEN_1,
	STATUS_BODY_2_VAL_LEN_2,
	STATUS_BODY_2_VAL_LEN_3,
	STATUS_BODY_2_VAL_LEN_4,
	STATUS_BODY_2_VAL_DATA,
	// 4. STDIN
	STATUS_BODY_3_DATA,
	// 5. RECORD
	STATUS_TAIL_PADDING,
};


#define EMIT_DATA_CB(FOR, ptr, len)                                 \
do {                                                                \
	if (settings->on_##FOR) {                                       \
		if (settings->on_##FOR(parser, ptr, len) != 0) {            \
			return i;                                               \
		}                                                           \
	}                                                               \
} while (0)

#define EMIT_NOTIFY_CB(FOR)                                         \
do {                                                                \
	if (settings->on_##FOR) {                                       \
		if (settings->on_##FOR(parser) != 0) {                      \
			return i;                                               \
		}                                                           \
	}                                                               \
} while (0)

#define ENDING_STATUS() if(parser->plen == 0) {                     \
	parser->stat = STATUS_HEAD_VERSION;                             \
}else{                                                              \
	parser->stat = STATUS_TAIL_PADDING;                             \
}

void fastcgi_parser_init(fastcgi_parser* parser, fastcgi_parser_settings* settings) {
	parser->stat = 0;
}

size_t fastcgi_parser_execute(fastcgi_parser* parser, fastcgi_parser_settings* settings, const char* data, size_t size) {
	size_t i = 0, mark = 0;
	while(i<size) {
		char c = data[i];
		int  last = (i == size - 1);
		switch(parser->stat) {
		case STATUS_HEAD_VERSION:
			if(c != FASTCGI_VERSION) {
				goto CONTINUE_STOP;
			}
			parser->stat = STATUS_HEAD_TYPE;
			break;
		case STATUS_HEAD_TYPE:
			if(c >= FASTCGI_TYPE_UNKNOWN) {
				goto CONTINUE_STOP;
			}
			parser->type   = c;
			parser->stat = STATUS_HEAD_REQUEST_ID_1;
			break;
		case STATUS_HEAD_REQUEST_ID_1:
			parser->request_id = c;
			parser->stat = STATUS_HEAD_REQUEST_ID_2;
			break;
		case STATUS_HEAD_REQUEST_ID_2:
			parser->request_id += c << 8;
			parser->stat = STATUS_HEAD_CONTENT_LEN_1;
			break;
		case STATUS_HEAD_CONTENT_LEN_1:
			parser->clen = c << 8;
			parser->stat = STATUS_HEAD_CONTENT_LEN_2;
			break;
		case STATUS_HEAD_CONTENT_LEN_2:
			parser->clen += c & 0xff;
			parser->stat = STATUS_HEAD_PADDING_LEN;
			break;
		case STATUS_HEAD_PADDING_LEN:
			parser->plen = c;
			parser->stat = STATUS_HEAD_RESERVED;
			break;
		case STATUS_HEAD_RESERVED:
			if(parser->clen > 0) {
				switch(parser->type) {
				case FASTCGI_TYPE_BEGIN_REQUEST:
					parser->stat = STATUS_BODY_1_ROLE_1;
				break;
				case FASTCGI_TYPE_PARAMS:
					parser->stat = STATUS_BODY_2_KEY_LEN_1;
				break;
				case FASTCGI_TYPE_STDIN:
					parser->stat = STATUS_BODY_3_DATA;
					mark = i;
				break;
				default:
					// 暂不支持其他类型的解析
					assert(0);
				}
			}else{
				switch(parser->type) {
				case FASTCGI_TYPE_PARAMS:
					EMIT_NOTIFY_CB(after_all_param);
					break;
				case FASTCGI_TYPE_STDIN:
					EMIT_NOTIFY_CB(after_data);
					break;
				default:
					// 暂不支持其他类型的解析
					assert(0);
				}
			}
			parser->stat = STATUS_HEAD_VERSION;
			break;
		case STATUS_BODY_1_ROLE_1:
			--parser->clen;
			parser->role   = c << 8;
			parser->stat = STATUS_BODY_1_ROLE_2;
			break;
		case STATUS_BODY_1_ROLE_2:
			--parser->clen;
			parser->role += c & 0xff;
			if(parser->role != FASTCGI_ROLE_RESPONDER) {
				// 暂不支持其他类型的解析
				assert(0);
			}
			parser->stat = STATUS_BODY_1_FLAGS;
			break;
		case STATUS_BODY_1_FLAGS:
			--parser->clen;
			parser->flag = c;
			parser->stat = STATUS_BODY_1_RESERVED_1;
			break;
		case STATUS_BODY_1_RESERVED_1:
		case STATUS_BODY_1_RESERVED_2:
		case STATUS_BODY_1_RESERVED_3:
		case STATUS_BODY_1_RESERVED_4:
			--parser->clen;
			++parser->stat;
			break;
		case STATUS_BODY_1_RESERVED_5:
			--parser->clen;
			EMIT_NOTIFY_CB(begin_request);
			ENDING_STATUS();
			break;
		case STATUS_BODY_2_KEY_LEN_1:
			--parser->clen;
			if(c & 0x80) {
				parser->klen = (c & 0x7f) << 24;
				++parser->stat;
			}else{
				parser->klen = c;
				parser->stat = STATUS_BODY_2_VAL_LEN_1;
			}
			break;
		case STATUS_BODY_2_KEY_LEN_2:
			--parser->clen;
			parser->klen += (c & 0xff) << 16;
			++parser->stat;
			break;
		case STATUS_BODY_2_KEY_LEN_3:
			--parser->clen;
			parser->klen += (c & 0xff) << 8;
			++parser->stat;
			break;
		case STATUS_BODY_2_KEY_LEN_4:
			--parser->clen;
			parser->klen += (c & 0xff);
			parser->stat = STATUS_BODY_2_VAL_LEN_1;
			break;
		case STATUS_BODY_2_VAL_LEN_1:
			--parser->clen;
			if(c & 0x80) {
				parser->vlen = (c & 0x7f) << 24;
				parser->stat = STATUS_BODY_2_VAL_LEN_2;
			}else{
				parser->vlen = c;
				parser->stat = STATUS_BODY_2_KEY_DATA;
				mark = i;
			}
			break;
		case STATUS_BODY_2_VAL_LEN_2:
			--parser->clen;
			parser->vlen += (c & 0xff) << 16;
			++parser->stat;
			break;
		case STATUS_BODY_2_VAL_LEN_3:
			--parser->clen;
			parser->vlen += (c & 0xff) << 8;
			++parser->stat;
			break;
		case STATUS_BODY_2_VAL_LEN_4:
			--parser->clen;
			parser->vlen += (c & 0xff);
			parser->stat = STATUS_BODY_2_KEY_DATA;
			mark = i;
			break;
		case STATUS_BODY_2_KEY_DATA:
			--parser->clen;
			if(--parser->klen == 0) {
				EMIT_DATA_CB(param_key, data + mark, i - mark);
				if(parser->clen == 0) {
					ENDING_STATUS();
				}else if(parser->vlen > 0) {
					parser->stat = STATUS_BODY_2_VAL_DATA;
					mark = i;
				}else{
					EMIT_NOTIFY_CB(after_param);
					parser->stat = STATUS_BODY_2_KEY_LEN_1;
				}
			}
			if(last) {
				EMIT_DATA_CB(param_key, data + mark, i - mark);
			}
			break;
		case STATUS_BODY_2_VAL_DATA:
			--parser->clen;
			if(--parser->vlen == 0) {
				EMIT_DATA_CB(param_val, data + mark, i - mark);
				EMIT_NOTIFY_CB(after_param);
				if(parser->clen == 0) {
					ENDING_STATUS();
				}else{
					parser->stat = STATUS_BODY_2_KEY_LEN_1;
				}
			}
			if(last) {
				EMIT_DATA_CB(param_val, data + mark, i - mark);
			}
			break;
		case STATUS_BODY_3_DATA:
			if(--parser->clen == 0) {
				EMIT_DATA_CB(data, data + mark, i - mark);
				ENDING_STATUS();
			}
			if(last) {
				EMIT_DATA_CB(data, data + mark, i - mark);
			}
			break;
		case STATUS_TAIL_PADDING:
			if(--parser->plen == 0) {
				parser->stat = STATUS_HEAD_VERSION;
			}
			break;
		}
CONTINUE_NEXT:
		++i;
		continue;
CONTINUE_STOP:
		break;
	}
	return i;
}
