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

#include <filesystem>
#include <iostream>

#define IS_RESLICE

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
    std::cout << dicom_reader->GetOutput()->GetScalarTypeAsString() << std::endl; // short
    std::cout << dicom_reader->GetOutput()->GetDimensions()[0] << ", " << dicom_reader->GetOutput()->GetDimensions()[1]
              << ", " << dicom_reader->GetOutput()->GetDimensions()[2] << std::endl;

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

    // my sample nii changed original dicom oreitation, so have to transform it here
    // transform nii from zyx to xyz
    vtkNew<vtkImagePermute> permute;
    permute->SetInputData(nii_img_data);
    permute->SetFilteredAxes(2, 1, 0);
    permute->Update();
    auto mask_img = permute->GetOutput();

    vtkNew<vtkImageCast> mask_cast;
    mask_cast->SetInputData(mask_img); // nii_img_data
    mask_cast->SetOutputScalarTypeToUnsignedChar();
    mask_cast->Update();
    auto mask_img_data = mask_cast->GetOutput();
    std::cout << mask_img_data->GetScalarTypeAsString() << std::endl; // unsigned char

    vtkNew<vtkImageMask> mask;
    mask->SetImageInputData(dicom_reader->GetOutput());
    mask->SetMaskInputData(mask_img_data);
    mask->SetNotMask(true);
    mask->Update();
    std::cout << mask->GetOutput()->GetScalarTypeAsString() << std::endl; // short

    vtkNew<vtkImageCast> img_cast;
    img_cast->SetInputData(nii_reader->GetOutput());
    img_cast->SetOutputScalarTypeToShort();
    img_cast->Update();

#ifdef IS_RESLICE
    vtkNew<vtkLookupTable> table;
    table->Build();
    table->SetRange(0, 255);

    vtkNew<vtkImageResliceToColors> nii_reslice;
    nii_reslice->SetOutputFormatToRGB();
    nii_reslice->SetLookupTable(table);
    nii_reslice->SetInputData(img_cast->GetOutput());
    nii_reslice->Update();

    vtkNew<vtkImageResliceToColors> masked_reslice;
    masked_reslice->SetOutputFormatToRGB();
    masked_reslice->SetLookupTable(table);
    masked_reslice->SetInputData(mask->GetOutput());
    masked_reslice->Update();
#endif // IS_RESLICE

    vtkNew<vtkImageBlend> blender;
#ifdef IS_RESLICE
    blender->AddInputConnection(masked_reslice->GetOutputPort());
    blender->AddInputConnection(nii_reslice->GetOutputPort());
#else
    blender->AddInputConnection(mask->GetOutputPort());
    blender->AddInputConnection(img_cast->GetOutputPort());
#endif // IS_RESLICE

    blender->SetOpacity(0, 0.5);
    blender->SetOpacity(1, 0.5);

    vtkNew<vtkImageViewer2> viewer;
    viewer->SetInputConnection(blender->GetOutputPort());
    viewer->GetWindowLevel()->SetWindow(500);
    viewer->SetSliceOrientationToYZ();

    vtkNew<vtkRenderWindowInteractor> interactor;
    vtkNew<myInteractorStyler> style;
    style->setImageViewer(viewer);
    viewer->SetupInteractor(interactor);
    interactor->SetInteractorStyle(style);

    viewer->Render();
    interactor->Start();

    return 0;
}