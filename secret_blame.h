#ifndef PHP_SECRET_BLAME_HDR
#define PHP_SECRET_BLAME_HDR

#define PHP_SECRET_BLAME_VERSION "1.0"
#define PHP_SECRET_BLAME_EXTNAME "secret_blame"

extern PHP_MINIT_FUNCTION (secret_blame);
extern PHP_RINIT_FUNCTION (secret_blame);
extern PHP_MINFO_FUNCTION (secret_blame);

extern zend_module_entry secret_blame_module_entry;
#define phpext_secret_blame_ptr &secret_blame_module_entry

#endif /* PHP_SECRET_BLAME_HDR */