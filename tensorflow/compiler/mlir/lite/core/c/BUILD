load("//tensorflow:tensorflow.default.bzl", "get_compatible_with_portable")

package(
    # copybara:uncomment default_applicable_licenses = ["//tensorflow:license"],
    default_visibility = [
        "//visibility:public",
    ],
    licenses = ["notice"],
)

# LINT.IfChange
cc_library(
    name = "common",
    srcs = [],
    hdrs = ["builtin_op_data.h"],
    compatible_with = get_compatible_with_portable(),
    visibility = [
        "//tensorflow/compiler/mlir/lite/kernels:__pkg__",
        "//tensorflow/compiler/mlir/quantization/tensorflow/utils:__pkg__",
    ],
)
# LINT.ThenChange(//tensorflow/lite/core/c/BUILD)
