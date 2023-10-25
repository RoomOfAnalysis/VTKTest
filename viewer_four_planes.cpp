#include <vtkNew.h>
#include <vtkOBJReader.h>
#include <vtkPolyDataMapper.h>
#include <vtkNamedColors.h>
#include <vtkActor.h>
#include <vtkProperty.h>
#include <vtkCornerAnnotation.h>
#include <vtkTextProperty.h>
#include <vtkCallbackCommand.h>
#include <vtkRenderer.h>
#include <vtkCamera.h>
#include <vtkRenderWindow.h>
#include <vtkRenderWindowInteractor.h>
#include <vtkInteractorStyleTrackballCamera.h>
#include <vtkPoints.h>
#include <vtkPolyLine.h>
#include <vtkSphereSource.h>
#include <vtkGlyph3D.h>
#include <vtkRendererCollection.h>
#include <vtkDICOMReader.h>
#include <vtkStringArray.h>
#include <vtkDICOMSorter.h>
#include <vtkImageViewer2.h>
#include <vtkImageMapToWindowLevelColors.h>
#include <vtkLineSource.h>
#include <vtkPointData.h>

#include <iostream>
#include <sstream>
#include <fstream>
#include <string>
#include <filesystem>
#include <array>

class myCameraMotionInteractorStyle: public vtkInteractorStyleTrackballCamera
{
public:
    static myCameraMotionInteractorStyle* New();
    vtkTypeMacro(myCameraMotionInteractorStyle, vtkInteractorStyleTrackballCamera);

    myCameraMotionInteractorStyle() = default;
    ~myCameraMotionInteractorStyle() = default;

    void setRenderer(vtkRenderer* renderer) { m_renderer = renderer; }

    void setPathPoints(vtkPolyData* path_data)
    {
        m_path_data = path_data;
        auto* camera = m_renderer->GetActiveCamera();
        camera->SetPosition(m_path_data->GetPoint(0));
        camera->SetFocalPoint(m_path_data->GetPoint(1));
        m_renderer->ResetCameraClippingRange();
    }

    void setImageViewers(vtkImageViewer2* sagittal_viewer, vtkImageViewer2* coronal_viewer,
                         vtkImageViewer2* axial_viewer)
    {
        m_image_viewers[0] = sagittal_viewer;
        m_image_viewers[1] = coronal_viewer;
        m_image_viewers[2] = axial_viewer;

        for (auto i = 0; i < 3; i++)
        {
            m_slice_rng[i * 2] = m_image_viewers[i]->GetSliceMin();
            m_slice_rng[i * 2 + 1] = m_image_viewers[i]->GetSliceMax();

            m_crossline_renderers[i] = vtkSmartPointer<vtkRenderer>::New();
            m_crossline_renderers[i]->SetLayer(1);
            m_crossline_renderers[i]->InteractiveOff();
            m_image_viewers[i]->GetRenderer()->SetLayer(0);
            auto* window = m_image_viewers[i]->GetRenderWindow();
            window->SetNumberOfLayers(2);
            window->AddRenderer(m_crossline_renderers[i]);
        }
    }

