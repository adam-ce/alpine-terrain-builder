function(mark_target_includes_as_system target_name)
    if (NOT TARGET ${target_name})
        return()
    endif()

    # ALIAS targets are read-only; resolve to the real target first.
    get_target_property(alias_target ${target_name} ALIASED_TARGET)
    if (alias_target)
        set(target_name ${alias_target})
    endif()

    get_target_property(interface_include_dirs
        ${target_name}
        INTERFACE_INCLUDE_DIRECTORIES
    )

    if (interface_include_dirs)
        set_target_properties(${target_name} PROPERTIES
            INTERFACE_INCLUDE_DIRECTORIES ""
            INTERFACE_SYSTEM_INCLUDE_DIRECTORIES "${interface_include_dirs}"
        )
    endif()
endfunction()
