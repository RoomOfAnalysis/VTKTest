#include <vtkNew.h>
#include <vtkDelimitedTextReader.h>
#include <vtkTable.h>
#include <vtkPoints.h>
#include <vtkPolyData.h>
#include <vtkDelaunay2D.h>
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
#include <vtkTextureMapToPlane.h>
#include <vtkCubeAxesActor.h>
#include <vtkTextProperty.h>
#include <vtkPointData.h>

#include <vtkSurfaceReconstructionFilter.h>
#include <vtkContourFilter.h>
#include <vtkReverseSense.h>

#include <vtkImageData.h>
#include <vtkImageAccumulate.h>

#include <vtkPCANormalEstimation.h>
#include <vtkSignedDistance.h>
#include <vtkExtractSurface.h>

#include <vtkPLYWriter.h>
#include <vtkPolyDataNormals.h>
#include <vtkDoubleArray.h>
#include <vtkFloatArray.h>

#include <vector>
#include <string>
#include <string_view>
#include <charconv>
#include <fstream>

//#define USE_DELAUNAY2D
#define USE_EXTRACT_SURFACE
//#define PLOT_VERTEX
#define EXPORT_POINTS_PLY

constexpr int DOWN_SAMPLE_SPACING = 10;

std::vector<std::string_view> split(std::string_view sv, std::string_view delimeter = " ")
{
    std::vector<std::string_view> tokens{};
    size_t pos = 0;
    while ((pos = sv.find(delimeter)) != std::string_view::npos)
    {
        auto token = sv.substr(0, pos);
        if (!token.empty()) tokens.push_back(token);
        sv = sv.substr(pos + delimeter.size());
    }
    if (!sv.empty()) tokens.push_back(sv);
    return tokens;
}

template <typename T> T parse_num(std::string_view token)
{
    T num{};
    std::from_chars(token.data(), token.data() + token.size(), num);
    return num;
}

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

bool write2ply(vtkSmartPointer<vtkPolyData> polydata, std::string filename)
{
    if (!HasPointNormals(polydata))
    {
        // `vtkPolyDataNormals` requires polydata has triangle or polygon polys
        vtkNew<vtkPolyDataNormals> normalGenerator;
        normalGenerator->SetInputData(polydata);
        normalGenerator->ComputePointNormalsOn();
        normalGenerator->ComputeCellNormalsOff();
        //normalGenerator->SetFeatureAngle(0.1);
        //normalGenerator->SetSplitting(1);
        //normalGenerator->SetConsistency(0);
        //normalGenerator->SetAutoOrientNormals(0);
        //normalGenerator->SetFlipNormals(0);
        //normalGenerator->SetNonManifoldTraversal(1);
        normalGenerator->Update();

        polydata = normalGenerator->GetOutput();

        std::cout << std::boolalpha << HasPointNormals(polydata) << std::endl;
    }

    vtkNew<vtkPLYWriter> plyWriter;
    plyWriter->SetFileName(filename.c_str());
    plyWriter->SetInputData(polydata);
    return plyWriter->Write() == 1;
}

