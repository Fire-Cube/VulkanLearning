function(add_copy_directory target_name source_dir target_dir)
    file(RELATIVE_PATH rel_source "${CMAKE_SOURCE_DIR}" "${source_dir}")
    file(RELATIVE_PATH rel_target "${CMAKE_SOURCE_DIR}" "${target_dir}")

    file(GLOB_RECURSE copy_sources RELATIVE "${source_dir}" CONFIGURE_DEPENDS LIST_DIRECTORIES FALSE "${source_dir}/*")
    set(outputs "")

    foreach (file IN LISTS copy_sources)
        set(src "${source_dir}/${file}")
        set(dst "${target_dir}/${file}")
        get_filename_component(dst_dir "${dst}" DIRECTORY)

        add_custom_command(
                OUTPUT "${dst}"
                COMMAND ${CMAKE_COMMAND} -E make_directory "${dst_dir}"
                COMMAND ${CMAKE_COMMAND} -E copy_if_different "${src}" "${dst}"
                DEPENDS "${src}"
                COMMENT "Copying ${rel_source}/${file} to ${rel_target}/${file}"
                VERBATIM
        )

        list(APPEND outputs "${dst}")

    endforeach ()

    add_custom_target(${target_name} ALL
            DEPENDS "${outputs}"
    )

endfunction()
