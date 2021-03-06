/*
handle16 <-> wow64 handle32
*/

#include "config.h"
#include "wine/port.h"

#include <assert.h>
#include <stdarg.h>
#include <errno.h>

#include "windows.h"
#include "wine/winbase16.h"
#include "windef.h"
#include "winbase.h"
#include "winerror.h"
#include "windows/wownt32.h"
#include "excpt.h"
#include "winternl.h"
#include "kernel16_private.h"
#include "wine/exception.h"
#include "wine/debug.h"

WINE_DEFAULT_DEBUG_CHANNEL(thunk);
#define HANDLE_RESERVED 32
typedef struct
{
	HANDLE handle32;
    DWORD wndproc;
    DWORD dlgproc;
    HINSTANCE16 hInst16;
    HMENU16 hMenu16;
} HANDLE_DATA;
HANDLE_DATA handle_hwnd[65536];
WORD get_handle16_data(HANDLE h, HANDLE_DATA handles[], HANDLE_DATA **o);
BOOL is_reserved_handle32(HANDLE h)
{
    SSIZE_T signedh = (SSIZE_T)h;
    if (signedh < HANDLE_RESERVED && signedh > -HANDLE_RESERVED)
    {
        return TRUE;
    }
    return FALSE;
}
BOOL is_reserved_handle16(HANDLE16 h)
{
    INT16 signedh = (INT16)h;
    if (signedh < HANDLE_RESERVED && signedh > -HANDLE_RESERVED)
    {
        return TRUE;
    }
    return FALSE;
}
WORD get_handle16(HANDLE h, HANDLE_DATA handles[])
{
	if (is_reserved_handle32(h))
	{
		return h;
	}
	HANDLE_DATA *hd;
	int hnd16 = get_handle16_data(h, handles, &hd);
	hd->handle32 = h;
	return hnd16;
}
WORD get_handle16_data(HANDLE h, HANDLE_DATA handles[], HANDLE_DATA **o)
{
	//?
	if (is_reserved_handle32(h))
	{
		*o = &handles[(size_t)h];
		return h;
	}
	WORD fhandle = 0;
	for (WORD i = HANDLE_RESERVED; i; i++)
	{
		if (!handles[i].handle32 && !fhandle)
		{
			fhandle = i;
		}
		if (handles[i].handle32 == h)
		{
			*o = &handles[i];
			return i;
		}
	}
	if (!fhandle)
	{
		ERR("Could not allocate a handle.\n");
	}
	*o = &handles[fhandle];
    memset(*o, 0, sizeof(HANDLE_DATA));
	return fhandle;
}
BOOL get_handle32_data(WORD h, HANDLE_DATA handles[], HANDLE_DATA **o)
{
    if (!h)
    {
        *o = NULL;
        return FALSE;
    }
    if (is_reserved_handle16(h))
	{
		*o = &handles[(size_t)h];
		(*o)->handle32 = h;
		return TRUE;
	}
	*o = &handles[h];
    return TRUE;
}
HANDLE get_handle32(WORD h, HANDLE_DATA handles[])
{
	if (is_reserved_handle16(h))
	{
		return (UINT16)h;
	}
	return handles[h].handle32 ? handles[h].handle32 : h;
}
//handle16 -> wow64 handle32
HANDLE WINAPI K32WOWHandle32HWND(WORD handle)
{
	HANDLE h32 = get_handle32(handle, handle_hwnd);
	TRACE("handle16 0x%04X->handle32 0x%X\n", handle, h32);
	return h32;
}
//handle16 <- wow64 handle32
HANDLE16 WINAPI K32WOWHandle16HWND(HANDLE handle)
{
    HANDLE16 h16 = get_handle16(handle, handle_hwnd);
	TRACE("handle32 0x%X->handle16 0x%04X\n", handle, h16);
	return h16;
}
__declspec(dllexport) void SetWindowHInst16(WORD hWnd16, HINSTANCE16 hinst16)
{
    HANDLE_DATA *dat;
    if (!get_handle32_data(hWnd16, handle_hwnd, &dat))
    {
        ERR("Invalid Window Handle SetWindowHInst16(%04X,%04X)\n", hWnd16, hinst16);
        return;
    }
    dat->hInst16 = hinst16;
}
__declspec(dllexport) HINSTANCE16 GetWindowHInst16(WORD hWnd16)
{
    HANDLE_DATA *dat;
    if (!get_handle32_data(hWnd16, handle_hwnd, &dat))
    {
        ERR("Invalid Window Handle GetWindowHInst16(%04X)\n", hWnd16);
        return 0;
    }
    return dat->hInst16;
}

__declspec(dllexport) void SetWindowHMenu16(WORD hWnd16, HMENU16 h)
{
    HANDLE_DATA *dat;
    if (!get_handle32_data(hWnd16, handle_hwnd, &dat))
    {
        ERR("Invalid Window Handle SetWindowHMenu16(%04X,%04X)\n", hWnd16, h);
        return;
    }
    dat->hMenu16 = h;
}
__declspec(dllexport) HMENU16 GetWindowHMenu16(WORD hWnd16)
{
	HANDLE_DATA *dat;
	if (!get_handle32_data(hWnd16, handle_hwnd, &dat))
	{
		ERR("Invalid Window Handle GetWindowHMenu16(%04X)\n", hWnd16);
		return 0;
	}
	if (dat->hMenu16)
	{
		return dat->hMenu16;
	}
	return WOWHandle16(GetMenu(dat->handle32), WOW_TYPE_HMENU);
}

__declspec(dllexport) void SetWndProc16(WORD hWnd16, DWORD WndProc)
{
	HANDLE_DATA *dat;
	if (!get_handle32_data(hWnd16, handle_hwnd, &dat))
	{
		ERR("Invalid Window Handle SetWndProc16(%04X,%04X)\n", hWnd16, WndProc);
		return;
	}
	dat->wndproc = WndProc;
}
__declspec(dllexport) DWORD GetWndProc16(WORD hWnd16)
{
	HANDLE_DATA *dat;
	if (!get_handle32_data(hWnd16, handle_hwnd, &dat))
	{
		ERR("Invalid Window Handle GetWndProc16(%04X)\n", hWnd16);
		return 0;
	}
	return dat->wndproc;
}


__declspec(dllexport) void SetDlgProc16(WORD hWnd16, DWORD WndProc)
{
    HANDLE_DATA *dat;
    if (!get_handle32_data(hWnd16, handle_hwnd, &dat))
    {
        ERR("Invalid Window Handle SetDlgProc16(%04X,%04X)\n", hWnd16, WndProc);
        return;
    }
    dat->dlgproc = WndProc;
}
__declspec(dllexport) DWORD GetDlgProc16(WORD hWnd16)
{
    HANDLE_DATA *dat;
    if (!get_handle32_data(hWnd16, handle_hwnd, &dat))
    {
        ERR("Invalid Window Handle GetDlgProc16(%04X)\n", hWnd16);
        return 0;
    }
    return dat->dlgproc;
}
