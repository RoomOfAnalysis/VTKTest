#include <vtkNew.h>
#include <vtkPoints.h>
#include <vtkPolyData.h>
#include <vtkPolyDataMapper.h>
#include <vtkNamedColors.h>
#include <vtkActor.h>
#include <vtkProperty.h>
#include <vtkRenderer.h>
#include <vtkRenderWindow.h>
#include <vtkRenderWindowInteractor.h>
#include <vtkInteractorStyleTrackballCamera.h>
#include <vtkImageReader2Factory.h>
#include <vtkImageReader2.h>
#include <vtkTexture.h>
#include <vtkTextureMapToPlane.h>
#include <vtkCubeAxesActor.h>
#include <vtkTextProperty.h>
#include <vtkPointData.h>

#include <vtkImageData.h>
#include <vtkImageAccumulate.h>

#include <vtkPCANormalEstimation.h>
#include <vtkSignedDistance.h>
#include <vtkExtractSurface.h>

#include <vtkPolyDataNormals.h>
#include <vtkDoubleArray.h>
#include <vtkFloatArray.h>

#include <vtkTransform.h>
#include <vtkTransformPolyDataFilter.h>
#include <vtkReverseSense.h>

#include "load_3d.h"

bool HasPointNormals(vtkPolyData* polydata)
{
    std::cout << "In HasPointNormals: " << polydata->GetNumberOfPoints() << std::endl;
    std::cout << "Looking for point normals..." << std::endl;

    // Count points
    vtkIdType numPoints = polydata->GetNumberOfPoints();
    std::cout << "There are " << numPoints << " points." << std::endl;

    // Count triangles
    vtkIdType numPolys = polydata->GetNumberOfPolys();
    std::cout << "There are " << numPolys << " polys." << std::endl;

    // Double normals in an array
    vtkDoubleArray* normalDataDouble = dynamic_cast<vtkDoubleArray*>(polydata->GetPointData()->GetArray("Normals"));

    if (normalDataDouble)
    {
        int nc = normalDataDouble->GetNumberOfTuples();
        std::cout << "There are " << nc << " components in normalDataDouble" << std::endl;
        return true;
    }

    // Float normals in an array
    vtkFloatArray* normalDataFloat = dynamic_cast<vtkFloatArray*>(polydata->GetPointData()->GetArray("Normals"));

    if (normalDataFloat)
    {
        int nc = normalDataFloat->GetNumberOfTuples();
        std::cout << "There are " << nc << " components in normalDataFloat" << std::endl;
        return true;
    }

    // Double
    vtkDoubleArray* normalsDouble = dynamic_cast<vtkDoubleArray*>(polydata->GetPointData()->GetNormals());

    if (normalsDouble)
    {
        std::cout << "There are " << normalsDouble->GetNumberOfComponents() << " components in normalsDouble"
                  << std::endl;
        return true;
    }

    // Float
    vtkFloatArray* normalsFloat = dynamic_cast<vtkFloatArray*>(polydata->GetPointData()->GetNormals());

    if (normalsFloat)
    {
        std::cout << "There are " << normalsFloat->GetNumberOfComponents() << " components in normalsFloat"
                  << std::endl;
        return true;
    }

    // Generic type
    vtkDataArray* normalsGeneric = polydata->GetPointData()->GetNormals(); // works
    if (normalsGeneric)
    {
        std::cout << "There are " << normalsGeneric->GetNumberOfTuples() << " normals in normalsGeneric" << std::endl;

        double testDouble[3];
        normalsGeneric->GetTuple(0, testDouble);

        std::cout << "Double: " << testDouble[0] << " " << testDouble[1] << " " << testDouble[2] << std::endl;
        return true;
    }

    // If the function has not yet quit, there were none of these types of normals
    std::cout << "Normals not found!" << std::endl;
    return false;
}

