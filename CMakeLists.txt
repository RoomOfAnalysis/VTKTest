cmake_minimum_required(VERSION 3.5)

if(WIN32)
    set(CMAKE_TOOLCHAIN_FILE "D:/vcpkg/scripts/buildsystems/vcpkg.cmake" CACHE INTERNAL "" FORCE)
endif()

project(VTKTest VERSION 0.1 LANGUAGES CXX)

set(CMAKE_SUPPRESS_DEVELOPER_WARNINGS ON CACHE INTERNAL "" FORCE)  # -Wno-dev

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if(WIN32)
    set(DICOM_DIR "D:/vcpkg/installed/x64-windows/share/vtk-dicom/")
endif()

find_package(VTK 9.2 REQUIRED)
find_package(DCMTK CONFIG REQUIRED)
find_package(DICOM REQUIRED)

# to support compressed dicom
# build vtk-dicom with GDCM
# https://dgobbi.github.io/vtk-dicom/doc/api/installation.html
set(VTK_DICOM_LIBRARIES vtkDICOM)

function(add_exe name)
	add_executable(${name} ${name}.cpp)
    target_link_libraries(${name}
        PRIVATE DCMTK::DCMTK
        PRIVATE VTK::DICOM
        PRIVATE ${VTK_LIBRARIES}
    )
    vtk_module_autoinit(
        TARGETS ${name}
        MODULES ${VTK_LIBRARIES}
    )
    if(CMAKE_BUILD_TYPE STREQUAL "Release")
        set_property(TARGET ${name} PROPERTY WIN32_EXECUTABLE TRUE)
    endif()
endfunction()

add_exe(mask)
add_exe(viewer_3d)
add_exe(viewer_four_planes)
add_exe(viewer_3d_plane)
add_exe(overlay)
add_exe(blend)
add_exe(rotate_scene)
add_exe(slice_direction)
add_exe(viewer)
add_exe(image_editor)
add_exe(image_comparator)
add_exe(ray_cast_mip)
add_exe(ctf_lut)
add_exe(offscreen)
add_exe(ome_tiff)
add_exe(surface_viewer)
add_exe(gen_mesh)
