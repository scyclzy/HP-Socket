#include "stdafx.h"
#include "tunio.h"
#include "TapTun.h"
#include <assert.h>

CTapTun::CTapTun(LPCSTR nicName) :
	m_readSem(16, 16),
	m_writeSem(LONG_MAX)
{
	hTap = OpenTun("MYTAP");
	assert(hTap != INVALID_HANDLE_VALUE);
	m_IoCompleteListener = nullptr;
}

VOID CTapTun::FileReadCompletionRoutine(DWORD dwErrorCode,
	DWORD dwNumberOfBytesTransfered, LPOVERLAPPED lpOverlapped)
{
	assert(dwErrorCode == ERROR_SUCCESS);
	assert(dwNumberOfBytesTransfered <= ETH_MAX_LENGTH);

	PerIoData *io = CONTAINING_RECORD(lpOverlapped, PerIoData, Overlapped);
	CTapTun *pThis = (CTapTun*)io->pTapTun;
	io->dwLength = dwNumberOfBytesTransfered;

	pThis->ReadComplete(io);
	
	return ;
}

DWORD CTapTun::TapTunReadThreadProc(LPVOID lpParam)
{
	CTapTun *tap = (CTapTun*)lpParam;
	tap->ReadWork();
	return 0;
}

DWORD WINAPI CTapTun::TapTunWriteThreadProc(LPVOID lpParam) {
	CTapTun *tap = (CTapTun*)lpParam;
	tap->WriteWork();
	return 0;
}

void CTapTun::ReadWork()
{
	BOOL status = FALSE;
	DWORD dwResult = 0;

	while (TRUE) {
		dwResult = WaitForSingleObjectEx(m_readSem, INFINITE, TRUE);
		if (dwResult == WAIT_IO_COMPLETION) {
			continue;
		}

		assert(dwResult == WAIT_OBJECT_0);

		PerIoData *io = nullptr;
		if (m_FreeReadIoDatas.empty())
		{
			io = new PerIoData;
		}
		else {
			io = m_FreeReadIoDatas.front();
			m_FreeReadIoDatas.pop();
		}

		assert(io != nullptr);
		::ZeroMemory(io, sizeof(PerIoData));
		io->pTapTun = this;

		status = ReadFileEx(hTap, io->ioData, sizeof(io->ioData),
			&io->Overlapped, FileReadCompletionRoutine);

		assert(status == TRUE);
	}
}

void CTapTun::WriteWork() {
	BOOL status = FALSE;
	DWORD dwResult = 0;

	while (TRUE)
	{
		dwResult = WaitForSingleObjectEx(m_writeSem, INFINITE, TRUE);
		if (dwResult == WAIT_IO_COMPLETION) {
			continue;
		}

		assert(dwResult == WAIT_OBJECT_0);

		PerIoData *io = nullptr;
		{
			CCriSecLock lock(m_WriteCs);
			assert(!m_WriteIoDatas.empty());
			io = m_WriteIoDatas.front();
			m_WriteIoDatas.pop();
		}

		status = WriteFileEx(hTap, io->ioData, io->dwLength, &io->Overlapped,
			FileWriteCompletionRoutine);

		assert(status == TRUE);
	}
}

void CTapTun::ReadComplete(PerIoData *io)
{
	assert(m_IoCompleteListener != nullptr);
	m_IoCompleteListener->OnReadComplete(io->ioData, io->dwLength);
	m_FreeReadIoDatas.push(io);
	m_readSem.Release();
}

VOID CTapTun::FileWriteCompletionRoutine(DWORD dwErrorCode,
	DWORD dwNumberOfBytesTransfered,
	LPOVERLAPPED lpOverlapped)
{
	assert(dwErrorCode == ERROR_SUCCESS);

	PerIoData *io = CONTAINING_RECORD(lpOverlapped, PerIoData, Overlapped);
	assert(dwNumberOfBytesTransfered == io->dwLength);

	CTapTun *pThis = (CTapTun*)io->pTapTun;
	pThis->WriteComplete(io);
	return ;
}

void CTapTun::WriteComplete(PerIoData * io)
{
	CCriSecLock lock(m_FreeWriteCs);
	m_FreeWriteIoDatas.push(io);
}

void CTapTun::WriteQueue(LPBYTE lpData, UINT nLen)
{
	assert(nLen <= ETH_MAX_LENGTH);

	PerIoData *io = nullptr;
	{
		CCriSecLock lock(m_FreeWriteCs);
		if (!m_FreeWriteIoDatas.empty()) {
			io = m_FreeWriteIoDatas.front();
			m_FreeWriteIoDatas.pop();
		}
	}

	if (io == nullptr) {
		io = new PerIoData;
	}
	
	assert(io != nullptr);

	::ZeroMemory(&io->Overlapped, sizeof(io->Overlapped));
	io->pTapTun = this;
	io->dwLength = nLen;
	memcpy(io->ioData, lpData, nLen);

	{
		CCriSecLock lock(m_WriteCs);
		m_WriteIoDatas.push(io);
	}

	m_writeSem.Release();
}

BOOL CTapTun::StartWork()
{
	return (StartReadWork() && StartWriteWork());
}

BOOL CTapTun::StartReadWork()
{
	DWORD dwTid = 0;
	HANDLE hThread = NULL;
	BOOL status = FALSE;

	hThread = CreateThread(NULL, 0, TapTunReadThreadProc, this, 0, &dwTid);

	assert(hThread != NULL);

	if (hThread == NULL) {
		return FALSE;
	}

	return TRUE;
}

BOOL CTapTun::StartWriteWork()
{
	DWORD dwTid = 0;
	HANDLE hThread = NULL;
	BOOL status = FALSE;

	hThread = CreateThread(NULL, 0, TapTunWriteThreadProc, this, 0, &dwTid);
	assert(hThread != NULL);

	if (hThread == NULL) {
		return FALSE;
	}

	return TRUE;
}
