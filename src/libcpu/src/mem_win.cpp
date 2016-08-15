#include "common/platform.h"

#ifdef PLATFORM_WINDOWS
#include "common/decaf_assert.h"
#include "mem.h"
#include "cpu_internal.h"
#include <atomic>
#include <Windows.h>

namespace mem
{

static HANDLE
sHandlerHandle = INVALID_HANDLE_VALUE;

static thread_local uint32_t
sSegfaultAddr = 0;

static uint32_t
sHostPageSize;

static_assert(sizeof(std::atomic<uint8_t>) == 1, "Using locks from exception handlers is bad");
static std::atomic<uint8_t>*
sPageFlags;

void
setupMemoryTracker()
{
   SYSTEM_INFO sysInfo;
   GetSystemInfo(&sysInfo);
   sHostPageSize = sysInfo.dwPageSize;

   uint32_t totalPages = 0x100000000u / sHostPageSize;
   sPageFlags = new std::atomic<uint8_t>[totalPages];
   memset(sPageFlags, 0, totalPages);
}

static void
coreSegfaultEntry()
{
   uint32_t segfaultAddr = sSegfaultAddr;
   sSegfaultAddr = 0;
   cpu::gSegfaultHandler(segfaultAddr);
}

static LONG CALLBACK
exceptionHandler(PEXCEPTION_POINTERS info)
{
   if (info->ExceptionRecord->ExceptionCode == STATUS_ACCESS_VIOLATION) {
      auto address = info->ExceptionRecord->ExceptionInformation[1];

      // We only care about our 3 PPC cores
      if (cpu::this_core::id() > 0xFF) {
         return EXCEPTION_CONTINUE_SEARCH;
      }

      // We only are about memory within our PPC memory range
      auto memBase = base();
      if (address != 0 && (address < memBase || address >= memBase + 0x100000000)) {
         return EXCEPTION_CONTINUE_SEARCH;
      }

      // Calculate the address in our PPC range
      uint32_t ppcAddress = static_cast<uint32_t>(address - memBase);


      // If there is no CPU segfault handler registered, keep searching
      if (!cpu::gSegfaultHandler) {
         return EXCEPTION_CONTINUE_SEARCH;
      }

      // Jump to our CPU's exception handler function
      sSegfaultAddr = ppcAddress;
      info->ContextRecord->Rip = reinterpret_cast<DWORD64>();
      return EXCEPTION_CONTINUE_EXECUTION;
   }

   return EXCEPTION_CONTINUE_SEARCH;
}

void
installExceptionHandler()
{
   decaf_check(sHandlerHandle == INVALID_HANDLE_VALUE);
   AddVectoredExceptionHandler(1, exceptionHandler);
}

void
removeExceptionHandler()
{
   decaf_check(sHandlerHandle != INVALID_HANDLE_VALUE);
   RemoveVectoredExceptionHandler(sHandlerHandle);
}

} // namespace mem

#endif