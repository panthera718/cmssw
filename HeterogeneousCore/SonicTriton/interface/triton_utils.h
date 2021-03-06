#ifndef HeterogeneousCore_SonicTriton_triton_utils
#define HeterogeneousCore_SonicTriton_triton_utils

#include <string>
#include <string_view>
#include <vector>

#include "request_grpc.h"

namespace triton_utils {

  using Error = nvidia::inferenceserver::client::Error;

  template <typename T>
  std::string printVec(const std::vector<T>& vec, const std::string& delim = ", ");

  //helper to turn triton error into exception
  void throwIfError(const Error& err, std::string_view msg);

  //helper to turn triton error into warning
  bool warnIfError(const Error& err, std::string_view msg);

}  // namespace triton_utils

extern template std::string triton_utils::printVec(const std::vector<int64_t>& vec, const std::string& delim);
extern template std::string triton_utils::printVec(const std::vector<uint8_t>& vec, const std::string& delim);
extern template std::string triton_utils::printVec(const std::vector<float>& vec, const std::string& delim);

#endif
