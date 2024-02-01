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
#include <vtkImageData.h>
#include <vtkInteractorStyleImage.h>
#include <vtkImageActor.h>

#include <iostream>
#include <sstream>
#include <fstream>
#include <string>
#include <filesystem>
#include <array>

//#define M_DEBUG

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

        auto* dims = m_image_viewers[0]->GetInput()->GetDimensions();

        // YZ
        m_crossline_sources[0] = vtkSmartPointer<vtkLineSource>::New();
        m_crossline_sources[0]->SetPoint1(0, -dims[1], 0);
        m_crossline_sources[0]->SetPoint2(0, dims[1], 0);
        m_crossline_sources[0]->Update();
        m_crossline_sources[1] = vtkSmartPointer<vtkLineSource>::New();
        m_crossline_sources[1]->SetPoint1(0, 0, -dims[2]);
        m_crossline_sources[1]->SetPoint2(0, 0, dims[2]);
        m_crossline_sources[1]->Update();

        // XZ
        m_crossline_sources[2] = vtkSmartPointer<vtkLineSource>::New();
        m_crossline_sources[2]->SetPoint1(-dims[0], 0, 0);
        m_crossline_sources[2]->SetPoint2(dims[0], 0, 0);
        m_crossline_sources[2]->Update();
        m_crossline_sources[3] = vtkSmartPointer<vtkLineSource>::New();
        m_crossline_sources[3]->SetPoint1(0, 0, -dims[2]);
        m_crossline_sources[3]->SetPoint2(0, 0, dims[2]);
        m_crossline_sources[3]->Update();

        // XY
        m_crossline_sources[4] = vtkSmartPointer<vtkLineSource>::New();
        m_crossline_sources[4]->SetPoint1(-dims[0], 0, 0);
        m_crossline_sources[4]->SetPoint2(dims[0], 0, 0);
        m_crossline_sources[4]->Update();
        m_crossline_sources[5] = vtkSmartPointer<vtkLineSource>::New();
        m_crossline_sources[5]->SetPoint1(0, -dims[1], 0);
        m_crossline_sources[5]->SetPoint2(0, dims[1], 0);
        m_crossline_sources[5]->Update();

        double colors[3][3] = {{1, 0, 0}, {0, 1, 0}, {0, 0, 1}};
        for (auto i = 0; i < 3; i++)
        {
            for (auto j = 0; j < 2; j++)
            {
                vtkNew<vtkPolyDataMapper> mapper;
                mapper->SetInputConnection(m_crossline_sources[i * 2 + j]->GetOutputPort());
                vtkNew<vtkActor> actor;
                actor->SetMapper(mapper);
                actor->GetProperty()->SetLineWidth(2);
                actor->GetProperty()->SetColor(colors[j]);
                m_image_viewers[i]->GetRenderer()->AddActor(actor);
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
            m_image_viewers[i]->SetSlice(slice_no[i]);
            draw_cross_line(tgt_pos, i);
        }

        Interactor->Render();
    }

    [[nodiscard]] std::array<int, 3> point_to_slice(double* point) const
    {
        auto* spacing = m_image_viewers[0]->GetInput()->GetSpacing();
        return {static_cast<int>(point[0] / spacing[0]), static_cast<int>(point[1] / spacing[1]),
                static_cast<int>(point[2] / spacing[2])};
    }

    void draw_cross_line(double* point, int viewer_idx)
    {
#ifdef M_DEBUG
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
#endif

        auto* sz = m_image_viewers[viewer_idx]->GetSize();

        if (viewer_idx == 0)
        {
            m_crossline_sources[0]->SetPoint1(point[0], -sz[0] * 2, point[2]);
            m_crossline_sources[0]->SetPoint2(point[0], sz[0] * 2, point[2]);
            m_crossline_sources[0]->Update();
            m_crossline_sources[1]->SetPoint1(point[0], point[1], -sz[1] * 2);
            m_crossline_sources[1]->SetPoint2(point[0], point[1], sz[1] * 2);
            m_crossline_sources[1]->Update();
        }
        else if (viewer_idx == 1)
        {
            m_crossline_sources[2]->SetPoint1(-sz[0] * 2, point[1], point[2]);
            m_crossline_sources[2]->SetPoint2(sz[0] * 2, point[1], point[2]);
            m_crossline_sources[2]->Update();
            m_crossline_sources[3]->SetPoint1(point[0], point[1], -sz[1] * 2);
            m_crossline_sources[3]->SetPoint2(point[0], point[1], sz[1] * 2);
            m_crossline_sources[3]->Update();
        }
        else if (viewer_idx == 2)
        {
            m_crossline_sources[4]->SetPoint1(-sz[0] * 2, point[1], point[2] + 1);
            m_crossline_sources[4]->SetPoint2(sz[0] * 2, point[1], point[2] + 1);
            m_crossline_sources[4]->Update();
            m_crossline_sources[5]->SetPoint1(point[0], -sz[1] * 2, point[2] + 1);
            m_crossline_sources[5]->SetPoint2(point[0], sz[1] * 2, point[2] + 1);
            m_crossline_sources[5]->Update();
        }
        m_image_viewers[viewer_idx]->GetRenderer()->ResetCameraClippingRange();
        m_image_viewers[viewer_idx]->Render();
    }

private:
    vtkRenderer* m_renderer = nullptr;
    vtkPolyData* m_path_data = nullptr;
    int m_current_camera_pos_index = 0;

    vtkImageViewer2* m_image_viewers[3]{};

#ifdef M_DEBUG
    vtkSmartPointer<vtkSphereSource> m_sphere = {};
#endif
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
        if (auto* camera = render->GetActiveCamera(); camera) camera->Roll(180);
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

#ifdef M_DEBUG
    vtkNew<vtkInteractorStyleTrackballCamera> st;
    vtkNew<vtkRenderWindowInteractor> it;
    it->SetInteractorStyle(st);
    sagittal_viewer->GetRenderWindow()->SetInteractor(it);
#endif

    style->setImageViewers(sagittal_viewer, coronal_viewer, axial_viewer);

    vtkNew<vtkRenderWindow> m_render_window;
    m_render_window->SetSize(500, 500);
    m_render_window->AddRenderer(obj_renderer);
    m_render_window->SetInteractor(obj_interactor);
    m_render_window->Render();

    obj_interactor->Start();

    return 0;
}