#include <vtkSmartPointer.h>
#include <vtkDICOMReader.h>
#include <vtkDICOMSorter.h>
#include <vtkStringArray.h>
#include <vtkImagePlaneWidget.h>
#include <vtkRenderWindowInteractor.h>
#include <vtkImageData.h>
#include <vtkInteractorStyleImage.h>
#include <vtkObjectFactory.h>
#include <vtkImageMapToWindowLevelColors.h>
#include <vtkImageResliceToColors.h>
#include <vtkRenderWindow.h>
#include <vtkRenderer.h>
#include <vtkCellPicker.h>
#include <vtkProperty.h>
#include <vtkTextProperty.h>
#include <vtkCameraOrientationWidget.h>
#include <vtkCameraOrientationRepresentation.h>
#include <vtkImageDataOutlineFilter.h>
#include <vtkPolyDataMapper.h>
#include <vtkMath.h>
#include <vtkTransform.h>
#include <vtkGPUVolumeRayCastMapper.h>
#include <vtkPiecewiseFunction.h>
#include <vtkColorTransferFunction.h>
#include <vtkVolumeProperty.h>
#include <vtkVolume.h>
#include <vtkPlaneSource.h>
#include <vtkPolyData.h>
#include <vtkPoints.h>
#include <vtkPolyLine.h>
#include <vtkCellArray.h>
#include <vtkSphereSource.h>

#include <filesystem>
#include <iostream>

/*
vtkImagePlaneWidget interaction with mouse:
1. mouse wheel + cursor motion -> change widget orientation and position
   ---------
   |c|xxx|c|
   |x|ooo|x|
   |x|ooo|x|
   |x|ooo|x|
   |c|xxx|c|
   ---------
   corner region (c) -> rotate widget with fixed normal orientation
   edge region (x) -> flip widget by rotate normal orientation
   center region (o) -> move widget along normal orientation
2. mouse left button + cursor motion -> pick point on slice
3. mouse right button + cursor motion -> change window level and width
4. shift + mouse wheel -> scale slice (move upwards -> enlarge, downwards-> shrink)
5. ctrl + mouse wheel:
    - when cursor inside the center region (o):
        move slice in any direction 
    - when cursor inside the edge region (x)
        stretch or shrink the edge
    - when cursor inside the corner region (c)
        stretch or shrink the corner
*/

class myPickerCallback: public vtkCommand
{
public:
    static myPickerCallback* New();
    vtkTypeMacro(myPickerCallback, vtkCommand);

    myPickerCallback() = default;
    ~myPickerCallback() = default;

    void Execute(vtkObject* caller, unsigned long, void*) override
    {
        auto* picker = reinterpret_cast<vtkCellPicker*>(caller);
        if (auto* actor = picker->GetActor(); actor != nullptr) // actor is valid when picking on plane
        {
            auto* pos = picker->GetPickPosition(); // global coordinate
            std::cout << pos[0] << ", " << pos[1] << ", " << pos[2] << '\n';
        }
    }
};
vtkStandardNewMacro(myPickerCallback);

// https://examples.vtk.org/site/Cxx/PolyData/DeletePoint/
void ReallyDeletePoint(vtkSmartPointer<vtkPoints> points, vtkIdType id)
{
    vtkNew<vtkPoints> newPoints;

    for (vtkIdType i = 0; i < points->GetNumberOfPoints(); i++)
    {
        if (i != id)
        {
            double p[3];
            points->GetPoint(i, p);
            newPoints->InsertNextPoint(p);
        }
    }

    points->ShallowCopy(newPoints);
}

class myInteractorStyle: public vtkInteractorStyleTrackballCamera
{
public:
    static myInteractorStyle* New();
    vtkTypeMacro(myInteractorStyle, vtkInteractorStyleTrackballCamera);

    myInteractorStyle() = default;
    ~myInteractorStyle() = default;

