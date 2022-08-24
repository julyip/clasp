#ifndef gctools_threadlocal_H
#define gctools_threadlocal_H

#include <signal.h>
#include <functional>
#include <algorithm> // copy
#include <clasp/gctools/threadlocal.fwd.h>

typedef core::T_O*(*T_OStartUp)(core::T_O*);
typedef void(*voidStartUp)(void);


namespace core {

#ifdef DEBUG_DYN_ENV_STACK
extern bool global_debug_dyn_env_stack;
#endif

#define STARTUP_FUNCTION_CAPACITY_INIT 128
#define STARTUP_FUNCTION_CAPACITY_MULTIPLIER 2
  struct StartUp {
    typedef enum {T_O_function, void_function} FunctionEnum;
    FunctionEnum _Type;
    size_t       _Position;
    void*        _Function;
    StartUp() {};
    StartUp(FunctionEnum type, size_t p, void* f) : _Type(type), _Position(p), _Function(f) {};
    bool operator<(const StartUp& other) {
      return this->_Position < other._Position;
    }
  };

  struct StartupInfo {
    size_t _capacity;
    size_t _count;
    StartUp* _functions;

  StartupInfo() : _capacity(0), _count(0), _functions(NULL) {};
  };
};

namespace core {
struct CleanupFunctionNode {
  std::function<void(void)> _CleanupFunction;
  CleanupFunctionNode*      _Next;
  CleanupFunctionNode(const std::function<void(void)>& cleanup, CleanupFunctionNode* next)
    : _CleanupFunction(cleanup), _Next(next) {};

};
};

namespace llvmo {
class ObjectFile_O;
typedef gctools::smart_ptr<ObjectFile_O> ObjectFile_sp;
class CodeBase_O;
typedef gctools::smart_ptr<CodeBase_O> CodeBase_sp;

};
namespace core {

// Used to save a bit of the state for use in nonlocal exits.
struct VirtualMachineStackState {
  core::T_O**    _framePointer;
  core::T_O**    _stackPointer;
  VirtualMachineStackState(core::T_O** fp, core::T_O** sp)
    : _framePointer(fp), _stackPointer(sp) {};
};

#define STACK_GROWS_UP 1
//#define DEBUG_VIRTUAL_MACHINE 0
#ifdef DEBUG_VIRTUAL_MACHINE
#define VM_ASSERT_ALIGNED(ptr) if (((uintptr_t)(ptr))&0x7) { printf("%s:%d:%s Unaligned pointer %p\n", __FILE__, __LINE__, __FUNCTION__, (void*)(ptr)); abort(); }
#define VM_STACK_POINTER_CHECK(vm) if ((vm)._Running&&(vm)._stackPointer&&!((vm)._stackBottom<=(vm)._stackPointer && (vm)._stackPointer<=(vm)._stackTop) ) { printf("%s:%d:%s _stackPointer %p is out of stack _stackTop %p _stackBottom %p\n", __FILE__, __LINE__, __FUNCTION__, (void*)((vm)._stackPointer), (void*)((vm)._stackTop), (void*)((vm)._stackBottom)); abort(); }
#define VM_FRAME_POINTER_CHECK(vm) if ((vm)._Running&&(vm)._framePointer&&!((vm)._stackBottom<=(vm)._framePointer && (vm)._framePointer<=(vm)._stackTop) ) { printf("%s:%d:%s _framePointer %p is out of stack _stackTop %p _stackBottom %p\n", __FILE__, __LINE__, __FUNCTION__, (void*)((vm)._framePointer), (void*)((vm)._stackTop), (void*)((vm)._stackBottom)); abort(); }
#define VM_CHECK(vm) VM_STACK_POINTER_CHECK(vm); VM_FRAME_POINTER_CHECK(vm)
#else
#define VM_ASSERT_ALIGNED(ptr)
#define VM_CHECK(vm)
#endif

struct VirtualMachine {
  static constexpr size_t MaxStackWords = 16384; // 16K words for now.
  bool           _Running;
  core::T_O*     _stackBottom[MaxStackWords];
  size_t         _stackBytes;
  core::T_O**    _stackTop;
  core::T_O**    _framePointer;
  core::T_O**    _stackPointer;
  core::T_sp     _currentFunction;
  core::T_O**    _literals;
  unsigned char* _pc;

  inline void shutdown() {
    this->_Running = false;
  }
  inline void push(core::T_O* value) {
#ifdef STACK_GROWS_UP
    this->_stackPointer++;
#else
    this->_stackPointer--;
#endif
    VM_CHECK(*this);
    VM_ASSERT_ALIGNED(this->_stackPointer);
    *this->_stackPointer = value;
  }

