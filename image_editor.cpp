#include <vtkNew.h>
#include <vtkImageReader2Factory.h>
#include <vtkImageReader2.h>
#include <vtkImageActor.h>
#include <vtkImageMapper3D.h>
#include <vtkRenderer.h>
#include <vtkNamedColors.h>
#include <vtkRenderWindow.h>
#include <vtkRenderWindowInteractor.h>
#include <vtkInteractorStyleImage.h>

#include <vtkBorderRepresentation.h>
#include <vtkBorderWidget.h>
#include <vtkCommand.h>
#include <vtkImageClip.h>
#include <vtkMath.h>
#include <vtkProperty2D.h>
#include <vtkInformation.h>
#include <vtkStreamingDemandDrivenPipeline.h>

#include <vtkTransform.h>
#include <vtkImageReslice.h>
#include <vtkImageData.h>

#include <vtkCamera.h>

#include <vtkCallbackCommand.h>

class MInteractorStyleImage: public vtkInteractorStyleImage
{
public:
    MInteractorStyleImage() = default;

    static MInteractorStyleImage* New() { return new MInteractorStyleImage; }
    vtkTypeMacro(MInteractorStyleImage, vtkInteractorStyleImage);

    void SetImageTransform(vtkSmartPointer<vtkTransform> transform, double center[3])
    {
        m_transfrom = transform;
        memcpy(m_center, center, sizeof(double) * 3);
    }

    void SetBorderWidget(vtkSmartPointer<vtkBorderWidget> border_widget) { m_border_widget = border_widget; }

    //// disable left mouse window level function
    //void WindowLevel() override {}

    // disable left mouse for window level and rotation
    void OnLeftButtonDown() override {}
    void OnLeftButtonUp() override {}

    void OnMiddleButtonUp() override
    {
        vtkInteractorStyleImage::OnMiddleButtonUp();
        // sync zoom and translation
        m_border_widget->InvokeEvent(vtkCommand::InteractionEvent);
        GetInteractor()->GetRenderWindow()->Render();
    }
    void OnRightButtonUp() override
    {
        vtkInteractorStyleImage::OnRightButtonUp();
        // sync zoom and translation
        m_border_widget->InvokeEvent(vtkCommand::InteractionEvent);
        GetInteractor()->GetRenderWindow()->Render();
    }

    void OnChar() override
    {
        auto* rwi = GetInteractor();
        auto key = rwi->GetKeyCode();
        if (key == 'r')
        {
            m_transfrom->Translate(m_center[0], m_center[1], m_center[2]);
            m_transfrom->RotateWXYZ(10, 0, 0, 1);
            m_transfrom->Translate(-m_center[0], -m_center[1], -m_center[2]);
            m_transfrom->Update();

            rwi->GetRenderWindow()->Render();
        }
    }

private:
    vtkSmartPointer<vtkTransform> m_transfrom;
    double m_center[3]{};
    vtkSmartPointer<vtkBorderWidget> m_border_widget;
};

class MBorderCallback: public vtkCommand
{
public:
    MBorderCallback() = default;

    static MBorderCallback* New() { return new MBorderCallback; }
    vtkTypeMacro(MBorderCallback, vtkCommand);

    void SetRenderer(vtkSmartPointer<vtkRenderer> renderer) { m_renderer = renderer; }
    void SetImageActor(vtkSmartPointer<vtkImageActor> actor) { m_img_actor = actor; }
    void SetClipFilter(vtkSmartPointer<vtkImageClip> clip) { m_clip_filter = clip; }

