#pragma once

#include <queue>
#include <Windows.h>
#include "../../Common/Src/Semaphore.h"
#include "../../Common/Src/CriticalSection.h"

#define ETH_MAX_LENGTH (1514)

class ITapTunListener {
public:
	virtual void OnReadComplete(LPBYTE lpData, UINT nLen) = 0;
};

struct PerIoData {
	OVERLAPPED Overlapped;
	LPVOID pTapTun;
	DWORD dwLength;
	BYTE ioData[ETH_MAX_LENGTH];
};

class CTapTun {
public:
	CTapTun(LPCSTR nicName);
	void setListener(ITapTunListener *listener) {
		m_IoCompleteListener = listener;
	}

private:
	HANDLE hTap;
	ITapTunListener *m_IoCompleteListener;
	CSEM m_readSem;
	std::queue<PerIoData *> m_FreeReadIoDatas;
	CSEM m_writeSem;
	std::queue<PerIoData *> m_FreeWriteIoDatas;
	CCriSec m_FreeWriteCs;
	std::queue<PerIoData *> m_WriteIoDatas;
	CCriSec m_WriteCs;

	static VOID CALLBACK FileReadCompletionRoutine(
		_In_    DWORD        dwErrorCode,
		_In_    DWORD        dwNumberOfBytesTransfered,
		_Inout_ LPOVERLAPPED lpOverlapped
	);

	static DWORD WINAPI TapTunReadThreadProc(LPVOID lpParam);
	static DWORD WINAPI TapTunWriteThreadProc(LPVOID lpParam);
	void ReadWork();
	void WriteWork();

	void ReadComplete(PerIoData *io);

	static VOID CALLBACK FileWriteCompletionRoutine(
		_In_    DWORD        dwErrorCode,
		_In_    DWORD        dwNumberOfBytesTransfered,
		_Inout_ LPOVERLAPPED lpOverlapped
	);

	void WriteComplete(PerIoData * io);

public:
	void WriteQueue(LPBYTE lpData, UINT nLen);
	BOOL StartWork();
	BOOL StartReadWork();
	BOOL StartWriteWork();
};