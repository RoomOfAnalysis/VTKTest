#include <vtkNew.h>
#include <vtkDelimitedTextReader.h>
#include <vtkTable.h>
#include <vtkPoints.h>
#include <vtkPolyData.h>
#include <vtkDelaunay2D.h>

#include <vtkSurfaceReconstructionFilter.h>
#include <vtkContourFilter.h>
#include <vtkReverseSense.h>

#include <vtkImageData.h>
#include <vtkImageAccumulate.h>

#include <vtkPCANormalEstimation.h>
#include <vtkSignedDistance.h>
#include <vtkExtractSurface.h>

#include <vtkTextureMapToPlane.h>

#include <vtkPLYWriter.h>
#include <vtkPolyDataNormals.h>
#include <vtkDoubleArray.h>
#include <vtkFloatArray.h>

//#define USE_DELAUNAY2D
#define USE_EXTRACT_SURFACE
#define PLOT

#ifdef PLOT
#include <vtkPolyDataMapper.h>
#include <vtkNamedColors.h>
#include <vtkActor.h>
#include <vtkProperty.h>
#include <vtkVertexGlyphFilter.h>
#include <vtkRenderer.h>
#include <vtkRenderWindow.h>
#include <vtkRenderWindowInteractor.h>
#include <vtkInteractorStyleTrackballCamera.h>
#include <vtkImageReader2Factory.h>
#include <vtkImageReader2.h>
#include <vtkTexture.h>
#include <vtkCubeAxesActor.h>
#include <vtkTextProperty.h>
#include <vtkPointData.h>
#include <vtkTransformTextureCoords.h>
#endif

#include <chrono>
#include <iostream>

vtkSmartPointer<vtkPolyData> delaunay2d(vtkSmartPointer<vtkPolyData> polydata)
{
    vtkNew<vtkDelaunay2D> delaunay;
    delaunay->SetInputData(polydata);
    delaunay->SetTolerance(0.001);
    delaunay->Update();

    return delaunay->GetOutput();
}

vtkSmartPointer<vtkPolyData> surface_reconstruction_filter(vtkSmartPointer<vtkPolyData> polydata)
{
    vtkNew<vtkSurfaceReconstructionFilter> surf;
    surf->SetInputData(polydata);
    //surf->SetNeighborhoodSize(20);
    //surf->SetSampleSpacing(0.05);

    vtkNew<vtkContourFilter> cf;
    cf->SetFastMode(true);
    cf->SetInputConnection(surf->GetOutputPort());
    cf->SetValue(0, 0.0);

    // Sometimes the contouring algorithm can create a volume whose gradient
    // vector and ordering of polygon (using the right hand rule) are
    // inconsistent. vtkReverseSense cures this problem.
    vtkNew<vtkReverseSense> reverse;
    reverse->SetInputConnection(cf->GetOutputPort());
    reverse->ReverseCellsOn();
    reverse->ReverseNormalsOn();
    reverse->Update();

    return reverse->GetOutput();
}

// seems same with vtkSurfaceReconstructionFilter
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
    double radius = std::max(std::max(range[0], range[1]), range[2]) / static_cast<double>(dimension) * 4; // ~4 voxels
    // std::cout << "Radius: " << radius << std::endl;

    distance->SetRadius(radius);
    distance->SetDimensions(dimension, dimension, dimension);
    distance->SetBounds(bounds[0] - range[0] * .1, bounds[1] + range[0] * .1, bounds[2] - range[1] * .1,
                        bounds[3] + range[1] * .1, bounds[4] - range[2] * .1, bounds[5] + range[2] * .1);

    vtkNew<vtkExtractSurface> surface;
    surface->SetInputConnection(distance->GetOutputPort());
    surface->SetRadius(radius * .99);
    surface->HoleFillingOn();
    surface->Update();

    return surface->GetOutput();
}

