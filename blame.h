#ifndef PHP_BLAME_HDR
#define PHP_BLAME_HDR

#define PHP_BLAME_VERSION "1.0"
#define PHP_BLAME_EXTNAME "blame"

extern PHP_MINIT_FUNCTION (blame);

extern zend_module_entry blame_module_entry;
#define phpext_blame_ptr &blame_module_entry

#endif /* PHP_BLAME_HDR */