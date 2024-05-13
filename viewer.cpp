#include <vtkSmartPointer.h>
#include <vtkDICOMReader.h>
#include <vtkDICOMSorter.h>
#include <vtkStringArray.h>
#include <vtkNIFTIImageReader.h>
#include <vtkImageViewer2.h>
#include <vtkRenderWindowInteractor.h>
#include <vtkImageData.h>
#include <vtkInteractorStyleImage.h>

#include <vtkCornerAnnotation.h>
#include <vtkTextProperty.h>
#include <vtkCallbackCommand.h>
#include <vtkRenderer.h>
#include <vtkRenderWindow.h>
#include <vtkObjectFactory.h>

#include <filesystem>
#include <iostream>
#include <sstream>

class myInteractorStyler final: public vtkInteractorStyleImage
{
public:
    static myInteractorStyler* New();

    vtkTypeMacro(myInteractorStyler, vtkInteractorStyleImage);

    void setImageViewer(vtkImageViewer2* imageViewer, int slice_no = 0)
    {
        m_viewer = imageViewer;
        m_slice_min = imageViewer->GetSliceMin();
        m_slice_max = imageViewer->GetSliceMax();
        m_slice = slice_no <= 0 ? (m_slice_min + m_slice_max) / 2 : slice_no;
        m_viewer->SetSlice(m_slice);
    }

protected:
    void OnMouseWheelForward() override { moveSliceForward(); }

    void OnMouseWheelBackward() override { moveSliceBackward(); }

private:
    void moveSliceForward()
    {
        if (m_slice < m_slice_max)
        {
            m_slice += 1;

            m_viewer->SetSlice(m_slice);
            m_viewer->Render();
        }
        std::cout << m_slice << '\n';
    }

    void moveSliceBackward()
    {
        if (m_slice > m_slice_min)
        {
            m_slice -= 1;

            m_viewer->SetSlice(m_slice);
            m_viewer->Render();
        }
        std::cout << m_slice << '\n';
    }

    void OnChar() override
    {
        if (Interactor->GetKeyCode() == 'x')
            m_viewer->SetSliceOrientationToYZ();
        else if (Interactor->GetKeyCode() == 'y')
            m_viewer->SetSliceOrientationToXZ();
        else if (Interactor->GetKeyCode() == 'z')
            m_viewer->SetSliceOrientationToXY();
    }

private:
    vtkImageViewer2* m_viewer;
    int m_slice;
    int m_slice_min;
    int m_slice_max;
};
vtkStandardNewMacro(myInteractorStyler);

int main(int argc, char* argv[])
{
    if (argc != 2)
    {
        std::cerr << "1) dicom or nii file path" << std::endl;
        return EXIT_FAILURE;
    }

    vtkSmartPointer<vtkImageData> img_data = nullptr;

    std::filesystem::path dir_path{argv[1]};
    if (std::filesystem::is_directory(dir_path))
    {
        vtkNew<vtkDICOMReader> dicom_reader;
        vtkNew<vtkStringArray> dicom_img_paths;
        vtkNew<vtkDICOMSorter> sorter;
        int count = 0;
        for (auto const& it : std::filesystem::directory_iterator(dir_path))
            dicom_img_paths->InsertValue(count++, it.path().string());
        sorter->SetInputFileNames(dicom_img_paths);
        sorter->Update();
        //dicom_reader->SetMemoryRowOrderToFileNative();
        dicom_reader->SetFileNames(sorter->GetFileNamesForSeries(0));
        dicom_reader->SetDataByteOrderToLittleEndian();
        dicom_reader->Update(0);
        img_data = dicom_reader->GetOutput();
    }
    else if (dir_path.extension() == ".nii" || dir_path.extension() == ".gz")
    {
        vtkNew<vtkNIFTIImageReader> nii_reader;
        if (!nii_reader->CanReadFile(dir_path.string().c_str()))
        {
            std::cerr << "ERROR: vtk NIFTI image reader cannot read the provided file: " << dir_path << std::endl;
            return EXIT_FAILURE;
        }
        nii_reader->SetFileName(dir_path.string().c_str());
        nii_reader->Update();
        img_data = nii_reader->GetOutput();
    }

    std::cout << img_data->GetScalarTypeAsString() << std::endl;
    std::cout << img_data->GetDimensions()[0] << ", " << img_data->GetDimensions()[1] << ", "
              << img_data->GetDimensions()[2] << std::endl;

    std::cout << "image data scalar range: " << img_data->GetScalarRange()[0] << ", " << img_data->GetScalarRange()[1]
              << '\n';

    vtkNew<vtkImageViewer2> viewer;
    viewer->SetInputData(img_data);
    viewer->SetSliceOrientationToXY();

    vtkNew<vtkRenderWindowInteractor> interactor;
    vtkNew<myInteractorStyler> style;
    style->setImageViewer(viewer);
    viewer->SetupInteractor(interactor);
    interactor->SetInteractorStyle(style);

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
    viewer->GetRenderer()->AddViewProp(corner_overlay);
    viewer->GetRenderer()->AddObserver(vtkCommand::EndEvent, fps_callback);

    viewer->GetRenderWindow()->SetWindowName("viewer");
    viewer->GetRenderWindow()->SetSize(500, 500);
    viewer->Render();
    interactor->Start();

    return 0;
}