cmake_minimum_required(VERSION 3.10)
project(kthread_driver C)


# executes the Linux command uname -r, captures its output, and stores it in a CMake variable named KERNEL_VERSION.
execute_process(
    COMMAND uname -r
    OUTPUT_VARIABLE   KERNEL_VERSION
    OUTPUT_STRIP_TRAILING_WHITESPACE
)

message(STATUS "Kernel version: ${KERNEL_VERSION}")

#Here we are setting Variable KERNEL_DIR
set(KERNEL_DIR /lib/modules/${KERNEL_VERSION}/build)



# Build module target
add_custom_target(build_module
    COMMAND make 
            -C ${KERNEL_DIR} 
            M=${CMAKE_CURRENT_SOURCE_DIR} 
            modules
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    COMMENT "Building kthread_driver kernel module"
)


# Clean module target
add_custom_target(clean_module
    COMMAND make 
            -C ${KERNEL_DIR} 
            M=${CMAKE_CURRENT_SOURCE_DIR} 
            clean
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    COMMENT "Cleaning kthread_driver build files"
)