    void Execute(vtkObject* caller, unsigned long, void*) override
    {
        vtkBorderWidget* border_widget = reinterpret_cast<vtkBorderWidget*>(caller);

        // Get the world coordinates of the two corners of the box.
        // use parallel projection camera instead of perspective to avoid xy positions changing by z
        // or use camera's transformation matrix to compute real xy?
        auto lower_left_coord =
            static_cast<vtkBorderRepresentation*>(border_widget->GetRepresentation())->GetPositionCoordinate();
        auto lower_left = lower_left_coord->GetComputedWorldValue(m_renderer);
        std::cout << "Lower left coordinate: " << lower_left[0] << ", " << lower_left[1] << ", " << lower_left[2]
                  << std::endl;
        lower_left[2] = 0;

        auto upper_right_coord =
            static_cast<vtkBorderRepresentation*>(border_widget->GetRepresentation())->GetPosition2Coordinate();
        auto upper_right = upper_right_coord->GetComputedWorldValue(m_renderer);
        std::cout << "Upper right coordinate: " << upper_right[0] << ", " << upper_right[1] << ", " << upper_right[2]
                  << std::endl;
        upper_right[2] = 0;

        const double* bounds = m_img_actor->GetBounds();
        double xmin = bounds[0];
        double xmax = bounds[1];
        double ymin = bounds[2];
        double ymax = bounds[3];

        //std::cout << xmin << ", " << xmax << ", " << ymin << ", " << ymax << std::endl;

        if ((lower_left[0] > xmin) && (upper_right[0] < xmax) && (lower_left[1] > ymin) && (upper_right[1] < ymax))
            m_clip_filter->SetOutputWholeExtent(vtkMath::Round(lower_left[0]), vtkMath::Round(upper_right[0]),
                                                vtkMath::Round(lower_left[1]), vtkMath::Round(upper_right[1]), 0, 1);
        else
            std::cout << "box is NOT inside image" << std::endl;
    }

private:
    vtkSmartPointer<vtkRenderer> m_renderer;
    vtkSmartPointer<vtkImageActor> m_img_actor;
    vtkSmartPointer<vtkImageClip> m_clip_filter;
};

class MouseWheelCallback final: public vtkCallbackCommand
{
public:
    static MouseWheelCallback* New() { return new MouseWheelCallback; }
    vtkTypeMacro(MouseWheelCallback, vtkCallbackCommand);

    void SetBorderWidget(vtkSmartPointer<vtkBorderWidget> border_widget) { m_border_widget = border_widget; }

    void Execute(vtkObject* caller, unsigned long evId, void*) override
    {
        m_border_widget->InvokeEvent(vtkCommand::InteractionEvent);
        m_border_widget->GetInteractor()->Render();
    }

private:
    vtkSmartPointer<vtkBorderWidget> m_border_widget;
};