    void set_plane_widget_and_renderer(vtkImagePlaneWidget* plane_widget, vtkRenderer* renderer)
    {
        m_plane_widget = plane_widget;

        // use customized picker
        if (!m_picker) m_picker = vtkSmartPointer<vtkCellPicker>::New();
        m_picker->SetTolerance(0.005);
        //vtkNew<myPickerCallback> picker_callback;
        //m_picker->AddObserver(vtkCommand::EndPickEvent, picker_callback);
        m_plane_widget->SetPicker(m_picker);

        m_points = vtkSmartPointer<vtkPoints>::New();
        m_data = vtkSmartPointer<vtkPolyData>::New();
        m_line_actor = vtkSmartPointer<vtkActor>::New();
        m_endpoint_actor = vtkSmartPointer<vtkActor>::New();

        m_line_actor->GetProperty()->SetColor(0, 1, 0);
        m_line_actor->GetProperty()->SetLineWidth(3);

        vtkNew<vtkSphereSource> sphere;
        sphere->SetCenter(0.0, 0.0, 0.0);
        sphere->SetRadius(5.0);
        sphere->SetPhiResolution(100);
        sphere->SetThetaResolution(100);

        vtkNew<vtkPolyDataMapper> mapper;
        mapper->SetInputConnection(sphere->GetOutputPort());
        m_endpoint_actor->SetMapper(mapper);
        m_endpoint_actor->GetProperty()->SetColor(1, 0, 0);
        m_endpoint_actor->SetVisibility(false);

        renderer->AddActor(m_line_actor);
        renderer->AddActor(m_endpoint_actor);
    }

    void set_original_orientation_and_pos(char ori = 'x', int slice_no = 0)
    {
        m_original_plane_orientation = ori;
        m_original_plane_pos = slice_no;
    }

    void set_volume(vtkVolume* volume) { m_volume = volume; }

    void OnChar() override
    {
        if (m_picker == nullptr) return;

        // pick point
        if (Interactor->GetKeyCode() == 'p')
        {
            if (auto* actor = m_picker->GetActor();
                actor != nullptr) // plane texture actor is valid when picking on plane
            {
                double pos[3]{};
                m_picker->GetPickPosition(pos); // global coordinate
                if (std::memcmp(pos, m_pos, sizeof(m_pos)) == 0) return;
                memcpy(m_pos, pos, sizeof(m_pos));
                std::cout << pos[0] << ", " << pos[1] << ", " << pos[2] << '\n';
                // draw line
                auto n = m_points->GetNumberOfPoints();
                m_points->InsertNextPoint(m_pos);
            }
        }
        else if (Interactor->GetKeyCode() == 'c') // clear picked points
        {
            m_points = vtkSmartPointer<vtkPoints>::New();
        }
        else if (Interactor->GetKeyCode() == 'r') // reset slice plane orientation and pos
        {
            switch (m_original_plane_orientation)
            {
            case 'x':
                m_plane_widget->SetPlaneOrientationToXAxes();
                break;
            case 'y':
                m_plane_widget->SetPlaneOrientationToYAxes();
                break;
            case 'z':
                m_plane_widget->SetPlaneOrientationToZAxes();
                break;
            default:
                m_plane_widget->SetPlaneOrientationToXAxes();
                break;
            }
            m_plane_widget->SetSliceIndex(m_original_plane_pos);
        }
        else if (Interactor->GetKeyCode() == 'v') // show/hide volume
        {
            m_volume->SetVisibility(m_volume->GetVisibility() == 0 ? 1 : 0);
        }

        // draw endpoint
        if (m_points->GetNumberOfPoints() > 0)
        {
            m_endpoint_actor->SetPosition(m_points->GetPoint(m_points->GetNumberOfPoints() - 1));
            m_endpoint_actor->SetVisibility(true);
        }
        else
            m_endpoint_actor->SetVisibility(false);

        vtkNew<vtkPolyLine> lines;
        lines->GetPointIds()->SetNumberOfIds(m_points->GetNumberOfPoints());
        for (auto i = 0; i < m_points->GetNumberOfPoints(); i++)
            lines->GetPointIds()->SetId(i, i);
        vtkNew<vtkCellArray> cells;
        cells->InsertNextCell(lines);

        m_data->SetPoints(m_points);
        m_data->SetLines(cells);

        // TODO: better way than create new mapper every time
        vtkNew<vtkPolyDataMapper> mapper;
        mapper->SetInputData(m_data);
        m_line_actor->SetMapper(mapper);

        // TODO: project lines on slice plane?
    }

private:
    vtkImagePlaneWidget* m_plane_widget = nullptr;
    vtkSmartPointer<vtkCellPicker> m_picker{};
    vtkSmartPointer<vtkPolyData> m_data{};
    vtkSmartPointer<vtkPoints> m_points{};
    vtkSmartPointer<vtkActor> m_line_actor{};
    vtkSmartPointer<vtkActor> m_endpoint_actor{};

