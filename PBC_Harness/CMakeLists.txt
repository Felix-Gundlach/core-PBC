# Copyright (c) 2023 - 2026 Chair for Design Automation, TUM
# Copyright (c) 2025 - 2026 Munich Quantum Software Company GmbH
# All rights reserved.
#
# SPDX-License-Identifier: MIT
#
# Licensed under the MIT License

# Local harness for the PBC experiments. Enable with -DMQT_BUILD_PBC_HARNESS=ON.

add_executable(pbc-simple-setup simple_setup.cpp)
target_link_libraries(
  pbc-simple-setup
  PRIVATE
    MLIRQCOpenQASMTranslation
    MLIRQCDialect
    MLIRMQTDialect
    MLIRCBitDialect
    MLIRArithDialect
    MLIRFuncDialect
    MLIRMathDialect
    MLIRMemRefDialect
    MLIRSCFDialect
    MLIRPass
    MLIRTransforms)
mqt_mlir_target_use_project_options(pbc-simple-setup)

# The harness writes its output files to this directory, whatever its working directory is.
target_compile_definitions(
  pbc-simple-setup
  PRIVATE PBC_HARNESS_OUTPUT_DIR="${CMAKE_CURRENT_SOURCE_DIR}/outputs")
