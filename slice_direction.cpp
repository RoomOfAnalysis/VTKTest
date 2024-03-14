#include <dcmtk/dcmdata/dcfilefo.h>
#include <dcmtk/dcmdata/dcdeftag.h>

#include <vtkVector.h>
#include <vtkNew.h>
#include <vtkDICOMReader.h>
#include <vtkStringArray.h>
#include <vtkDICOMSorter.h>
#include <vtkImageData.h>
#include <vtkImagePlaneWidget.h>
#include <vtkRenderWindowInteractor.h>
#include <vtkProperty.h>
#include <vtkTextProperty.h>
#include <vtkRenderWindowInteractor.h>
#include <vtkRenderWindow.h>
#include <vtkRenderer.h>
#include <vtkInteractorStyleTrackballCamera.h>
#include <vtkCubeAxesActor.h>
#include <vtkNamedColors.h>
#include <vtkAnnotatedCubeActor.h>
#include <vtkOrientationMarkerWidget.h>
#include <vtkPlaneSource.h>
#include <vtkPolyDataMapper.h>
#include <vtkTextActor.h>

#include <iostream>
#include <filesystem>
#include <string>
#include <string_view>
#include <charconv>

// https://www.cnblogs.com/h2zZhou/p/9072967.html
static std::string ComputeOrientation(vtkVector3<double> const& vector)
{
    std::cout << vector << '\n';

    std::string orientation;
    orientation.reserve(3);

    char orientationX = vector.GetX() < 0 ? 'R' : 'L';
    char orientationY = vector.GetY() < 0 ? 'A' : 'P';
    char orientationZ = vector.GetZ() < 0 ? 'F' : 'H';

    double absX = fabs(vector.GetX());
    double absY = fabs(vector.GetY());
    double absZ = fabs(vector.GetZ());

    for (auto i = 0; i < 3; i++)
        if (absX > .0001 && absX > absY && absX > absZ)
        {
            orientation += orientationX;
            absX = 0;
        }
        else if (absY > .0001 && absY > absX && absY > absZ)
        {
            orientation += orientationY;
            absY = 0;
        }
        else if (absZ > .0001 && absZ > absX && absZ > absY)
        {
            orientation += orientationZ;
            absZ = 0;
        }
        else
            break;

    return orientation;
}

static std::vector<std::string_view> split(std::string_view sv, std::string_view delimeter = " ")
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

static std::pair<double, bool> parse_double(std::string_view sv)
{
    double x{};
    if (std::from_chars(sv.data(), sv.data() + sv.size(), x).ec == std::errc{}) return std::make_pair(x, true);
    return std::make_pair(x, false);
}

static vtkVector3<double> parse_3d(std::string const& data)
{
    vtkVector3<double> res{};
    if (auto ns = split(data, "\\"); !ns.empty())
        for (auto i = 0; i < 3; i++)
            if (auto [x, ok] = parse_double(ns[i]); !ok)
                std::cerr << "error to parse double from: " << ns[i] << '\n';
            else
                res[i] = x;
    return res;
}

static vtkSmartPointer<vtkAnnotatedCubeActor> MakeAnnotatedCubeActor(vtkNamedColors* colors)
{
    // A cube with labeled faces.
    vtkNew<vtkAnnotatedCubeActor> cube;
    cube->SetXPlusFaceText("R");  // Right
    cube->SetXMinusFaceText("L"); // Left
    cube->SetYPlusFaceText("A");  // Anterior
    cube->SetYMinusFaceText("P"); // Posterior
    cube->SetZPlusFaceText("S");  // Superior/Cranial
    cube->SetZMinusFaceText("I"); // Inferior/Caudal
    cube->SetFaceTextScale(0.5);
    cube->GetCubeProperty()->SetColor(colors->GetColor3d("Gainsboro").GetData());

    cube->GetTextEdgesProperty()->SetColor(colors->GetColor3d("LightSlateGray").GetData());

    // Change the vector text colors.
    cube->GetXPlusFaceProperty()->SetColor(colors->GetColor3d("Tomato").GetData());
    cube->GetXMinusFaceProperty()->SetColor(colors->GetColor3d("Tomato").GetData());
    cube->GetYPlusFaceProperty()->SetColor(colors->GetColor3d("DeepSkyBlue").GetData());
    cube->GetYMinusFaceProperty()->SetColor(colors->GetColor3d("DeepSkyBlue").GetData());
    cube->GetZPlusFaceProperty()->SetColor(colors->GetColor3d("SeaGreen").GetData());
    cube->GetZMinusFaceProperty()->SetColor(colors->GetColor3d("SeaGreen").GetData());
    return cube;
}

