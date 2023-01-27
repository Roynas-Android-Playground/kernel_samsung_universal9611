#if defined(__UTS__)
#include <utsrelease.h>
#elif defined(__CC__)
#include <compile.h>
#endif
#include <stdio.h>

int main(void)
{
#if defined(UTS_RELEASE)
	printf("%s", UTS_RELEASE);
#elif defined(LINUX_COMPILER)
        printf("%s", LINUX_COMPILER);
#endif
	return 0;
}