    void setXYZRng(double xmin, double xmax, double ymin, double ymax, double zmin, double zmax)
    {
        m_xyz_rng[0] = xmin;
        m_xyz_rng[1] = xmax;
        m_xyz_rng[2] = ymin;
        m_xyz_rng[3] = ymax;
        m_xyz_rng[4] = zmin;
        m_xyz_rng[5] = zmax;

        for (auto i = 0; i < 3; i++)
        {
            auto* sz = m_image_viewers[i]->GetSize();
            m_crossline_sources[i * 2] = vtkSmartPointer<vtkLineSource>::New();
            m_crossline_sources[i * 2 + 1] = vtkSmartPointer<vtkLineSource>::New();
            m_crossline_sources[i * 2]->SetPoint1(-sz[0], 0, 0.01);
            m_crossline_sources[i * 2]->SetPoint2(sz[0], 0, 0.01);
            m_crossline_sources[i * 2]->Update();
            m_crossline_sources[i * 2 + 1]->SetPoint1(0, -sz[1], 0.01);
            m_crossline_sources[i * 2 + 1]->SetPoint2(0, sz[1], 0.01);
            m_crossline_sources[i * 2 + 1]->Update();

            for (auto j = 0; j < 2; j++)
            {
                vtkNew<vtkPolyDataMapper> mapper;
                mapper->SetInputConnection(m_crossline_sources[i * 2 + j]->GetOutputPort());
                vtkNew<vtkActor> actor;
                actor->SetMapper(mapper);
                actor->SetScale(10);
                if (j == 0)
                    actor->GetProperty()->SetColor(1, 0, 0);
                else
                    actor->GetProperty()->SetColor(0, 1, 0);
                m_crossline_renderers[i]->AddActor(actor);
            }
        }
    }

protected:
    void OnMouseWheelForward() override
    {
        if (m_current_camera_pos_index < m_path_data->GetNumberOfPoints() - 1)
            moveCameraToNthPos(++m_current_camera_pos_index);
    }
    void OnMouseWheelBackward() override
    {
        if (m_current_camera_pos_index > 1) moveCameraToNthPos(--m_current_camera_pos_index);
    }

private:
    void moveCameraToNthPos(int n)
    {
        auto* camera = m_renderer->GetActiveCamera();
        camera->SetPosition(m_path_data->GetPoint(n - 1));
        camera->SetFocalPoint(m_path_data->GetPoint(n + 1));

        auto* tgt_pos = m_path_data->GetPoint(n);
        auto slice_no = point_to_slice(tgt_pos);
        for (auto i = 0; i < 3; i++)
        {
            draw_cross_line(slice_no, i);
            m_image_viewers[i]->SetSlice(slice_no[i]);
            m_image_viewers[i]->Render();
        }

        Interactor->Render();
    }

    [[nodiscard]] std::array<int, 3> point_to_slice(double* point) const
    {
        return {static_cast<int>((point[0] - m_xyz_rng[0]) / (m_xyz_rng[1] - m_xyz_rng[0]) *
                                 (m_slice_rng[1] - m_slice_rng[0])) +
                    m_slice_rng[0],
                static_cast<int>((point[1] - m_xyz_rng[2]) / (m_xyz_rng[3] - m_xyz_rng[2]) *
                                 (m_slice_rng[3] - m_slice_rng[2])) +
                    m_slice_rng[2],
                static_cast<int>((point[2] - m_xyz_rng[4]) / (m_xyz_rng[5] - m_xyz_rng[4]) *
                                 (m_slice_rng[5] - m_slice_rng[4])) +
                    m_slice_rng[4]};
    }

    void draw_cross_line(std::array<int, 3> const& point, int viewer_idx)
    {
        if (!m_sphere)
        {
            vtkNew<vtkPolyDataMapper> mapper1;
            m_sphere = vtkSmartPointer<vtkSphereSource>::New();
            m_sphere->SetRadius(100);
            mapper1->SetInputConnection(m_sphere->GetOutputPort());
            for (auto i = 0; i < 3; i++)
            {
                vtkNew<vtkActor> actor1;
                actor1->SetMapper(mapper1);
                actor1->GetProperty()->SetLineWidth(10);
                actor1->GetProperty()->SetColor(0, 1, 0);
                m_image_viewers[i]->GetRenderer()->AddActor(actor1);
            }
        }

        m_sphere->SetCenter(point[0], point[1], point[2]);
        m_sphere->Update();

        for (auto i = 0; i < 3; i++)
        {
            int x = 0, y = 0;
            switch (i)
            {
            case 0: // YZ
                x = point[1];
                y = point[2];
                break;
            case 1: // XZ
                x = point[0];
                y = point[2];
                break;
            case 2: // XY
                x = point[0];
                y = point[1];
                break;
            default:
                break;
            }
            auto* sz = m_image_viewers[i]->GetSize();
            m_crossline_sources[i * 2]->SetPoint1(-sz[0], y, 0.01);
            m_crossline_sources[i * 2]->SetPoint2(sz[0], y, 0.01);
            m_crossline_sources[i * 2]->Update();
            m_crossline_sources[i * 2 + 1]->SetPoint1(x, -sz[1], 0.01);
            m_crossline_sources[i * 2 + 1]->SetPoint2(x, sz[1], 0.01);
            m_crossline_sources[i * 2 + 1]->Update();
        }
    }

private:
    vtkRenderer* m_renderer = nullptr;
    vtkPolyData* m_path_data = nullptr;
    int m_current_camera_pos_index = 0;

    double m_xyz_rng[6]{};
    vtkImageViewer2* m_image_viewers[3]{};
    int m_slice_rng[6]{};

    vtkSmartPointer<vtkSphereSource> m_sphere = {};

