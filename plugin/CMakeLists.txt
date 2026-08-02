set(LINEAR_SDK_HEADERS "${LINEAR_AE_SDK_ROOT}/Examples/Headers")
set(LINEAR_SDK_RESOURCES "${LINEAR_AE_SDK_ROOT}/Examples/Resources")
set(LINEAR_SDK_UTIL "${LINEAR_AE_SDK_ROOT}/Examples/Util")

math(EXPR LINEAR_PIPL_VERSION
    "(${PROJECT_VERSION_MAJOR} << 19) | (${PROJECT_VERSION_MINOR} << 15) | (${PROJECT_VERSION_PATCH} << 11) | 1")
configure_file(LinearPiPL.r.in "${CMAKE_CURRENT_BINARY_DIR}/LinearPiPL.r" @ONLY)

if(WIN32)
    set(LINEAR_PIPL_RR "${CMAKE_CURRENT_BINARY_DIR}/LinearPiPL.rr")
    set(LINEAR_PIPL_RRC "${CMAKE_CURRENT_BINARY_DIR}/LinearPiPL.rrc")
    set(LINEAR_PIPL_RC "${CMAKE_CURRENT_BINARY_DIR}/LinearPiPL.rc")
    add_custom_command(
        OUTPUT "${LINEAR_PIPL_RC}"
        COMMAND cl /I "${LINEAR_SDK_HEADERS}" /EP
                "${CMAKE_CURRENT_BINARY_DIR}/LinearPiPL.r"
                > "${LINEAR_PIPL_RR}"
        COMMAND "${LINEAR_SDK_RESOURCES}/PiPLTool"
                "${LINEAR_PIPL_RR}" "${LINEAR_PIPL_RRC}"
        COMMAND cl /D MSWindows /EP "${LINEAR_PIPL_RRC}"
                > "${LINEAR_PIPL_RC}"
        DEPENDS
            "${CMAKE_CURRENT_BINARY_DIR}/LinearPiPL.r"
            "${LINEAR_SDK_RESOURCES}/PiPLTool.exe"
        COMMENT "Compiling Linear PiPL resource"
        VERBATIM
    )
    set_source_files_properties("${LINEAR_PIPL_RC}" PROPERTIES GENERATED TRUE)
endif()

add_library(linear_adobe MODULE
    PluginEntry.cpp
    LabelEditorWin.cpp
    "${LINEAR_SDK_UTIL}/AEGP_SuiteHandler.cpp"
    "${LINEAR_SDK_UTIL}/MissingSuiteError.cpp"
    "${LINEAR_PIPL_RC}"
)
target_link_libraries(linear_adobe PRIVATE linear_core)
target_include_directories(linear_adobe PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}"
    "${LINEAR_SDK_HEADERS}"
    "${LINEAR_SDK_HEADERS}/SP"
    "${LINEAR_SDK_HEADERS}/Win"
    "${LINEAR_SDK_RESOURCES}"
    "${LINEAR_SDK_UTIL}"
)
target_compile_definitions(linear_adobe PRIVATE MSWindows WIN32 _WINDOWS)
target_compile_options(linear_adobe PRIVATE /W4 /permissive- /EHsc /utf-8)
set_target_properties(linear_adobe PROPERTIES
    PREFIX ""
    OUTPUT_NAME "${LINEAR_PRODUCT_NAME}"
    SUFFIX ".aex"
)
