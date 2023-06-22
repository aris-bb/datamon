// clang-format off
#include "pch.hpp"
// clang-format on

#include "libdatamon.hpp"

#include "interval_tree.hpp"

size_t veh_refcount = 0;
HANDLE veh_handle = nullptr;

std::mutex& veh_mutex() {
  static std::mutex mutex;
  return mutex;
}

datamon::IntervalTree<datamon::InterceptorFn>& interval_tree() {
  static datamon::IntervalTree<datamon::InterceptorFn> tree;
  return tree;
}

MEMORY_BASIC_INFORMATION virtual_query(uintptr_t address) {
  MEMORY_BASIC_INFORMATION mbi;
  if (!VirtualQuery(reinterpret_cast<void*>(address), &mbi, sizeof(mbi))) {
    throw std::runtime_error{"Failed to query memory."};
  }
  return mbi;
}

// protects a memory range on a region granularity
void protect_memory(uintptr_t address, size_t size,
                    std::function<DWORD(DWORD)> protection_modifier) {
  uintptr_t start = address;
  uintptr_t end = address + size;
  while (start < end) {
    MEMORY_BASIC_INFORMATION mbi = virtual_query(start);
    DWORD old_protection = mbi.Protect;
    DWORD new_protection = protection_modifier(old_protection);
    if (old_protection != new_protection) {
      if (!VirtualProtect(mbi.BaseAddress, std::min(size, mbi.RegionSize),
                          new_protection, &old_protection)) {
        throw std::runtime_error{"Failed to protect memory."};
      }
    }
    start += mbi.RegionSize;
  }
}

// vectored exception handler
LONG NTAPI handler(PEXCEPTION_POINTERS exception_pointers) {
  std::unique_lock lock{veh_mutex()};

  if (interval_tree().empty()) {
    // no interceptors registered, continue search
    return EXCEPTION_CONTINUE_SEARCH;
  }

  // store the last data address and restore PAGE_GUARD protection after the
  // single step (since it gets cleared)
  thread_local uintptr_t last_data_address = 0;

  if (exception_pointers->ExceptionRecord->ExceptionCode ==
      STATUS_GUARD_PAGE_VIOLATION) {
    // page guard, call any interceptors that watch this address

#ifdef _WIN64
#define XIP Rip
#else
#define XIP Eip
#endif

    // address of code that caused the exception
    uintptr_t accessing_address =
        static_cast<uintptr_t>(exception_pointers->ContextRecord->XIP);

    bool read =
        exception_pointers->ExceptionRecord->ExceptionInformation[0] == 0;

    // address of the data being read or written
    uintptr_t data_address = static_cast<uintptr_t>(
        exception_pointers->ExceptionRecord->ExceptionInformation[1]);

    // TODO: maybe here we could infer what value is being attempted to be
    // written by disassembling the code that caused the exception

    // call all interceptors that watch this address
    auto interceptors = interval_tree().query(data_address);
    for (auto& [start, end, interceptor, id] : interceptors) {
      interceptor(accessing_address, read,
                  reinterpret_cast<void*>(data_address));
    }

    // set the single step flag to capture the next instruction
    exception_pointers->ContextRecord->EFlags |= 0x100;

    last_data_address = data_address;

    return EXCEPTION_CONTINUE_EXECUTION;
  } else if (last_data_address &&
             exception_pointers->ExceptionRecord->ExceptionCode ==
                 STATUS_SINGLE_STEP) {
    // restore PAGE_GUARD protection
    protect_memory(last_data_address, 1,
                   [](DWORD protect) { return protect | PAGE_GUARD; });

    last_data_address = 0;

    return EXCEPTION_CONTINUE_EXECUTION;
  }

  return EXCEPTION_CONTINUE_SEARCH;
}

datamon::Datamon::Datamon(uintptr_t address, size_t size,
                          InterceptorFn interceptor)
    : address_(address), size_(size), interceptor_(interceptor) {
  std::unique_lock lock{veh_mutex()};

  // if this is the first time we instantiated datamon, create the veh handler
  if (veh_refcount == 0) {
    // create the handler
    if (veh_handle = AddVectoredExceptionHandler(1, &handler); !veh_handle) {
      throw std::runtime_error{"Failed to create vectored exception handler."};
    }
  }

  ++veh_refcount;

  // add the interceptor function to the interval tree
  interceptor_entry_id_ =
      interval_tree().insert({address_, address_ + size_, interceptor_});

  // set the memory protection
  protect_memory(address_, size_,
                 [](DWORD protect) { return protect | PAGE_GUARD; });
}

datamon::Datamon::~Datamon() {
  std::unique_lock lock{veh_mutex()};

  // restore the memory protection
  protect_memory(address_, size_,
                 [](DWORD protect) { return protect & ~PAGE_GUARD; });

  // erase the interceptor function from the interval tree
  interval_tree().erase(interceptor_entry_id_);

  --veh_refcount;

  // if we are done with the veh handler, dispose it
  if (veh_refcount == 0 && veh_handle) {
    if (!RemoveVectoredExceptionHandler(veh_handle)) {
      // failed to remove the veh handler...
      // fail silently for now since we can't throw exceptions in the destructor
    }
    veh_handle = nullptr;
  }
}