// =============================================================================
// Copyright 2024 Simeon Manolov <s.manolloff@gmail.com>.  All rights reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//    http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
// =============================================================================

#pragma once

#ifdef MAX_SCHEMA_VERSION
  #if MAX_SCHEMA_VERSION < 1
    #undef MAX_SCHEMA_VERSION
    #define MAX_SCHEMA_VERSION 1
  #endif
#else
  #define MAX_SCHEMA_VERSION 1
#endif

#ifdef MIN_SCHEMA_VERSION
  #if MIN_SCHEMA_VERSION > 1
    #undef MAX_SCHEMA_VERSION
    #define MIN_SCHEMA_VERSION 1
  #endif
#else
  #define MIN_SCHEMA_VERSION 1
#endif

#include "types.h"
#include "constants.h"
