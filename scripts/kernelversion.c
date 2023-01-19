#include <utsrelease.h>
#include <stdio.h>

int main(void)
{
#ifdef UTS_RELEASE
	printf("%s", UTS_RELEASE);
#endif
	return 0;
}