  inline core::T_O* pop() {
    core::T_O* value = *this->_stackPointer;
#ifdef STACK_GROWS_UP
    this->_stackPointer--;
#else
    this->_stackPointer++;
#endif
    VM_CHECK(*this);
    return value;
  }
  
  // Drop NELEMS slots on the stack all in one go.
  inline void drop(size_t nelems) {
#ifdef STACK_GROWS_UP
    this->_stackPointer -= nelems;
#else
    this->_stackPointer += nelems;
#endif
    VM_CHECK(*this);
    VM_ASSERT_ALIGNED(this->_stackPointer);
  }

  // Get a pointer to the nth element from the stack
  // i.e. 0 is most recently pushed, 1 the next most recent, etc.
  inline core::T_O** stackref(ptrdiff_t n) {
#ifdef STACK_GROWS_UP
    return this->_stackPointer - n;
#else
    return this->_stackPointer + n;
#endif
    VM_CHECK(*this);
    VM_ASSERT_ALIGNED(this->_stackPointer);
  }

  inline VirtualMachineStackState save() {
    return VirtualMachineStackState(this->_framePointer, this->_stackPointer);
  }
  inline void load(VirtualMachineStackState vmss) {
    this->_framePointer = vmss._framePointer;
    this->_stackPointer = vmss._stackPointer;
    VM_CHECK(*this);
    VM_ASSERT_ALIGNED(this->_stackPointer);
    VM_ASSERT_ALIGNED(this->_framePointer);
  }

  // Push a new frame with NLOCALS local variables.
  // Return the old frame pointer.
  inline T_O** push_frame(size_t nlocals) {
    T_O** ret = this->_framePointer;
    this->_framePointer = this->_stackPointer;
#ifdef STACK_GROWS_UP
    this->_stackPointer += nlocals;
#else
    this->_stackPointer -= nlocals;
#endif
    VM_CHECK(*this);
    VM_ASSERT_ALIGNED(this->_stackPointer);
    return ret;
  }

  inline void pop_frame(T_O** old_fp) {
    this->_stackPointer = this->_framePointer;
    this->_framePointer = old_fp;
  }

  inline void setreg(size_t base, core::T_O* value) {
#ifdef STACK_GROWS_UP
    *(this->_framePointer + base + 1) = value;
#else
    *(this->_framePointer - base) = value;
#endif
  }

  // Copy N elements from SOURCE into the current frame's register file
  // starting at BASE.
  // If the stack grows up, SOURCE should be a pointer to the first element
  // of the source data; otherwise the last.
  inline void copytoreg(core::T_O** source, size_t n, size_t base) {
#ifdef STACK_GROWS_UP
    VM_CHECK(*this);
    VM_ASSERT_ALIGNED(source);
    std::copy(source, source + n, this->_framePointer + base + 1);
#else
    std::copy_backward(source - n, source, this->_framePointer - base);
#endif
  }

  // Fill OBJECT into N registers starting at BASE.
  inline void fillreg(core::T_O* object, size_t n, size_t base) {
#ifdef STACK_GROWS_UP
    VM_CHECK(*this);
    VM_ASSERT_ALIGNED(this->_framePointer+base+1);
    std::fill(this->_framePointer + base + 1, this->_framePointer + base + n + 1,
              object);
#else
    std::fill(this->_framePointer - base, this->_framePointer - base + n,
              object);
#endif
  }

  // Get a pointer to the nth register in the current frame.
  inline core::T_O** reg(size_t n) {
#ifdef STACK_GROWS_UP
    VM_CHECK(*this);
    VM_ASSERT_ALIGNED(this->_framePointer+n+1);
    return this->_framePointer + n + 1;
#else
    return this->_framePointer - n;
#endif
  }

  // Compute how many elements are on the stack in the current frame
  // but which are not part of the register file.
  inline size_t npushed(size_t nlocals) {
#ifdef STACK_GROWS_UP
    VM_CHECK(*this);
    VM_ASSERT_ALIGNED(this->_stackPointer);
    VM_ASSERT_ALIGNED(this->_framePointer);
    return this->_stackPointer - nlocals - this->_framePointer;
#else
    return this->_framePointer - nlocals - this->_stackPointer;
#endif
  }

  // Copy the n most recent pushes to the given memory.
  // The most recent push goes to the end of the range.
  // Unlike copytoreg, here the destination is the pointer to the start
  // of the range regardless of stack growth direction.
  template < class OutputIter >
  inline void copyto(size_t n, OutputIter dest) {
#ifdef STACK_GROWS_UP
    VM_CHECK(*this);
    VM_ASSERT_ALIGNED(this->_stackPointer+1-n);
    std::copy(this->_stackPointer + 1 - n, this->_stackPointer + 1, dest);
#else
    std::copy(this->_stackPointer, this->_stackPointer + n, dest);
#endif
  }

