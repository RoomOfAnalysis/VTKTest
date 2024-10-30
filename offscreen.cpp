#include <vtkActor.h>
#include <vtkGraphicsFactory.h>
#include <vtkNamedColors.h>
#include <vtkNew.h>
#include <vtkPNGWriter.h>
#include <vtkPolyDataMapper.h>
#include <vtkProperty.h>
#include <vtkRenderWindow.h>
#include <vtkRenderer.h>
#include <vtkSphereSource.h>
#include <vtkWindowToImageFilter.h>

// code from https://examples.vtk.org/site/Cxx/Utilities/OffScreenRendering/
int main(int argc, char* argv[])
{
    vtkNew<vtkNamedColors> colors;

    // the doc says: you must turn on VTK_OPENGL_HAS_OSMESA in the VTK advanced build configuration
    // and use the following `vtkGraphicsFactory` code
    // while offscreen seems  still working if i comment them out or use `0` instead of 1
    // according to the [wiki](https://en.wikipedia.org/wiki/Mesa_(computer_graphics)),
    // mesa is only a software implementation of opengl api, i don't know why they need it...
    // actually just using fbo in opengl is able to achive offscreen rendering...
    // https://www.paraview.org/Wiki/VTK/OpenGL#Mesa,_Off_Screen_Mesa,_Mangled_Mesa
    // https://vtkusers.public.kitware.narkive.com/Y0H4loJW/vtk-offscreen-rendering
    // https://www.kitware.com/off-screen-rendering-through-the-native-platform-interface-egl/

    // // Setup offscreen rendering
    // vtkNew<vtkGraphicsFactory> graphics_factory;
    // graphics_factory->SetOffScreenOnlyMode(1);
    // graphics_factory->SetUseMesaClasses(1);

    // i think this answer from mail list solves my question:
    // https://public.kitware.com/pipermail/vtk-developers/2018-November/036541.html
    // OffScreen does not require EGL or Mesa.
    // You should be able to do offscreen with any opengl implementation.
    // When folks mention offscreen, sometimes they mean "rendering without an
    // xserver" or "rendering on a system without opengl 3.2"  for those cases you
    // can use EGL or Mesa or OSMesa depending on the issue.

    // Create a sphere
    vtkNew<vtkSphereSource> sphereSource;

    // Create a mapper and actor
    vtkNew<vtkPolyDataMapper> mapper;
    mapper->SetInputConnection(sphereSource->GetOutputPort());

    vtkNew<vtkActor> actor;
    actor->SetMapper(mapper);
    actor->GetProperty()->SetColor(colors->GetColor3d("White").GetData());

    // A renderer and render window
    vtkNew<vtkRenderer> renderer;
    vtkNew<vtkRenderWindow> renderWindow;
    renderWindow->SetOffScreenRendering(1);
    renderWindow->AddRenderer(renderer);

    // Add the actors to the scene
    renderer->AddActor(actor);
    renderer->SetBackground(colors->GetColor3d("SlateGray").GetData());

    renderWindow->Render();

    vtkNew<vtkWindowToImageFilter> windowToImageFilter;
    windowToImageFilter->SetInput(renderWindow);
    windowToImageFilter->Update();

    vtkNew<vtkPNGWriter> writer;
    writer->SetFileName("screenshot.png");
    writer->SetInputConnection(windowToImageFilter->GetOutputPort());
    writer->Write();

    return EXIT_SUCCESS;
}