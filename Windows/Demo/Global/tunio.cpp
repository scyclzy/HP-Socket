#include "stdafx.h"
#include "tunio.h"

char * get_device_guid(const char *dev, char *buf, int buf_len) {
	LONG status;
	HKEY network_connections_key;
	DWORD len;

	int i = 0;

	status = RegOpenKeyExA(
		HKEY_LOCAL_MACHINE,
		NETWORK_CONNECTIONS_KEY,
		0,
		KEY_READ,
		&network_connections_key);

	if (status != ERROR_SUCCESS)
	{
		return NULL;
	}

	while (TRUE)
	{
		char enum_name[256];
		char connection_string[256];
		HKEY connection_key;
		CHAR name_data[256];
		DWORD name_type;
		const CHAR name_string[] = "Name";

		len = sizeof(enum_name);
		status = RegEnumKeyExA(
			network_connections_key,
			i,
			enum_name,
			&len,
			NULL,
			NULL,
			NULL,
			NULL);
		if (status == ERROR_NO_MORE_ITEMS)
		{
			break;
		}
		else if (status != ERROR_SUCCESS)
		{
			return NULL;
		}

		snprintf(connection_string, sizeof(connection_string),
			"%s\\%s\\Connection",
			NETWORK_CONNECTIONS_KEY, enum_name);

		status = RegOpenKeyExA(
			HKEY_LOCAL_MACHINE,
			connection_string,
			0,
			KEY_READ,
			&connection_key);

		if (status != ERROR_SUCCESS)
		{
			;
		}
		else
		{
			len = sizeof(name_data);
			status = RegQueryValueExA(
				connection_key,
				name_string,
				NULL,
				&name_type,
				(LPBYTE)name_data,
				&len);

			if (status != ERROR_SUCCESS || name_type != REG_SZ)
			{
				;
			}
			else
			{
				if (strncmp(dev, name_data, strlen(dev)) == 0) {
					strncpy(buf, enum_name, buf_len);
					return buf;
				}
			}
			RegCloseKey(connection_key);
		}
		++i;
	}

	RegCloseKey(network_connections_key);

	return NULL;
}

HANDLE OpenTun(LPCSTR szDev) {

	char dev_guid[256] = { 0 };
	char device_path[256] = { 0 };

	if (get_device_guid(szDev, dev_guid, sizeof(dev_guid)) == NULL) {
		return INVALID_HANDLE_VALUE;
	}

	/* Open Windows TAP-Windows adapter */
	snprintf(device_path, sizeof(device_path), "%s%s%s",
		USERMODEDEVICEDIR,
		dev_guid,
		TAP_WIN_SUFFIX);

	HANDLE h = CreateFileA(
		device_path,
		GENERIC_READ | GENERIC_WRITE,
		0,                /* was: FILE_SHARE_READ */
		0,
		OPEN_EXISTING,
		FILE_ATTRIBUTE_SYSTEM | FILE_FLAG_OVERLAPPED,
		0
	);

	return h;
}

void CloseTun(HANDLE hTun) {
	CloseHandle(hTun);
	return;
}

DWORD ReadTun(HANDLE hTun, LPVOID lpBuffer, DWORD nLen) {
	DWORD read_size = 0;
	BOOL status = FALSE;

	status = ReadFile(
		hTun,
		lpBuffer,
		nLen,
		&read_size,
		NULL
	);

	if (status)
	{
		return read_size;
	}

	return 0;
}

DWORD WriteTun(HANDLE hTun, LPCSTR lpBuffer, DWORD nLen) {
	BOOL status = FALSE;
	DWORD write_size = 0;

	status = WriteFile(
		hTun,
		lpBuffer,
		nLen,
		&write_size,
		NULL
	);

	if (status)
	{
		return write_size;
	}

	return 0;
}

