#include <vtkCamera.h>
#include <vtkColorTransferFunction.h>
#include <vtkContourValues.h>
#include <vtkInteractorStyleTrackballCamera.h>
#include <vtkMetaImageReader.h>
#include <vtkNamedColors.h>
#include <vtkNew.h>
#include <vtkOpenGLGPUVolumeRayCastMapper.h>
#include <vtkPiecewiseFunction.h>
#include <vtkRenderWindow.h>
#include <vtkRenderWindowInteractor.h>
#include <vtkRenderer.h>
#include <vtkVolume.h>
#include <vtkVolumeProperty.h>

#include <iostream>

#include "load_dicom.h"
#include "load_3d.h"
#include "ray_cast_actor.h"

int main(int argc, char* argv[])
{
    if (argc < 2)
    {
        std::cout << "Usage: " << argv[0] << " filepath [is_poly=false]" << std::endl;
        std::cout << "e.g. dicom" << std::endl;
        return EXIT_FAILURE;
    }

    bool is_poly = false;
    vtkSmartPointer<vtkImageData> imgdata = nullptr;
    if (argc > 2 && std::string(argv[2]) == "true")
    {
        auto polydata = ReadPolyData(argv[1]);
        double spacing[]{1, 1, 1};
        imgdata = ConvertMeshPolyDataToImageData(polydata, spacing);
        is_poly = true;
    }
    else
        imgdata = ReadDicomFolder(argv[1]);

    assert(imgdata);

    vtkNew<vtkNamedColors> colors;

    vtkNew<vtkOpenGLGPUVolumeRayCastMapper> mapper;
    mapper->SetInputData(imgdata);
    mapper->Update();
    // mapper->AutoAdjustSampleDistancesOff();
    // mapper->SetSampleDistance(0.5);
    mapper->AutoAdjustSampleDistancesOn();
    mapper->SetBlendModeToMaximumIntensity();

    vtkNew<vtkColorTransferFunction> colorTransferFunction;
    colorTransferFunction->RemoveAllPoints();
    if (is_poly)
    {
        colorTransferFunction->AddRGBPoint(0, 1.0, 1.0, 1.0);
        colorTransferFunction->AddRGBPoint(255, 1.0, 1.0, 1.0);
    }
    else
    {
        // bone: https://examples.vtk.org/site/Cxx/VolumeRendering/FixedPointVolumeRayCastMapperCT/
        colorTransferFunction->AddRGBPoint(-3024, 0, 0, 0, 0.5, 0.0);
        colorTransferFunction->AddRGBPoint(-16, 0.73, 0.25, 0.30, 0.49, .61);
        colorTransferFunction->AddRGBPoint(641, .90, .82, .56, .5, 0.0);
        colorTransferFunction->AddRGBPoint(3071, 1, 1, 1, .5, 0.0);
    }

    vtkNew<vtkPiecewiseFunction> scalarOpacity;
    if (is_poly)
        scalarOpacity->AddSegment(0, 1.0, 256, 0.1);
    else
    {
        scalarOpacity->AddPoint(-3024, 0, 0.5, 0.0);
        scalarOpacity->AddPoint(-16, 0, .49, .61);
        scalarOpacity->AddPoint(641, .72, .5, 0.0);
        scalarOpacity->AddPoint(3071, .71, 0.5, 0.0);
    }

    vtkNew<vtkVolumeProperty> volumeProperty;
    volumeProperty->SetInterpolationTypeToLinear();
    volumeProperty->SetColor(colorTransferFunction);
    volumeProperty->SetScalarOpacity(scalarOpacity);

    vtkNew<vtkVolume> volume;
    volume->SetMapper(mapper);
    volume->SetProperty(volumeProperty);

    vtkNew<vtkRenderer> renderer;
    renderer->AddVolume(volume);
    renderer->SetBackground(colors->GetColor3d("cornflower").GetData());
    renderer->ResetCamera();

    vtkNew<vtkRenderWindow> renderWindow;
    renderWindow->SetSize(800, 600);
    renderWindow->AddRenderer(renderer);
    renderWindow->SetWindowName("RayCastIsosurface");

    vtkNew<vtkInteractorStyleTrackballCamera> style;

    vtkNew<vtkRenderWindowInteractor> interactor;
    interactor->SetRenderWindow(renderWindow);
    interactor->SetInteractorStyle(style);

    renderer->ResetCamera();
    renderer->ResetCameraClippingRange();

    renderWindow->Render();

    interactor->Start();

    return EXIT_SUCCESS;
}