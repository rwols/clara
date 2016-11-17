function(target_set_rtti target boolean)
	if (CMAKE_COMPILER_IS_GNUCXX OR CMAKE_COMPILER_IS_GNUCC OR CMAKE_CXX_COMPILER_ID STREQUAL "AppleClang" OR CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
		if (NOT ${boolean})
			target_compile_options(${target} PUBLIC "-fno-rtti")
		endif ()
	else ()
		# Assume Microsoft compiler.
		if (NOT ${boolean})
			target_compile_options(${target} PUBLIC "/EHsc")
		endif ()
	endif ()
endfunction()