#include <windows.h>
typedef BOOL (WINAPI *pWaitOnAddress)(volatile VOID*, PVOID, SIZE_T, DWORD);
typedef void (WINAPI *pWakeByAddressSingle)(PVOID);
BOOL WINAPI WaitOnAddress(volatile VOID* Address, PVOID CompareAddress, SIZE_T AddressSize, DWORD dwMilliseconds) {
  static pWaitOnAddress fn = NULL;
  if (!fn) fn = (pWaitOnAddress)GetProcAddress(GetModuleHandleA("kernel32.dll"), "WaitOnAddress");
  if (fn) return fn(Address, CompareAddress, AddressSize, dwMilliseconds);
  Sleep(dwMilliseconds == INFINITE ? 1 : dwMilliseconds);
  return TRUE;
}
void WINAPI WakeByAddressSingle(PVOID Address) {
  static pWakeByAddressSingle fn = NULL;
  if (!fn) fn = (pWakeByAddressSingle)GetProcAddress(GetModuleHandleA("kernel32.dll"), "WakeByAddressSingle");
  if (fn) fn(Address);
}
