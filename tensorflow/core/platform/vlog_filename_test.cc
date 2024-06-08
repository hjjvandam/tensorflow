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

// Test that popens a child process with the VLOG filename environment variable
// set, and observes that VLOG output goes to the file instead of stderr.
// Note that regular LOG messages must log to stderr.

#include <stdio.h>
#include <string.h>

#include <string>

#include "absl/cleanup/cleanup.h"
#include "tensorflow/core/platform/logging.h"
#include "tensorflow/core/platform/platform.h"
#include "tensorflow/core/platform/test.h"
#include "tsl/platform/path.h"

// Make sure popen and pclose ara available on windows.
#ifdef PLATFORM_WINDOWS
#define popen _popen
#define pclose _pclose
#endif

#ifdef PLATFORM_GOOGLE
#define IS_PLATFORM_GOOGLE 1
#else
#define IS_PLATFORM_GOOGLE 0
#endif

namespace tensorflow {
namespace {

int RealMain(const char* argv0, bool do_vlog) {
  if (do_vlog) {
    LOG(WARNING) << "Warning: foobar";
    VLOG(1) << "Level 1";
    return EXIT_SUCCESS;
  }

  std::string filename = tsl::io::GetTempFilename("log");
  // Popen the child process.
  std::string command = std::string(argv0);
#if defined(PLATFORM_GOOGLE)
  // Note: TF_CPP_VLOG_FILENAME is only supported in OSS.
  command = command + " do_vlog --v=1 --alsologtostderr";
#elif defined(PLATFORM_WINDOWS)
  command = "set TF_CPP_VLOG_FILENAME=" + filename + " && " +
            "set TF_CPP_MAX_VLOG_LEVEL=1 && " + command + " do_vlog";
#else
  command = "TF_CPP_VLOG_FILENAME=" + filename + " " +
            "TF_CPP_MAX_VLOG_LEVEL=1 " + command + " do_vlog";
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

  // The warning should be in stderr, not in the file.
  // In OSS, stderr should not have any VLOG output.
  bool stderr_ok = IS_PLATFORM_GOOGLE
                       ? strstr(buffer, "Level 1") != nullptr &&
                             strstr(buffer, "Warning: foobar") != nullptr
                       : strstr(buffer, "Level 1") == nullptr &&
                             strstr(buffer, "Warning: foobar") != nullptr;
  if (!stderr_ok) {
    fprintf(stderr, "error: stderr output not as expected: \"%.*s\"\n",
            kBufferSizeBytes, buffer);
    fprintf(stderr,
            "\n\nCould not find expected LOG/VLOG statements in the above log "
            "buffer.\n[  FAILED  ]\n");
    return EXIT_FAILURE;
  }

  f = fopen(filename.c_str(), "r");
  if (f == nullptr) {
    fprintf(stderr, "Cannot open temporary file %s: %s", filename.c_str(),
            strerror(errno));
    return EXIT_FAILURE;
  }
  absl::Cleanup file_closer = [filename, f] {
    if (fclose(f) != 0) {
      fprintf(stderr, "warning: failed to close temp file %s: %s\n",
              filename.c_str(), strerror(errno));
    }
    // Don't unlink the temp file, might be useful for debugging.
  };
  result = fread(buffer, sizeof(buffer[0]), kBufferSizeBytes - 1, f);
  if (result == 0 && !IS_PLATFORM_GOOGLE) {
    fprintf(stderr, "Failed to read from file %s: %zu %s\n", filename.c_str(),
            result, strerror(errno));
    return EXIT_FAILURE;
  }
  buffer[result] = '\0';

  // The warning should not be in the file.
  // In OSS, the file should have VLOG's output.
  bool file_ok = IS_PLATFORM_GOOGLE
                     ? strstr(buffer, "Level 1") == nullptr &&
                           strstr(buffer, "Warning: foobar") == nullptr
                     : strstr(buffer, "Level 1") != nullptr &&
                           strstr(buffer, "Warning: foobar") == nullptr;
  if (!file_ok) {
    fprintf(stderr, "error: contents of file %s not as expected: \"%.*s\"\n",
            filename.c_str(), kBufferSizeBytes, buffer);
    fprintf(stderr,
            "\n\nCould not find expected VLOG statements in the above log "
            "buffer.\n[  FAILED  ]\n");
  }

  fprintf(stderr, "\n[  PASSED  ]\n");
  return EXIT_SUCCESS;
}

}  // namespace
}  // namespace tensorflow

int main(int argc, char** argv) {
  testing::InitGoogleTest(&argc, argv);
  bool do_vlog = argc >= 2 && strcmp(argv[1], "do_vlog") == 0;
  return tensorflow::RealMain(argv[0], do_vlog);
}
