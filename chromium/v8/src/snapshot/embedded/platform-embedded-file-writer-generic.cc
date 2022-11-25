// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/snapshot/embedded/platform-embedded-file-writer-generic.h"

#include <algorithm>
#include <cinttypes>

#include "src/common/globals.h"

namespace v8 {
namespace internal {

namespace {

const char* DirectiveAsString(DataDirective directive) {
  switch (directive) {
    case kByte:
      return ".byte";
    case kLong:
      return ".long";
    case kQuad:
      return ".quad";
    case kOcta:
      return ".octa";
  }
  UNREACHABLE();
}

}  // namespace

void PlatformEmbeddedFileWriterGeneric::SectionText() {
  if (target_os_ == EmbeddedTargetOs::kChromeOS) {
    fprintf(fp_, ".section .text.hot.embedded\n");
  } else if (target_os_ == EmbeddedTargetOs::kOS2) {
    // a.out doesn't support .section
    fprintf(fp_, ".text\n");
  } else {
    fprintf(fp_, ".section .text\n");
  }
}

void PlatformEmbeddedFileWriterGeneric::SectionData() {
  if (target_os_ == EmbeddedTargetOs::kOS2) {
    // a.out doesn't support .section
    fprintf(fp_, ".data\n");
  } else {
    fprintf(fp_, ".section .data\n");
  }
}

void PlatformEmbeddedFileWriterGeneric::SectionRoData() {
  if (target_os_ == EmbeddedTargetOs::kOS2) {
    // a.out doesn't support .section and has no .rodata,
    // use .text segment explicitly
    fprintf(fp_, ".text\n");
  } else {
    fprintf(fp_, ".section .rodata\n");
  }
}

void PlatformEmbeddedFileWriterGeneric::DeclareUint32(const char* name,
                                                      uint32_t value) {
  DeclareSymbolGlobal(name);
  DeclareLabel(name);
  IndentedDataDirective(kLong);
  fprintf(fp_, "%d", value);
  Newline();
}

void PlatformEmbeddedFileWriterGeneric::DeclarePointerToSymbol(
    const char* name, const char* target) {
  DeclareSymbolGlobal(name);
  DeclareLabel(name);
  fprintf(fp_, "  %s %s%s\n", DirectiveAsString(PointerSizeDirective()),
          symbol_prefix_, target);
}

void PlatformEmbeddedFileWriterGeneric::DeclareSymbolGlobal(const char* name) {
  fprintf(fp_, ".global %s%s\n", symbol_prefix_, name);
  // These symbols are not visible outside of the final binary, this allows for
  // reduced binary size, and less work for the dynamic linker.
  fprintf(fp_, ".hidden %s\n", name);
}

void PlatformEmbeddedFileWriterGeneric::AlignToCodeAlignment() {
  fprintf(fp_, ".balign 32\n");
}

void PlatformEmbeddedFileWriterGeneric::AlignToDataAlignment() {
  // On Windows ARM64, s390, PPC and possibly more platforms, aligned load
  // instructions are used to retrieve v8_Default_embedded_blob_ and/or
  // v8_Default_embedded_blob_size_. The generated instructions require the
  // load target to be aligned at 8 bytes (2^3).
  fprintf(fp_, ".balign 8\n");
}

void PlatformEmbeddedFileWriterGeneric::Comment(const char* string) {
  fprintf(fp_, "// %s\n", string);
}

void PlatformEmbeddedFileWriterGeneric::DeclareLabel(const char* name) {
  fprintf(fp_, "%s%s:\n", symbol_prefix_, name);
}

void PlatformEmbeddedFileWriterGeneric::SourceInfo(int fileid,
                                                   const char* filename,
                                                   int line) {
  if (target_os_ == EmbeddedTargetOs::kOS2) {
    // a.out doesn't support .debug_line/.debug_info sections.
    // TODO: use .stabs/.stabd instead.
  } else {
    fprintf(fp_, ".loc %d %d\n", fileid, line);
  }
}

void PlatformEmbeddedFileWriterGeneric::DeclareFunctionBegin(const char* name,
                                                             uint32_t size) {
  if (ENABLE_CONTROL_FLOW_INTEGRITY_BOOL) {
    DeclareSymbolGlobal(name);
  }

  DeclareLabel(name);

  if (target_arch_ == EmbeddedTargetArch::kArm ||
      target_arch_ == EmbeddedTargetArch::kArm64) {
    // ELF format binaries on ARM use ".type <function name>, %function"
    // to create a DWARF subprogram entry.
    fprintf(fp_, ".type %s%s, %%function\n", symbol_prefix_, name);
  } else {
    // Other ELF Format binaries use ".type <function name>, @function"
    // to create a DWARF subprogram entry.
    fprintf(fp_, ".type %s%s, @function\n", symbol_prefix_, name);
  }
  fprintf(fp_, ".size %s%s, %u\n", symbol_prefix_, name, size);
}

void PlatformEmbeddedFileWriterGeneric::DeclareFunctionEnd(const char* name) {}

void PlatformEmbeddedFileWriterGeneric::FilePrologue() {
  // TODO(v8:10026): Add ELF note required for BTI.
}

void PlatformEmbeddedFileWriterGeneric::DeclareExternalFilename(
    int fileid, const char* filename) {
  // Replace any Windows style paths (backslashes) with forward
  // slashes.
  std::string fixed_filename(filename);
  std::replace(fixed_filename.begin(), fixed_filename.end(), '\\', '/');
  fprintf(fp_, ".file %d \"%s\"\n", fileid, fixed_filename.c_str());
}

void PlatformEmbeddedFileWriterGeneric::FileEpilogue() {
  if (target_os_ != EmbeddedTargetOs::kOS2) {
    // Omitting this section can imply an executable stack, which is usually
    // a linker warning/error. C++ compilers add these automatically, but
    // compiling assembly requires the .note.GNU-stack section to be inserted
    // manually.
    // Additional documentation:
    // https://wiki.gentoo.org/wiki/Hardened/GNU_stack_quickstart
    fprintf(fp_, ".section .note.GNU-stack,\"\",%%progbits\n");
  }
}

int PlatformEmbeddedFileWriterGeneric::IndentedDataDirective(
    DataDirective directive) {
  return fprintf(fp_, "  %s ", DirectiveAsString(directive));
}

DataDirective PlatformEmbeddedFileWriterGeneric::ByteChunkDataDirective()
    const {
#if defined(V8_TARGET_ARCH_MIPS) || defined(V8_TARGET_ARCH_MIPS64)
  // MIPS uses a fixed 4 byte instruction set, using .long
  // to prevent any unnecessary padding.
  return kLong;
#else
  // Other ISAs just listen to the base
  return PlatformEmbeddedFileWriterBase::ByteChunkDataDirective();
#endif
}

}  // namespace internal
}  // namespace v8