    double m_pos[3]{};

    char m_original_plane_orientation = 'x';
    int m_original_plane_pos = 0;

    vtkVolume* m_volume = nullptr;
};
vtkStandardNewMacro(myInteractorStyle);

int main(int argc, char* argv[])
{
    if (argc != 2)
    {
        std::cerr << "1) dicom dir path" << std::endl;
        return EXIT_FAILURE;
    }

    std::filesystem::path dicom_dir_path{argv[1]};
    vtkNew<vtkDICOMReader> dicom_reader;
    vtkNew<vtkStringArray> dicom_img_paths;
    vtkNew<vtkDICOMSorter> sorter;
    int count = 0;
    for (auto const& it : std::filesystem::directory_iterator(dicom_dir_path))
        dicom_img_paths->InsertValue(count++, it.path().string());
    sorter->SetInputFileNames(dicom_img_paths);
    sorter->Update();
    dicom_reader->SetFileNames(sorter->GetFileNamesForSeries(0));
    dicom_reader->SetDataByteOrderToLittleEndian();
    dicom_reader->Update(0);
    std::cout << dicom_reader->GetOutput()->GetScalarTypeAsString() << std::endl; // short
    std::cout << dicom_reader->GetOutput()->GetDimensions()[0] << ", " << dicom_reader->GetOutput()->GetDimensions()[1]
              << ", " << dicom_reader->GetOutput()->GetDimensions()[2] << std::endl;

    auto x_dim = dicom_reader->GetOutput()->GetDimensions()[0];

    vtkNew<vtkGPUVolumeRayCastMapper> volume_mapper;
    volume_mapper->SetInputConnection(dicom_reader->GetOutputPort());
    //volume_mapper->SetBlendModeToMaximumIntensity();

    vtkNew<vtkPiecewiseFunction> opacity;
    opacity->AddSegment(0, 0, x_dim - 1, 1);

    vtkNew<vtkColorTransferFunction> color;
    color->AddRGBSegment(0, 0.5, 0.1, 0.1, x_dim - 1, 1, 1, 1);

    vtkNew<vtkVolumeProperty> volume_property;
    volume_property->SetScalarOpacity(opacity);
    volume_property->SetColor(color);
    volume_property->SetInterpolationTypeToLinear();
    volume_property->ShadeOn();
    volume_property->SetAmbient(0.4);
    volume_property->SetDiffuse(0.6);
    volume_property->SetSpecular(0.2);

    vtkNew<vtkVolume> volume;
    volume->SetMapper(volume_mapper);
    volume->SetProperty(volume_property);

    vtkNew<vtkRenderer> renderer, renderer_plane;
    vtkNew<vtkRenderWindow> render_window;
    render_window->AddRenderer(renderer);
    render_window->AddRenderer(renderer_plane);
    render_window->SetSize(800, 800);
    render_window->SetWindowName("ImagePlaneWidget");

    renderer->AddVolume(volume);

    renderer->SetViewport(0, 0, 0.5, 1);
    renderer_plane->SetViewport(0.5, 0, 1, 1);

    vtkNew<vtkRenderWindowInteractor> interactor;
    interactor->SetRenderWindow(render_window);
    vtkNew<myInteractorStyle> style; //vtkNew<vtkInteractorStyleTrackballCamera> style;
    interactor->SetInteractorStyle(style);

    style->set_volume(volume);

    vtkNew<vtkCameraOrientationWidget> cam_orient_manipulator;
    cam_orient_manipulator->SetParentRenderer(renderer);
    vtkCameraOrientationRepresentation::SafeDownCast(cam_orient_manipulator->GetRepresentation())->AnchorToLowerRight();

    vtkNew<vtkImageDataOutlineFilter> outline;
    outline->SetInputConnection(dicom_reader->GetOutputPort());
    vtkNew<vtkPolyDataMapper> outline_mapper;
    outline_mapper->SetInputConnection(outline->GetOutputPort());
    vtkNew<vtkActor> outline_actor;
    outline_actor->SetMapper(outline_mapper);
    outline_actor->GetProperty()->SetColor(1, 0, 0);
    renderer->AddActor(outline_actor);

    vtkNew<vtkImagePlaneWidget> plane_widget;
    plane_widget->SetInteractor(interactor);

    style->set_plane_widget_and_renderer(plane_widget, renderer);

    plane_widget->RestrictPlaneToVolumeOn();
    plane_widget->DisplayTextOn();
    plane_widget->SetResliceInterpolateToCubic();
    plane_widget->SetInputConnection(dicom_reader->GetOutputPort());

    plane_widget->GetCursorProperty()->SetColor(0, 1, 0);
    plane_widget->GetMarginProperty()->SetColor(0, 1, 1);
    plane_widget->GetPlaneProperty()->SetColor(0, 0, 1);
    plane_widget->GetSelectedPlaneProperty()->SetColor(1, 0, 0);
    plane_widget->GetTextProperty()->SetColor(1, 1, 1);

    //// change mouse behavior
    //plane_widget->SetLeftButtonAction(vtkImagePlaneWidget ::VTK_SLICE_MOTION_ACTION);
    //plane_widget->SetLeftButtonAutoModifier(vtkImagePlaneWidget::VTK_CONTROL_MODIFIER);

    plane_widget->SetPlaneOrientationToXAxes();
    plane_widget->SetSliceIndex(x_dim / 2);
    style->set_original_orientation_and_pos('x', x_dim / 2);

    //vtkNew<vtkTransform> transform;
    //// translate plane center
    //double pos[3]{dicom_reader->GetOutput()->GetDimensions()[0] / 2, dicom_reader->GetOutput()->GetDimensions()[1] / 2,
    //              dicom_reader->GetOutput()->GetDimensions()[2] / 2};
    //double center[3]{};
    //plane_widget->GetCenter(center);
    //transform->Translate(pos[0] - center[0], pos[1] - center[1], pos[2] - center[2]);

    //// rotate plane normal
    //double normal[3]{};
    //plane_widget->GetNormal(normal);
    //double dir[3]{1, 1, 0};
    //double axis[3]{};
    //double angle = vtkMath::AngleBetweenVectors(normal, dir) / vtkMath::Pi() * 180;
    //vtkMath::Cross(normal, dir, axis);
    //transform->RotateWXYZ(angle, axis);
    //double pts[3]{};
    //transform->TransformPoint(plane_widget->GetPoint1(), pts);
    //plane_widget->SetPoint1(pts);
    //transform->TransformPoint(plane_widget->GetPoint2(), pts);
    //plane_widget->SetPoint2(pts);
    //transform->TransformPoint(plane_widget->GetOrigin(), pts);
    //plane_widget->SetOrigin(pts);
    //plane_widget->UpdatePlacement();

    vtkNew<vtkPlaneSource> plane;
    vtkNew<vtkPolyDataMapper> plane_mapper;
    vtkNew<vtkActor> plane_actor;
    plane_mapper->SetInputConnection(plane->GetOutputPort());
    plane_actor->SetMapper(plane_mapper);
    plane_actor->SetTexture(plane_widget->GetTexture());
    renderer_plane->AddActor(plane_actor);

    interactor->Initialize();
    cam_orient_manipulator->On();
    plane_widget->On();
    render_window->Render();
    interactor->Start();

    return 0;
}