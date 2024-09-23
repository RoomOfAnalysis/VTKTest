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

#include <vtkCamera.h>
#include <vtkRendererCollection.h>

#include <vtkImageMagnify.h>
#include <vtkImageChangeInformation.h>

class MInteractorStyleImage: public vtkInteractorStyleImage
{
public:
    MInteractorStyleImage() = default;

    static MInteractorStyleImage* New() { return new MInteractorStyleImage; }
    vtkTypeMacro(MInteractorStyleImage, vtkInteractorStyleImage);

    // disable left mouse window level function
    void WindowLevel() override {}

    //// disable left mouse for window level and rotation
    //void OnLeftButtonDown() override {}
    //void OnLeftButtonUp() override {}

    void OnChar() override
    {
        auto* rwi = GetInteractor();
        auto key = rwi->GetKeyCode();
        if (key == 'r')
        {
            rwi->GetRenderWindow()->GetRenderers()->GetFirstRenderer()->ResetCamera();
            rwi->GetRenderWindow()->GetRenderers()->GetFirstRenderer()->GetActiveCamera()->SetRoll(0);
            rwi->GetRenderWindow()->Render();
        }
    }
};

int main(int argc, char* argv[])
{
    if (argc < 3)
    {
        std::cout << "Usage: " << argv[0] << " InputFilename1 e.g. Gourds1.png; InputFilename2 e.g. Gourds2.png"
                  << std::endl;
        return EXIT_FAILURE;
    }

    vtkNew<vtkImageReader2Factory> reader_factory;
    auto reader = vtkSmartPointer<vtkImageReader2>(reader_factory->CreateImageReader2(argv[1]));
    reader->SetFileName(argv[1]);
    reader->Update();

    auto reader2 = vtkSmartPointer<vtkImageReader2>(reader_factory->CreateImageReader2(argv[2]));
    reader2->SetFileName(argv[2]);
    reader2->Update();

    vtkNew<vtkNamedColors> colors;

    vtkNew<vtkRenderWindowInteractor> interactor;
    vtkNew<MInteractorStyleImage> style;
    interactor->SetInteractorStyle(style);

    vtkNew<vtkImageActor> actor;
    actor->GetMapper()->SetInputConnection(reader->GetOutputPort());

    // resample to the same resolution
    // require both image have the same aspect ratio
    int extents[6]{}, extents2[6]{};
    reader->GetDataExtent(extents);
    reader2->GetDataExtent(extents2);
    int xyz[3] = {extents[1] - extents[0], extents[3] - extents[2], extents[5] - extents[4]};
    int xyz2[3] = {extents2[1] - extents2[0], extents2[3] - extents2[2], extents2[5] - extents2[4]};

    //vtkNew<vtkImageMagnify> magnify_filter;
    //magnify_filter->SetInputConnection(reader2->GetOutputPort());
    //magnify_filter->SetMagnificationFactors(xyz[0] / xyz2[0], xyz[1] / xyz2[1], 1);
    //magnify_filter->Update();

    vtkNew<vtkImageChangeInformation> change_filter;
    //change_filter->SetInputConnection(magnify_filter->GetOutputPort());
    //change_filter->SetSpacingScale(magnify_filter->GetMagnificationFactors()[0],
    //                               magnify_filter->GetMagnificationFactors()[1],
    //                               magnify_filter->GetMagnificationFactors()[2]);
    change_filter->SetInputConnection(reader2->GetOutputPort());
    change_filter->SetSpacingScale(xyz[0] / xyz2[0], xyz[1] / xyz2[1], 1);

    vtkNew<vtkImageActor> actor2;
    actor2->GetMapper()->SetInputConnection(change_filter->GetOutputPort());

    // Define viewport ranges in normalized coordinates.
    // (xmin, ymin, xmax, ymax)
    double left_viewport[4] = {0.0, 0.0, 0.5, 1.0};
    double right_viewport[4] = {0.5, 0.0, 1.0, 1.0};

    vtkNew<vtkRenderer> left_renderer;
    left_renderer->SetViewport(left_viewport);
    left_renderer->AddActor(actor);
    left_renderer->SetBackground(colors->GetColor3d("DarkSlateGray").GetData());
    left_renderer->ResetCamera();

    vtkNew<vtkRenderer> right_renderer;
    right_renderer->SetViewport(right_viewport);
    right_renderer->AddActor(actor2);
    right_renderer->SetBackground(colors->GetColor3d("DimGray").GetData());
    right_renderer->SetActiveCamera(left_renderer->GetActiveCamera());

    vtkNew<vtkRenderWindow> render_window;
    render_window->AddRenderer(left_renderer);
    render_window->AddRenderer(right_renderer);
    render_window->SetWindowName("Image Editor");

    interactor->SetRenderWindow(render_window);
    render_window->Render();

    interactor->Start();

    return 0;
}