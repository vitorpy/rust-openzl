if(NOT COMMAND add_zs_test)
  function(add_zs_test TESTNAME)
      add_executable(${ARGV})
      target_link_libraries(${TESTNAME}
          PRIVATE
              GTest::gtest_main
      )
      gtest_discover_tests(${TESTNAME})
  endfunction()
endif()

enable_testing()
