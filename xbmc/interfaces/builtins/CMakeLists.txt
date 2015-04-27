set(SOURCES AddonBuiltins.cpp
            Builtins.cpp
            LibraryBuiltins.cpp)

core_add_library(interfaces_builtins)
add_dependencies(interfaces_builtins libcpluff)
