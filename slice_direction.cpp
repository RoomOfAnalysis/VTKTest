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
#include <vtkAxesActor.h>
#include <vtkCaptionActor2D.h>
#include <vtkPropAssembly.h>

#include <iostream>
#include <filesystem>
#include <string>
#include <string_view>
#include <charconv>

//// https://www.cnblogs.com/h2zZhou/p/9072967.html
//static std::string ComputeOrientation(vtkVector3<double> const& vector)
//{
//    std::cout << vector << '\n';
//
//    std::string orientation;
//    orientation.reserve(3);
//
//    char orientationX = vector.GetX() < 0 ? 'R' : 'L';
//    char orientationY = vector.GetY() < 0 ? 'A' : 'P';
//    char orientationZ = vector.GetZ() < 0 ? 'F' : 'H';
//
//    double absX = fabs(vector.GetX());
//    double absY = fabs(vector.GetY());
//    double absZ = fabs(vector.GetZ());
//
//    for (auto i = 0; i < 3; i++)
//        if (absX > .0001 && absX > absY && absX > absZ)
//        {
//            orientation += orientationX;
//            absX = 0;
//        }
//        else if (absY > .0001 && absY > absX && absY > absZ)
//        {
//            orientation += orientationY;
//            absY = 0;
//        }
//        else if (absZ > .0001 && absZ > absX && absZ > absY)
//        {
//            orientation += orientationZ;
//            absZ = 0;
//        }
//        else
//            break;
//
//    return orientation;
//}

//static std::vector<std::string_view> split(std::string_view sv, std::string_view delimeter = " ")
//{
//    std::vector<std::string_view> tokens{};
//    size_t pos = 0;
//    while ((pos = sv.find(delimeter)) != std::string_view::npos)
//    {
//        auto token = sv.substr(0, pos);
//        if (!token.empty()) tokens.push_back(token);
//        sv = sv.substr(pos + delimeter.size());
//    }
//    if (!sv.empty()) tokens.push_back(sv);
//    return tokens;
//}
//
//static std::pair<double, bool> parse_double(std::string_view sv)
//{
//    double x{};
//    if (std::from_chars(sv.data(), sv.data() + sv.size(), x).ec == std::errc{}) return std::make_pair(x, true);
//    return std::make_pair(x, false);
//}
//
//static vtkVector3<double> parse_3d(std::string const& data)
//{
//    vtkVector3<double> res{};
//    if (auto ns = split(data, "\\"); !ns.empty())
//        for (auto i = 0; i < 3; i++)
//            if (auto [x, ok] = parse_double(ns[i]); !ok)
//                std::cerr << "error to parse double from: " << ns[i] << '\n';
//            else
//                res[i] = x;
//    return res;
//}

