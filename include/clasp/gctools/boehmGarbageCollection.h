#pragma once

/*
    File: boehmGarbageCollection.h
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

namespace gctools {
void boehm_set_finalizer_list(gctools::Tagged object, gctools::Tagged finalizers);
void boehm_clear_finalizer_list(gctools::Tagged object);

void clasp_warn_proc(char* msg, GC_word arg);

void startupBoehm(gctools::ClaspInfo* claspInfo);
int runBoehm(gctools::ClaspInfo* claspInfo);
void shutdownBoehm();

}; // namespace gctools

extern "C" {
// Do the same thing that mps_park and mps_release do
void boehm_park();
void boehm_release();

void boehm_callback_reachable_object_find_stamps(void* ptr, size_t sz, void* client_data);
void boehm_callback_reachable_object_find_owners(void* ptr, size_t sz, void* client_data);
};

namespace gctools {
// ------------
struct ReachableClass {
  void update(size_t sz) {
    ++this->instances;
    this->totalSize += sz;
  };
  size_t instances = 0;
  size_t totalSize = 0;
};

typedef map<gctools::GCStampEnum, ReachableClass> ReachableClassMap;

struct FindStamp {
  gctools::GCStampEnum _stamp;
  std::vector<void*> _addresses;
  FindStamp(gctools::GCStampEnum stamp) : _stamp(stamp){};
};

struct FindOwner {
  void* _pointer;
  std::vector<void*> _addresses;
  FindOwner(void* pointer) : _pointer(pointer){};
};

/*!
 * claspgc_room - the GC specific implementation of ROOM
 */
void clasp_gc_room(std::ostringstream& OutputStream, RoomVerbosity verbosity);

void clasp_gc_registerRoots(void* rootsStart, size_t numberOfRoots);
void clasp_gc_deregisterRoots(void* rootsStart, size_t numberOfRoots);
}; // namespace gctools
