#include <vtkSmartPointer.h>
#include <vtkDICOMReader.h>
#include <vtkDICOMSorter.h>
#include <vtkStringArray.h>
#include <vtkNIFTIImageReader.h>
#include <vtkImageMask.h>
#include <vtkImageViewer2.h>
#include <vtkRenderWindowInteractor.h>
#include <vtkImageData.h>
#include <vtkImageCast.h>
#include <vtkInteractorStyleImage.h>
#include <vtkObjectFactory.h>
#include <vtkImageMapToWindowLevelColors.h>
#include <vtkImageBlend.h>
#include <vtkImagePermute.h>
#include <vtkLookupTable.h>
#include <vtkImageResliceToColors.h>

#include <vtkCornerAnnotation.h>
#include <vtkTextProperty.h>
#include <vtkCallbackCommand.h>
#include <vtkRenderer.h>
#include <vtkRenderWindow.h>

#include <filesystem>
#include <iostream>
#include <sstream>

class myInteractorStyler final: public vtkInteractorStyleImage
{
public:
    static myInteractorStyler* New();

    vtkTypeMacro(myInteractorStyler, vtkInteractorStyleImage);

    void setImageViewer(vtkImageViewer2* imageViewer)
    {
        m_viewer = imageViewer;
        m_slice_min = imageViewer->GetSliceMin();
        m_slice_max = imageViewer->GetSliceMax();
        m_slice = (m_slice_min + m_slice_max) / 2;
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

private:
    vtkImageViewer2* m_viewer;
    int m_slice;
    int m_slice_min;
    int m_slice_max;
};
vtkStandardNewMacro(myInteractorStyler);

int main(int argc, char* argv[])
{
    if (argc != 3)
    {
        std::cerr << "1) dicom dir path; 2) nii file path" << std::endl;
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
    auto dicom_img_data = dicom_reader->GetOutput();
    std::cout << dicom_img_data->GetScalarTypeAsString() << std::endl; // short
    std::cout << dicom_img_data->GetDimensions()[0] << ", " << dicom_img_data->GetDimensions()[1] << ", "
              << dicom_img_data->GetDimensions()[2] << std::endl;

    std::filesystem::path nii_file_path{argv[2]};
    vtkNew<vtkNIFTIImageReader> nii_reader;
    if (!nii_reader->CanReadFile(nii_file_path.string().c_str()))
    {
        std::cerr << "ERROR: vtk NIFTI image reader cannot read the provided file: " << nii_file_path << std::endl;
        return EXIT_FAILURE;
    }
    nii_reader->SetFileName(nii_file_path.string().c_str());
    nii_reader->Update();
    auto nii_img_data = nii_reader->GetOutput();
    std::cout << nii_img_data->GetScalarTypeAsString() << std::endl; // double
    std::cout << nii_img_data->GetDimensions()[0] << ", " << nii_img_data->GetDimensions()[1] << ", "
              << nii_img_data->GetDimensions()[2] << std::endl;

    auto mask_img = nii_img_data;

    vtkNew<vtkImageCast> mask_cast;
    mask_cast->SetInputData(mask_img); // nii_img_data
    mask_cast->SetOutputScalarTypeToUnsignedChar();
    mask_cast->Update();
    auto mask_img_data = mask_cast->GetOutput();
    std::cout << mask_img_data->GetScalarTypeAsString() << std::endl; // unsigned char

    vtkNew<vtkImageCast> img_cast;
    img_cast->SetInputData(mask_img);
    img_cast->SetOutputScalarTypeToShort();
    img_cast->Update();

    constexpr int window = 1400;
    constexpr int level = -500;

    std::cout << "dicom image data scalar range: " << dicom_img_data->GetScalarRange()[0] << ", "
              << dicom_img_data->GetScalarRange()[1] << '\n';
    vtkNew<vtkLookupTable> dicom_table;
    dicom_table->SetRange(level - window / 2.0, level + window / 2.0); // L - W/2.0, L + W/2.0
    dicom_table->SetValueRange(0, 1);
    dicom_table->SetSaturationRange(0, 0);
    dicom_table->SetRampToLinear();
    dicom_table->Build();

    vtkNew<vtkImageResliceToColors> dicom_reslice;
    dicom_reslice->SetOutputFormatToRGB();
    dicom_reslice->SetLookupTable(dicom_table);
    dicom_reslice->SetInputData(dicom_img_data);
    dicom_reslice->Update();

    vtkNew<vtkLookupTable> nii_table;
    nii_table->SetNumberOfColors(2);
    nii_table->SetTableRange(img_cast->GetOutput()->GetScalarRange());
    nii_table->SetTableValue(0, 0, 0, 0, 0);
    nii_table->SetTableValue(1, 1, 0, 0, 1);
    nii_table->Build();

    vtkNew<vtkImageResliceToColors> nii_reslice;
    nii_reslice->SetOutputFormatToRGBA();
    nii_reslice->SetLookupTable(nii_table);
    nii_reslice->SetInputData(img_cast->GetOutput());
    nii_reslice->Update();

    vtkNew<vtkImageBlend> blender;
    blender->AddInputConnection(dicom_reslice->GetOutputPort());
    blender->AddInputConnection(nii_reslice->GetOutputPort());
    blender->SetOpacity(1, 1);

    vtkNew<vtkImageViewer2> viewer;
    viewer->SetInputConnection(blender->GetOutputPort());
    // TODO: window/level will affect the displaying color of slices
    //viewer->GetWindowLevel()->SetWindow(window);
    //viewer->GetWindowLevel()->SetLevel(level);
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

    viewer->GetRenderWindow()->SetWindowName("blend");
    viewer->GetRenderWindow()->SetSize(500, 500);
    viewer->Render();
    interactor->Start();

    return 0;
}