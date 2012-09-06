#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#include "php.h"
#include "blame.h"

zend_module_entry blame_module_entry = {
#if ZEND_MODULE_API_NO >= 20010901
  STANDARD_MODULE_HEADER,
#endif
  PHP_BLAME_EXTNAME,
  NULL,
  PHP_MINIT (blame),
  NULL,
  NULL,
  NULL,
  NULL,
#if ZEND_MODULE_API_NO >= 20010901
  PHP_BLAME_VERSION,
#endif
  STANDARD_MODULE_PROPERTIES
};

#ifdef COMPILE_DL_BLAME
ZEND_GET_MODULE (blame)
#endif

void (*old_error_cb) (int type, const char *filenm, uint lineno,
  const char *fmt, va_list args);

char *
who_deserves_blame (const char *filename, uint lineno)
{
  char *guilty = 0;
  char cmd[512];
  zval *inp = 0, *rval = 0, *fun = 0, **inpp = &inp;

  TSRMLS_FETCH ();

  sprintf (cmd, "(git blame -L%d,+1 --porcelain %s | sed -n 2p | sed s/'author '//) 2>/dev/null", lineno, filename);

  MAKE_STD_ZVAL (inp);
  ZVAL_STRING (inp, cmd, 1);

  MAKE_STD_ZVAL (fun);
  ZVAL_STRING (fun, "shell_exec", 1);

  if (SUCCESS == call_user_function_ex (EG (function_table), 0, fun, &rval, 1, &inpp, 0, 0 TSRMLS_CC)) {
    if (rval && (IS_STRING == Z_TYPE_P (rval))) {
      int len = Z_STRLEN_P (rval);

      guilty = emalloc (len + 1);

      strncpy (guilty, Z_STRVAL_P (rval), len);
      if (guilty[len - 1] == '\n') {
        guilty[len - 1] = '\0';
      }
    }
    zval_ptr_dtor (&rval);
  }

  zval_ptr_dtor (&inp);
  zval_ptr_dtor (&fun);

  return guilty;
}

void
blame_error_cb (int type, const char *error_filename, uint error_lineno, const char *format, va_list args)
{
  char *guilty;

  guilty = who_deserves_blame (error_filename, error_lineno);
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

PHP_MINIT_FUNCTION (blame)
{
  old_error_cb = zend_error_cb;
  zend_error_cb = blame_error_cb;
}




