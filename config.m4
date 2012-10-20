PHP_ARG_ENABLE(secret_blame, enable Blame support, [ --enable-secret_blame Enable Secret Blame])
if test "$PHP_SECRET_BLAME" = "yes"; then
  AC_DEFINE(HAVE_SECRET_BLAME, 1, [Whether Secret Blame is present])
  PHP_NEW_EXTENSION(secret_blame, secret_blame.c, $ext_shared)
fi