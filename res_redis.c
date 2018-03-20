/*
 * Asterisk -- An open source telephony toolkit.
 *
 * Copyright (C) 2018, Rodrigo Azevedo
 *
 * Rodrigo Azevedo <rodrigowbazevedo@yahoo.com.br>
 *
 * See http://www.asterisk.org for more information about
 * the Asterisk project. Please do not directly contact
 * any of the maintainers of this project for assistance;
 * the project provides a web site, mailing lists and IRC
 * channels for your use.
 *
 * This program is free software, distributed under the terms of
 * the MIT License. See the LICENSE file at the top of the source tree.
 *
 * Please follow coding guidelines
 * http://svn.digium.com/view/asterisk/trunk/doc/CODING-GUIDELINES
 */

/*! \file
 *
 * \brief REDIS() redis get/set value for key
 *
 * \author\verbatim Rodrigo Azevedo <rodrigowbazevedo@yahoo.com.br> \rodrigowbazevedo
 *
 * \ingroup applications
 */

/*** MODULEINFO
	<defaultenabled>yes</defaultenabled>
	<depend>hiredis</depend>
	<support_level>core</support_level>
***/
#include "asterisk.h"

ASTERISK_FILE_VERSION(__FILE__, "$Revision: 200656 $")

#include "asterisk/file.h"
#include "asterisk/channel.h"
#include "asterisk/pbx.h"
#include "asterisk/module.h"
#include "asterisk/app.h"
#include "asterisk/utils.h"

#include <stdlib.h>
#include <string.h>

/* Embedded hiredis */
#include "<libhiredis/hiredis.h>"


/*** DOCUMENTATION
	<function name="REDIS" language="en_US">
		<synopsis>
			gets or sets the value for a key in the cache store
		</synopsis>
		<syntax>
			<parameter name="key" required="true">
				<para>key to be looked up, or set</para>
			</parameter>
		</syntax>
		<description>
			<para>gets or sets the value for a key in the cache store. when used in write mode,
			the function invokes the set Redis command.</para>
		</description>
	</function>
 ***/

/*
STANDARD CONFIGURATION FILE (/etc/asterisk/res_redis.conf)
=========================================================
[general]
ttl=0                                 ; sets the default time-to-live, in seconds, for the key-value entries added
                                      ;   or modified in the cache store. default value is 0, which means the entries
                                      ;   will persist forever.
;keyprefix=                           ; whatever string you specify here is prepended to each key that is retrieved
                                      ;   or stored, so that you can create some sort of a "domain" for your
                                      ;   asterisk server
server=localhost                      ; Server can be a fqdn or an ip address, if no entries, the module will at
;server=redis.server.com              ;   least attempt to connect to a redis running on the localhost
;port=6379                            ; The default redis port is 6379.

UNIT TESTING (using a dialplan macro)
=====================================
[macro-mcdtest]
exten => s,1,noop(>>>> performing Redis tests)
exten => s,n,answer()
exten => s,n,set(REDIS(wrtest)=hello)
exten => s,n,noop(>>>> test 1 (write / read): '${REDIS(wrtest)}' == 'hello')
exten => s,n,hangup()
*/


#define CONFIG_FILE_NAME          "res_redis.conf"
#define REDIS_MAX_KEY             512

// redis properties
redisContext *redis;
char keyprefix[65] = "";
static unsigned int redisttl;

/*
  // returned errors in the REDISRESULT variable:
  REDIS_OK = 0,
  REDIS_ERR = -1,
  REDIS_ERR_IO = 1,       // Error in read or write
  REDIS_ERR_EOF = 3,      // End of file
  REDIS_ERR_PROTOCOL = 4, // Protocol error
  REDIS_ERR_OOM = 5,      // Out of memory
  REDIS_ERR_OTHER = 2,    // Everything else...
  // leaving room for expansion to future redis error codes
  // the rest of them are numbers we generate
*/

#define REDIS_ARGUMENT_NEEDED      127

static void redis_set_operation_result(struct ast_channel *chan, int result) {
	char *numresult = NULL;
	ast_asprintf(&numresult, "%d", result);
	pbx_builtin_setvar_helper(chan, "REDISRESULT", numresult);
}

