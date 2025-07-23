function(compile_shaders)
    cmake_parse_arguments(ARG "" "TARGET" "FILES" ${ARGN})
    if(NOT ARG_FILES)
        message(FATAL_ERROR "compile_shaders(): pass shaders via FILES <list>")
    endif()
    if(NOT ARG_TARGET)
        set(ARG_TARGET shaders)
    endif()

    find_program(GLSLANG_VALIDATOR glslangValidator REQUIRED)

    set(outputs)
    foreach(src IN LISTS ARG_FILES)
        cmake_path(RELATIVE_PATH src BASE_DIRECTORY "${CMAKE_SOURCE_DIR}/src/shaders" OUTPUT_VARIABLE rel)
        cmake_path(GET rel PARENT_PATH rel_dir)
        set(out "${CMAKE_BINARY_DIR}/shaders/${rel}.spv")

        add_custom_command(
                OUTPUT   "${out}"
                COMMAND  ${CMAKE_COMMAND} -E make_directory "${CMAKE_BINARY_DIR}/${rel_dir}"
                COMMAND  ${GLSLANG_VALIDATOR} -V "${src}" -o "${out}"
                DEPENDS  "${src}"
                COMMENT  "Compile ${rel} -> ${rel}.spv"
                VERBATIM
        )
        list(APPEND outputs "${out}")
    endforeach()

    add_custom_target(${ARG_TARGET} ALL DEPENDS ${outputs})
    set(${ARG_TARGET}_SPV ${outputs} PARENT_SCOPE)

endfunction()