vtkSmartPointer<vtkPolyData> extract_surface(vtkSmartPointer<vtkPolyData> polydata)
{
    double bounds[6];
    polydata->GetBounds(bounds);
    double range[3];
    for (int i = 0; i < 3; ++i)
        range[i] = bounds[2 * i + 1] - bounds[2 * i];

    int sampleSize = polydata->GetNumberOfPoints() * 0.00005;
    if (sampleSize < 10) sampleSize = 10;

    vtkNew<vtkPCANormalEstimation> normals;
    normals->SetInputData(polydata);
    normals->SetSampleSize(sampleSize);
    normals->SetNormalOrientationToGraphTraversal();
    normals->FlipNormalsOn();

    vtkNew<vtkSignedDistance> distance;
    distance->SetInputConnection(normals->GetOutputPort());

    int dimension = 256;
    double radius;
    radius = std::max(std::max(range[0], range[1]), range[2]) / static_cast<double>(dimension) * 4; // ~4 voxels
    std::cout << "Radius: " << radius << std::endl;

    distance->SetRadius(radius);
    distance->SetDimensions(dimension, dimension, dimension);
    distance->SetBounds(bounds[0] - range[0] * .1, bounds[1] + range[0] * .1, bounds[2] - range[1] * .1,
                        bounds[3] + range[1] * .1, bounds[4] - range[2] * .1, bounds[5] + range[2] * .1);

    vtkNew<vtkExtractSurface> surface;
    surface->SetInputConnection(distance->GetOutputPort());
    surface->SetRadius(radius * .99);
    surface->Update();

    return surface->GetOutput();
}

