/*
    File: hashTableEqualp.cc
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
// #define DEBUG_LEVEL_FULL

#include <clasp/core/foundation.h>
#include <clasp/core/common.h>
#include <clasp/core/hashTableEqualp.h>
#include <clasp/core/wrappers.h>
namespace core {

// ----------------------------------------------------------------------
//

HashTableEqualp_sp HashTableEqualp_O::create(Mapping_sp mapping, Number_sp rehashSize, double rehashThreshold) {
  return gctools::GC<HashTableEqualp_O>::allocate(mapping, rehashSize, rehashThreshold);
}

HashTableEqualp_sp HashTableEqualp_O::create(uint sz, Number_sp rehashSize, double rehashThreshold) {
  return create(StrongMapping_O::make(sz), rehashSize, rehashThreshold);
}

HashTableEqualp_sp HashTableEqualp_O::create_default() {
  DoubleFloat_sp rhs = DoubleFloat_O::create(2.0);
  HashTableEqualp_sp ht = create(16, rhs, DEFAULT_REHASH_THRESHOLD);
  return ht;
}

#if 0
    void HashTableEqualp_O::serialize(::serialize::SNodeP node)
    {
        this->Bases::serialize(node);
	// Archive other instance variables here
    }
#endif

bool HashTableEqualp_O::keyTest(T_sp entryKey, T_sp searchKey) const {

  bool equalp = cl__equalp(entryKey, searchKey);
  //        printf("%s:%d HashTableEqualp_O::keyTest testing if %s equalp %s
  //        -->%d\n",__FILE__,__LINE__,_rep_(entryKey).c_str(),_rep_(searchKey).c_str(),equalp);
  return equalp;
}

void HashTableEqualp_O::sxhashEffect(T_sp obj, HashGenerator& hg) const {
  clasp_sxhash_equalp(obj, hg);
}

}; // namespace core
