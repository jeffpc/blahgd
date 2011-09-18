#include <stdlib.h>

#include <xmlrpc-c/base.h>
#include <xmlrpc-c/server.h>
#include <xmlrpc-c/server_cgi.h>

static xmlrpc_value *ping(xmlrpc_env * const envP,
			  xmlrpc_value * const paramArrayP,
			  void * const serverInfo,
			  void * const channelInfo)
{
	char *src, *tgt;

	/* Parse our argument array. */
	xmlrpc_decompose_value(envP, paramArrayP, "(ss)", &src, &tgt);
	if (envP->fault_occurred)
		return NULL;

	/* we got a pingback for 'tgt' from 'src' */
	/*
	author="XXX";
	email="XXX";
	post_time=<get current time>
	remote_addr=<get the IP>
	url=src

	make a comment
	*/

	free(src);
	free(tgt);

	return xmlrpc_build_value(envP, "s", "");
}

int blahg_pingback()
{
	xmlrpc_registry * registryP;
	xmlrpc_env env;

	xmlrpc_env_init(&env);

	registryP = xmlrpc_registry_new(&env);

	xmlrpc_registry_add_method(&env, registryP, NULL, "pingback.ping",
				   &ping, NULL);

	xmlrpc_server_cgi_process_call(registryP);

	return 0;
}
