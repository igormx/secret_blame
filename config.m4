PHP_ARG_ENABLE(blame, enable Blame support, [ --enable-blame   Enable Blame])
if test "$PHP_BLAME" = "yes"; then
  AC_DEFINE(HAVE_BLAME, 1, [Whether Blame is present])
  PHP_NEW_EXTENSION(blame, blame.c, $ext_shared)
fi