int main(int argc, char* argv[])
{
    if (argc < 2)
    {
        std::cout << "Usage: " << argv[0] << " InputFilename e.g. Gourds.png" << std::endl;
        return EXIT_FAILURE;
    }

    vtkNew<vtkImageReader2Factory> reader_factory;
    auto reader = vtkSmartPointer<vtkImageReader2>(reader_factory->CreateImageReader2(argv[1]));
    reader->SetFileName(argv[1]);
    reader->Update();

    vtkNew<vtkNamedColors> colors;

    vtkNew<vtkRenderWindowInteractor> interactor;
    vtkNew<MInteractorStyleImage> style;
    interactor->SetInteractorStyle(style);

    vtkNew<vtkBorderWidget> border_widget;
    auto* border_widget_rep = static_cast<vtkBorderRepresentation*>(border_widget->GetRepresentation());
    double pos[4]{0.4, 0.4, 0, 0};  // normalized viewport
    double pos2[4]{0.1, 0.1, 0, 0}; // relative to pos
    border_widget_rep->SetPosition(pos);
    border_widget_rep->SetPosition2(pos2);
    border_widget->SetInteractor(interactor);
    static_cast<vtkBorderRepresentation*>(border_widget->GetRepresentation())
        ->GetBorderProperty()
        ->SetColor(colors->GetColor3d("Lime").GetData());
    border_widget->SelectableOff();

    double bounds[6];
    reader->GetOutput()->GetBounds(bounds);

    vtkNew<vtkTransform> transform;
    double center[3];
    center[0] = (bounds[1] + bounds[0]) / 2.0;
    center[1] = (bounds[3] + bounds[2]) / 2.0;
    center[2] = (bounds[5] + bounds[4]) / 2.0;

    // Rotate about the center
    transform->Translate(center[0], center[1], center[2]);
    transform->RotateWXYZ(0, 0, 0, 1);
    transform->Translate(-center[0], -center[1], -center[2]);

    style->SetImageTransform(transform, center);
    style->SetBorderWidget(border_widget);

    vtkNew<vtkImageReslice> reslice;
    reslice->SetInputConnection(reader->GetOutputPort());
    reslice->SetResliceTransform(transform);
    reslice->SetInterpolationModeToCubic();
    reslice->SetOutputSpacing(reader->GetOutput()->GetSpacing()[0], reader->GetOutput()->GetSpacing()[1],
                              reader->GetOutput()->GetSpacing()[2]);
    reslice->SetOutputOrigin(reader->GetOutput()->GetOrigin()[0], reader->GetOutput()->GetOrigin()[1],
                             reader->GetOutput()->GetOrigin()[2]);
    reslice->SetOutputExtent(reader->GetOutput()->GetExtent()); // Use a larger extent than the
                                                                // original image's to prevent clipping.

    vtkNew<vtkImageActor> actor;
    actor->GetMapper()->SetInputConnection(reslice->GetOutputPort());

    vtkNew<vtkImageClip> image_clip;
    image_clip->SetInputConnection(reslice->GetOutputPort());
    reslice->UpdateInformation();
    image_clip->SetOutputWholeExtent(
        reslice->GetOutputInformation(0)->Get(vtkStreamingDemandDrivenPipeline::WHOLE_EXTENT()));
    image_clip->ClipDataOn();

    vtkNew<vtkImageActor> clip_actor;
    clip_actor->GetMapper()->SetInputConnection(image_clip->GetOutputPort());

    // Define viewport ranges in normalized coordinates.
    // (xmin, ymin, xmax, ymax)
    double left_viewport[4] = {0.0, 0.0, 0.5, 1.0};
    double right_viewport[4] = {0.5, 0.0, 1.0, 1.0};

    vtkNew<vtkRenderer> left_renderer;
    left_renderer->SetViewport(left_viewport);
    left_renderer->AddActor(actor);
    left_renderer->SetBackground(colors->GetColor3d("DarkSlateGray").GetData());
    left_renderer->ResetCamera();
    // use parallel projection instead of perspective to avoid xy positions changing by z
    left_renderer->GetActiveCamera()->SetParallelProjection(true);

    vtkNew<vtkRenderer> right_renderer;
    right_renderer->SetViewport(right_viewport);
    right_renderer->AddActor(clip_actor);
    right_renderer->SetBackground(colors->GetColor3d("DimGray").GetData());
    right_renderer->ResetCamera();
    right_renderer->GetActiveCamera()->SetParallelProjection(true);

    vtkNew<MBorderCallback> border_callback;
    border_callback->SetRenderer(left_renderer);
    border_callback->SetImageActor(actor);
    border_callback->SetClipFilter(image_clip);

    border_widget->AddObserver(vtkCommand::InteractionEvent, border_callback);

    vtkNew<MouseWheelCallback> mouse_wheel_callback;
    mouse_wheel_callback->SetBorderWidget(border_widget);

    interactor->AddObserver(vtkCommand::MouseWheelForwardEvent, mouse_wheel_callback);
    interactor->AddObserver(vtkCommand::MouseWheelBackwardEvent, mouse_wheel_callback);

    vtkNew<vtkRenderWindow> render_window;
    render_window->AddRenderer(left_renderer);
    render_window->AddRenderer(right_renderer);
    render_window->SetWindowName("Image Editor");

    interactor->SetRenderWindow(render_window);
    render_window->Render();

    border_widget->On();
    interactor->Start();

    return 0;
}