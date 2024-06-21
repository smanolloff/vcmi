==== Adding a new version of BAI ===

Example: create v3

1. create schema/v3:
  - copy schema/v1 to schema/v3 update constants/types as needed

2. create BAI/v3:
  - copy BAI/v1 to BAI/v3 update all references as needed

3. Add newly added files from 1. and 2. into CMakeLists
4. Add #include "v3/schema.h" in schema/schema.h
5. Add #include "v3/BAI.h" and a "case 3" switch statement in BAI/base.cpp
