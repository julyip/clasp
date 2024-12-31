
/*
    File: gc_interface.cc
*/

/*
Copyright (c) 2014, Christian E. Schafmeister

CLASP is free software; you can redistribute it and/or
modify it under the terms of the GNU Library General Public
License as published by the Free Software Foundation; either
version 2 of the License, or (at your option) any later version.

See directory 'clasp/licenses' for full details.

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.
*/
/* -^- */

#ifndef SCRAPING // #endif at bottom

#include <clasp/gctools/exposeCommon.h>

namespace gctools {

/* This is where the class_layout codes are included
from clasp_gc.cc
They are generated by the layout analyzer. */

Layout_code* get_stamp_layout_codes() {
  static Layout_code codes[] = {
#if defined(USE_PRECISE_GC)
#ifndef RUNNING_PRECISEPREP
  // sometimes we get sizeof(NIL) - this will quiet those errors
  // This may be a terrible idea because it may hide deeper problems
#define NIL uintptr_t

#define GC_OBJ_SCAN_HELPERS
#include CLASP_GC_CC
#undef GC_OBJ_SCAN_HELPERS
#undef NIL
#endif // #ifndef RUNNING_PRECISEPREP
#endif // #if defined(USE_PRECISE_GC)
    {layout_end, 0, 0, 0, 0, ""}
  };
  return &codes[0];
};
}; // namespace gctools

extern void initialize_exposeFunctions1();
extern void initialize_exposeFunctions2();
extern void initialize_exposeFunctions3();

void initialize_functions() {
  //  printf("%s:%d About to initialize_functions\n", __FILE__, __LINE__ );
  initialize_exposeFunctions1();
  initialize_exposeFunctions2();
  initialize_exposeFunctions3();
};

extern "C" {
using namespace gctools;

size_t obj_kind(core::T_O* tagged_ptr) {
  const core::T_O* client = untag_object<const core::T_O*>(tagged_ptr);
  const Header_s* header = reinterpret_cast<const Header_s*>(GeneralPtrToHeaderPtr(client));
  return (size_t)(header->_badge_stamp_wtag_mtag.stamp_());
}

const char* obj_kind_name(core::T_O* tagged_ptr) {
  core::T_O* client = untag_object<core::T_O*>(tagged_ptr);
  const Header_s* header = reinterpret_cast<const Header_s*>(GeneralPtrToHeaderPtr(client));
  return obj_name(header->_badge_stamp_wtag_mtag.stamp_());
}

bool valid_stamp(gctools::stamp_t stamp) {
#if defined(USE_BOEHM)
  if (stamp <= STAMP_UNSHIFT_WTAG(gctools::STAMPWTAG_max)) {
    return true;
  }
  return false;
#else
  MISSING_GC_SUPPORT();
#endif
}
const char* obj_name(gctools::stamp_t stamp) {
#if defined(USE_BOEHM)
  if (stamp <= global_unshifted_nowhere_stamp_names.size()) {
    //    printf("%s:%d obj_name stamp= %lu\n", __FILE__, __LINE__, stamp);
    return global_unshifted_nowhere_stamp_names[stamp].c_str();
  }
  printf("%s:%d obj_name stamp = %lu is out of bounds - max is %lu\n", __FILE__, __LINE__, (uintptr_t)stamp,
         global_unshifted_nowhere_stamp_names.size());
  return "BoehmNoClass";
#else
  MISSING_GC_SUPPORT();
#endif
}

/*! I'm using a format_header so MPS gives me the object-pointer */
#define GC_DEALLOCATOR_METHOD
void obj_deallocate_unmanaged_instance(gctools::smart_ptr<core::T_O> obj) {
  void* client = &*obj;
  // The client must have a valid header
#if defined(USE_PRECISE_GC)
#ifndef RUNNING_PRECISEPREP
#define GC_OBJ_DEALLOCATOR_TABLE
#include CLASP_GC_CC
#undef GC_OBJ_DEALLOCATOR_TABLE
#endif
#endif

  const gctools::Header_s* header = reinterpret_cast<const gctools::Header_s*>(GeneralPtrToHeaderPtr(client));
  ASSERTF(header->_badge_stamp_wtag_mtag.stampP(), "obj_deallocate_unmanaged_instance called without a valid object");
  gctools::GCStampEnum stamp = (GCStampEnum)(header->_badge_stamp_wtag_mtag.stamp_());
#ifndef RUNNING_PRECISEPREP
#if defined(USE_MPS) || defined(USE_PRECISE_GC)
  size_t jump_table_index = (size_t)stamp; // - stamp_first_general;
  printf("%s:%d Calculated jump_table_index %lu\n", __FILE__, __LINE__, jump_table_index);
  goto*(OBJ_DEALLOCATOR_table[jump_table_index]);
#define GC_OBJ_DEALLOCATOR
#include CLASP_GC_CC
#undef GC_OBJ_DEALLOCATOR
#endif // USE_MPS
#endif
};
#undef GC_DEALLOCATOR_METHOD
};