static vtkSmartPointer<vtkAnnotatedCubeActor> MakeAnnotatedCubeActor(vtkNamedColors* colors)
{
    // A cube with labeled faces.
    // LPS
    vtkNew<vtkAnnotatedCubeActor> cube;
    cube->SetXPlusFaceText("L");  // Left
    cube->SetXMinusFaceText("R"); // Right
    cube->SetYPlusFaceText("P");  // Posterior
    cube->SetYMinusFaceText("A"); // Anterior
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

static vtkSmartPointer<vtkAxesActor> MakeAxesActor(std::array<double, 3>& scale, std::array<std::string, 3>& xyzLabels)
{
    vtkNew<vtkAxesActor> axes;
    axes->SetScale(scale[0], scale[1], scale[2]);
    axes->SetShaftTypeToCylinder();
    axes->SetXAxisLabelText(xyzLabels[0].c_str());
    axes->SetYAxisLabelText(xyzLabels[1].c_str());
    axes->SetZAxisLabelText(xyzLabels[2].c_str());
    axes->SetCylinderRadius(0.5 * axes->GetCylinderRadius());
    axes->SetConeRadius(1.025 * axes->GetConeRadius());
    axes->SetSphereRadius(1.5 * axes->GetSphereRadius());
    vtkTextProperty* tprop = axes->GetXAxisCaptionActor2D()->GetCaptionTextProperty();
    tprop->ItalicOn();
    tprop->ShadowOn();
    tprop->SetFontFamilyToTimes();
    // Use the same text properties on the other two axes.
    axes->GetYAxisCaptionActor2D()->GetCaptionTextProperty()->ShallowCopy(tprop);
    axes->GetZAxisCaptionActor2D()->GetCaptionTextProperty()->ShallowCopy(tprop);
    return axes;
}

static vtkSmartPointer<vtkPropAssembly> MakeCubeActor(std::array<double, 3>& scale,
                                                      std::array<std::string, 3>& xyzLabels, vtkNamedColors* colors)
{
    // We are combining a vtk.vtkAxesActor and a vtk.vtkAnnotatedCubeActor
    // into a vtk.vtkPropAssembly
    vtkSmartPointer<vtkAnnotatedCubeActor> cube = MakeAnnotatedCubeActor(colors);
    vtkSmartPointer<vtkAxesActor> axes = MakeAxesActor(scale, xyzLabels);

    // Combine orientation markers into one with an assembly.
    vtkNew<vtkPropAssembly> assembly;
    assembly->AddPart(axes);
    assembly->AddPart(cube);
    return assembly;
}

// https://examples.vtk.org/site/Cxx/Annotation/MultiLineText/
vtkSmartPointer<vtkTextActor> CreateLabel(char pos, char const* input)
{
    constexpr int font_size = 18;
    vtkNew<vtkTextActor> txt;
    vtkTextProperty* txtprop = txt->GetTextProperty();
    txtprop->SetFontFamilyToArial();
    txtprop->BoldOn();
    txtprop->SetFontSize(font_size);
    txtprop->SetColor(1, 1, 1);
    txt->SetInput(input);
    txt->GetPositionCoordinate()->SetCoordinateSystemToNormalizedViewport();
    switch (pos)
    {
    case 'L':
        txt->GetPositionCoordinate()->SetValue(0, 0.5);
        txtprop->SetJustificationToLeft();
        txtprop->SetVerticalJustificationToCentered();
        break;
    case 'R':
        txt->GetPositionCoordinate()->SetValue(1, 0.5);
        txtprop->SetJustificationToRight();
        txtprop->SetVerticalJustificationToCentered();
        break;
    case 'U':
        txt->GetPositionCoordinate()->SetValue(0.5, 1);
        txtprop->SetJustificationToCentered();
        txtprop->SetVerticalJustificationToTop();
        break;
    case 'D':
        txt->GetPositionCoordinate()->SetValue(0.5, 0);
        txtprop->SetJustificationToCentered();
        txtprop->SetVerticalJustificationToBottom();
        break;

    default:
        break;
    }
    return txt;
}

// https://examples.vtk.org/site/Cxx/VisualizationAlgorithms/AnatomicalOrientation/
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
    //! Set the ordering of the image rows in memory.
    /*!
     *  If the order is BottomUp (which is the default) then
     *  the images will be flipped when they are read from disk.
     *  The native orientation of DICOM images is top-to-bottom.
     */
    dicom_reader->SetMemoryRowOrderToFileNative();
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
    std::array<double, 3> scale{{1.5, 1.5, 1.5}};
    std::array<std::string, 3> xyzLabels{{"X", "Y", "Z"}};
    auto lps_actor = MakeCubeActor(scale, xyzLabels, colors);
    vtkNew<vtkOrientationMarkerWidget> om_widget;
    om_widget->SetOrientationMarker(lps_actor);
    om_widget->SetViewport(0, 0, 0.2, 0.2); // lower left in the viewport
    om_widget->SetInteractor(interactor);
    om_widget->On();

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
    // TODO: obtain orientation from plane widget slice
    auto text_actor_left = CreateLabel('L', "L"), text_actor_right = CreateLabel('R', "R"),
         text_actor_up = CreateLabel('U', "U"), text_actor_down = CreateLabel('D', "D");
    renderer_plane->AddActor(text_actor_left);
    renderer_plane->AddActor(text_actor_right);
    renderer_plane->AddActor(text_actor_up);
    renderer_plane->AddActor(text_actor_down);

    render_window->Render();
    renderer->ResetCamera();

    interactor->Initialize();
    om_widget->InteractiveOff();
    interactor->Start();

    return 0;
}