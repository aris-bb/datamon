#pragma once

#include <cstdint>

namespace datamon {

//! @brief The type of the interception function.
//! @param accessing_address The address of the code that is accessing the data.
//! @param read Whether the data is being read or written.
//! @param data The data being read or written.
using InterceptorFn = void (*)(void* accessing_address, bool read, void* data);

//! @brief Allows you to intercept access to arbitrary data.
class Datamon {
 public:
  //! @brief Creates a new Datamon instance.
  //! @param address The address of the data to be monitored.
  //! @param size The size of the data to be monitored.
  //! @param interceptor The interceptor callback function to call when the data
  //! is accessed.
  Datamon(void* address, size_t size, InterceptorFn interceptor);
  ~Datamon();

  Datamon(const Datamon&) = delete;
  Datamon(Datamon&&) = delete;
  Datamon& operator=(const Datamon&) = delete;
  Datamon& operator=(Datamon&&) = delete;

 private:
  void* address_;
  size_t size_;
  InterceptorFn interceptor_;

  // the ID of the interceptor entry in the interval tree
  size_t interceptor_entry_id_;
};

}  // namespace datamon