    vtkSmartPointer<vtkRenderer> m_crossline_renderers[3] = {};
    vtkSmartPointer<vtkLineSource> m_crossline_sources[6] = {};
};
vtkStandardNewMacro(myCameraMotionInteractorStyle);

int main(int argc, char* argv[])
{
    if (argc != 4)
    {
        std::cerr << "1) dimcom series folder; 2) obj file path; 3) path points file" << std::endl;
        return 1;
    }

    std::filesystem::path dicom_dir_path{argv[1]};
    vtkNew<vtkDICOMReader> dicom_reader;
    vtkNew<vtkStringArray> dicom_img_paths;
    vtkNew<vtkDICOMSorter> dicom_sorter;
    int count = 0;
    for (auto const& it : std::filesystem::directory_iterator(dicom_dir_path))
        dicom_img_paths->InsertValue(count++, it.path().string());
    dicom_sorter->SetInputFileNames(dicom_img_paths);
    dicom_sorter->Update();
    dicom_reader->SetFileNames(dicom_sorter->GetFileNamesForSeries(0));
    dicom_reader->SetDataByteOrderToLittleEndian();
    dicom_reader->Update(0);

    vtkNew<vtkOBJReader> obj_reader;
    obj_reader->SetFileName(argv[2]);
    obj_reader->Update();

    vtkNew<vtkPoints> m_path_points;
    std::ifstream fs(argv[3]);
    std::string line;
    while (std::getline(fs, line))
    {
        double x, y, z;
        std::stringstream ss;
        ss << line;
        ss >> x >> y >> z;
        m_path_points->InsertNextPoint(x, y, z);
    }
    fs.close();

    vtkNew<vtkNamedColors> colors;
    auto backgound_color = colors->GetColor3d("Black");

    // model
    vtkNew<vtkPolyDataMapper> mapper;
    mapper->SetInputConnection(obj_reader->GetOutputPort());
    vtkNew<vtkActor> m_obj_actor;
    m_obj_actor->SetMapper(mapper);
    m_obj_actor->GetProperty()->SetAmbientColor(0.97, 0.5, 0.5);
    m_obj_actor->GetProperty()->SetDiffuseColor(0.97, 0.67, 0.67);
    m_obj_actor->GetProperty()->SetDiffuse(0.5);
    m_obj_actor->GetProperty()->SetAmbient(0.8);
    m_obj_actor->GetProperty()->SetSpecular(0.1);
    m_obj_actor->GetProperty()->SetOpacity(1.0);

    // path
    vtkNew<vtkPolyData> path_data;
    path_data->SetPoints(m_path_points);
    vtkNew<vtkPolyLine> path_lines;
    path_lines->GetPointIds()->SetNumberOfIds(m_path_points->GetNumberOfPoints());
    for (auto i = 0; i < m_path_points->GetNumberOfPoints(); i++)
        path_lines->GetPointIds()->SetId(i, i);
    vtkNew<vtkCellArray> path_cells;
    path_cells->InsertNextCell(path_lines);
    path_data->SetLines(path_cells);
    vtkNew<vtkPolyDataMapper> path_mapper;
    path_mapper->SetInputData(path_data);
    vtkNew<vtkActor> path_actor;
    path_actor->SetMapper(path_mapper);
    path_actor->GetProperty()->SetColor(colors->GetColor3d("Blue").GetData());
    path_actor->GetProperty()->SetLineWidth(5);

    // endpoints
    vtkNew<vtkPoints> endpoints;
    endpoints->InsertNextPoint(m_path_points->GetPoint(0));
    endpoints->InsertNextPoint(m_path_points->GetPoint(m_path_points->GetNumberOfPoints() - 1));
    vtkNew<vtkPolyData> endpoints_data;
    endpoints_data->SetPoints(endpoints);
    vtkNew<vtkSphereSource> sphere;
    sphere->SetRadius(2);
    sphere->SetPhiResolution(100);
    sphere->SetThetaResolution(100);
    vtkNew<vtkGlyph3D> glyph;
    glyph->SetSourceConnection(sphere->GetOutputPort());
    glyph->SetInputData(endpoints_data);
    glyph->Update();
    vtkNew<vtkPolyDataMapper> endpoints_mapper;
    endpoints_mapper->SetInputConnection(glyph->GetOutputPort());
    endpoints_mapper->ScalarVisibilityOff(); // use color from actor
    vtkNew<vtkActor> endpoints_actor;
    endpoints_actor->GetProperty()->SetColor(colors->GetColor3d("Red").GetData());
    endpoints_actor->SetMapper(endpoints_mapper);

    // fps
    vtkNew<vtkCornerAnnotation> corner_overlay;
    corner_overlay->GetTextProperty()->SetColor(1.0, 0.72, 0.0);
    corner_overlay->SetText(vtkCornerAnnotation::UpperRight, "FPS: 0");
    vtkNew<vtkCallbackCommand> fps_callback;
    fps_callback->SetCallback(
        [](vtkObject* caller, long unsigned int vtkNotUsed(eventId), void* clientData, void* vtkNotUsed(callData)) {
            auto* renderer = static_cast<vtkRenderer*>(caller);
            auto* corner_overlay = reinterpret_cast<vtkCornerAnnotation*>(clientData);
            double timeInSeconds = renderer->GetLastRenderTimeInSeconds();
            double fps = 1.0 / timeInSeconds;
            std::ostringstream out;
            out << "FPS: ";
            out.precision(2);
            out << fps;
            corner_overlay->SetText(vtkCornerAnnotation::UpperRight, out.str().c_str());
        });
    fps_callback->SetClientData(corner_overlay.Get());

    // obj renderer
    vtkNew<vtkRenderer> obj_renderer;
    double obj_viewport[4] = {0, 0, 0.5, 0.5};
    obj_renderer->SetViewPoint(obj_viewport);
    obj_renderer->AddActor(m_obj_actor);
    obj_renderer->AddActor(path_actor);
    obj_renderer->AddActor(endpoints_actor);
    obj_renderer->AddViewProp(corner_overlay);
    obj_renderer->AddObserver(vtkCommand::EndEvent, fps_callback);
    obj_renderer->SetBackground(backgound_color.GetData());
    obj_renderer->ResetCamera();
    obj_renderer->GetActiveCamera()->Azimuth(0);
    obj_renderer->GetActiveCamera()->Elevation(-85);
    obj_renderer->GetActiveCamera()->Dolly(1.2);
    obj_renderer->ResetCameraClippingRange();

    vtkNew<vtkRenderWindowInteractor> obj_interactor;
    vtkNew<myCameraMotionInteractorStyle> style;
    style->setRenderer(obj_renderer);
    style->setPathPoints(path_data);
    obj_interactor->SetInteractorStyle(style);

    // sagittal viewer
    vtkNew<vtkImageViewer2> sagittal_viewer;
    sagittal_viewer->SetInputConnection(dicom_reader->GetOutputPort());
    sagittal_viewer->GetWindowLevel()->SetWindow(500);
    sagittal_viewer->SetSliceOrientationToYZ();
    // correct orientation
    if (auto* render = sagittal_viewer->GetRenderer(); render)
    {
        if (auto* camera = render->GetActiveCamera(); camera)
            camera->Roll(180);
    }
    sagittal_viewer->Render();

    // coronal viewer
    vtkNew<vtkImageViewer2> coronal_viewer;
    coronal_viewer->SetInputConnection(dicom_reader->GetOutputPort());
    coronal_viewer->GetWindowLevel()->SetWindow(500);
    coronal_viewer->SetSliceOrientationToXZ();
    // correct orientation
    if (auto* render = coronal_viewer->GetRenderer(); render)
    {
        if (auto* camera = render->GetActiveCamera(); camera)
        {
            auto* camera_pos = camera->GetPosition();
            camera->SetPosition(camera_pos[0], -camera_pos[1], camera_pos[2]);
            double scale = camera->GetParallelScale();
            render->ResetCamera();
            camera->SetParallelScale(scale);
            camera->Roll(180);
        }
    }
    coronal_viewer->Render();

    // axial viewer
    vtkNew<vtkImageViewer2> axial_viewer;
    axial_viewer->SetInputConnection(dicom_reader->GetOutputPort());
    axial_viewer->GetWindowLevel()->SetWindow(500);
    axial_viewer->SetSliceOrientationToXY();
    axial_viewer->Render();

    style->setImageViewers(sagittal_viewer, coronal_viewer, axial_viewer);
    auto* bounds = mapper->GetBounds();
    style->setXYZRng(bounds[0], bounds[1], bounds[2], bounds[3], bounds[4], bounds[5]);

    vtkNew<vtkRenderWindow> m_render_window;
    m_render_window->SetSize(500, 500);
    m_render_window->AddRenderer(obj_renderer);
    m_render_window->SetInteractor(obj_interactor);
    m_render_window->Render();

    obj_interactor->Start();

    return 0;
}