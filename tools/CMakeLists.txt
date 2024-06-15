set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wno-unused-result")

set(LIBS
    frontend
    backend_llvm
    backend_arm
    optimizer
    utils
)

add_dependencies(${LIBS} vendor_libsysy)

set(mainLLVM "mainLLVM")
add_executable(${mainLLVM} mainLLVM.c)
target_link_libraries(${mainLLVM} PUBLIC ${LIBS})
install(TARGETS ${mainLLVM}
  RUNTIME DESTINATION bin
  COMPONENT ${mainLLVM})

set(mainRPi "mainRPi")
add_executable(${mainRPi} mainRPi.c)
target_link_libraries(${mainRPi} PUBLIC ${LIBS})
install(TARGETS ${mainRPi}
  RUNTIME DESTINATION bin
  COMPONENT ${mainRPi})