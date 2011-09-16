#include <xmlrpc-c/base.h>
#include <xmlrpc-c/server.h>
#include <xmlrpc-c/server_cgi.h>

static xmlrpc_value *ping(xmlrpc_env * const envP,
			  xmlrpc_value * const paramArrayP,
			  void * const serverInfo,
			  void * const channelInfo)
{
	xmlrpc_int32 x, y, z;

	/* Parse our argument array. */
	xmlrpc_decompose_value(envP, paramArrayP, "(ii)", &x, &y);
	if (envP->fault_occurred)
		return NULL;

	/* Add our two numbers. */
	z = x + y;

	/* Return our result. */
	return xmlrpc_build_value(envP, "i", z);
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
