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
#include <vtkSphereSource.h>
#include <vtkRendererCollection.h>
#include <vtkCameraOrientationWidget.h>
#include <vtkCameraOrientationRepresentation.h>
#include <vtkTransform.h>

#include <iostream>
#include <sstream>
#include <fstream>
#include <string>

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

class mInteractorStyle: public vtkInteractorStyleTrackballCamera
{
public:
    static mInteractorStyle* New();
    vtkTypeMacro(mInteractorStyle, vtkInteractorStyleTrackballCamera);

    void set_actor(vtkActor* actor) { m_obj_actor = actor; }

    mInteractorStyle() = default;
    ~mInteractorStyle() = default;

protected:
    void OnChar() override
    {
        auto* rwi = GetInteractor();
        auto key = rwi->GetKeyCode();
        if (key == 'a' || key == 'e' || key == 'r')
        {
            auto* renderer = rwi->GetRenderWindow()->GetRenderers()->GetFirstRenderer();
            auto* camera = renderer->GetActiveCamera();

            if (key == 'r')
                camera->Roll(10); // Roll
            else
            {
                auto* camera_position = camera->GetPosition();
                auto* camera_focal_point = camera->GetFocalPoint();
                auto* camera_view_up = camera->GetViewUp();
                auto* camera_transform_matrix = camera->GetViewTransformMatrix();

                double e_axis[3];
                e_axis[0] = -camera_transform_matrix->GetElement(0, 0);
                e_axis[1] = -camera_transform_matrix->GetElement(0, 1);
                e_axis[2] = -camera_transform_matrix->GetElement(0, 2);

                auto* center = m_obj_actor->GetCenter();

                // https://stackoverflow.com/questions/43046798/how-to-rotate-a-vtk-scene-around-a-specific-point
                auto transform = vtkSmartPointer<vtkTransform>::New();
                transform->Identity();
                transform->Translate(center[0], center[1], center[2]);
                if (key == 'a')
                    transform->RotateWXYZ(10, camera_view_up); // Azimuth (L-R)
                else
                    transform->RotateWXYZ(10, e_axis); // Elevation (U-D)
                transform->Translate(-center[0], -center[1], -center[2]);

                double new_position[3];
                transform->TransformPoint(camera_position, new_position); // Transform Position
                double new_focal_point[3];
                transform->TransformPoint(camera_focal_point, new_focal_point); // Transform Focal Point

                camera->SetPosition(new_position);
                camera->SetFocalPoint(new_focal_point);
            }

            // Orhthogonalize View Up
            camera->OrthogonalizeViewUp();
            renderer->ResetCameraClippingRange();

            rwi->Render();
        }
    }

    //// wrong, due to camera viewup and focal point
    //void OnChar() override
    //{
    //    auto* rwi = GetInteractor();
    //    auto key = rwi->GetKeyCode();
    //    if (key == 'a' || key == 'e' || key == 'r')
    //    {
    //        auto* renderer = rwi->GetRenderWindow()->GetRenderers()->GetFirstRenderer();
    //        auto* camera = renderer->GetActiveCamera();
    //        switch (key)
    //        {
    //        case 'a':
    //            camera->Azimuth(10);
    //            break;
    //        case 'e':
    //            camera->Elevation(10);
    //            break;
    //        case 'r':
    //            camera->Roll(10);
    //            break;
    //        default:
    //            break;
    //        }
    //    }
    //    rwi->Render();
    //}

private:
    vtkActor* m_obj_actor = nullptr;
};
vtkStandardNewMacro(mInteractorStyle);

int main(int argc, char* argv[])
{
    if (argc != 2)
    {
        std::cerr << "1) obj file path" << std::endl;
        return 1;
    }

    auto model_data = ReadPolyData(argv[1]);

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
    m_renderer->AddViewProp(corner_overlay);
    m_renderer->AddObserver(vtkCommand::EndEvent, fps_callback);
    m_renderer->SetBackground(backgound_color.GetData());
    m_renderer->ResetCamera();
    m_renderer->GetActiveCamera()->Azimuth(0);
    m_renderer->GetActiveCamera()->Elevation(-85);
    m_renderer->GetActiveCamera()->Dolly(1.2);
    m_renderer->ResetCameraClippingRange();

    vtkNew<vtkRenderWindowInteractor> interactor;
    vtkNew<mInteractorStyle> style;
    style->set_actor(m_obj_actor);
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

    m_render_window->Render();
    cam_orient_manipulator->SquareResize(); // need call here to change the rep anchor position
    interactor->Start();

    return 0;
}