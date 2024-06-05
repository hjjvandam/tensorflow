/* Copyright 2017 The TensorFlow Authors. All Rights Reserved.

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
==============================================================================*/

// Test that popens a child process with the LOG-ing environment variable set
// for the logging framework, and observes changes to the global log level.

#include <stdio.h>
#include <string.h>

#include "tensorflow/core/platform/logging.h"
#include "tensorflow/core/platform/platform.h"
#include "tensorflow/core/platform/test.h"

// Make sure popen and pclose ara available on windows.
#ifdef PLATFORM_WINDOWS
#define popen _popen
#define pclose _pclose
#endif

namespace tensorflow {
namespace {

int RealMain(const char* argv0, bool do_log) {
  if (do_log) {
    LOG(INFO) << "INFO";
    LOG(WARNING) << "WARNING";
    LOG(ERROR) << "ERROR";
    return EXIT_SUCCESS;
  }

  // Popen the child process.
  std::string command = std::string(argv0);
#if defined(PLATFORM_GOOGLE)
  command = command + " do_log --minloglevel=1 --alsologtostderr";
#elif defined(PLATFORM_WINDOWS)
  command = "set TF_CPP_MIN_LOG_LEVEL=1 && " + command + " do_log";
#else
  command = "TF_CPP_MIN_LOG_LEVEL=1 " + command + " do_log";
#endif
  command += " 2>&1";
  fprintf(stderr, "Running: \"%s\"\n", command.c_str());
  FILE* f = popen(command.c_str(), "r");
  if (f == nullptr) {
    fprintf(stderr, "Failed to popen child: %s\n", strerror(errno));
    return EXIT_FAILURE;
  }

  // Read data from the child's stdout.
  constexpr int kBufferSizeBytes = 8192;
  char buffer[kBufferSizeBytes];
  size_t result = fread(buffer, sizeof(buffer[0]), kBufferSizeBytes - 1, f);
  if (result == 0) {
    fprintf(stderr, "Failed to read from child stdout: %zu %s\n", result,
            strerror(errno));
    return EXIT_FAILURE;
  }
  buffer[result] = '\0';
  int status = pclose(f);
  if (status == -1) {
    fprintf(stderr, "Failed to close popen child: %s\n", strerror(errno));
    return EXIT_FAILURE;
  }

  bool ok = strstr(buffer, "WARNING") != nullptr &&
            strstr(buffer, "ERROR") != nullptr &&
            strstr(buffer, "INFO") == nullptr;
  if (!ok) {
    fprintf(stderr, "error: LOG output not as expected: \"%.*s\"\n",
            kBufferSizeBytes, buffer);
    fprintf(stderr,
            "\n\nCould not find expected LOG statements in the above log "
            "buffer.\n[  FAILED  ]\n");
    return EXIT_FAILURE;
  }

  fprintf(stderr, "\n[  PASSED  ]\n");
  return EXIT_SUCCESS;
}

}  // namespace
}  // namespace tensorflow

int main(int argc, char** argv) {
  testing::InitGoogleTest(&argc, argv);
  bool do_log = argc >= 2 && strcmp(argv[1], "do_log") == 0;
  return tensorflow::RealMain(argv[0], do_log);
}