int main(int argc, char* argv[])
{
    if (argc < 2)
    {
        std::cout << "Usage: " << argv[0] << " Filename e.g mesh.ply texture.png(optional)" << std::endl;
        return EXIT_FAILURE;
    }

    auto meshdata = ReadPolyData(argv[1]);

    double bounds[6]{};
    meshdata->GetBounds(bounds);
    std::cout << bounds[0] << ", " << bounds[1] << ", " << bounds[2] << ", " << bounds[3] << ", " << bounds[4] << ", "
              << bounds[5] << std::endl;

    if (!HasPointNormals(meshdata))
        meshdata = extract_surface(meshdata);
    else
    {
        // mirror meshdata (done by CPU)
        vtkNew<vtkTransform> trans;
        trans->PostMultiply();
        trans->Scale(1, -1, 1);
        trans->Translate(0, bounds[2] + bounds[3], 0);
        vtkNew<vtkTransformPolyDataFilter> tf;
        tf->SetInputData(meshdata);
        tf->SetTransform(trans);

        // after mirror ops (mirror mesh / mirror texture / mirror plane / mirror mesh actor), the actor turns pure black -> which means the normals are pointing towards the center of the object instead of pointing outward (flipped normals)
        // so have reverse the normals
        vtkNew<vtkReverseSense> rs;
        rs->SetInputConnection(tf->GetOutputPort());
        rs->SetReverseNormals(true);
        rs->SetReverseCells(false);
        rs->Update();

        meshdata = rs->GetOutput();
    }

    // Visualize
    vtkNew<vtkNamedColors> colors;

    vtkNew<vtkPolyDataMapper> meshMapper;
    vtkNew<vtkActor> meshActor;
    if (argc == 3)
    {
        vtkNew<vtkImageReader2Factory> readerFactory;
        vtkSmartPointer<vtkImageReader2> textureFile;
        textureFile.TakeReference(readerFactory->CreateImageReader2(argv[2]));
        textureFile->SetFileName(argv[2]);
        textureFile->Update();

        // std::cout << textureFile->GetDataExtent()[0] << ", " << textureFile->GetDataExtent()[1] << ", "
        //           << textureFile->GetDataExtent()[2] << ", " << textureFile->GetDataExtent()[3] << ", "
        //           << textureFile->GetDataExtent()[4] << ", " << textureFile->GetDataExtent()[5] << std::endl;

        vtkNew<vtkTexture> atext;
        atext->SetInputConnection(textureFile->GetOutputPort());
        atext->InterpolateOn();

        // // mirror texture
        // vtkNew<vtkTransform> trans;
        // trans->PostMultiply();
        // trans->Scale(1, -1, 1);
        // trans->Translate(0, 2, 0);
        // atext->SetTransform(trans);

        // https://github.com/pyvista/pyvista/blob/release/0.44/pyvista/core/filters/data_set.py#L1999
        vtkNew<vtkTextureMapToPlane> texturePlane;
        texturePlane->SetOrigin(bounds[0], bounds[2], bounds[4]); // xmin, ymin, zmin -> BOTTOM LEFT CORNER
        texturePlane->SetPoint1(bounds[1], bounds[2], bounds[4]); // xmax, ymin, zmin -> BOTTOM RIGHT CORNER
        texturePlane->SetPoint2(bounds[0], bounds[3], bounds[4]); // xmin, ymax, zmin -> TOP LEFT CORNER

        // // mirror texturePlane
        // texturePlane->SetOrigin(bounds[0], bounds[3], bounds[4]);
        // texturePlane->SetPoint1(bounds[1], bounds[3], bounds[4]);
        // texturePlane->SetPoint2(bounds[0], bounds[2], bounds[4]);
        texturePlane->SetInputData(meshdata);

        meshMapper->SetInputConnection(texturePlane->GetOutputPort());
        meshMapper->ScalarVisibilityOff();

        meshActor->SetTexture(atext);
    }
    else
    {
        meshMapper->SetInputData(meshdata);
        meshMapper->ScalarVisibilityOff();

        meshActor->GetProperty()->SetColor(colors->GetColor3d("LightGoldenrodYellow").GetData());
        meshActor->GetProperty()->EdgeVisibilityOn();
        meshActor->GetProperty()->SetEdgeColor(colors->GetColor3d("CornflowerBlue").GetData());
        meshActor->GetProperty()->SetLineWidth(3);
        meshActor->GetProperty()->RenderLinesAsTubesOn();
    }
    meshActor->SetMapper(meshMapper);

    // // mirror meshActor (done by GPU pipeline)
    // vtkNew<vtkTransform> trans;
    // trans->PostMultiply();
    // trans->Scale(1, -1, 1);
    // trans->Translate(0, bounds[2] + bounds[3], 0);
    // meshActor->SetUserTransform(trans);

    if (auto* t = meshActor->GetTexture(); t)
    {
        // vtkNew<vtkImageAccumulate> acc;
        // acc->SetInputData(t->GetImageDataInput(0));
        // acc->Update();
        // double rgb[3]{};
        // acc->GetMean(rgb);
        // //std::cout << rgb[0] << ", " << rgb[1] << ", " << rgb[2] << std::endl;
        // auto m = (rgb[0] + rgb[1] + rgb[2]) / 3;
        // auto a = std::max(1. - m / 255 * 1.5, 0.);
        // std::cout << a << std::endl;
        // meshActor->GetProperty()->SetAmbient(a);
    }
    else
        meshActor->GetProperty()->SetAmbient(1.);
    meshActor->GetProperty()->SetDiffuse(1.);
    // meshActor->GetProperty()->SetSpecular(.4);
    // meshActor->GetProperty()->SetSpecularPower(50);

    vtkNew<vtkRenderer> renderer;
    renderer->SetBackground(colors->GetColor3d("Black").GetData());
    vtkNew<vtkRenderWindow> renderWindow;
    renderWindow->SetSize(600, 600);
    renderWindow->SetWindowName("SurfaceViewer");
    renderWindow->AddRenderer(renderer);
    vtkNew<vtkRenderWindowInteractor> renderWindowInteractor;
    renderWindowInteractor->SetRenderWindow(renderWindow);
    vtkNew<vtkInteractorStyleTrackballCamera> style;
    renderWindowInteractor->SetInteractorStyle(style);

    renderer->AddActor(meshActor);

    vtkNew<vtkCubeAxesActor> cubeAxisActor;
    cubeAxisActor->SetUseTextActor3D(1);
    cubeAxisActor->SetCamera(renderer->GetActiveCamera());
    cubeAxisActor->GetTitleTextProperty(0)->SetColor(1, 0, 0);
    cubeAxisActor->GetTitleTextProperty(0)->SetFontSize(48);
    cubeAxisActor->GetLabelTextProperty(0)->SetColor(1, 1, 1);
    cubeAxisActor->GetTitleTextProperty(1)->SetColor(0, 1, 0);
    cubeAxisActor->GetLabelTextProperty(1)->SetColor(1, 1, 1);
    cubeAxisActor->GetTitleTextProperty(2)->SetColor(0, 0, 1);
    cubeAxisActor->GetLabelTextProperty(2)->SetColor(1, 1, 1);
    cubeAxisActor->DrawXGridlinesOn();
    cubeAxisActor->DrawYGridlinesOn();
    cubeAxisActor->DrawZGridlinesOn();
    cubeAxisActor->SetGridLineLocation(vtkCubeAxesActor::GridVisibility::VTK_GRID_LINES_FURTHEST);
    cubeAxisActor->XAxisMinorTickVisibilityOff();
    cubeAxisActor->YAxisMinorTickVisibilityOff();
    cubeAxisActor->ZAxisMinorTickVisibilityOff();
    cubeAxisActor->SetFlyModeToStaticEdges();
    cubeAxisActor->SetBounds(meshActor->GetBounds());

    renderer->AddActor(cubeAxisActor);

    renderer->ResetCamera();

    renderWindow->Render();
    renderWindowInteractor->Start();

    return EXIT_SUCCESS;
}