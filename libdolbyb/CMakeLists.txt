include_directories(${CMAKE_CURRENT_BINARY_DIR})

add_library(dolbyb Calibrate DCfilter DiodeClip FindOutSmp HPF1 HPF2 HPF2SetVals HPFAdj LPFAdj Param SetGate SidePath libdolbyb)