  VirtualMachine();
  ~VirtualMachine();

};



#define IHS_BACKTRACE_SIZE 16
  struct ThreadLocalState {

    core::T_sp            _ObjectFiles;
    mp::Process_sp        _Process;
    DynamicBindingStack   _Bindings;
    /*! Pending interrupts */
    List_sp               _PendingInterrupts;
    /*! Save CONS records so we don't need to do allocations
        to add to _PendingInterrupts */
    List_sp               _SparePendingInterruptRecords; // signal_queue on ECL
    List_sp               _BufferStr8NsPool;
    List_sp               _BufferStrWNsPool;
    StringOutputStream_sp _BFormatStringOutputStream;
    StringOutputStream_sp _WriteToStringOutputStream;

    MultipleValues _MultipleValues;
    void* _sigaltstack_buffer;
    size_t  _unwinds;
    stack_t _original_stack;
    std::string       _initializer_symbol;
    void*             _object_file_start;
    size_t            _object_file_size;
    gctools::GCRootsInModule*  _GCRootsInModule;
    StartupInfo       _Startup;
    bool              _Breakstep; // Should we check for breaks?
    // What frame are we stepping over? NULL means step-into mode.
    void*             _BreakstepFrame;
    // Stuff for SJLJ unwinding
    List_sp           _DynEnvStackBottom;
    T_sp              _UnwindDest;
    size_t            _UnwindDestIndex;
#ifdef DEBUG_IHS
    // Save the last return address before IHS screws up
    void*                    _IHSBacktrace[IHS_BACKTRACE_SIZE];
#endif
    size_t                 _xorshf_x; // Marsaglia's xorshf generator
    size_t                 _xorshf_y;
    size_t                 _xorshf_z;
    CleanupFunctionNode*   _CleanupFunctions;
    mp::SpinLock _SparePendingInterruptRecordsSpinLock;
    uint64_t   _BytesAllocated;
    uint64_t            _Tid;
    uintptr_t           _BacktraceBasePointer;
    VirtualMachine      _VM;
    
#ifdef DEBUG_MONITOR_SUPPORT
    // When enabled, maintain a thread-local map of strings to FILE*
    // used for logging. This is so that per-thread log files can be
    // generated.  These log files are automatically closed when the
    // thread exits.
    std::map<std::string,FILE*> _MonitorFiles;
#endif

  public:
    // Methods
    ThreadLocalState(bool dummy);
    void finish_initialization_main_thread(core::T_sp theNilObject);
    ThreadLocalState();
    void initialize_thread(mp::Process_sp process, bool initialize_GCRoots);

    void dynEnvStackTest(core::T_sp val) const;
    void dynEnvStackSet(core::T_sp val) {
#ifdef DEBUG_DYN_ENV_STACK
      if (core::global_debug_dyn_env_stack)
        this->dynEnvStackTest(val);
#endif
      this->_DynEnvStackBottom = val;
    }
    core::T_sp dynEnvStackGet() const {
#ifdef DEBUG_DYN_ENV_STACK
      if (core::global_debug_dyn_env_stack)
        this->dynEnvStackTest(this->_DynEnvStackBottom);
#endif
      return this->_DynEnvStackBottom;
    }
    
    uint32_t random();

    llvmo::ObjectFile_sp topObjectFile();
    void pushObjectFile(llvmo::ObjectFile_sp of);
    void popObjectFile();
    inline DynamicBindingStack& bindings() { return this->_Bindings; };
    
    ~ThreadLocalState();
  };

  void thread_local_register_cleanup(const std::function<void(void)>& cleanup);
  void thread_local_invoke_and_clear_cleanup();

}; // namespace core


namespace gctools {

  void registerBytesAllocated(size_t bytes);
};


struct ThreadManager {
  struct Worker {
#ifdef USE_BOEHM
    GC_stack_base _StackBase;
#endif
    gctools::ThreadLocalStateLowLevel _StateLowLevel;
    core::ThreadLocalState _State;
    // Worker must be allocated at the top of the worker thread function
    // It uses RAII to register/deregister our thread
    Worker() : _StateLowLevel((void*)this), _State(false) {
//      printf("%s:%d:%s Starting\n", __FILE__, __LINE__, __FUNCTION__ );
#ifdef USE_BOEHM
      GC_get_stack_base(&this->_StackBase);
      GC_register_my_thread(&this->_StackBase);
      my_thread_low_level = &this->_StateLowLevel;
#endif
    };
    ~Worker() {
//      printf("%s:%d:%s Stopping\n", __FILE__, __LINE__, __FUNCTION__ );
#ifdef USE_BOEHM
      GC_unregister_my_thread();
#endif
    };
  };
  void register_thread(std::thread& th) {
    // Do nothing for now
  };
  void unregister_thread(std::thread& th) {
    // Do nothing for now
  };
};





#endif