// ----------------------------------------------------------------------
//
// Declare all global symbols
//
//

#define ADJUST_SYMBOL_INDEX(_xx_) (_xx_ + NUMBER_OF_CORE_SYMBOLS)
#define DECLARE_ALL_SYMBOLS
#ifndef SCRAPING
#include SYMBOLS_SCRAPED_INC_H
#endif
#undef DECLARE_ALL_SYMBOLS
#undef ADJUST_SYMBOL_INDEX

//
// Bootstrapping
//

void setup_bootstrap_packages(core::BootStrapCoreSymbolMap* bootStrapSymbolMap) {
#define BOOTSTRAP_PACKAGES
#ifndef SCRAPING
#include SYMBOLS_SCRAPED_INC_H
#endif
#undef BOOTSTRAP_PACKAGES
}

template <class TheClass> void set_one_static_class_symbol(core::BootStrapCoreSymbolMap* symbols, const std::string& full_name) {
  std::string orig_package_part, orig_symbol_part;
  core::colon_split(full_name, orig_package_part, orig_symbol_part);
  std::string package_part, symbol_part;
  package_part = core::lispify_symbol_name(orig_package_part);
  symbol_part = core::lispify_symbol_name(orig_symbol_part);
  //  printf("%s:%d set_one_static_class_symbol --> %s:%s\n", __FILE__, __LINE__, package_part.c_str(), symbol_part.c_str() );
  core::SymbolStorage store;
  bool found = symbols->find_symbol(package_part, symbol_part, store);
  if (!found) {
    printf("%s:%d ERROR!!!! The static class symbol %s was not found orig_symbol_part=|%s| symbol_part=|%s|!\n", __FILE__, __LINE__,
           full_name.c_str(), orig_symbol_part.c_str(), symbol_part.c_str());
    abort();
  }
  if (store._PackageName != package_part) {
    printf("%s:%d For symbol %s there is a mismatch in the package desired %s and the one retrieved %s\n", __FILE__, __LINE__,
           full_name.c_str(), package_part.c_str(), store._PackageName.c_str());
    SIMPLE_ERROR("Mismatch of package when setting a class symbol");
  }
  //  printf("%s:%d Setting static_class_symbol to %s\n", __FILE__, __LINE__, _safe_rep_(store._Symbol).c_str());
  TheClass::set_static_class_symbol(store._Symbol);
}

void set_static_class_symbols(core::BootStrapCoreSymbolMap* bootStrapSymbolMap) {
#ifndef SCRAPING
  // Another place where we include INIT_CLASSES_INC_H when USE_PRECISE_GC
#define SET_CLASS_SYMBOLS
#include INIT_CLASSES_INC_H
#undef SET_CLASS_SYMBOLS
#endif
}

#define ALLOCATE_ALL_SYMBOLS_HELPERS
#undef ALLOCATE_ALL_SYMBOLS
#ifndef SCRAPING
#include SYMBOLS_SCRAPED_INC_H
#endif
#undef ALLOCATE_ALL_SYMBOLS_HELPERS

void allocate_symbols(core::BootStrapCoreSymbolMap* symbols){
#define ALLOCATE_ALL_SYMBOLS
#ifndef SCRAPING
#include SYMBOLS_SCRAPED_INC_H
#endif
#undef ALLOCATE_ALL_SYMBOLS
};

extern void initialize_exposeClasses1();
extern void initialize_exposeClasses2();
extern void initialize_exposeClasses3();

void initialize_classes_and_methods() {
  initialize_exposeClasses1();
  initialize_exposeClasses2();
  initialize_exposeClasses3();
}

