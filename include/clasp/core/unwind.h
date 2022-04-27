#ifndef core_Unwind_H
#define core_Unwind_H

#include <csetjmp>
#include <clasp/core/foundation.h>
#include <clasp/core/object.h>
#include <clasp/core/symbol.h>

namespace core {

FORWARD(DynEnv);
FORWARD(DestDynEnv);

class DynEnv_O : public General_O {
  LISP_ABSTRACT_CLASS(core, CorePkg, DynEnv_O, "DynEnv", General_O);
public:
  typedef enum {
      Continue, Proceed, OutOfExtent, Abandoned, FallBack
  } SearchStatus;
public:
  DynEnv_O() {}; // clang complains without this for some reason.
  DynEnv_O(T_sp a_outer) : outer(a_outer) {};
  virtual ~DynEnv_O() {};
  // either the outer dynenv, or NIL if this is the top.
  T_sp outer;
public:
  /* Return information about this dynamic environment to the search
   * phase. Return values mean the following:
   * - Continue: Nothing important needs to be noted.
   * - OutOfExtent: The destination is not actually on the stack.
   * - Abandoned: See CLHS 5.2: We are in the middle of an outer
       unwinding operation, and the destination of this inner
       unwinding has been invalidated by the outer.
   * - FallBack: C++ code on the stack has unknown destructors or
       catch blocks, so the C++ unwinder must be used.
   * Proceed may not be returned by this function. It can only be
   * returned from sjlj_unwind_search to indicate we can proceed
   * with this unwinder (as opposed to the fallback C++ unwinder). */
  virtual SearchStatus search() const = 0;
#ifdef UNWIND_INVALIDATE_STRICT
  /* Mark this dynamic environment as abandoned per CLHS 5.2,
   * if applicable. */
  virtual void invalidate() {};
#endif
  /* Undo this environment. */
  virtual void proceed(DestDynEnv_sp dest, size_t index) {};
};

/* A dynenv of this class is used to warn our unwinder that there is
 * C++ code with unknown dynamic environment, e.g. because nontrivial
 * destructors need to be run, there are catch blocks, or we simply
 * don't know. */
class UnknownDynEnv_O : public DynEnv_O {
  LISP_CLASS(core, CorePkg, UnknownDynEnv_O, "UnknownDynEnv", DynEnv_O);
public:
  UnknownDynEnv_O(T_sp outer) : DynEnv_O(outer) {};
  virtual ~UnknownDynEnv_O() {};
public:
  virtual SearchStatus search() const { return FallBack; };
};

// Abstract class of dynenvs that can be destinations for unwinding.
class DestDynEnv_O : public DynEnv_O {
  LISP_ABSTRACT_CLASS(core, CorePkg, DestDynEnv_O, "DestDynEnv", DynEnv_O);
public:
  void* frame; // for fallback
  jmp_buf* target;
#ifdef UNWIND_INVALIDATE_STRICT
  bool valid = true;
#endif
  DestDynEnv_O() : DynEnv_O() {};
  DestDynEnv_O(T_sp outer, void* a_frame, jmp_buf* a_target) :
    DynEnv_O(outer), frame(a_frame), target(a_target) {};
  virtual ~DestDynEnv_O() {};
  virtual SearchStatus search() const { return Continue; };
#ifdef UNWIND_INVALIDATE_STRICT
  virtual void invalidate() { valid = false; }
#endif
  virtual void proceed(DestDynEnv_sp dest, size_t index) {};
  /* This function returns the new dynamic environment that will be in
   * place after unwinding to this destination. */
  virtual T_sp unwound_dynenv() = 0;
};

// Dynenv for a CL:BLOCK.
FORWARD(BlockDynEnv);
class BlockDynEnv_O : public DestDynEnv_O {
  LISP_CLASS(core, CorePkg, BlockDynEnv_O, "BlockDynEnv", DestDynEnv_O);
public:
  using DestDynEnv_O::DestDynEnv_O; // inherit constructor
  virtual ~BlockDynEnv_O() {};
  virtual T_sp unwound_dynenv() { return outer; }
};

// Dynenv for a CL:TAGBODY.
class TagbodyDynEnv_O : public DestDynEnv_O {
  LISP_CLASS(core, CorePkg, TagbodyDynEnv_O, "TagbodyDynEnv", DestDynEnv_O);
public:
  using DestDynEnv_O::DestDynEnv_O;
  virtual ~TagbodyDynEnv_O() {};
  virtual T_sp unwound_dynenv() { return this->asSmartPtr(); }
};

// Dynenv for a CL:UNWIND-PROTECT cleanup.
class UnwindProtectDynEnv_O : public DynEnv_O {
  LISP_CLASS(core, CorePkg, UnwindProtectDynEnv_O, "UnwindProtectDynEnv", DynEnv_O);
public:
  UnwindProtectDynEnv_O(T_sp outer, jmp_buf* a_target)
    : DynEnv_O(outer), target(a_target) {};
  virtual ~UnwindProtectDynEnv_O() {};
public:
  jmp_buf* target;
public:
  virtual SearchStatus search() const { return Continue; };
  [[noreturn]] virtual void proceed(DestDynEnv_sp, size_t);
};

// Dynenv for a special variable binding.
class BindingDynEnv_O : public DynEnv_O {
  LISP_CLASS(core, CorePkg, BindingDynEnv_O, "BindingDynEnv_O", DynEnv_O);
public:
  BindingDynEnv_O(T_sp outer, Symbol_sp a_sym, T_sp a_old)
    : DynEnv_O(outer), sym(a_sym), old(a_old) {};
  virtual ~BindingDynEnv_O() {};
  Symbol_sp sym;
  T_sp old;
  virtual SearchStatus search() const { return Continue; };
  virtual void proceed(DestDynEnv_sp, size_t);
};

// Functions

/* Generally, unwinding proceeds in two steps, similar to the Itanium
 * unwinder. The first step checks through the dynamic environment to
 * ensure the destination is valid, and determines whether we can use
 * this faster unwinder or have to fall back to the slow C++ one.
 * The second step, armed with that information, actually jumps. */

/* Check through the dynamic environment for the destination.
 * This has no side effects and does not signal errors. */
DynEnv_O::SearchStatus sjlj_unwind_search(DestDynEnv_sp dest);
/* Actually carry out an unwinding to the given destination.
 * sjlj_unwind_search must have returned Proceed beforehand, or the
 * effects are undefined and bad things may happen.
 * This is intended to be used from unwind-protect cleanups, in order
 * to continue unwinding without needing to search again. */
[[noreturn]] void sjlj_unwind_proceed(DestDynEnv_sp dest, size_t index);
/* Carry out an unwinding to the given destination.
 * Takes care of the search aspect and signaling errors, so this is
 * the main entry point. */
[[noreturn]] void sjlj_unwind(DestDynEnv_sp dest, size_t index);

}; // namespace core

#endif // core_Unwind_H guard
