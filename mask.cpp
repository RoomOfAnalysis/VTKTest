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
#include <vtkDICOMMetaData.h>
#include <vtkMarchingCubes.h>
#include <vtkCenterOfMass.h>

#include <filesystem>
#include <iostream>

#define IS_RESLICE

//#define REQUIRE_TRANSFORM_AXIS

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

    //from https://dgobbi.github.io/vtk-dicom/doc/api/imageDisplay.html
    //MONOCHROME1 - negative image, where higher values are darker (e.g. radiographic film )
    //MONOCHROME2 - positive image, where higher values are brighter (e.g. CT, MR)
    //PALETTE COLOR - indexed color with palette
    //RGB - full-color image with separate RGB components
    auto* meta = dicom_reader->GetMetaData();
    auto photometric = meta->Get(DC::PhotometricInterpretation);
    if (photometric.Matches("MONOCHROME1"))
    {
        // display with a lookup table that goes from white to black
        std::cout << "MONOCHROME1" << std::endl;
    }
    else if (photometric.Matches("MONOCHROME2"))
    {
        // display with a lookup table that goes from black to white,
        // or display with a suitable pseudocolor lookup table
        std::cout << "MONOCHROME2" << std::endl;
    }
    else if (photometric.Matches("PALETTE*"))
    {
        // display with palette lookup table (see vtkDICOMLookupTable),
        // or convert to RGB with vtkDICOMApplyPalette
        std::cout << "PALETTE*" << std::endl;
    }
    else if (photometric.Matches("RGB*"))
    {
        // display RGB data directly
        std::cout << "RGB*" << std::endl;
    }
    else if (photometric.Matches("YBR*"))
    {
        // display RGB data directly
        std::cout << "YBR*" << std::endl;
    }
    //int dims[3];
    //dicom_img_data->GetDimensions(dims);
    //int min = INT_MAX, max = INT_MIN;
    //for (auto i = 0; i < dims[0]; i++)
    //    for (auto j = 0; j < dims[1]; j++)
    //        for (auto k = 0; k < dims[2]; k++)
    //        {
    //            min = min > dicom_img_data->GetScalarComponentAsDouble(i, j, k, 0) ?
    //                      dicom_img_data->GetScalarComponentAsDouble(i, j, k, 0) :
    //                      min;
    //            max = max < dicom_img_data->GetScalarComponentAsDouble(i, j, k, 0) ?
    //                      dicom_img_data->GetScalarComponentAsDouble(i, j, k, 0) :
    //                      max;
    //        }
    //std::cout << min << ", " << max << std::endl; // -1024 3071

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

    // find mask shape center
    vtkNew<vtkMarchingCubes> surface;
    surface->SetInputData(nii_img_data);
    surface->ComputeNormalsOn();
    surface->SetValue(0, 255);
    surface->Update();
    vtkNew<vtkCenterOfMass> com;
    com->SetInputData(surface->GetOutput());
    com->SetUseScalarsAsWeights(true);
    com->Update();
    double center[3];
    com->GetCenter(center);
    // FIXME: z position is correct, but x, y positions require calibration
    std::cout << "center before calibration: " << center[0] << ", " << center[1] << ", " << center[2] << std::endl;
    auto* dims = dicom_img_data->GetDimensions();
    center[0] *= dims[0] / (double)dims[2];
    center[1] *= dims[1] / (double)dims[2];
    std::cout << "center after calibration: " << center[0] << ", " << center[1] << ", " << center[2] << std::endl;

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
    mask->SetMaskedOutputValue(3072);
    mask->Update();
    std::cout << mask->GetOutput()->GetScalarTypeAsString() << std::endl; // short

#ifdef IS_RESLICE
    vtkNew<vtkImageResliceToColors> dicom_reslice;
    dicom_reslice->BypassOn(); // without lookup table
    dicom_reslice->SetOutputFormatToRGB();
    dicom_reslice->SetInputData(mask->GetOutput());
    dicom_reslice->Update();
#endif // IS_RESLICE

    vtkNew<vtkImageViewer2> viewer;
    viewer->SetInputConnection(dicom_reslice->GetOutputPort());
    viewer->SetSliceOrientationToXY();
    viewer->SetSlice(center[2]); // set to mask shape center

    vtkNew<vtkRenderWindowInteractor> interactor;
    vtkNew<myInteractorStyler> style;
    style->setImageViewer(viewer);
    viewer->SetupInteractor(interactor);
    interactor->SetInteractorStyle(style);

    viewer->Render();
    interactor->Start();

    return 0;
}