int main(int argc, char* argv[])
{
    if (argc < 2)
    {
        std::cout << "Usage: " << argv[0] << " Filename e.g height.csv texture.png(optional)" << std::endl;
        return EXIT_FAILURE;
    }

    // // vtkDelimitedTextReader can only skip the first row as header...
    // vtkNew<vtkDelimitedTextReader> reader;
    // reader->SetFileName(argv[1]);
    // reader->SetHaveHeaders(true);
    // reader->DetectNumericColumnsOn();
    // reader->SetFieldDelimiterCharacters(",");
    // reader->Update();

    // vtkTable* table = reader->GetOutput();
    // std::cout << "Table has " << table->GetNumberOfRows() << " rows." << std::endl;
    // std::cout << "Table has " << table->GetNumberOfColumns() << " columns." << std::endl;

    // vtkNew<vtkPoints> points;
    // // skip firt 4 rows
    // for (vtkIdType i = 5; i < table->GetNumberOfRows(); i++)
    //     for (vtkIdType j = 0; j < table->GetNumberOfColumns(); j++)
    //         points->InsertNextPoint(i - 5, j, (table->GetValue(i, j)).ToDouble());

    // std::cout << "There are " << points->GetNumberOfPoints() << " points." << std::endl;

    std::ifstream ifs(argv[1]);
    std::string line;
    std::getline(ifs, line);
    auto cols = parse_num<int>(split(line, ",")[1].substr(1));
    std::getline(ifs, line);
    auto rows = parse_num<int>(split(line, ",")[1].substr(1));

    std::cout << rows << " rows, " << cols << " cols" << std::endl;

    double dx = 2085.059 / (cols - 1);
    double dy = 1393.193 / (rows - 1);

    std::getline(ifs, line);
    std::getline(ifs, line);
    line.reserve(cols);
    vtkNew<vtkPoints> points;
    // downsample
    for (vtkIdType i = 0; i < rows; i += DOWN_SAMPLE_SPACING)
    {
        std::getline(ifs, line);
        auto c = split(line, ", ");
        for (vtkIdType j = 0; j < cols; j += DOWN_SAMPLE_SPACING)
            points->InsertNextPoint(j * dx, i * dy, parse_num<double>(c[j]));
    }

    std::cout << "There are " << points->GetNumberOfPoints() << " points." << std::endl;

    vtkNew<vtkPolyData> polydata;
    polydata->SetPoints(points);

#ifdef USE_DELAUNAY2D
    auto meshdata = delaunay2d(polydata);
#elif defined(USE_EXTRACT_SURFACE)
    auto meshdata = extract_surface(polydata);
#else
    auto meshdata = surface_reconstruction_filter(polydata);
#endif

#ifdef EXPORT_POINTS_PLY
    std::cout << "write2ply " << (write2ply(meshdata, "points.ply") ? "successfully" : "failed") << std::endl;
#endif

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
        textureFile->SetDataSpacing(dx, dy, 1);
        textureFile->Update();

        std::cout << textureFile->GetDataExtent()[0] << ", " << textureFile->GetDataExtent()[1] << ", "
                  << textureFile->GetDataExtent()[2] << ", " << textureFile->GetDataExtent()[3] << ", "
                  << textureFile->GetDataExtent()[4] << ", " << textureFile->GetDataExtent()[5] << std::endl;

        vtkNew<vtkTexture> atext;
        atext->SetInputConnection(textureFile->GetOutputPort());
        atext->InterpolateOn();

        double bounds[6]{};
        meshdata->GetBounds(bounds);
        std::cout << bounds[0] << ", " << bounds[1] << ", " << bounds[2] << ", " << bounds[3] << ", " << bounds[4]
                  << ", " << bounds[5] << std::endl;

        vtkNew<vtkTextureMapToPlane> texturePlane;
        texturePlane->SetOrigin(bounds[0], bounds[2], bounds[4]); // xmin, ymin, zmin -> BOTTOM LEFT CORNER
        texturePlane->SetPoint1(bounds[1], bounds[2], bounds[4]); // xmax, ymin, zmin -> BOTTOM RIGHT CORNER
        texturePlane->SetPoint2(bounds[0], bounds[3], bounds[4]); // xmin, ymax, zmin -> TOP LEFT CORNER
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

#ifdef PLOT_VERTEX
    vtkNew<vtkVertexGlyphFilter> glyphFilter;
    glyphFilter->SetInputData(polydata);

    vtkNew<vtkPolyDataMapper> pointMapper;
    pointMapper->SetInputConnection(glyphFilter->GetOutputPort());

    vtkNew<vtkActor> pointActor;
    pointActor->SetMapper(pointMapper);
    pointActor->GetProperty()->SetColor(colors->GetColor3d("DeepPink").GetData());
    pointActor->GetProperty()->SetPointSize(1);
    pointActor->GetProperty()->RenderPointsAsSpheresOn();
    renderer->AddActor(pointActor);
#endif

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