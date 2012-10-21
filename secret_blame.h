#ifndef PHP_SECRET_BLAME_HDR
#define PHP_SECRET_BLAME_HDR

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#include "php.h"

#define PHP_SECRET_BLAME_EXTNAME "secret_blame"

extern PHP_MINIT_FUNCTION(secret_blame);
extern PHP_MSHUTDOWN_FUNCTION(secret_blame);
extern PHP_RINIT_FUNCTION(secret_blame);

extern zend_module_entry secret_blame_module_entry;
#define phpext_secret_blame_ptr &secret_blame_module_entry

ZEND_BEGIN_MODULE_GLOBALS(secret_blame)
    zend_bool done_replacement;
	void (*orig_extension_loaded)(INTERNAL_FUNCTION_PARAMETERS);
	void (*orig_get_loaded_extensions)(INTERNAL_FUNCTION_PARAMETERS);
ZEND_END_MODULE_GLOBALS(secret_blame)

#ifdef ZTS
#define SECRET_BLAME_G(v) TSRMG(secret_blame_globals_id, zend_secret_blame_globals *, v)
#else
#define SECRET_BLAME_G(v) (secret_blame_globals.v)
#endif

#endif /* PHP_SECRET_BLAME_HDR */