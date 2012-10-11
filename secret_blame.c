#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#include "php.h"
#include "secret_blame.h"

zend_module_entry secret_blame_module_entry = {
#if ZEND_MODULE_API_NO >= 20010901
    STANDARD_MODULE_HEADER,
#endif
    PHP_SECRET_BLAME_EXTNAME,
    NULL,                      /* Module Functions       */
    PHP_MINIT (secret_blame),  /* Module  Initialization */
    NULL,                      /* Module  Shutdown       */
    PHP_RINIT (secret_blame),  /* Request Initialization */
    NULL,                      /* Request Shutdown       */
    PHP_MINFO (secret_blame),  /* Module  Information    */
#if ZEND_MODULE_API_NO >= 20010901
    PHP_SECRET_BLAME_VERSION,
#endif
    STANDARD_MODULE_PROPERTIES
};

#ifdef COMPILE_DL_SECRET_BLAME
ZEND_GET_MODULE (secret_blame)
#endif

ZEND_DECLARE_MODULE_GLOBALS(secret_blame);

static void
new_extension_loaded (INTERNAL_FUNCTION_PARAMETERS)
{
    int len = 0;
    char *str = 0;

    if ((SUCCESS == zend_parse_parameters_ex (ZEND_PARSE_PARAMS_QUIET, ZEND_NUM_ARGS () TSRMLS_CC, "s", &str, &len)) &&
        (sizeof ("secret_blame") - 1 == len) &&
        (0 == memcmp (str, "secret_blame", sizeof ("secret_blame") - 1))) {

        RETURN_FALSE;
    }

    SECRET_BLAME_G (orig_extension_loaded) (INTERNAL_FUNCTION_PARAM_PASSTHRU);
}

static int
loaded_extensions_iter (zval **ext TSRMLS_DC)
{
    if ((IS_STRING == Z_TYPE_PP (ext)) &&
        (sizeof ("secret_blame") - 1 == Z_STRLEN_PP (ext)) &&
        (0 == memcmp ("secret_blame", Z_STRVAL_PP (ext), sizeof ("secret_blame") - 1))) {

        return ZEND_HASH_APPLY_REMOVE|ZEND_HASH_APPLY_STOP;
    }
    return ZEND_HASH_APPLY_KEEP;
}

static void
new_get_loaded_extensions (INTERNAL_FUNCTION_PARAMETERS)
{
    SECRET_BLAME_G (orig_get_loaded_extensions) (INTERNAL_FUNCTION_PARAM_PASSTHRU);

    if (IS_ARRAY == Z_TYPE_P (return_value)) {
        zend_hash_apply (Z_ARRVAL_P (return_value), (apply_func_t)loaded_extensions_iter TSRMLS_CC);
    }
}

static void
replace_internal_fn (const char *name, void (*replacement)(INTERNAL_FUNCTION_PARAMETERS),
                     void (**store_original)(INTERNAL_FUNCTION_PARAMETERS) TSRMLS_DC)
{
    zend_internal_function *fn;

    if (SUCCESS == zend_hash_find (EG (function_table), name, strlen (name)+1, (void **)&fn)) {

        if (store_original) {
            *store_original = fn->handler;
        }

        fn->handler = replacement;
    }
}

char *
who_deserves_blame (const char *filename, uint lineno)
{
    FILE* fp;
    char buf[128], cmd[512], *guilty = NULL;

    sprintf (cmd, "git blame -L%d,+1 --porcelain %s 2>/dev/null | sed -n 2p | sed s/'author '// | tr -d '\n'", lineno, filename);
    fp = popen (cmd, "r");

    if (fp) {
        size_t rv = fread (&buf, 1, sizeof (buf) - 1, fp);

        if (rv > 0) {
            guilty = estrndup (buf, rv);
        }
        pclose (fp);
    }

    return guilty;
}

void (*orig_error_cb) (int type, const char *filenm, uint lineno, const char *fmt, va_list args);

static void
secret_blame_error_cb (int type, const char *error_filename, uint error_lineno, const char *format, va_list args)
{
    char *fmt, *guilty;

    guilty = who_deserves_blame (error_filename, error_lineno);

    if (!guilty) {
        orig_error_cb (type, error_filename, error_lineno, format, args);
        return;
    }

    fmt = emalloc (strlen (format) + strlen (guilty) + 64);
    sprintf (fmt, "Guilty: %s: %s", guilty, format);

    orig_error_cb (type, error_filename, error_lineno, fmt, args);

    efree (guilty);
    efree (fmt);
}

static char *
remove_if_present (char *buf, int buflen, char *fragment, int fragmentlen)
{
    char *out = NULL;
    char *found = php_memnstr (buf, fragment, fragmentlen, buf + buflen);

    if (found) {
        out = emalloc (buflen - fragmentlen);

        strncpy (out, buf, found - buf);
        strncpy (out + (found - buf), found + fragmentlen, (buf + buflen) - (found + fragmentlen));
    }

    return out;
}

char htmlfrag[] = "<h2><a name=\"module_secret_blame\">secret_blame</a></h2>\n";
char clifrag[] = "secret_blame\n";
#define htmlfraglen (sizeof (htmlfrag) - 1)
#define clifraglen (sizeof (clifrag) - 1)

static void
secret_blame_output_handler (char *output, uint output_len, char **handled_output, uint *handled_output_len, int mode TSRMLS_DC)
{
    char *out = NULL;

    if (!output || !output_len || !handled_output || !handled_output_len) {
        return;
    }

    *handled_output = NULL;

    out = remove_if_present (output, output_len, htmlfrag, sizeof (htmlfrag) - 1);
    if (out) {
        *handled_output = out;
        *handled_output_len = output_len - htmlfraglen;
        return;
    }

    out = remove_if_present (output, output_len, clifrag, sizeof (clifrag) - 1);
    if (out) {
        *handled_output = out;
        *handled_output_len = output_len - clifraglen;
        return;
    }
}

static void
secret_blame_init_globals (zend_secret_blame_globals *secret_blame_globals)
{
    secret_blame_globals->done_replacement = 0;
}


PHP_MINIT_FUNCTION (secret_blame)
{
    orig_error_cb = zend_error_cb;
    zend_error_cb = &secret_blame_error_cb;

    ZEND_INIT_MODULE_GLOBALS (secret_blame, secret_blame_init_globals, NULL);

    return SUCCESS;
}

PHP_RINIT_FUNCTION (secret_blame)
{
    if (0 == SECRET_BLAME_G (done_replacement)) {
        SECRET_BLAME_G (done_replacement) = 1;

        replace_internal_fn ("extension_loaded", &new_extension_loaded,
                             &SECRET_BLAME_G (orig_extension_loaded) TSRMLS_CC);

        replace_internal_fn ("get_loaded_extensions", &new_get_loaded_extensions,
                             &SECRET_BLAME_G (orig_get_loaded_extensions) TSRMLS_CC);
    }

    php_output_start_internal (ZEND_STRL ("default output handler"), secret_blame_output_handler,
                               (size_t)40960, PHP_OUTPUT_HANDLER_STDFLAGS TSRMLS_CC);

    return SUCCESS;
}

PHP_MINFO_FUNCTION (secret_blame)
{
}

