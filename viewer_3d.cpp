#include <vtkNew.h>
#include <vtkOBJReader.h>
#include <vtkPLYReader.h>
#include <vtkXMLPolyDataReader.h>
#include <vtkSTLReader.h>
#include <vtkPolyDataReader.h>
#include <vtkBYUReader.h>
#include <vtkSimplePointsReader.h>
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
#include <vtkCameraOrientationWidget.h>
#include <vtkCameraOrientationRepresentation.h>

#include <iostream>
#include <sstream>
#include <fstream>
#include <string>

#define WITH_PATH

vtkSmartPointer<vtkPolyData> ReadPolyData(const char* fileName)
{
    vtkSmartPointer<vtkPolyData> polyData;
    std::string extension = vtksys::SystemTools::GetFilenameLastExtension(std::string(fileName));

    // Drop the case of the extension
    std::transform(extension.begin(), extension.end(), extension.begin(), ::tolower);

    if (extension == ".ply")
    {
        vtkNew<vtkPLYReader> reader;
        reader->SetFileName(fileName);
        reader->Update();
        polyData = reader->GetOutput();
    }
    else if (extension == ".vtp")
    {
        vtkNew<vtkXMLPolyDataReader> reader;
        reader->SetFileName(fileName);
        reader->Update();
        polyData = reader->GetOutput();
    }
    else if (extension == ".obj")
    {
        vtkNew<vtkOBJReader> reader;
        reader->SetFileName(fileName);
        reader->Update();
        polyData = reader->GetOutput();
    }
    else if (extension == ".stl")
    {
        vtkNew<vtkSTLReader> reader;
        reader->SetFileName(fileName);
        reader->Update();
        polyData = reader->GetOutput();
    }
    else if (extension == ".vtk")
    {
        vtkNew<vtkPolyDataReader> reader;
        reader->SetFileName(fileName);
        reader->Update();
        polyData = reader->GetOutput();
    }
    else if (extension == ".g")
    {
        vtkNew<vtkBYUReader> reader;
        reader->SetGeometryFileName(fileName);
        reader->Update();
        polyData = reader->GetOutput();
    }
    else if (extension == ".xyz")
    {
        vtkNew<vtkSimplePointsReader> reader;
        reader->SetFileName(fileName);
        reader->Update();
        polyData = reader->GetOutput();
    }
    else
    {
        std::cerr << "Unsupported 3D Format, use default Sphere Source" << std::endl;
        vtkNew<vtkSphereSource> source;
        source->Update();
        polyData = source->GetOutput();
    }
    return polyData;
}

class myCameraMotionInteractorStyle: public vtkInteractorStyleTrackballCamera
{
public:
    static myCameraMotionInteractorStyle* New();
    vtkTypeMacro(myCameraMotionInteractorStyle, vtkInteractorStyleTrackballCamera);

    myCameraMotionInteractorStyle() = default;
    ~myCameraMotionInteractorStyle() = default;

    void setPathPoints(vtkPolyData* path_data)
    {
        m_path_data = path_data;
        auto* renderer = Interactor->GetRenderWindow()->GetRenderers()->GetFirstRenderer();
        auto* camera = renderer->GetActiveCamera();
        camera->SetPosition(m_path_data->GetPoint(0));
        camera->SetFocalPoint(m_path_data->GetPoint(1));
        renderer->ResetCameraClippingRange();
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
        auto* renderer = Interactor->GetRenderWindow()->GetRenderers()->GetFirstRenderer();
        auto* camera = renderer->GetActiveCamera();
        camera->SetPosition(m_path_data->GetPoint(n - 1));
        camera->SetFocalPoint(m_path_data->GetPoint(n + 1));
        Interactor->Render();
    }

private:
    vtkPolyData* m_path_data = nullptr;
    int m_current_camera_pos_index = 0;
};
vtkStandardNewMacro(myCameraMotionInteractorStyle);

int main(int argc, char* argv[])
{
#ifdef WITH_PATH
    if (argc != 3)
    {
        std::cerr << "1) obj file path; 2) path points file" << std::endl;
        return 1;
    }
#else
    if (argc != 2)
    {
        std::cerr << "1) obj file path" << std::endl;
        return 1;
    }
#endif

    auto model_data = ReadPolyData(argv[1]);

#ifdef WITH_PATH
    vtkNew<vtkPoints> m_path_points;
    std::ifstream fs(argv[2]);
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
#endif

    vtkNew<vtkNamedColors> colors;
    auto backgound_color = colors->GetColor3d("Black");

    // model
    vtkNew<vtkPolyDataMapper> mapper;
    mapper->SetInputData(model_data);
    vtkNew<vtkActor> m_obj_actor;
    m_obj_actor->SetMapper(mapper);
    m_obj_actor->GetProperty()->SetAmbientColor(0.97, 0.5, 0.5);
    m_obj_actor->GetProperty()->SetDiffuseColor(0.97, 0.67, 0.67);
    m_obj_actor->GetProperty()->SetDiffuse(0.5);
    m_obj_actor->GetProperty()->SetAmbient(0.8);
    m_obj_actor->GetProperty()->SetSpecular(0.1);
    m_obj_actor->GetProperty()->SetOpacity(1.0);

#ifdef WITH_PATH
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
#endif

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

    vtkNew<vtkRenderer> m_renderer;
    m_renderer->AddActor(m_obj_actor);
#ifdef WITH_PATH
    m_renderer->AddActor(path_actor);
    m_renderer->AddActor(endpoints_actor);
#endif
    m_renderer->AddViewProp(corner_overlay);
    m_renderer->AddObserver(vtkCommand::EndEvent, fps_callback);
    m_renderer->SetBackground(backgound_color.GetData());
    m_renderer->ResetCamera();
    m_renderer->GetActiveCamera()->Azimuth(0);
    m_renderer->GetActiveCamera()->Elevation(-85);
    m_renderer->GetActiveCamera()->Dolly(1.2);
    m_renderer->ResetCameraClippingRange();

    vtkNew<vtkRenderWindowInteractor> interactor;
#ifdef WITH_PATH
    vtkNew<myCameraMotionInteractorStyle> style;
#else
    vtkNew<vtkInteractorStyleTrackballCamera> style;
#endif
    interactor->SetInteractorStyle(style);

    vtkNew<vtkRenderWindow> m_render_window;
    m_render_window->SetSize(500, 500);
    m_render_window->AddRenderer(m_renderer);
    m_render_window->SetInteractor(interactor);

    vtkNew<vtkCameraOrientationWidget> cam_orient_manipulator;
    cam_orient_manipulator->SetParentRenderer(m_renderer);
    vtkCameraOrientationRepresentation::SafeDownCast(cam_orient_manipulator->GetRepresentation())->AnchorToLowerLeft();
    cam_orient_manipulator->On();

    interactor->Initialize();
#ifdef WITH_PATH
    style->setPathPoints(path_data);
#endif

    m_render_window->Render();
    cam_orient_manipulator->SquareResize();  // need call here to change the rep anchor position
    interactor->Start();

    return 0;
}