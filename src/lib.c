
#include <ltl2ba.h>

#define STR(x)	#x
#define XSTR(x)	STR(x)

const char * ltl2ba_version(void)
{
  static const char version[] = XSTR(LTL2BA_VERSION_MAJOR) "."
                                XSTR(LTL2BA_VERSION_MINOR);
  return version;
}
