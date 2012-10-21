#include "secret_blame.h"

zend_module_entry secret_blame_module_entry = {
#if ZEND_MODULE_API_NO >= 20010901
	STANDARD_MODULE_HEADER,
#endif
	PHP_SECRET_BLAME_EXTNAME,     /* Extension Name (Required!) */
	NULL,                         /* Module Functions */
	PHP_MINIT(secret_blame),      /* Module Initialization */
	PHP_MSHUTDOWN(secret_blame),  /* Module Shutdown */
	PHP_RINIT(secret_blame),      /* Request Initialization */
	NULL,                         /* Request Shutdown */
	NULL,                         /* Module Information */
#if ZEND_MODULE_API_NO >= 20010901
	NULL,                         /* Version Information */
#endif
	STANDARD_MODULE_PROPERTIES
};

#ifdef COMPILE_DL_SECRET_BLAME
ZEND_GET_MODULE(secret_blame)
#endif

ZEND_DECLARE_MODULE_GLOBALS(secret_blame);

static void
new_extension_loaded(INTERNAL_FUNCTION_PARAMETERS)
{
	int rv, len = 0;
	char *str = 0;

	rv = zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s", &str, &len);
	if (FAILURE == rv) {
		RETURN_FALSE;
	}

	if (0 == strncmp(str, "secret_blame", len)) {
		RETURN_FALSE;
	}

	SECRET_BLAME_G(orig_extension_loaded)(INTERNAL_FUNCTION_PARAM_PASSTHRU);
}

static int
remove_sb(zval **ext TSRMLS_DC)
{
	if ((IS_STRING == Z_TYPE_PP(ext)) &&
		(0 == strncmp(Z_STRVAL_PP(ext), "secret_blame", Z_STRLEN_PP(ext)))) {
		return ZEND_HASH_APPLY_REMOVE|ZEND_HASH_APPLY_STOP;
	}
	return ZEND_HASH_APPLY_KEEP;
}

static void
new_get_loaded_extensions(INTERNAL_FUNCTION_PARAMETERS)
{
	SECRET_BLAME_G(orig_get_loaded_extensions)(INTERNAL_FUNCTION_PARAM_PASSTHRU);

	if (IS_ARRAY == Z_TYPE_P(return_value)) {
		zend_hash_apply(Z_ARRVAL_P(return_value), (apply_func_t)remove_sb TSRMLS_CC);
	}
}

static void
replace_internal_fn(const char *name, void (*replacement)(INTERNAL_FUNCTION_PARAMETERS),
					void (**store_original)(INTERNAL_FUNCTION_PARAMETERS) TSRMLS_DC)
{
	zend_internal_function *fn;

	if (SUCCESS == zend_hash_find(EG(function_table), name, strlen(name)+1, (void **)&fn)) {
		if (store_original) {
			*store_original = fn->handler;
		}
		fn->handler = replacement;
	}
}

char *
who_deserves_blame(const char *filenm, uint lineno)
{
	FILE* fp;
	char buf[128], cmd[1024], *guilty = NULL;

	sprintf(cmd, "cd `dirname %s`; git blame -L%d,+1 --porcelain %s 2>/dev/null |"
				  "sed -n 2p | sed s/'author '// | tr -d '\n'", filenm, lineno, filenm);
	fp = popen(cmd, "r");
	if (fp) {
		size_t rv = fread(&buf, 1, sizeof(buf) - 1, fp);

		if (rv > 0) {
			guilty = estrndup(buf, rv);
		}
		pclose(fp);
	}
	return guilty;
}

static void (*orig_error_cb)(int type, const char *filenm, uint lineno,
                             const char *fmt, va_list args);

static void
secret_blame_error_cb(int type, const char *filenm, uint lineno, const char *fmt, va_list args)
{
	char *newfmt, *guilty = who_deserves_blame(filenm, lineno);

	if (!guilty) {
		orig_error_cb(type, filenm, lineno, fmt, args);
		return;
	}

	newfmt = emalloc(strlen(fmt) + strlen(guilty) + 64);
	sprintf(newfmt, "Guilty: %s: %s", guilty, fmt);
	orig_error_cb(type, filenm, lineno, newfmt, args);

	efree(guilty);
	efree(newfmt);
}

static int
remove_substring(char *out, int outlen, char *needle)
{
	int nlen = strlen(needle);
	char *mstart = php_memnstr(out, needle, nlen, out + outlen); 

	if (mstart) {
		char *mend = mstart + nlen;

		memmove(mstart, mend, (out + outlen) - mend);
		return nlen;
	}

	return 0;
}

static void
secret_blame_handler(char *buf, uint buflen,
                     char **handled, uint *handledlen, int mode TSRMLS_DC)
{
	int outlen = buflen;
	char *out = emalloc(buflen);

	memcpy(out, buf, buflen);

	outlen -= remove_substring(out, outlen, "<tr><td>secret_blame</td></tr>");
	outlen -= remove_substring(out, outlen, "secret_blame\n");
	outlen -= remove_substring(out, outlen, "'--enable-secret_blame'");

	if (outlen != buflen) {
		*handled = out;
		*handledlen = outlen;
	} else {
		efree (out);
	}
}

static void
sb_init_globals(zend_secret_blame_globals *globals)
{
	globals->done_replacement = 0;
}

PHP_MINIT_FUNCTION(secret_blame)
{
	orig_error_cb = zend_error_cb;
	zend_error_cb = &secret_blame_error_cb;

	ZEND_INIT_MODULE_GLOBALS(secret_blame, sb_init_globals, NULL);

	return SUCCESS;
}

PHP_MSHUTDOWN_FUNCTION(secret_blame)
{
	zend_error_cb = orig_error_cb;

	return SUCCESS;
}

PHP_RINIT_FUNCTION(secret_blame)
{
	if (0 == SECRET_BLAME_G(done_replacement)) {
		SECRET_BLAME_G(done_replacement) = 1;

		replace_internal_fn("extension_loaded", &new_extension_loaded,
		                    &SECRET_BLAME_G(orig_extension_loaded) TSRMLS_CC);

		replace_internal_fn("get_loaded_extensions", &new_get_loaded_extensions,
		                    &SECRET_BLAME_G(orig_get_loaded_extensions) TSRMLS_CC);
	}

#if PHP_API_VERSION >= 20100412
	php_output_start_internal(ZEND_STRL("default output handler"), secret_blame_handler,
	                          (size_t)40960, PHP_OUTPUT_HANDLER_STDFLAGS TSRMLS_CC);
#else
	php_ob_set_internal_handler(secret_blame_handler, (uint)40960,
  	                            "default output handler", 1 TSRMLS_CC);
#endif
	return SUCCESS;
}
