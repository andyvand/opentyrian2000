#ifdef WINDOWS
#include <windows.h>

BOOL __stdcall DllMain(HANDLE hDLL, DWORD dwReason, LPVOID lpReserved)
{
	(void)hDLL;
	(void)dwReason;
	(void)lpReserved;

	return (TRUE);
}
#endif
