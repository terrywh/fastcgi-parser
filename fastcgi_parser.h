#ifndef FASTCGI_PARSER_H
#define FASTCGI_PARSER_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdlib.h>
#include <ctype.h>

enum fastcgi_flags_t {
	// VERSION
	FASTCGI_VERSION            = 1,
	// TYPE
	FASTCGI_TYPE_BEGIN_REQUEST = 1,
	FASTCGI_TYPE_ABORT_REQUEST = 2,
	FASTCGI_TYPE_END_REQUEST   = 3,
	FASTCGI_TYPE_PARAMS        = 4,
	FASTCGI_TYPE_STDIN         = 5,
	FASTCGI_TYPE_STDOUT        = 6,
	FASTCGI_TYPE_STDERR        = 7,
	FASTCGI_TYPE_DATA          = 8,
	FASTCGI_TYPE_GET_VALUES    = 9,
	FASTCGI_TYPE_GET_VALUES_RESULT = 10,
	FASTCGI_TYPE_UNKNOWN       = 11,
	// FLAGS
	FASTCGI_FLAGS_KEEP_CONN    = 0x01,
	// ROLE
	FASTCGI_ROLE_RESPONDER     = 1,
	FASTCGI_ROLE_AUTHORIZER    = 2,
	FASTCGI_ROLE_FILTER        = 3,
};

typedef struct {
	// user data pointer
	void* data;
	// connection / request properties
	int   type;
	int   role;
	int   request_id;
	int   flag;
	// parser status / cache
	int   stat;
	int   clen;
	int   plen;
	int   klen;
	int   vlen;
} fastcgi_parser;

typedef int (*fastcgi_data_cb)(fastcgi_parser*, const char*, size_t);
typedef int (*fastcgi_notify_cb)(fastcgi_parser*);

typedef struct {
	fastcgi_notify_cb on_begin_request;
	fastcgi_data_cb   on_param_key;
	fastcgi_data_cb   on_param_val;
	fastcgi_notify_cb on_end_param; // called after each pair (param k/v)
	fastcgi_data_cb   on_data;
	fastcgi_notify_cb on_end_data;
	fastcgi_notify_cb on_end_request;
} fastcgi_parser_settings;

void fastcgi_parser_init(fastcgi_parser* parser, fastcgi_parser_settings* settings);
size_t fastcgi_parser_execute(fastcgi_parser* parser, fastcgi_parser_settings* settings, const char* data, size_t len);

#ifdef __cplusplus
} /* extern "C" { */
#endif

#endif // FASTCGI_PARSER_H
