// Copyright 2020 The TensorFlow Runtime Authors
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

//===- ref_count.cc ---------------------------------------------*- C++ -*-===//
//
// This file holds the global counter for the number of
// reference_counted_objects.
//
//===----------------------------------------------------------------------===//

#include "tfrt/support/ref_count.h"

#include <atomic>

namespace tfrt {

std::atomic<size_t> total_reference_counted_objects{0};
}