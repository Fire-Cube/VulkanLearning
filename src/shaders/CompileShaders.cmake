function(compile_shaders)
    if(ARGC LESS 4)
        message(FATAL_ERROR
                "Usage: compile_shaders <TARGET> <SRC_DIR> <OUT_DIR> <shader1> [shader2 ...]\n"
                "Only ${ARGC} argument(s) given."
        )
    endif()

    list(GET ARGV 0 target_name)
    list(GET ARGV 1 shader_source_dir)
    list(GET ARGV 2 shader_output_dir)
    list(SUBLIST ARGV 3 -1 shader_files)

    find_program(GLSLANG_VALIDATOR glslangValidator REQUIRED)

    set(generated_spvs)
    foreach(shader_file IN LISTS shader_files)
        file(RELATIVE_PATH rel_path_src "${shader_source_dir}" "${shader_file}")
        get_filename_component(rel_dir "${rel_path_src}" PATH)

        set(out_spv "${shader_output_dir}/${rel_path_src}.spv")

        file(RELATIVE_PATH rel_input   "${CMAKE_SOURCE_DIR}" "${shader_file}")
        file(RELATIVE_PATH rel_output  "${CMAKE_SOURCE_DIR}" "${out_spv}")

        add_custom_command(
                OUTPUT   "${out_spv}"
                COMMAND  ${CMAKE_COMMAND} -E make_directory "${shader_output_dir}/${rel_dir}"
                COMMAND  ${GLSLANG_VALIDATOR} -s -V "${shader_file}" -o "${out_spv}"
                DEPENDS  "${shader_file}"
                COMMENT  "Compile GLSL shader ${rel_input} -> ${rel_output}"
                VERBATIM
        )

        list(APPEND generated_spvs "${out_spv}")

    endforeach()

    add_custom_target(${target_name}
            ALL
            DEPENDS ${generated_spvs}
    )

    set(${target_name}_SPV ${generated_spvs} PARENT_SCOPE)

endfunction()
