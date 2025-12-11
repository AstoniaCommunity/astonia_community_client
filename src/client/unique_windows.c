#include <windows.h>

#include "dll.h"
#include "astonia.h"
#include "client/client.h"
#include "client/client_private.h"

void save_unique(void)
{
	HKEY hk;

	if (RegCreateKey(HKEY_CURRENT_USER, "Software\\Microsoft\\Notepad", &hk) != ERROR_SUCCESS) {
		return;
	}

	unique = unique ^ 0xfe2abc82U;
	usum = unique ^ 0x3e5fba04U;

	RegSetValueEx(hk, "fInput1", 0, REG_DWORD, (void *)&unique, 4);
	RegSetValueEx(hk, "fInput2", 0, REG_DWORD, (void *)&usum, 4);
}

void load_unique(void)
{
	HKEY hk;
	int size = 4, type;

	if (RegCreateKey(HKEY_CURRENT_USER, "Software\\Microsoft\\Notepad", &hk) != ERROR_SUCCESS) {
		return;
	}

	RegQueryValueEx(hk, "fInput1", 0, (void *)&type, (void *)&unique, (void *)&size);
	RegQueryValueEx(hk, "fInput2", 0, (void *)&type, (void *)&usum, (void *)&size);

	if ((unique ^ 0x3e5fba04U) != usum) {
		unique = usum = 0;
	} else {
		unique = unique ^ 0xfe2abc82U;
	}
}
