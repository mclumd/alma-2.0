# cmake_minimum_required(VERSION <specify CMake version here>)
project(alma_2_0 C)
cmake_minimum_required(VERSION 3.9)

set(CMAKE_C_STANDARD 11)
set(CMAKE_BUILD_TYPE DEBUG)

include_directories(.)
include_directories(mpc)
include_directories(tommyds/tommyds)

add_executable(alma.x
        mpc/mpc.c
        mpc/mpc.h
        tommyds/tommyds/tommyarray.c
        tommyds/tommyds/tommyhash.c
        tommyds/tommyds/tommyhashlin.c
        tommyds/tommyds/tommylist.c
        alma.c
        alma_backsearch.c
        alma_command.c
        alma_fif.c
        alma_formula.c
        alma_kb.c
        alma_parser.c
        alma_print.c
        alma_proc.c
        alma_unify.c
	res_task_heap.c
	compute_priority.c)