int main(int argc, char* argv[])
{
    if (argc != 2)
    {
        std::cerr << "1) dicom folder path" << std::endl;
        return 1;
    }

    std::filesystem::path dicom_folder{argv[1]};
    // first file
    std::filesystem::path file_path;
    for (auto const& entry : std::filesystem::directory_iterator{dicom_folder})
        if (entry.is_regular_file()) file_path = entry.path();
    auto file = std::make_unique<DcmFileFormat>();
    if (file->loadFile(file_path.c_str()).bad())
    {
        std::cerr << "Can NOT open file: " << argv[1];
        return -1;
    }
    auto* data_set = file->getDataset();
    std::string image_position, image_orientation, patient_position;
    data_set->findAndGetOFStringArray(DCM_ImagePositionPatient, image_position);
    data_set->findAndGetOFStringArray(DCM_ImageOrientationPatient, image_orientation);
    data_set->findAndGetOFStringArray(DCM_PatientPosition, patient_position);
    std::cout << "image_position: " << image_position << '\n'
              << "image_orientation: " << image_orientation << '\n'
              << "patient_position: " << patient_position << '\n';

    // read series
    vtkNew<vtkDICOMReader> dicom_reader;
    vtkNew<vtkStringArray> dicom_img_paths;
    vtkNew<vtkDICOMSorter> dicom_sorter;
    int count = 0;
    for (auto const& it : std::filesystem::directory_iterator(dicom_folder))
        dicom_img_paths->InsertValue(count++, it.path().string());
    dicom_sorter->SetInputFileNames(dicom_img_paths);
    dicom_sorter->Update();
    dicom_reader->SetFileNames(dicom_sorter->GetFileNamesForSeries(0));
    dicom_reader->SetDataByteOrderToLittleEndian();
    dicom_reader->Update(0);

    auto x_dim = dicom_reader->GetOutput()->GetDimensions()[0];

    vtkNew<vtkRenderer> renderer, renderer_plane;
    vtkNew<vtkRenderWindow> render_window;
    render_window->AddRenderer(renderer);
    render_window->AddRenderer(renderer_plane);
    render_window->SetSize(800, 800);
    render_window->SetWindowName("SliceDirection");
    renderer->SetViewport(0, 0, 0.5, 1);
    renderer_plane->SetViewport(0.5, 0, 1, 1);
    vtkNew<vtkRenderWindowInteractor> interactor;
    interactor->SetRenderWindow(render_window);
    vtkNew<vtkInteractorStyleTrackballCamera> style;
    interactor->SetInteractorStyle(style);

    // cube axis with dimension labels
    vtkNew<vtkCubeAxesActor> cube_axis_actor;
    cube_axis_actor->SetUseTextActor3D(1);
    cube_axis_actor->SetBounds(dicom_reader->GetOutput()->GetBounds());
    cube_axis_actor->SetCamera(renderer->GetActiveCamera());
    cube_axis_actor->GetTitleTextProperty(0)->SetColor(1, 0, 0);
    cube_axis_actor->GetTitleTextProperty(0)->SetFontSize(48);
    cube_axis_actor->GetLabelTextProperty(0)->SetColor(1, 1, 1);
    cube_axis_actor->GetTitleTextProperty(1)->SetColor(0, 1, 0);
    cube_axis_actor->GetLabelTextProperty(1)->SetColor(1, 1, 1);
    cube_axis_actor->GetTitleTextProperty(2)->SetColor(0, 0, 1);
    cube_axis_actor->GetLabelTextProperty(2)->SetColor(1, 1, 1);
    cube_axis_actor->DrawXGridlinesOn();
    cube_axis_actor->DrawYGridlinesOn();
    cube_axis_actor->DrawZGridlinesOn();
    cube_axis_actor->SetGridLineLocation(cube_axis_actor->VTK_GRID_LINES_FURTHEST);
    cube_axis_actor->XAxisMinorTickVisibilityOff();
    cube_axis_actor->YAxisMinorTickVisibilityOff();
    cube_axis_actor->ZAxisMinorTickVisibilityOff();
    cube_axis_actor->SetFlyModeToStaticEdges();
    renderer->AddActor(cube_axis_actor);

    // Dicom LPS coordinate system
    vtkNew<vtkNamedColors> colors;
    auto annotated_cube_axis_actor = MakeAnnotatedCubeActor(colors);
    vtkNew<vtkOrientationMarkerWidget> oriebtation_makrer_widget;
    oriebtation_makrer_widget->SetOrientationMarker(annotated_cube_axis_actor);
    oriebtation_makrer_widget->SetViewport(0, 0, 0.2, 0.2); // lower left in the viewport
    oriebtation_makrer_widget->SetInteractor(interactor);
    oriebtation_makrer_widget->On();

    // plane widget
    vtkNew<vtkImagePlaneWidget> plane_widget;
    plane_widget->SetInteractor(interactor);
    plane_widget->RestrictPlaneToVolumeOn();
    plane_widget->DisplayTextOn();
    plane_widget->SetResliceInterpolateToCubic();
    plane_widget->SetInputConnection(dicom_reader->GetOutputPort());
    plane_widget->GetCursorProperty()->SetColor(0, 1, 0);
    plane_widget->GetMarginProperty()->SetColor(0, 1, 1);
    plane_widget->GetPlaneProperty()->SetColor(0, 0, 1);
    plane_widget->GetSelectedPlaneProperty()->SetColor(1, 0, 0);
    plane_widget->GetTextProperty()->SetColor(1, 1, 1);
    plane_widget->SetPlaneOrientationToXAxes();
    plane_widget->SetSliceIndex(x_dim / 2);
    plane_widget->On();

    // slice plane
    vtkNew<vtkPlaneSource> plane;
    vtkNew<vtkPolyDataMapper> plane_mapper;
    vtkNew<vtkActor> plane_actor;
    plane_mapper->SetInputConnection(plane->GetOutputPort());
    plane_actor->SetMapper(plane_mapper);
    plane_actor->SetTexture(plane_widget->GetTexture());
    renderer_plane->AddActor(plane_actor);

    // slice direction
    // text actors' positions are in display coordinate
    // TODO: use viewport pos
    // TODO: obtain orientation from plane widget slice
    vtkNew<vtkTextActor> text_actor_left, text_actor_right, text_actor_up, text_actor_down;
    text_actor_left->SetDisplayPosition(450, 400);
    text_actor_left->SetInput("L");
    text_actor_right->SetDisplayPosition(750, 400);
    text_actor_right->SetInput("R");
    text_actor_up->SetDisplayPosition(600, 600);
    text_actor_up->SetInput("U");
    text_actor_down->SetDisplayPosition(600, 200);
    text_actor_down->SetInput("D");
    renderer_plane->AddActor(text_actor_left);
    renderer_plane->AddActor(text_actor_right);
    renderer_plane->AddActor(text_actor_up);
    renderer_plane->AddActor(text_actor_down);

    render_window->Render();
    renderer->ResetCamera();

    interactor->Initialize();
    oriebtation_makrer_widget->InteractiveOff();
    interactor->Start();

    return 0;
}