int main(int argc, char* argv[])
{
    // if (argc < 4)
    // {
    //     std::cout << "Usage: " << argv[0] << " Filename e.g height.txt output.ply texture.png" << std::endl;
    //     return EXIT_FAILURE;
    // }
    std::cout << "Height Path: " << argv[1] << '\n'
              << "Mesh Path: " << argv[2] << '\n'
              << "Texture Path: " << argv[3] << std::endl;

    auto tp = std::chrono::high_resolution_clock::now();

    // vtkDelimitedTextReader can only skip the first row as header...
    vtkNew<vtkDelimitedTextReader> reader;
    reader->SetFileName(argv[1]);
    reader->DetectNumericColumnsOn();
    reader->SetFieldDelimiterCharacters(" ");
    reader->Update();

    vtkTable* table = reader->GetOutput();

    vtkNew<vtkPoints> points;
    for (vtkIdType i = 0; i < table->GetNumberOfRows(); i += 1)
        points->InsertNextPoint((table->GetValue(i, 0)).ToFloat(), (table->GetValue(i, 1)).ToFloat(),
                                (table->GetValue(i, 2)).ToFloat());

    vtkNew<vtkPolyData> polydata;
    polydata->SetPoints(points);

    auto tp1 = std::chrono::high_resolution_clock::now();
    std::cout << "Read XYZ Data Consumes: " << std::chrono::duration_cast<std::chrono::milliseconds>(tp1 - tp).count()
              << " ms" << std::endl;
    tp = tp1;

#ifdef USE_DELAUNAY2D
    auto meshdata = delaunay2d(polydata);
#elif defined(USE_EXTRACT_SURFACE)
    auto meshdata = extract_surface(polydata);
#else
    auto meshdata = surface_reconstruction_filter(polydata);
#endif

    tp1 = std::chrono::high_resolution_clock::now();
    std::cout << "Surface Triangulation Consumes: "
              << std::chrono::duration_cast<std::chrono::milliseconds>(tp1 - tp).count() << " ms" << std::endl;
    tp = tp1;

#ifdef PLOT
    // Visualize
    vtkNew<vtkNamedColors> colors;

    vtkNew<vtkPolyDataMapper> meshMapper;
    vtkNew<vtkActor> meshActor;
#endif
    if (argc >= 3)
    {
        double bounds[6]{};
        meshdata->GetBounds(bounds);

        vtkNew<vtkTextureMapToPlane> texturePlane;
        texturePlane->SetOrigin(bounds[0], bounds[2], bounds[4]); // xmin, ymin, zmin -> BOTTOM LEFT CORNER
        texturePlane->SetPoint1(bounds[1], bounds[2], bounds[4]); // xmax, ymin, zmin -> BOTTOM RIGHT CORNER
        texturePlane->SetPoint2(bounds[0], bounds[3], bounds[4]); // xmin, ymax, zmin -> TOP LEFT CORNER
        texturePlane->SetInputData(meshdata);

        if (argc >= 4)
        {
            // write mesh to ply
            vtkNew<vtkPLYWriter> plyWriter;
            plyWriter->SetFileName(argv[2]);
            plyWriter->SetInputConnection(texturePlane->GetOutputPort());
            plyWriter->Write();
            plyWriter->Update();
        }

        tp1 = std::chrono::high_resolution_clock::now();
        std::cout << "Write Surface Mesh Consumes: "
                  << std::chrono::duration_cast<std::chrono::milliseconds>(tp1 - tp).count() << " ms" << std::endl;

        std::cout << "Successfully Generate Surface Mesh" << std::endl;

#ifdef PLOT
        // need flip R (inverted y)
        // https://examples.vtk.org/site/VTKBook/08Chapter8/#Figure%208-10
        vtkNew<vtkTransformTextureCoords> transformTextureCoords;
        transformTextureCoords->SetInputConnection(texturePlane->GetOutputPort());
        transformTextureCoords->SetFlipR(true);

        //meshMapper->SetInputConnection(texturePlane->GetOutputPort());
        meshMapper->SetInputConnection(transformTextureCoords->GetOutputPort());
        meshMapper->ScalarVisibilityOff();

        vtkNew<vtkImageReader2Factory> readerFactory;
        vtkSmartPointer<vtkImageReader2> textureFile;
        textureFile.TakeReference(readerFactory->CreateImageReader2(argv[3]));
        textureFile->SetFileName(argv[3]);
        textureFile->Update();

        std::cout << textureFile->GetDataExtent()[0] << ", " << textureFile->GetDataExtent()[1] << ", "
                  << textureFile->GetDataExtent()[2] << ", " << textureFile->GetDataExtent()[3] << ", "
                  << textureFile->GetDataExtent()[4] << ", " << textureFile->GetDataExtent()[5] << std::endl;

        vtkNew<vtkTexture> atext;
        atext->SetInputConnection(textureFile->GetOutputPort());
        atext->InterpolateOn();
        meshActor->SetTexture(atext);
#endif
    }
#ifdef PLOT
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
    if (auto* t = meshActor->GetTexture(); t)
    {
        vtkNew<vtkImageAccumulate> acc;
        acc->SetInputData(t->GetImageDataInput(0));
        acc->Update();
        double rgb[3]{};
        acc->GetMean(rgb);
        //std::cout << rgb[0] << ", " << rgb[1] << ", " << rgb[2] << std::endl;
        auto m = (rgb[0] + rgb[1] + rgb[2]) / 3;
        auto a = std::max(1. - m / 255 * 1.5, 0.);
        meshActor->GetProperty()->SetAmbient(a);
    }
    else
        meshActor->GetProperty()->SetAmbient(1.);
    meshActor->GetProperty()->SetDiffuse(1.);
    meshActor->GetProperty()->SetSpecular(.4);
    meshActor->GetProperty()->SetSpecularPower(50);

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
#endif

    return EXIT_SUCCESS;
}