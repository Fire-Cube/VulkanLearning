function(add_copy_directory target_name source_dir target_dir)
    file(RELATIVE_PATH rel_source "${CMAKE_SOURCE_DIR}" "${source_dir}")
    file(RELATIVE_PATH rel_target "${CMAKE_SOURCE_DIR}" "${target_dir}")

    add_custom_command(
            OUTPUT "${target_dir}"
            COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${source_dir}"
            "${target_dir}"
            COMMENT "Recursively copying ${rel_source} to ${rel_target}"
            VERBATIM
    )

    add_custom_target(${target_name} ALL
            DEPENDS "${target_dir}"
    )

endfunction()
