#include <windows.h>
#include <string.h>
#ifndef STRSAFE_E_INSUFFICIENT_BUFFER
#define STRSAFE_E_INSUFFICIENT_BUFFER ((HRESULT)0x8007007AL)
#endif
HRESULT __stdcall PathCchCombine(PWSTR pszPathOut, size_t cchPathOut, PCWSTR pszPathIn, PCWSTR pszMore) {
  if (!pszPathOut || cchPathOut == 0 || cchPathOut > 32768) return E_INVALIDARG;
  wchar_t tmp[32768];
  tmp[0]=0;
  if (pszPathIn) { wcsncpy(tmp, pszPathIn, 32767); tmp[32767]=0; }
  if (pszMore && pszMore[0]) {
    size_t len = wcslen(tmp);
    if (len > 0 && tmp[len-1] != L'\\' && tmp[len-1] != L'/') { if (len+1 < 32768) { tmp[len++]=L'\\'; tmp[len]=0; } }
    wcsncat(tmp, pszMore, 32767-len);
  }
  if (wcslen(tmp) >= cchPathOut) return STRSAFE_E_INSUFFICIENT_BUFFER;
  wcscpy(pszPathOut, tmp);
  return S_OK;
}
