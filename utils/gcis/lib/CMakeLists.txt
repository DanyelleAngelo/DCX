#add aqui
ADD_LIBRARY(gc-is STATIC
        util.cpp
        simple8b.cpp
        gcis_s8b_codec.cpp
        gcis_unary_codec.cpp
        gcis_eliasfano_codec.cpp
        gcis_eliasfano_codec_no_lcp.cpp
        gcis_gap_codec.cpp
        eliasfano.cpp
        sais_nong.cpp
        ../include/eliasfano.hpp
        ../include/gcis_unary.hpp
        ../include/gcis_s8b.hpp
        ../include/gcis_eliasfano.hpp
        ../include/gcis_eliasfano_no_lcp.hpp
        ../include/gcis_gap_codec.hpp
        ../include/gcis_gap.hpp
        ../include/sais_nong.hpp
        ../external/malloc_count/malloc_count.h #to dcx
        ../external/malloc_count/stack_count.h #to dcx
        )

ADD_LIBRARY(gc-is-statistics STATIC
        util.cpp
        simple8b.cpp
        gcis_s8b_codec.cpp
        gcis_unary_codec.cpp
        gcis_eliasfano_codec.cpp
        gcis_eliasfano_codec_no_lcp.cpp
        gcis_gap_codec.cpp
        eliasfano.cpp
        sais_nong.cpp
        ../include/eliasfano.hpp
        ../include/gcis_unary.hpp
        ../include/gcis_s8b.hpp
        ../include/gcis_eliasfano.hpp
        ../include/gcis_eliasfano_no_lcp.hpp
        ../include/gcis_gap_codec.hpp
        ../include/gcis_gap.hpp
        ../include/sais_nong.hpp)

target_compile_definitions(gc-is-statistics PRIVATE  MEM_MONITOR REPORT )
install (TARGETS gc-is ARCHIVE DESTINATION ${CMAKE_SOURCE_DIR}/lib)
install (TARGETS gc-is-statistics ARCHIVE DESTINATION ${CMAKE_SOURCE_DIR}/lib)