static int redis_load_config(void) {

	struct ast_config *cfg;
	struct ast_flags config_flags = { 0 };

	if (!(cfg = ast_config_load(CONFIG_FILE_NAME, config_flags))) {
		ast_log(LOG_ERROR, "missing res_redis resource config file '%s'\n", CONFIG_FILE_NAME);
		return 1;
	} else if (cfg == CONFIG_STATUS_FILEINVALID) {
		ast_log(LOG_ERROR, "res_redis resource config file '" CONFIG_FILE_NAME "' invalid format.\n");
		return 1;
	}


	char server[128] = "127.0.0.1";
	const char *servervalue;
	if ((servervalue = ast_variable_retrieve(cfg, "general", "server"))){
		sprintf(server, "%s", servervalue);
	}

	int port = 6379;
	const char *portvalue;
	if ((portvalue = ast_variable_retrieve(cfg, "general", "ttl"))){
		port = atoi(portvalue);
	}

	ast_log(LOG_DEBUG, "res_redis configured server: '%s:%d'\n", server, port);

	redisttl = 0;
	const char *ttlvalue;
	if ((ttlvalue = ast_variable_retrieve(cfg, "general", "ttl")))
		redisttl = atoi(ttlvalue);
	ast_log(LOG_DEBUG, "default time to live for key-value entries set to %d seconds\n", redisttl);

	const char *kp;
	if ((kp = ast_variable_retrieve(cfg, "general", "keyprefix"))) {
		sprintf(keyprefix, "%s", kp);
	}

	// launch redis client
	redis = redisConnect(server, port);
	if(redis == NULL){
		ast_log("Can't allocate redis context\n");
		return 1;
	}

	if(redis->err){
		ast_log(LOG_ERROR, "res_redis failed to start \nError %d: %s\n", redis->err, redis->errstr);
		return 1;
	}

	ast_config_destroy(cfg);
	return 0;

}

static int redis_read(struct ast_channel *chan,
	const char *cmd, char *parse, char *buffer, size_t buflen
) {
	char *key = (char *)ast_malloc(REDIS_MAX_KEY);
	buffer[0] = 0;

	if (ast_strlen_zero(parse)) {
		ast_log(LOG_WARNING, "REDIS() requires argument (key)\n");
		redis_set_operation_result(chan, REDIS_ARGUMENT_NEEDED);
		free(key);
		return 0;
	}
	sprintf(key, "%s%s", keyprefix, parse);

	redisReply *reply;
	reply = redisCommand(c,"GET %s", key);
	if(redis->err){
		ast_log(LOG_ERROR, "REDIS() error %d: %s\n", redis->err, redis->errstr);
		redis_set_operation_result(chan, redis->err);
	}else{
		ast_copy_string(buffer, reply->str, buflen);
		redis_set_operation_result(chan, REDIS_OK);
	}
	freeReplyObject(reply);

	free(key);
	return 0;
}

static int redis_write(
	struct ast_channel *chan, const char *cmd, char *parse, const char *value
) {

	redis_set_operation_result(chan, REDIS_OK);

	char *key = (char *)ast_malloc(REDIS_MAX_KEY);

	if (ast_strlen_zero(parse)) {
		ast_log(LOG_WARNING, "REDIS() requires argument (key)\n");
		redis_set_operation_result(chan, REDIS_ARGUMENT_NEEDED);
		free(key);
		return 0;
	}
	sprintf(key, "%s%s", keyprefix, parse);
	ast_log(LOG_DEBUG, "setting value for key: %s=%s timeout: %d\n", key, value, redisttl);


	redisReply *reply;
	if(redisttl > 0){
		reply = redisCommand(c,"SET %s %s EX %u", key, value, redisttl);
	}else{
		reply = redisCommand(c,"SET %s %s", key, value);
	}
 
	if(redis->err){
		ast_log(LOG_ERROR, "REDIS() error %d: %s\n", redis->err, redis->errstr);
		redis_set_operation_result(chan, redis->err);
	}
	freeReplyObject(reply);

	free(key);
	return 0;
}

static struct ast_custom_function acf_redis = {
	.name = "REDIS",
	.read = redis_read,
	.write = redis_write
};

static int load_module(void) {
	int ret = 0;
	ret = redis_load_config();
	ret |= ast_custom_function_register(&acf_redis);
	return ret;
}

static int unload_module(void) {
	redisFree(redis);
	int ret = 0;
	ret |= ast_custom_function_unregister(&acf_redis);
	return ret;
}

AST_MODULE_INFO_STANDARD(ASTERISK_GPL_KEY, "Redis access functions");