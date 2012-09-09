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
  char buf[128];
  char cmd[512];
  char *guilty = NULL;
  FILE* fp;
  size_t rv;

  sprintf (cmd, "git blame -L%d,+1 --porcelain %s 2>/dev/null | sed -n 2p | sed s/'author '// | tr -d '\n'", lineno, filename);
  fp = popen (cmd, "r");
  if (fp) {
    rv = fread (&buf, 1, sizeof (buf) - 1, fp);
    if (rv > 0) {
      guilty = ecalloc (rv + 1, 1);
      memcpy (guilty, buf, rv);
    }

    pclose (fp);
  }

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




