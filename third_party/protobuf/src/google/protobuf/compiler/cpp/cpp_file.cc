// Protocol Buffers - Google's data interchange format
// Copyright 2008 Google Inc.  All rights reserved.
// http://code.google.com/p/protobuf/
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//     * Neither the name of Google Inc. nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

// Author: kenton@google.com (Kenton Varda)
//  Based on original Protocol Buffers design by
//  Sanjay Ghemawat, Jeff Dean, and others.

#include <google/protobuf/compiler/cpp/cpp_file.h>
#include <google/protobuf/compiler/cpp/cpp_enum.h>
#include <google/protobuf/compiler/cpp/cpp_service.h>
#include <google/protobuf/compiler/cpp/cpp_extension.h>
#include <google/protobuf/compiler/cpp/cpp_helpers.h>
#include <google/protobuf/compiler/cpp/cpp_message.h>
#include <google/protobuf/compiler/cpp/cpp_field.h>
#include <google/protobuf/io/printer.h>
#include <google/protobuf/descriptor.pb.h>
#include <google/protobuf/stubs/strutil.h>

namespace google {
namespace protobuf {
namespace compiler {
namespace cpp {

// ===================================================================

FileGenerator::FileGenerator(const FileDescriptor* file,
                             const string& dllexport_decl)
  : file_(file),
    message_generators_(
      new scoped_ptr<MessageGenerator>[file->message_type_count()]),
    enum_generators_(
      new scoped_ptr<EnumGenerator>[file->enum_type_count()]),
    service_generators_(
      new scoped_ptr<ServiceGenerator>[file->service_count()]),
    extension_generators_(
      new scoped_ptr<ExtensionGenerator>[file->extension_count()]),
    dllexport_decl_(dllexport_decl) {

  for (int i = 0; i < file->message_type_count(); i++) {
    message_generators_[i].reset(
      new MessageGenerator(file->message_type(i), dllexport_decl));
  }

  for (int i = 0; i < file->enum_type_count(); i++) {
    enum_generators_[i].reset(
      new EnumGenerator(file->enum_type(i), dllexport_decl));
  }

  for (int i = 0; i < file->service_count(); i++) {
    service_generators_[i].reset(
      new ServiceGenerator(file->service(i), dllexport_decl));
  }

  for (int i = 0; i < file->extension_count(); i++) {
    extension_generators_[i].reset(
      new ExtensionGenerator(file->extension(i), dllexport_decl));
  }

  SplitStringUsing(file_->package(), ".", &package_parts_);
}

FileGenerator::~FileGenerator() {}

void FileGenerator::GenerateHeader(io::Printer* printer) {
  string filename_identifier = FilenameIdentifier(file_->name());

  // Generate top of header.
  printer->Print(
    "// Generated by the protocol buffer compiler.  DO NOT EDIT!\n"
    "// source: $filename$\n"
    "\n"
    "#ifndef PROTOBUF_$filename_identifier$__INCLUDED\n"
    "#define PROTOBUF_$filename_identifier$__INCLUDED\n"
    "\n"
    "#include <string>\n"
    "\n",
    "filename", file_->name(),
    "filename_identifier", filename_identifier);

  printer->Print(
    "#include <google/protobuf/stubs/common.h>\n"
    "\n");

  // Verify the protobuf library header version is compatible with the protoc
  // version before going any further.
  printer->Print(
    "#if GOOGLE_PROTOBUF_VERSION < $min_header_version$\n"
    "#error This file was generated by a newer version of protoc which is\n"
    "#error incompatible with your Protocol Buffer headers.  Please update\n"
    "#error your headers.\n"
    "#endif\n"
    "#if $protoc_version$ < GOOGLE_PROTOBUF_MIN_PROTOC_VERSION\n"
    "#error This file was generated by an older version of protoc which is\n"
    "#error incompatible with your Protocol Buffer headers.  Please\n"
    "#error regenerate this file with a newer version of protoc.\n"
    "#endif\n"
    "\n",
    "min_header_version",
      SimpleItoa(protobuf::internal::kMinHeaderVersionForProtoc),
    "protoc_version", SimpleItoa(GOOGLE_PROTOBUF_VERSION));

  // OK, it's now safe to #include other files.
  printer->Print(
    "#include <google/protobuf/generated_message_util.h>\n"
    "#include <google/protobuf/repeated_field.h>\n"
    "#include <google/protobuf/extension_set.h>\n");

  if (HasDescriptorMethods(file_)) {
    printer->Print(
      "#include <google/protobuf/generated_message_reflection.h>\n");
  }

  if (HasGenericServices(file_)) {
    printer->Print(
      "#include <google/protobuf/service.h>\n");
  }


  for (int i = 0; i < file_->dependency_count(); i++) {
    printer->Print(
      "#include \"$dependency$.pb.h\"\n",
      "dependency", StripProto(file_->dependency(i)->name()));
  }

  printer->Print(
    "// @@protoc_insertion_point(includes)\n");

  // Open namespace.
  GenerateNamespaceOpeners(printer);

  // Forward-declare the AddDescriptors, AssignDescriptors, and ShutdownFile
  // functions, so that we can declare them to be friends of each class.
  printer->Print(
    "\n"
    "// Internal implementation detail -- do not call these.\n"
    "void $dllexport_decl$ $adddescriptorsname$();\n",
    "adddescriptorsname", GlobalAddDescriptorsName(file_->name()),
    "dllexport_decl", dllexport_decl_);

  printer->Print(
    // Note that we don't put dllexport_decl on these because they are only
    // called by the .pb.cc file in which they are defined.
    "void $assigndescriptorsname$();\n"
    "void $shutdownfilename$();\n"
    "\n",
    "assigndescriptorsname", GlobalAssignDescriptorsName(file_->name()),
    "shutdownfilename", GlobalShutdownFileName(file_->name()));

  // Generate forward declarations of classes.
  for (int i = 0; i < file_->message_type_count(); i++) {
    message_generators_[i]->GenerateForwardDeclaration(printer);
  }

  printer->Print("\n");

  // Generate enum definitions.
  for (int i = 0; i < file_->message_type_count(); i++) {
    message_generators_[i]->GenerateEnumDefinitions(printer);
  }
  for (int i = 0; i < file_->enum_type_count(); i++) {
    enum_generators_[i]->GenerateDefinition(printer);
  }

  printer->Print(kThickSeparator);
  printer->Print("\n");

  // Generate class definitions.
  for (int i = 0; i < file_->message_type_count(); i++) {
    if (i > 0) {
      printer->Print("\n");
      printer->Print(kThinSeparator);
      printer->Print("\n");
    }
    message_generators_[i]->GenerateClassDefinition(printer);
  }

  printer->Print("\n");
  printer->Print(kThickSeparator);
  printer->Print("\n");

  if (HasGenericServices(file_)) {
    // Generate service definitions.
    for (int i = 0; i < file_->service_count(); i++) {
      if (i > 0) {
        printer->Print("\n");
        printer->Print(kThinSeparator);
        printer->Print("\n");
      }
      service_generators_[i]->GenerateDeclarations(printer);
    }

    printer->Print("\n");
    printer->Print(kThickSeparator);
    printer->Print("\n");
  }

  // Declare extension identifiers.
  for (int i = 0; i < file_->extension_count(); i++) {
    extension_generators_[i]->GenerateDeclaration(printer);
  }

  printer->Print("\n");
  printer->Print(kThickSeparator);
  printer->Print("\n");

  // Generate class inline methods.
  for (int i = 0; i < file_->message_type_count(); i++) {
    if (i > 0) {
      printer->Print(kThinSeparator);
      printer->Print("\n");
    }
    message_generators_[i]->GenerateInlineMethods(printer);
  }

  printer->Print(
    "\n"
    "// @@protoc_insertion_point(namespace_scope)\n");

  // Close up namespace.
  GenerateNamespaceClosers(printer);

  // Emit GetEnumDescriptor specializations into google::protobuf namespace:
  if (HasDescriptorMethods(file_)) {
    // The SWIG conditional is to avoid a null-pointer dereference
    // (bug 1984964) in swig-1.3.21 resulting from the following syntax:
    //   namespace X { void Y<Z::W>(); }
    // which appears in GetEnumDescriptor() specializations.
    printer->Print(
        "\n"
        "#ifndef SWIG\n"
        "namespace google {\nnamespace protobuf {\n"
        "\n");
    for (int i = 0; i < file_->message_type_count(); i++) {
      message_generators_[i]->GenerateGetEnumDescriptorSpecializations(printer);
    }
    for (int i = 0; i < file_->enum_type_count(); i++) {
      enum_generators_[i]->GenerateGetEnumDescriptorSpecializations(printer);
    }
    printer->Print(
        "\n"
        "}  // namespace google\n}  // namespace protobuf\n"
        "#endif  // SWIG\n");
  }

  printer->Print(
    "\n"
    "// @@protoc_insertion_point(global_scope)\n"
    "\n");

  printer->Print(
    "#endif  // PROTOBUF_$filename_identifier$__INCLUDED\n",
    "filename_identifier", filename_identifier);
}

void FileGenerator::GenerateSource(io::Printer* printer) {
  printer->Print(
    "// Generated by the protocol buffer compiler.  DO NOT EDIT!\n"
    "\n"
    // The generated code calls accessors that might be deprecated. We don't
    // want the compiler to warn in generated code.
    "#define INTERNAL_SUPPRESS_PROTOBUF_FIELD_DEPRECATION\n"
    "#include \"$basename$.pb.h\"\n"
    "\n"
    "#include <algorithm>\n"    // for swap()
    "\n"
    "#include <google/protobuf/stubs/once.h>\n"
    "#include <google/protobuf/io/coded_stream.h>\n"
    "#include <google/protobuf/wire_format_lite_inl.h>\n",
    "basename", StripProto(file_->name()));

  if (HasDescriptorMethods(file_)) {
    printer->Print(
      "#include <google/protobuf/descriptor.h>\n"
      "#include <google/protobuf/reflection_ops.h>\n"
      "#include <google/protobuf/wire_format.h>\n");
  }

  printer->Print(
    "// @@protoc_insertion_point(includes)\n");

  GenerateNamespaceOpeners(printer);

  if (HasDescriptorMethods(file_)) {
    printer->Print(
      "\n"
      "namespace {\n"
      "\n");
    for (int i = 0; i < file_->message_type_count(); i++) {
      message_generators_[i]->GenerateDescriptorDeclarations(printer);
    }
    for (int i = 0; i < file_->enum_type_count(); i++) {
      printer->Print(
        "const ::google::protobuf::EnumDescriptor* $name$_descriptor_ = NULL;\n",
        "name", ClassName(file_->enum_type(i), false));
    }

    if (HasGenericServices(file_)) {
      for (int i = 0; i < file_->service_count(); i++) {
        printer->Print(
          "const ::google::protobuf::ServiceDescriptor* $name$_descriptor_ = NULL;\n",
          "name", file_->service(i)->name());
      }
    }

    printer->Print(
      "\n"
      "}  // namespace\n"
      "\n");
  }

  // Define our externally-visible BuildDescriptors() function.  (For the lite
  // library, all this does is initialize default instances.)
  GenerateBuildDescriptors(printer);

  // Generate enums.
  for (int i = 0; i < file_->enum_type_count(); i++) {
    enum_generators_[i]->GenerateMethods(printer);
  }

  // Generate classes.
  for (int i = 0; i < file_->message_type_count(); i++) {
    printer->Print("\n");
    printer->Print(kThickSeparator);
    printer->Print("\n");
    message_generators_[i]->GenerateClassMethods(printer);
  }

  if (HasGenericServices(file_)) {
    // Generate services.
    for (int i = 0; i < file_->service_count(); i++) {
      if (i == 0) printer->Print("\n");
      printer->Print(kThickSeparator);
      printer->Print("\n");
      service_generators_[i]->GenerateImplementation(printer);
    }
  }

  // Define extensions.
  for (int i = 0; i < file_->extension_count(); i++) {
    extension_generators_[i]->GenerateDefinition(printer);
  }

  printer->Print(
    "\n"
    "// @@protoc_insertion_point(namespace_scope)\n");

  GenerateNamespaceClosers(printer);

  printer->Print(
    "\n"
    "// @@protoc_insertion_point(global_scope)\n");
}

void FileGenerator::GenerateBuildDescriptors(io::Printer* printer) {
  // AddDescriptors() is a file-level procedure which adds the encoded
  // FileDescriptorProto for this .proto file to the global DescriptorPool
  // for generated files (DescriptorPool::generated_pool()).  It always runs
  // at static initialization time, so all files will be registered before
  // main() starts.  This procedure also constructs default instances and
  // registers extensions.
  //
  // Its sibling, AssignDescriptors(), actually pulls the compiled
  // FileDescriptor from the DescriptorPool and uses it to populate all of
  // the global variables which store pointers to the descriptor objects.
  // It also constructs the reflection objects.  It is called the first time
  // anyone calls descriptor() or GetReflection() on one of the types defined
  // in the file.

  // In optimize_for = LITE_RUNTIME mode, we don't generate AssignDescriptors()
  // and we only use AddDescriptors() to allocate default instances.
  if (HasDescriptorMethods(file_)) {
    printer->Print(
      "\n"
      "void $assigndescriptorsname$() {\n",
      "assigndescriptorsname", GlobalAssignDescriptorsName(file_->name()));
    printer->Indent();

    // Make sure the file has found its way into the pool.  If a descriptor
    // is requested *during* static init then AddDescriptors() may not have
    // been called yet, so we call it manually.  Note that it's fine if
    // AddDescriptors() is called multiple times.
    printer->Print(
      "$adddescriptorsname$();\n",
      "adddescriptorsname", GlobalAddDescriptorsName(file_->name()));

    // Get the file's descriptor from the pool.
    printer->Print(
      "const ::google::protobuf::FileDescriptor* file =\n"
      "  ::google::protobuf::DescriptorPool::generated_pool()->FindFileByName(\n"
      "    \"$filename$\");\n"
      // Note that this GOOGLE_CHECK is necessary to prevent a warning about "file"
      // being unused when compiling an empty .proto file.
      "GOOGLE_CHECK(file != NULL);\n",
      "filename", file_->name());

    // Go through all the stuff defined in this file and generated code to
    // assign the global descriptor pointers based on the file descriptor.
    for (int i = 0; i < file_->message_type_count(); i++) {
      message_generators_[i]->GenerateDescriptorInitializer(printer, i);
    }
    for (int i = 0; i < file_->enum_type_count(); i++) {
      enum_generators_[i]->GenerateDescriptorInitializer(printer, i);
    }
    if (HasGenericServices(file_)) {
      for (int i = 0; i < file_->service_count(); i++) {
        service_generators_[i]->GenerateDescriptorInitializer(printer, i);
      }
    }

    printer->Outdent();
    printer->Print(
      "}\n"
      "\n");

    // ---------------------------------------------------------------

    // protobuf_AssignDescriptorsOnce():  The first time it is called, calls
    // AssignDescriptors().  All later times, waits for the first call to
    // complete and then returns.
    printer->Print(
      "namespace {\n"
      "\n"
      "GOOGLE_PROTOBUF_DECLARE_ONCE(protobuf_AssignDescriptors_once_);\n"
      "inline void protobuf_AssignDescriptorsOnce() {\n"
      "  ::google::protobuf::GoogleOnceInit(&protobuf_AssignDescriptors_once_,\n"
      "                 &$assigndescriptorsname$);\n"
      "}\n"
      "\n",
      "assigndescriptorsname", GlobalAssignDescriptorsName(file_->name()));

    // protobuf_RegisterTypes():  Calls
    // MessageFactory::InternalRegisterGeneratedType() for each message type.
    printer->Print(
      "void protobuf_RegisterTypes(const ::std::string&) {\n"
      "  protobuf_AssignDescriptorsOnce();\n");
    printer->Indent();

    for (int i = 0; i < file_->message_type_count(); i++) {
      message_generators_[i]->GenerateTypeRegistrations(printer);
    }

    printer->Outdent();
    printer->Print(
      "}\n"
      "\n"
      "}  // namespace\n");
  }

  // -----------------------------------------------------------------

  // ShutdownFile():  Deletes descriptors, default instances, etc. on shutdown.
  printer->Print(
    "\n"
    "void $shutdownfilename$() {\n",
    "shutdownfilename", GlobalShutdownFileName(file_->name()));
  printer->Indent();

  for (int i = 0; i < file_->message_type_count(); i++) {
    message_generators_[i]->GenerateShutdownCode(printer);
  }

  printer->Outdent();
  printer->Print(
    "}\n");

  // -----------------------------------------------------------------

  // Now generate the AddDescriptors() function.
  printer->Print(
    "\n"
    "void $adddescriptorsname$() {\n"
    // We don't need any special synchronization here because this code is
    // called at static init time before any threads exist.
    "  static bool already_here = false;\n"
    "  if (already_here) return;\n"
    "  already_here = true;\n"
    "  GOOGLE_PROTOBUF_VERIFY_VERSION;\n"
    "\n",
    "adddescriptorsname", GlobalAddDescriptorsName(file_->name()));
  printer->Indent();

  // Call the AddDescriptors() methods for all of our dependencies, to make
  // sure they get added first.
  for (int i = 0; i < file_->dependency_count(); i++) {
    const FileDescriptor* dependency = file_->dependency(i);
    // Print the namespace prefix for the dependency.
    vector<string> dependency_package_parts;
    SplitStringUsing(dependency->package(), ".", &dependency_package_parts);
    printer->Print("::");
    for (int i = 0; i < dependency_package_parts.size(); i++) {
      printer->Print("$name$::",
                     "name", dependency_package_parts[i]);
    }
    // Call its AddDescriptors function.
    printer->Print(
      "$name$();\n",
      "name", GlobalAddDescriptorsName(dependency->name()));
  }

  if (HasDescriptorMethods(file_)) {
    // Embed the descriptor.  We simply serialize the entire FileDescriptorProto
    // and embed it as a string literal, which is parsed and built into real
    // descriptors at initialization time.
    FileDescriptorProto file_proto;
    file_->CopyTo(&file_proto);
    string file_data;
    file_proto.SerializeToString(&file_data);

    printer->Print(
      "::google::protobuf::DescriptorPool::InternalAddGeneratedFile(");

    // Only write 40 bytes per line.
    static const int kBytesPerLine = 40;
    for (int i = 0; i < file_data.size(); i += kBytesPerLine) {
      printer->Print("\n  \"$data$\"",
        "data", CEscape(file_data.substr(i, kBytesPerLine)));
    }
    printer->Print(
      ", $size$);\n",
      "size", SimpleItoa(file_data.size()));

    // Call MessageFactory::InternalRegisterGeneratedFile().
    printer->Print(
      "::google::protobuf::MessageFactory::InternalRegisterGeneratedFile(\n"
      "  \"$filename$\", &protobuf_RegisterTypes);\n",
      "filename", file_->name());
  }

  // Allocate and initialize default instances.  This can't be done lazily
  // since default instances are returned by simple accessors and are used with
  // extensions.  Speaking of which, we also register extensions at this time.
  for (int i = 0; i < file_->message_type_count(); i++) {
    message_generators_[i]->GenerateDefaultInstanceAllocator(printer);
  }
  for (int i = 0; i < file_->extension_count(); i++) {
    extension_generators_[i]->GenerateRegistration(printer);
  }
  for (int i = 0; i < file_->message_type_count(); i++) {
    message_generators_[i]->GenerateDefaultInstanceInitializer(printer);
  }

  printer->Print(
    "::google::protobuf::internal::OnShutdown(&$shutdownfilename$);\n",
    "shutdownfilename", GlobalShutdownFileName(file_->name()));

  printer->Outdent();

  printer->Print(
    "}\n"
    "\n"
    "// Force AddDescriptors() to be called at static initialization time.\n"
    "struct StaticDescriptorInitializer_$filename$ {\n"
    "  StaticDescriptorInitializer_$filename$() {\n"
    "    $adddescriptorsname$();\n"
    "  }\n"
    "} static_descriptor_initializer_$filename$_;\n"
    "\n",
    "adddescriptorsname", GlobalAddDescriptorsName(file_->name()),
    "filename", FilenameIdentifier(file_->name()));
}

void FileGenerator::GenerateNamespaceOpeners(io::Printer* printer) {
  if (package_parts_.size() > 0) printer->Print("\n");

  for (int i = 0; i < package_parts_.size(); i++) {
    printer->Print("namespace $part$ {\n",
                   "part", package_parts_[i]);
  }
}

void FileGenerator::GenerateNamespaceClosers(io::Printer* printer) {
  if (package_parts_.size() > 0) printer->Print("\n");

  for (int i = package_parts_.size() - 1; i >= 0; i--) {
    printer->Print("}  // namespace $part$\n",
                   "part", package_parts_[i]);
  }
}

}  // namespace cpp
}  // namespace compiler
}  // namespace protobuf
}  // namespace google
