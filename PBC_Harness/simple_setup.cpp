/*
 * Copyright (c) 2023 - 2026 Chair for Design Automation, TUM
 * Copyright (c) 2025 - 2026 Munich Quantum Software Company GmbH
 * All rights reserved.
 *
 * SPDX-License-Identifier: MIT
 *
 * Licensed under the MIT License
 */

#include "mqt/Dialect/CBit/IR/CBitDialect.h"
#include "mqt/Dialect/MQT/IR/MQTDialect.h"
#include "mqt/Dialect/QC/IR/QCDialect.h"
#include "mqt/Target/OpenQASM/TranslateOpenQASMToQC.h"

#include "mlir/Dialect/Arith/IR/Arith.h"
#include "mlir/Dialect/Func/IR/FuncOps.h"
#include "mlir/Dialect/Math/IR/Math.h"
#include "mlir/Dialect/MemRef/IR/MemRef.h"
#include "mlir/Dialect/SCF/IR/SCF.h"
#include "mlir/IR/BuiltinOps.h"
#include "mlir/IR/DialectRegistry.h"
#include "mlir/IR/MLIRContext.h"
#include "mlir/Pass/Pass.h" // IWYU pragma: keep
#include "mlir/Pass/PassManager.h"
#include "mlir/Support/LLVM.h"
#include "mlir/Transforms/Passes.h"
#include "mlir/Transforms/ViewOpGraph.h"

#include "llvm/ADT/SmallString.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/FileSystem.h"
#include "llvm/Support/MemoryBuffer.h"
#include "llvm/Support/Path.h"
#include "llvm/Support/raw_ostream.h"

#include <cstdint>
#include <string>
#include <system_error>

// Namespaces
using namespace mlir;

// Target Programs
extern const char* const BELL_STATE_QASM;
const char* const BELL_STATE_QASM = R"(OPENQASM 3.0;
include "stdgates.inc";

qubit[2] q;
bit[2] c;

h q[0];
cx q[0], q[1];

c = measure q;
)";

namespace {
enum class EmitMode : uint8_t { Stdout, File, Dot };
} // namespace

// Output files, relative to PBC_HARNESS_OUTPUT_DIR. The build sets that macro
// to PBC_Harness/outputs.
static constexpr const char* MLIR_OUTPUT_FILE = "qc-output.qc.mlir";
static constexpr const char* DOT_OUTPUT_FILE = "qc-output.dot";

/// Remove duplicate constants and other redundant operations from the module.
///
/// The canonicalizer moves constants to the start of the function and merges
/// equal constants. CSE then merges equal operations that have no side effects.
/// CSE does not merge two `memref.load` operations if any write occurs between
/// them. QC gates write the qubit resource, so each qubit reload stays.
///
/// @param moduleOp The module to simplify in place.
/// @return Success if all passes succeed, failure otherwise.
namespace {
LogicalResult simplifyModule(ModuleOp moduleOp) {
  PassManager pm(moduleOp.getContext());
  pm.addPass(createCanonicalizerPass());
  pm.addPass(createCSEPass());
  return pm.run(moduleOp);
}
} // namespace

// me code :)
int main(int argc, char** argv) {
  /*
   * Command Line Options
   */
  llvm::cl::opt<EmitMode> emitMode(
      "emit", llvm::cl::desc("Where to write the QC IR"),
      llvm::cl::values(
          clEnumValN(EmitMode::Stdout, "stdout", "Print the IR to stdout"),
          clEnumValN(EmitMode::File, "file",
                     "Write the IR to PBC_Harness/outputs/qc-output.qc.mlir"),
          clEnumValN(EmitMode::Dot, "dot",
                     "Write the view-op-graph Graphviz output to "
                     "PBC_Harness/outputs/qc-output.dot")),
      llvm::cl::init(EmitMode::Stdout));
  llvm::cl::opt<bool> simplify(
      "simplify",
      llvm::cl::desc("Run canonicalize and CSE before emitting the IR"),
      llvm::cl::init(false));
  llvm::cl::ParseCommandLineOptions(argc, argv, "PBC simple setup harness\n");

  /*
   * Preparing Dialects
   */
  DialectRegistry registry;
  registry.insert<arith::ArithDialect, cbit::CBitDialect, func::FuncDialect,
                  math::MathDialect, memref::MemRefDialect,
                  mlir::mqt::MQTDialect, qc::QCDialect, scf::SCFDialect>();
  MLIRContext context;
  context.appendDialectRegistry(registry);
  context.loadAllAvailableDialects();

  /*
   * QC-MLIR preparation
   */
  std::string qasmString = BELL_STATE_QASM;
  auto moduleOp = qc::translateOpenQASMToQC(qasmString, &context);
  if (!moduleOp) {
    // been told errors go to the context, so I shouldn't need to do stuff here
    return 1;
  }

  if (simplify && failed(simplifyModule(*moduleOp))) {
    return 1;
  }

  /*
   * Time to implement a PBC conversion pass
   */

  /*
   * Output MLIR
   */
  if (emitMode == EmitMode::Stdout) {
    moduleOp->print(llvm::outs());
    return 0;
  }

  llvm::SmallString<128> outputFile(PBC_HARNESS_OUTPUT_DIR);
  std::error_code ec = llvm::sys::fs::create_directories(outputFile);
  if (ec) {
    llvm::errs() << "Cannot create " << outputFile << ": " << ec.message()
                 << "\n";
    return 1;
  }
  llvm::sys::path::append(outputFile, emitMode == EmitMode::File
                                          ? MLIR_OUTPUT_FILE
                                          : DOT_OUTPUT_FILE);
  llvm::raw_fd_ostream os(outputFile, ec);
  if (ec) {
    llvm::errs() << "Cannot open " << outputFile << ": " << ec.message()
                 << "\n";
    return 1;
  }

  if (emitMode == EmitMode::File) {
    moduleOp->print(os);
    return 0;
  }

  // The pass does not change the IR. It only prints the graph to the stream.
  auto pm = PassManager::on<ModuleOp>(&context);
  pm.addPass(createViewOpGraphPass(os));
  if (failed(pm.run(moduleOp->getOperation()))) {
    return 1;
  }

  return 0;
}
