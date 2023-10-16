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

//#define REQUIRE_TRANSFORM_AXIS

//#define WITH_BLEND

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

#ifdef REQUIRE_TRANSFORM_AXIS
    // my sample nii changed original dicom oreitation (simpleITK), so have to transform it here
    // transform nii from zxy to xyz
    vtkNew<vtkImagePermute> permute;
    permute->SetInputData(nii_img_data);
    permute->SetFilteredAxes(1, 2, 0);
    permute->Update();
    auto mask_img = permute->GetOutput();
    std::cout << mask_img->GetDimensions()[0] << ", " << mask_img->GetDimensions()[1] << ", "
              << mask_img->GetDimensions()[2] << std::endl;
#else
    auto mask_img = nii_img_data;
#endif

    vtkNew<vtkImageCast> mask_cast;
    mask_cast->SetInputData(mask_img); // nii_img_data
    mask_cast->SetOutputScalarTypeToUnsignedChar();
    mask_cast->Update();
    auto mask_img_data = mask_cast->GetOutput();
    std::cout << mask_img_data->GetScalarTypeAsString() << std::endl; // unsigned char

    vtkNew<vtkImageMask> mask;
    mask->SetImageInputData(dicom_img_data);
    mask->SetMaskInputData(mask_img_data);
    mask->SetNotMask(true);
    mask->SetMaskedOutputValue(-1e5);
    mask->Update();
    std::cout << mask->GetOutput()->GetScalarTypeAsString() << std::endl; // short

#ifdef WITH_BLEND
    vtkNew<vtkImageCast> img_cast;
    img_cast->SetInputData(mask_img);
    img_cast->SetOutputScalarTypeToShort();
    img_cast->Update();
#endif

    constexpr int window = 1000;
    constexpr int level = 500;

#ifdef IS_RESLICE
    vtkNew<vtkLookupTable> table;
    table->Build();
    table->SetRange(0, 255);
    table->SetNanColor(1, 0, 0, 1);

#ifdef WITH_BLEND
    vtkNew<vtkImageResliceToColors> nii_reslice;
    nii_reslice->SetOutputFormatToRGB();
    nii_reslice->SetLookupTable(table);
    nii_reslice->SetInputData(img_cast->GetOutput());
    nii_reslice->Update();
#endif

    vtkNew<vtkImageResliceToColors> masked_reslice;
    masked_reslice->SetOutputFormatToRGB();
    masked_reslice->SetLookupTable(table);
    masked_reslice->SetInputData(mask->GetOutput());
    masked_reslice->Update();
#endif // IS_RESLICE

#ifdef WITH_BLEND
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
#endif

    vtkNew<vtkImageViewer2> viewer;
#ifdef WITH_BLEND
    viewer->SetInputConnection(blender->GetOutputPort());
#else
    viewer->SetInputConnection(masked_reslice->GetOutputPort());
#endif
    viewer->GetWindowLevel()->SetWindow(window);
    viewer->GetWindowLevel()->SetLevel(level);
    viewer->SetSliceOrientationToXY();

    vtkNew<vtkRenderWindowInteractor> interactor;
    vtkNew<myInteractorStyler> style;
    style->setImageViewer(viewer);
    viewer->SetupInteractor(interactor);
    interactor->SetInteractorStyle(style);

    viewer->Render();
    interactor->Start();

    return 0;
}