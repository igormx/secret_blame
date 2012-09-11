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
  NULL,
  PHP_MINIT (secret_blame),
  NULL,
  PHP_RINIT (secret_blame),
  NULL,
  PHP_MINFO (secret_blame),
#if ZEND_MODULE_API_NO >= 20010901
  PHP_SECRET_BLAME_VERSION,
#endif
  STANDARD_MODULE_PROPERTIES
};

#ifdef COMPILE_DL_SECRET_BLAME
ZEND_GET_MODULE (secret_blame)
#endif

void (*old_error_cb) (int type, const char *filenm, uint lineno,
  const char *fmt, va_list args);


char *
who_deserves_blame (const char *filename, uint lineno)
{
  char buf[128];
  char cmd[512];
  char *guilty = NULL;
  FILE* fp;

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

int replaced_extension_loaded = 0;
void (*orig_extension_loaded) (INTERNAL_FUNCTION_PARAMETERS);

static void
new_extension_loaded (INTERNAL_FUNCTION_PARAMETERS)
{
  int len = 0;
  char *str = 0;

  if (SUCCESS == zend_parse_parameters_ex (ZEND_PARSE_PARAMS_QUIET,
      ZEND_NUM_ARGS () TSRMLS_CC, "s", &str, &len)) {

    if (len && str && (0 == strncmp (str, "secret_blame", len))) {
      RETURN_FALSE;
    }
  }

  orig_extension_loaded (INTERNAL_FUNCTION_PARAM_PASSTHRU);
}

void
replace_extension_loaded (TSRMLS_D)
{
  zend_function **efn;

  if (!replaced_extension_loaded) {
    if (SUCCESS == zend_hash_find (EG (function_table), "extension_loaded",
                                   sizeof ("extension_loaded"), (void **)&efn)) {

     orig_extension_loaded = ((zend_internal_function *) efn)->handler;

      ((zend_internal_function *) efn)->handler = &new_extension_loaded;
    }
    replaced_extension_loaded = 1;
  }
}

static void
secret_blame_error_cb (int type, const char *error_filename, uint error_lineno, const char *format, va_list args)
{
  char *guilty = who_deserves_blame (error_filename, error_lineno);

  if (guilty) {
    char *fmt = emalloc (strlen ("guilty: ") + strlen (format) + strlen (guilty) + 4);

    sprintf (fmt, "guilty: %s: %s", guilty, format);

    old_error_cb (type, error_filename, error_lineno, fmt, args);

    efree (guilty);
    efree (fmt);
  } else {
    old_error_cb (type, error_filename, error_lineno, format, args);
  }
}

static void
secret_blame_output_handler (char *output, uint output_len, char **handled_output, uint *handled_output_len, int mode TSRMLS_DC)
{
  char *found = NULL;
  char cmd[128];
  const char *sb = "secret_blame\n";
  int sblen = sizeof ("secret_blame\n") - 1;

  *handled_output = NULL;

  if (!output || !output_len) {
    return;
  }

  found = php_memnstr (output, sb, sblen, output + output_len);

  if (found) {
    int before_len = found - output;
    int after_len = output_len - before_len - sblen;
    char* out = emalloc (before_len + after_len);

    strncpy (out, output, before_len);
    strncpy (out + before_len, found + sblen, after_len);

    *handled_output = out;
    *handled_output_len = before_len + after_len;
  }
}

PHP_MINIT_FUNCTION (secret_blame)
{
  old_error_cb = zend_error_cb;
  zend_error_cb = secret_blame_error_cb;

  return SUCCESS;
}

PHP_RINIT_FUNCTION (secret_blame)
{
  replace_extension_loaded (TSRMLS_C);

  php_output_start_internal (ZEND_STRL ("default output handler"), secret_blame_output_handler, (size_t)40960, PHP_OUTPUT_HANDLER_STDFLAGS TSRMLS_CC);

  return SUCCESS;
}

PHP_MINFO_FUNCTION (secret_blame)
{
}