void dumpBoehmLayoutTables(std::ostream& fout) {
#define LAYOUT_STAMP(_class_) (gctools::GCStamp<_class_>::StampWtag >> gctools::BaseHeader_s::wtag_width)
  fmt::print(fout, "# dumpBoehmLayoutTables when static analyzer output is not available\n");
#define Init_class_kind(_class_)                                                                                                   \
  fmt::print(fout, "Init_class_kind( stamp={}, name=\"{}\", size={});\n", LAYOUT_STAMP(_class_), #_class_, sizeof(*(_class_*)0x0));
#define Init_templated_kind(_class_)                                                                                               \
  fmt::print(fout, "Init_templated_kind( stamp={}, name=\"{}\", size={});\n", LAYOUT_STAMP(_class_), #_class_,                     \
             sizeof(*(_class_*)0x0));
#define Init__fixed_field(_class_, _index_, _type_, _field_name_)                                                                  \
  fmt::print(fout, "Init__fixed_field( stamp={}, index={}, data_type={},field_name=\"{}\",field_offset={});\n",                    \
             LAYOUT_STAMP(_class_), _index_, (int)_type_, #_field_name_, offsetof(_class_, _field_name_));
#define Init__variable_array0(_class_, _data_field_)                                                                               \
  fmt::print(fout, "Init__variable_array0( stamp={}, name=\"{}\", offset={} );\n", LAYOUT_STAMP(_class_), #_data_field_,           \
             offsetof(_class_, _data_field_));
#define Init__variable_capacity(_class_, _value_type_, _end_, _capacity_)                                                          \
  fmt::print(fout, "Init__variable_capacity( stamp={}, element_size={}, end_offset={}, capacity_offset={} );\n",                   \
             LAYOUT_STAMP(_class_), sizeof(_class_::_value_type_), offsetof(_class_, _end_), offsetof(_class_, _capacity_));
#define Init__variable_field(_class_, _data_type_, _index_, _field_name_, _field_offset_)                                          \
  fmt::print(fout, "Init__variable_field( stamp={}, index={}, data_type={}, field_name=\"{}\", field_offset={} );\n",              \
             LAYOUT_STAMP(_class_), _index_, (int)_data_type_, _field_name_, _field_offset_);
#define Init_global_ints(_name_, _value_) fmt::print(fout, "Init_global_ints(name=\"{}\",value={});\n", _name_, _value_);
  printf("Dumping interface\n");
  gctools::dump_data_types(fout, "");
  //  core::registerOrDumpDtreeInfo(fout);
  Init_class_kind(core::T_O);
  Init_class_kind(core::General_O);

  Init_class_kind(core::Cons_O);
  Init__fixed_field(core::Cons_O, 0, SMART_PTR_OFFSET, _Car);
  Init__fixed_field(core::Cons_O, 1, SMART_PTR_OFFSET, _Cdr);

  Init_class_kind(core::SimpleBaseString_O);
  Init__variable_array0(core::SimpleBaseString_O, _Data._Data);
  Init__variable_capacity(core::SimpleBaseString_O, value_type, _Data._MaybeSignedLength, _Data._MaybeSignedLength);
  Init__variable_field(core::SimpleBaseString_O, gctools::ctype_unsigned_char, 0, "only", 0);

  Init_class_kind(core::SimpleCharacterString_O);
  Init__variable_array0(core::SimpleCharacterString_O, _Data._Data);
  Init__variable_capacity(core::SimpleCharacterString_O, value_type, _Data._MaybeSignedLength, _Data._MaybeSignedLength);
  Init__variable_field(core::SimpleCharacterString_O, gctools::ctype_unsigned_int, 0, "only", 0);

  Init_class_kind(core::Function_O);

  Init_class_kind(core::Symbol_O);
  Init__fixed_field(core::Symbol_O, 0, SMART_PTR_OFFSET, _Name);
  Init__fixed_field(core::Symbol_O, 1, SMART_PTR_OFFSET, _HomePackage);
  Init__fixed_field(core::Symbol_O, 2, SMART_PTR_OFFSET, _Value);
  Init__fixed_field(core::Symbol_O, 3, SMART_PTR_OFFSET, _Function);
  Init__fixed_field(core::Symbol_O, 4, SMART_PTR_OFFSET, _SetfFunction);
  Init__fixed_field(core::Symbol_O, 5, SMART_PTR_OFFSET, _PropertyList);

  Init_class_kind(core::DestDynEnv_O);
  Init__fixed_field(core::DestDynEnv_O, 0, RAW_POINTER_OFFSET, target);

  Init_class_kind(core::LexDynEnv_O);
  Init__fixed_field(core::LexDynEnv_O, 0, RAW_POINTER_OFFSET, target);
  Init__fixed_field(core::LexDynEnv_O, 1, RAW_POINTER_OFFSET, frame);

  Init_class_kind(core::BlockDynEnv_O);
  Init__fixed_field(core::BlockDynEnv_O, 0, RAW_POINTER_OFFSET, target);
  Init__fixed_field(core::BlockDynEnv_O, 1, RAW_POINTER_OFFSET, frame);

  Init_class_kind(core::TagbodyDynEnv_O);
  Init__fixed_field(core::TagbodyDynEnv_O, 0, RAW_POINTER_OFFSET, target);
  Init__fixed_field(core::TagbodyDynEnv_O, 1, RAW_POINTER_OFFSET, frame);

  Init_class_kind(core::CatchDynEnv_O);
  Init__fixed_field(core::CatchDynEnv_O, 0, RAW_POINTER_OFFSET, target);
  Init__fixed_field(core::CatchDynEnv_O, 0, SMART_PTR_OFFSET, tag);

  Init_class_kind(core::UnwindProtectDynEnv_O);
  Init__fixed_field(core::UnwindProtectDynEnv_O, 0, RAW_POINTER_OFFSET, target);

  Init_class_kind(core::BindingDynEnv_O);
  Init__fixed_field(core::BindingDynEnv_O, 0, SMART_PTR_OFFSET, cell);
  Init__fixed_field(core::BindingDynEnv_O, 0, SMART_PTR_OFFSET, old);

  Init_class_kind(core::BytecodeModule_O);
  Init__fixed_field(core::BytecodeModule_O, 0, SMART_PTR_OFFSET, _Literals);
  Init__fixed_field(core::BytecodeModule_O, 1, SMART_PTR_OFFSET, _Bytecode);

  Init_class_kind(core::SimpleCoreFun_O);
  Init__fixed_field(core::SimpleCoreFun_O, 0, SMART_PTR_OFFSET, _TheSimpleFun);
  Init__fixed_field(core::SimpleCoreFun_O, 1, SMART_PTR_OFFSET, _FunctionDescription);
  Init__fixed_field(core::SimpleCoreFun_O, 2, SMART_PTR_OFFSET, _Code);
  for (int iii = 0; iii < NUMBER_OF_ENTRY_POINTS; iii++) {
    Init__fixed_field(core::SimpleCoreFun_O, 3 + iii, RAW_POINTER_OFFSET, _EntryPoints._EntryPoints[iii]);
  }
  Init__fixed_field(core::SimpleCoreFun_O, 3 + NUMBER_OF_ENTRY_POINTS, SMART_PTR_OFFSET, _localFun);

  Init_class_kind(core::BytecodeSimpleFun_O);
  Init__fixed_field(core::BytecodeSimpleFun_O, 0, SMART_PTR_OFFSET, _TheSimpleFun);
  Init__fixed_field(core::BytecodeSimpleFun_O, 1, SMART_PTR_OFFSET, _FunctionDescription);
  Init__fixed_field(core::BytecodeSimpleFun_O, 2, SMART_PTR_OFFSET, _Code);
  Init__fixed_field(core::BytecodeSimpleFun_O, 3, RAW_POINTER_OFFSET, _EntryPoints._EntryPoints[0]);

  Init_class_kind(core::FunctionDescription_O);
  Init__fixed_field(core::FunctionDescription_O, 0, SMART_PTR_OFFSET, _functionName);
  Init__fixed_field(core::FunctionDescription_O, 1, SMART_PTR_OFFSET, _sourcePathname);
  Init__fixed_field(core::FunctionDescription_O, 2, SMART_PTR_OFFSET, _lambdaList);
  Init__fixed_field(core::FunctionDescription_O, 3, SMART_PTR_OFFSET, _docstring);
  Init__fixed_field(core::FunctionDescription_O, 4, SMART_PTR_OFFSET, _declares);
  Init__fixed_field(core::FunctionDescription_O, 5, ctype_int, lineno);
  Init__fixed_field(core::FunctionDescription_O, 6, ctype_int, column);
  Init__fixed_field(core::FunctionDescription_O, 7, ctype_int, filepos);

  Init_class_kind(core::FuncallableInstance_O);
  Init__fixed_field(core::FuncallableInstance_O, 0, SMART_PTR_OFFSET, _TheSimpleFun);
  Init__fixed_field(core::FuncallableInstance_O, 1, SMART_PTR_OFFSET, _Rack);
  Init__fixed_field(core::FuncallableInstance_O, 2, SMART_PTR_OFFSET, _Class);
  Init__fixed_field(core::FuncallableInstance_O, 3, SMART_PTR_OFFSET, _RealFunction);

  Init_class_kind(core::Closure_O);
  Init__fixed_field(core::Closure_O, 0, SMART_PTR_OFFSET, _TheSimpleFun);
  Init__variable_array0(core::Closure_O, _Slots._Data);
  Init__variable_capacity(core::Closure_O, value_type, _Slots._MaybeSignedLength, _Slots._MaybeSignedLength);
  Init__variable_field(core::Closure_O, SMART_PTR_OFFSET, 0, "only", 0);

  Init_templated_kind(core::WrappedPointer_O);
  Init__fixed_field(core::WrappedPointer_O, 0, SMART_PTR_OFFSET, Class_);

  Init_class_kind(core::Package_O);
  Init__fixed_field(core::Package_O, 0, SMART_PTR_OFFSET, _InternalSymbols);
  Init__fixed_field(core::Package_O, 1, SMART_PTR_OFFSET, _ExternalSymbols);
  Init__fixed_field(core::Package_O, 2, SMART_PTR_OFFSET, _Shadowing);
  Init__fixed_field(core::Package_O, 3, SMART_PTR_OFFSET, _Name);
  Init__fixed_field(core::Package_O, 4, SMART_PTR_OFFSET, _Nicknames);
  Init__fixed_field(core::Package_O, 5, SMART_PTR_OFFSET, _LocalNicknames);
  Init__fixed_field(core::Package_O, 6, SMART_PTR_OFFSET, _Documentation);

  Init_class_kind(core::Instance_O);
  Init__fixed_field(core::Instance_O, 0, SMART_PTR_OFFSET, _Class);
  Init__fixed_field(core::Instance_O, 1, SMART_PTR_OFFSET, _Rack);

  Init_class_kind(core::Rack_O);
  Init__fixed_field(core::Rack_O, 0, ctype_size_t, _ShiftedStamp);
  Init__fixed_field(core::Rack_O, 1, SMART_PTR_OFFSET, _Sig);
  Init__variable_array0(core::Rack_O, _Slots);
  Init__variable_capacity(core::Rack_O, value_type, _Slots._Length, _Slots._Length);
  Init__variable_field(core::Rack_O, gctools::SMART_PTR_OFFSET, 0, "only", 0);

  Init_class_kind(core::Pathname_O);
  Init__fixed_field(core::Pathname_O, 0, SMART_PTR_OFFSET, _Host);
  Init__fixed_field(core::Pathname_O, 1, SMART_PTR_OFFSET, _Device);
  Init__fixed_field(core::Pathname_O, 2, SMART_PTR_OFFSET, _Directory);
  Init__fixed_field(core::Pathname_O, 3, SMART_PTR_OFFSET, _Name);
  Init__fixed_field(core::Pathname_O, 4, SMART_PTR_OFFSET, _Type);
  Init__fixed_field(core::Pathname_O, 5, SMART_PTR_OFFSET, _Version);

  Init_class_kind(core::LogicalPathname_O);
  Init__fixed_field(core::LogicalPathname_O, 0, SMART_PTR_OFFSET, _Host);
  Init__fixed_field(core::LogicalPathname_O, 1, SMART_PTR_OFFSET, _Device);
  Init__fixed_field(core::LogicalPathname_O, 2, SMART_PTR_OFFSET, _Directory);
  Init__fixed_field(core::LogicalPathname_O, 3, SMART_PTR_OFFSET, _Name);
  Init__fixed_field(core::LogicalPathname_O, 4, SMART_PTR_OFFSET, _Type);
  Init__fixed_field(core::LogicalPathname_O, 5, SMART_PTR_OFFSET, _Version);

  Init_class_kind(core::Vaslist_dummy_O);
  Init_class_kind(core::Unused_dummy_O);
  Init_class_kind(core::ClassHolder_O);
  Init_class_kind(core::SymbolToEnumConverter_O);
  Init_class_kind(llvmo::Attribute_O);
  Init_class_kind(llvmo::AttributeSet_O);
  Init_class_kind(core::ClassRepCreator_O);
  Init_class_kind(core::DerivableCxxClassCreator_O);
  Init_class_kind(core::FuncallableInstanceCreator_O);
  Init_class_kind(clbind::DummyCreator_O);
  Init_class_kind(core::InstanceCreator_O);
  Init_class_kind(core::StandardClassCreator_O);
  Init_class_kind(core::SingleDispatchGenericFunction_O);
  Init_class_kind(core::ImmobileObject_O);
  Init_class_kind(core::WeakPointer_O);
  Init_class_kind(llvmo::DebugLoc_O);
  Init_class_kind(core::Pointer_O);
  Init_class_kind(clasp_ffi::ForeignData_O);
  Init_class_kind(core::CxxObject_O);
  Init_class_kind(llvmo::MDBuilder_O);
  Init_class_kind(mp::ConditionVariable_O);
  Init_class_kind(core::NativeVector_int_O);
  Init_class_kind(llvmo::FunctionCallee_O);
  Init_class_kind(llvmo::DINodeArray_O);
  Init_class_kind(mp::Mutex_O);
  Init_class_kind(mp::RecursiveMutex_O);
  Init_class_kind(llvmo::DITypeRefArray_O);
  Init_class_kind(mp::SharedMutex_O);
  Init_class_kind(mp::Process_O);
  Init_class_kind(core::SingleDispatchMethod_O);
  Init_class_kind(core::Iterator_O);
  Init_class_kind(core::DirectoryIterator_O);
  Init_class_kind(core::RecursiveDirectoryIterator_O);
  Init_class_kind(core::Array_O);
  Init_class_kind(core::MDArray_O);
  Init_class_kind(core::MDArray_int16_t_O);
  Init_class_kind(core::MDArray_int8_t_O);
  Init_class_kind(core::MDArray_int32_t_O);
  Init_class_kind(core::MDArray_byte4_t_O);
  Init_class_kind(core::MDArray_float_O);
  Init_class_kind(core::MDArray_size_t_O);
  Init_class_kind(core::MDArray_byte8_t_O);
  Init_class_kind(core::MDArray_int64_t_O);
  Init_class_kind(core::MDArray_byte32_t_O);
  Init_class_kind(core::MDArray_byte2_t_O);
  Init_class_kind(core::MDArray_int2_t_O);
  Init_class_kind(core::MDArray_fixnum_O);
  Init_class_kind(core::MDArrayBaseChar_O);
  Init_class_kind(core::MDArray_byte64_t_O);
  Init_class_kind(core::MDArrayCharacter_O);
  Init_class_kind(core::MDArrayT_O);
  Init_class_kind(core::MDArrayBit_O);
  Init_class_kind(core::MDArray_byte16_t_O);
  Init_class_kind(core::SimpleMDArray_O);
  Init_class_kind(core::SimpleMDArray_int8_t_O);
  Init_class_kind(core::SimpleMDArray_short_float_O);
  Init_class_kind(core::SimpleMDArray_double_O);
  Init_class_kind(core::SimpleMDArray_long_float_O);
  Init_class_kind(core::SimpleMDArray_byte32_t_O);
  Init_class_kind(core::SimpleMDArrayT_O);
  Init_class_kind(core::SimpleMDArray_int2_t_O);
  Init_class_kind(core::SimpleMDArray_byte4_t_O);
  Init_class_kind(core::SimpleMDArray_int32_t_O);
  Init_class_kind(core::SimpleMDArray_float_O);
  Init_class_kind(core::SimpleMDArray_int16_t_O);
  Init_class_kind(core::SimpleMDArray_size_t_O);
  Init_class_kind(core::SimpleMDArray_int4_t_O);
  Init_class_kind(core::SimpleMDArrayCharacter_O);
  Init_class_kind(core::SimpleMDArray_byte2_t_O);
  Init_class_kind(core::SimpleMDArray_fixnum_O);
  Init_class_kind(core::SimpleMDArray_byte16_t_O);
  Init_class_kind(core::SimpleMDArrayBaseChar_O);
  Init_class_kind(core::SimpleMDArray_byte64_t_O);
  Init_class_kind(core::SimpleMDArrayBit_O);
  Init_class_kind(core::SimpleMDArray_byte8_t_O);
  Init_class_kind(core::SimpleMDArray_int64_t_O);
  Init_class_kind(core::MDArray_int4_t_O);
  Init_class_kind(core::MDArray_double_O);
  Init_class_kind(core::MDArray_short_float_O);
  Init_class_kind(core::MDArray_long_float_O);
  Init_class_kind(core::ComplexVector_O);
  Init_class_kind(core::ComplexVector_short_float_O);
  Init_class_kind(core::ComplexVector_double_O);
  Init_class_kind(core::ComplexVector_long_float_O);
  Init_class_kind(core::ComplexVector_int8_t_O);
  Init_class_kind(core::ComplexVector_byte64_t_O);
  Init_class_kind(core::ComplexVector_T_O);
  Init_class_kind(core::ComplexVector_int2_t_O);
  Init_class_kind(core::ComplexVector_int32_t_O);
  Init_class_kind(core::ComplexVector_byte16_t_O);
  Init_class_kind(core::ComplexVector_float_O);
  Init_class_kind(core::ComplexVector_int16_t_O);
  Init_class_kind(core::ComplexVector_int4_t_O);
  Init_class_kind(core::ComplexVector_size_t_O);
  Init_class_kind(core::ComplexVector_byte2_t_O);
  Init_class_kind(core::ComplexVector_byte8_t_O);
  Init_class_kind(core::ComplexVector_byte32_t_O);
  Init_class_kind(core::BitVectorNs_O);
  Init_class_kind(core::StrNs_O);
  Init_class_kind(core::Str8Ns_O);
  Init_class_kind(core::StrWNs_O);
  Init_class_kind(core::ComplexVector_byte4_t_O);
  Init_class_kind(core::ComplexVector_fixnum_O);
  Init_class_kind(core::ComplexVector_int64_t_O);
  Init_class_kind(core::AbstractSimpleVector_O);
  Init_class_kind(core::SimpleString_O);
  Init_class_kind(core::SimpleVector_int16_t_O);
  Init_class_kind(core::SimpleVector_byte16_t_O);
  Init_class_kind(core::SimpleBitVector_O);
  Init_class_kind(core::SimpleVector_int4_t_O);
  Init_class_kind(core::SimpleVector_byte32_t_O);
  Init_class_kind(core::SimpleVector_size_t_O);
  Init_class_kind(core::SimpleVector_short_float_O);
  Init_class_kind(core::SimpleVector_double_O);
  Init_class_kind(core::SimpleVector_long_float_O);
  Init_class_kind(core::SimpleVector_byte64_t_O);
  Init_class_kind(core::SimpleVector_int2_t_O);
  Init_class_kind(core::SimpleVector_int64_t_O);
  Init_class_kind(core::SimpleVector_fixnum_O);
  Init_class_kind(core::SimpleVector_int8_t_O);
  Init_class_kind(core::SimpleVector_float_O);
  Init_class_kind(core::SimpleVector_O);
  Init_class_kind(core::SimpleVector_byte8_t_O);
  Init_class_kind(core::SimpleVector_byte2_t_O);
  Init_class_kind(core::SimpleVector_int32_t_O);
  Init_class_kind(core::SimpleVector_byte4_t_O);
  Init_class_kind(core::Null_O);
  Init_class_kind(core::Character_dummy_O);
  Init_class_kind(llvmo::DataLayout_O);
  Init_class_kind(core::LoadTimeValues_O);
  Init_class_kind(core::SharpEqualWrapper_O);
  Init_class_kind(llvmo::ClaspJIT_O);
  Init_class_kind(core::Readtable_O);
  Init_class_kind(core::Exposer_O);
  Init_class_kind(core::CoreExposer_O);
  Init_class_kind(asttooling::AsttoolingExposer_O);
  Init_class_kind(llvmo::StructLayout_O);
  Init_class_kind(clasp_ffi::ForeignTypeSpec_O);
  Init_class_kind(core::DerivableCxxObject_O);
  Init_class_kind(clbind::ClassRep_O);
  Init_class_kind(core::SmallMap_O);
  Init_class_kind(mpip::Mpi_O);
  Init_class_kind(core::ExternalObject_O);
  Init_class_kind(llvmo::ExecutionEngine_O);
  Init_class_kind(llvmo::MCSubtargetInfo_O);
  Init_class_kind(llvmo::TargetSubtargetInfo_O);
  Init_class_kind(llvmo::Type_O);
  Init_class_kind(llvmo::FunctionType_O);
  Init_class_kind(llvmo::PointerType_O);
  Init_class_kind(llvmo::ArrayType_O);
  Init_class_kind(llvmo::VectorType_O);
  Init_class_kind(llvmo::StructType_O);
  Init_class_kind(llvmo::IntegerType_O);
  Init_class_kind(llvmo::JITDylib_O);
  Init_class_kind(llvmo::DIContext_O);
  Init_class_kind(llvmo::IRBuilderBase_O);
  Init_class_kind(llvmo::IRBuilder_O);
  Init_class_kind(llvmo::APFloat_O);
  Init_class_kind(llvmo::APInt_O);
  Init_class_kind(llvmo::DIBuilder_O);
  Init_class_kind(llvmo::SectionedAddress_O);
  Init_class_kind(llvmo::EngineBuilder_O);
  Init_class_kind(llvmo::Metadata_O);
  Init_class_kind(llvmo::MDNode_O);
  Init_class_kind(llvmo::DINode_O);
  Init_class_kind(llvmo::DIVariable_O);
  Init_class_kind(llvmo::DILocalVariable_O);
  Init_class_kind(llvmo::DIScope_O);
  Init_class_kind(llvmo::DIFile_O);
  Init_class_kind(llvmo::DIType_O);
  Init_class_kind(llvmo::DICompositeType_O);
  Init_class_kind(llvmo::DIDerivedType_O);
  Init_class_kind(llvmo::DIBasicType_O);
  Init_class_kind(llvmo::DISubroutineType_O);
  Init_class_kind(llvmo::DILocalScope_O);
  Init_class_kind(llvmo::DISubprogram_O);
  Init_class_kind(llvmo::DILexicalBlockBase_O);
  Init_class_kind(llvmo::DILexicalBlock_O);
  Init_class_kind(llvmo::DICompileUnit_O);
  Init_class_kind(llvmo::DIExpression_O);
  Init_class_kind(llvmo::DILocation_O);
  Init_class_kind(llvmo::ValueAsMetadata_O);
  Init_class_kind(llvmo::MDString_O);
  Init_class_kind(llvmo::Value_O);
  Init_class_kind(llvmo::Argument_O);
  Init_class_kind(llvmo::BasicBlock_O);
  Init_class_kind(llvmo::MetadataAsValue_O);
  Init_class_kind(llvmo::User_O);
  Init_class_kind(llvmo::Instruction_O);
  Init_class_kind(llvmo::UnaryInstruction_O);
  Init_class_kind(llvmo::VAArgInst_O);
  Init_class_kind(llvmo::LoadInst_O);
  Init_class_kind(llvmo::AllocaInst_O);
  Init_class_kind(llvmo::SwitchInst_O);
  Init_class_kind(llvmo::AtomicRMWInst_O);
  Init_class_kind(llvmo::LandingPadInst_O);
  Init_class_kind(llvmo::StoreInst_O);
  Init_class_kind(llvmo::UnreachableInst_O);
  Init_class_kind(llvmo::ReturnInst_O);
  Init_class_kind(llvmo::ResumeInst_O);
  Init_class_kind(llvmo::AtomicCmpXchgInst_O);
  Init_class_kind(llvmo::FenceInst_O);
  Init_class_kind(llvmo::CallBase_O);
  Init_class_kind(llvmo::CallInst_O);
  Init_class_kind(llvmo::InvokeInst_O);
  Init_class_kind(llvmo::PHINode_O);
  Init_class_kind(llvmo::IndirectBrInst_O);
  Init_class_kind(llvmo::BranchInst_O);
  Init_class_kind(llvmo::Constant_O);
  Init_class_kind(llvmo::GlobalValue_O);
  Init_class_kind(llvmo::Function_O);
  Init_class_kind(llvmo::GlobalVariable_O);
  Init_class_kind(llvmo::BlockAddress_O);
  Init_class_kind(llvmo::ConstantDataSequential_O);
  Init_class_kind(llvmo::ConstantDataArray_O);
  Init_class_kind(llvmo::ConstantStruct_O);
  Init_class_kind(llvmo::ConstantInt_O);
  Init_class_kind(llvmo::ConstantFP_O);
  Init_class_kind(llvmo::ConstantExpr_O);
  Init_class_kind(llvmo::ConstantPointerNull_O);
  Init_class_kind(llvmo::UndefValue_O);
  Init_class_kind(llvmo::ConstantArray_O);
  Init_class_kind(llvmo::TargetMachine_O);
  Init_class_kind(llvmo::LLVMTargetMachine_O);
  Init_class_kind(llvmo::ThreadSafeContext_O);
  Init_class_kind(llvmo::NamedMDNode_O);
  Init_class_kind(llvmo::Triple_O);
  Init_class_kind(llvmo::DWARFContext_O);
  Init_class_kind(llvmo::TargetOptions_O);
  Init_class_kind(llvmo::ObjectFile_O);
  Init_class_kind(llvmo::LLVMContext_O);
  Init_class_kind(llvmo::Module_O);
  Init_class_kind(llvmo::Target_O);
  Init_class_kind(llvmo::Linker_O);
  Init_class_kind(core::SmallMultimap_O);
  Init_class_kind(core::RandomState_O);
  Init_class_kind(core::HashTable_O);
  Init_class_kind(core::HashTableEqualp_O);
  Init_class_kind(core::HashTableEq_O);
  Init_class_kind(core::HashTableEql_O);
  Init_class_kind(core::HashTableEqual_O);
  Init_class_kind(llvmo::InsertPoint_O);
  Init_class_kind(core::Scope_O);
  Init_class_kind(core::FileScope_O);
  Init_class_kind(core::Path_O);
  Init_class_kind(core::Number_O);
  Init_class_kind(core::Real_O);
  Init_class_kind(core::Rational_O);
  Init_class_kind(core::Ratio_O);
  Init_class_kind(core::Integer_O);
  Init_class_kind(core::Bignum_O);
  Init_class_kind(core::Fixnum_dummy_O);
  Init_class_kind(core::Float_O);
  Init_class_kind(core::DoubleFloat_O);
  Init_class_kind(core::SingleFloat_dummy_O);
  Init_class_kind(core::LongFloat_O);
  Init_class_kind(core::ShortFloat_O);
  Init_class_kind(core::Complex_O);
  Init_class_kind(core::Stream_O);
  Init_class_kind(core::AnsiStream_O);
  Init_class_kind(core::TwoWayStream_O);
  Init_class_kind(core::SynonymStream_O);
  Init_class_kind(core::ConcatenatedStream_O);
  Init_class_kind(core::FileStream_O);
  Init_class_kind(core::PosixFileStream_O);
  Init_class_kind(core::CFileStream_O);
  Init_class_kind(core::BroadcastStream_O);
  Init_class_kind(core::StringStream_O);
  Init_class_kind(core::StringOutputStream_O);
  Init_class_kind(core::StringInputStream_O);
  Init_class_kind(core::EchoStream_O);
  Init_class_kind(core::FileStatus_O);
  Init_class_kind(core::SourcePosInfo_O);
  Init_class_kind(core::DirectoryEntry_O);
  Init_class_kind(core::LightUserData_O);
  Init_class_kind(core::UserData_O);
  Init_class_kind(core::Record_O);
  Init_class_kind(clbind::ClassRegistry_O);
  Init_class_kind(core::Cons_O);

  Init_templated_kind(core::WrappedPointer_O);
  Init_templated_kind(core::Creator_O);
  Init_templated_kind(clbind::ConstructorCreator_O);
};

void initialize_enums(){
// include INIT_CLASSES_INC_H despite USE_PRECISE_GC
#ifndef SCRAPING
#define ALL_ENUMS
#include <generated/enum_inc.h>
#undef ALL_ENUMS
#endif
};

extern void initialize_allocate_metaclasses(core::BootStrapCoreSymbolMap& bootStrapCoreSymbolMap);

void initialize_clasp() {
  // The bootStrapCoreSymbolMap keeps track of packages and symbols while they
  // are half-way initialized.
  core::BootStrapCoreSymbolMap bootStrapCoreSymbolMap;
  setup_bootstrap_packages(&bootStrapCoreSymbolMap);

  allocate_symbols(&bootStrapCoreSymbolMap);

  set_static_class_symbols(&bootStrapCoreSymbolMap);

  // Initialize metaclasses
  initialize_allocate_metaclasses(bootStrapCoreSymbolMap);

  initialize_enums();

  // Moved to lisp.cc
  //  initialize_functions();
  // initialize methods???
  //  initialize_source_info();
};

#endif // #ifndef SCRAPING at top
