set(SOURCES AddonBuiltins.cpp
            Builtins.cpp
            CECBuiltins.cpp
            GUIBuiltins.cpp
            GUIControlBuiltins.cpp
            GUIContainerBuiltins.cpp
            LibraryBuiltins.cpp
            ProfileBuiltins.cpp
            SkinBuiltins.cpp
            SystemBuiltins.cpp
            WeatherBuiltins.cpp)

core_add_library(interfaces_builtins)
add_dependencies(interfaces_builtins libcpluff)
