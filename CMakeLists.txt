cmake_minimum_required(VERSION 3.8)

project(MyGodot)

if ("${CMAKE_SYSTEM_NAME}" MATCHES "Windows")
    set(MYGODOT_PLATFORM windows)
elseif ("${CMAKE_SYSTEM_NAME}" MATCHES "Darwin")
    set(MYGODOT_PLATFORM osx)
elseif ("${CMAKE_SYSTEM_NAME}" MATCHES "Linux")
    set(MYGODOT_PLATFORM x11)
else ()
    message(FATAL_ERROR "Unknown platform, please modify this script")
    #possible values
    #``AIX``
    #``Android``
    #``ARTOS``
    #``BeOS``
    #``BlueGeneP-static``
    #``BlueGeneP-dynamic``
    #``BlueGeneQ-static``
    #``BlueGeneQ-dynamic``
    #``BSDOS``
    #``Catamount``
    #``CrayLinuxEnvironment``
    #``CYGWIN``
    #``Darwin``
    #``DOS``
    #``DragonFly``
    #``eCos``
    #``Euros``
    #``FreeBSD``
    #``Fuchsia``
    #``Generic``
    #``Generic-ADSP``
    #``Generic-ELF``
    #``GHS-MULTI``
    #``GNU``
    #``Haiku``
    #``HP-UX``
    #``iOS``
    #``kFreeBSD``
    #``Linux``
    #``Midipix``
    #``MirBSD``
    #``MP-RAS``
    #``MSYS``
    #``NetBSD``
    #``OpenBSD``
    #``OpenVMS``
    #``OS2``
    #``OSF1``
    #``QNX``
    #``RISCos``
    #``SCO_SV``
    #``SINIX``
    #``SunOS``
    #``syllable``
    #``Tru64``
    #``tvOS``
    #``ULTRIX``
    #``UNIX_SV``
    #``UnixWare``
    #``watchOS``
    #``Windows``
    #``WindowsCE``
    #``WindowsPhone``
    #``WindowsStore``
    #``Xenix``

endif ()


add_custom_target(MyGodot
    scons p="${MYGODOT_PLATFORM}" bits=64 tools=yes custom_modules="${CMAKE_CURRENT_LIST_DIR}/MyModules" -j9
    WORKING_DIRECTORY "${CMAKE_CURRENT_LIST_DIR}/godot"
)

add_subdirectory(godot/platform/android/java/nativeSrcsConfigs)

file(GLOB CUSTOM_MODULE_LIST
    RELATIVE "${CMAKE_CURRENT_LIST_DIR}/MyModules"
    MyModules/*
)

include_directories(
    godot
)

set(CUSTOM_MODULE_FILES)
foreach(module ${CUSTOM_MODULE_LIST})
    file(GLOB_RECURSE MODULE_FILES "MyModules/${module}/*")
    list(APPEND CUSTOM_MODULE_FILES ${MODULE_FILES})
    #    include_directories(
    #        "MyModules/${module}"
    #    )
endforeach ()

message("Module Sources: ${CUSTOM_MODULE_FILES}")
add_library(DontCompileMe STATIC "${CUSTOM_MODULE_FILES}")